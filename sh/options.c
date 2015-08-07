/*	$FreeBSD: head/bin/sh/options.c 265849 2014-05-10 19:18:49Z jilles $	*/
/*	static char sccsid[] = "@(#)options.c	8.2 (Berkeley) 5/4/95";	*/
/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include "shell.h"
#define DEFINE_OPTIONS
#include "options.h"
#undef DEFINE_OPTIONS
#include "nodes.h"	/* for other header files */
#include "eval.h"
#include "jobs.h"
#include "input.h"
#include "output.h"
#include "trap.h"
#include "var.h"
#include "memalloc.h"
#include "sherror.h"
#include "mystring.h"
#include "builtins.h"
#ifndef NO_HISTORY
#include "myhistedit.h"
#endif

cstring_t arg0;			/* value of $0 */
struct shparam shellparam;	/* current positional parameters */
cstring_t* argptr;			/* argument list for builtin commands */
cstring_t shoptarg;			/* set by nextopt (like getopt) */
cstring_t nextopt_optptr;		/* used by nextopt */

cstring_t minusc;			/* argument to -c option */


static void options(int32_t);
static void minus_o(cstring_t, int32_t);
static void setoption(int32_t, int32_t);
static int32_t getopts(cstring_t, cstring_t, cstring_t*, cstring_t**, cstring_t*);


/*
 * Process the shell command line arguments.
 */

void
procargs(int32_t argc, cstring_t* argv)
{
	int32_t i;
	cstring_t scriptname;
	argptr = argv;
	if (argc > 0)
		argptr++;
	for (i = 0; i < NOPTS; i++)
		optlist[i].val = 2;
	privileged = (getuid() != geteuid() || getgid() != getegid());
	options(1);
	if (*argptr == NULL && minusc == NULL)
		sflag = 1;
	if (iflag != 0 && sflag == 1 && isatty(0) && isatty(1))
	{
		iflag = 1;
		if (Eflag == 2)
			Eflag = 1;
	}
	if (mflag == 2)
		mflag = iflag;
	for (i = 0; i < NOPTS; i++)
		if (optlist[i].val == 2)
			optlist[i].val = 0;
	arg0 = argv[0];
	if (sflag == 0 && minusc == NULL)
	{
		scriptname = *argptr++;
		setinputfile(scriptname, 0);
		commandname = arg0 = scriptname;
	}
	/* POSIX 1003.2: first arg after -c cmd is $0, remainder $1... */
	if (argptr && minusc && *argptr)
		arg0 = *argptr++;
	shellparam.p = argptr;
	shellparam.reset = 1;
	/* assert(shellparam.malloc == 0 && shellparam.nparam == 0); */
	while (*argptr)
	{
		shellparam.nparam++;
		argptr++;
	}
	optschanged();
}


void
optschanged(void)
{
	setinteractive(iflag);
#ifndef NO_HISTORY
	histedit();
#endif
	setjobctl(mflag);
}

/*
 * Process shell options.  The global variable argptr contains a pvoid_t
 * to the argument list; we advance it past the options.
 */

static void
options(int32_t cmdline)
{
	cstring_t kp;
	cstring_t p;
	int32_t val;
	int32_t c;

	if (cmdline)
		minusc = NULL;
	while ((p = *argptr) != NULL)
	{
		argptr++;
		if ((c = *p++) == '-')
		{
			val = 1;
			/* A "-" or  "--" terminates options */
			if (p[0] == '\0')
				goto end_options1;
			if (p[0] == '-' && p[1] == '\0')
				goto end_options2;
			/**
			 * For the benefit of `#!' lines in shell scripts,
			 * treat a string of '-- *#.*' the same as '--'.
			 * This is needed so that a script starting with:
			 *	#!/bin/sh -- # -*- perl -*-
			 * will continue to work after a change is made to
			 * kern/imgact_shell.c to NOT token-ize the options
			 * specified on a '#!' line.  A bit of a kludge,
			 * but that trick is recommended in documentation
			 * for some scripting languages, and we might as
			 * well continue to support it.
			 */
			if (p[0] == '-')
			{
				kp = p + 1;
				while (*kp == ' ' || *kp == '\t')
					kp++;
				if (*kp == '#' || *kp == '\0')
					goto end_options2;
			}
		}
		else if (c == '+')
		{
			val = 0;
		}
		else
		{
			argptr--;
			break;
		}
		while ((c = *p++) != '\0')
		{
			if (c == 'c' && cmdline)
			{
				cstring_t q;
#ifdef NOHACK	/* removing this code allows sh -ce 'foo' for compat */
				if (*p == '\0')
#endif
					q = *argptr++;
				if (q == NULL || minusc != NULL)
					sherror("Bad -c option");
				minusc = q;
#ifdef NOHACK
				break;
#endif
			}
			else if (c == 'o')
			{
				minus_o(*argptr, val);
				if (*argptr)
					argptr++;
			}
			else
				setoption(c, val);
		}
	}
	return;
	/* When processing `set', a single "-" means turn off -x and -v */
end_options1:
	if (!cmdline)
	{
		xflag = vflag = 0;
		return;
	}
	/*
	 * When processing `set', a "--" means the remaining arguments
	 * replace the positional parameters in the active shell.  If
	 * there are no remaining options, then all the positional
	 * parameters are cleared (equivalent to doing ``shift $#'').
	 */
end_options2:
	if (!cmdline)
	{
		if (*argptr == NULL)
			setparam(argptr);
		return;
	}
	/*
	 * At this point we are processing options given to 'sh' on a command
	 * line.  If an end-of-options marker ("-" or "--") is followed by an
	 * arg of "#", then skip over all remaining arguments.  Some scripting
	 * languages (e.g.: perl) document that /bin/sh will implement this
	 * behavior, and they recommend that users take advantage of it to
	 * solve certain issues that can come up when writing a perl script.
	 * Yes, this feature is in /bin/sh to help users write perl scripts.
	 */
	p = *argptr;
	if (p != NULL && p[0] == '#' && p[1] == '\0')
	{
		while (*argptr != NULL)
			argptr++;
		/* We need to keep the final argument */
		argptr--;
	}
}

static void
minus_o(cstring_t name, int32_t val)
{
	int32_t i;
	if (name == NULL)
	{
		if (val)
		{
			/* "Pretty" output. */
			out1str("Current option settings\n");
			for (i = 0; i < NOPTS; i++)
				out1fmt("%-16s%s\n", optlist[i].name,
						optlist[i].val ? "on" : "off");
		}
		else
		{
			/* Output suitable for re-input to shell. */
			for (i = 0; i < NOPTS; i++)
				out1fmt("%s %co %s%s",
						i % 6 == 0 ? "set" : "",
						optlist[i].val ? '-' : '+',
						optlist[i].name,
						i % 6 == 5 || i == NOPTS - 1 ? "\n" : "");
		}
	}
	else
	{
		for (i = 0; i < NOPTS; i++)
			if (equal(name, optlist[i].name))
			{
				setoption(optlist[i].letter, val);
				return;
			}
		sherror("Illegal option -o %s", name);
	}
}


static void
setoption(int32_t flag, int32_t val)
{
	int32_t i;
	if (flag == 'p' && !val && privileged)
	{
		if (setgid(getgid()) == -1)
			sherror("setgid");
		if (setuid(getuid()) == -1)
			sherror("setuid");
	}
	for (i = 0; i < NOPTS; i++)
		if (optlist[i].letter == flag)
		{
			optlist[i].val = val;
			if (val)
			{
				/* #%$ hack for ksh semantics */
				if (flag == 'V')
					Eflag = 0;
				else if (flag == 'E')
					Vflag = 0;
			}
			return;
		}
	sherror("Illegal option -%c", flag);
}


/*
 * Set the shell parameters.
 */

void
setparam(cstring_t* argv)
{
	cstring_t* newparam;
	cstring_t* ap;
	int32_t nparam;
	for (nparam = 0 ; argv[nparam] ; nparam++);
	ap = newparam = ckmalloc((nparam + 1) * sizeof * ap);
	while (*argv)
	{
		*ap++ = savestr(*argv++);
	}
	*ap = NULL;
	freeparam(&shellparam);
	shellparam.malloc = 1;
	shellparam.nparam = nparam;
	shellparam.p = newparam;
	shellparam.reset = 1;
	shellparam.optnext = NULL;
}


/*
 * Free the list of positional parameters.
 */

void
freeparam(struct shparam* param)
{
	cstring_t* ap;
	if (param->malloc)
	{
		for (ap = param->p ; *ap ; ap++)
			ckfree(*ap);
		ckfree(param->p);
	}
}



/*
 * The shift builtin command.
 */

int32_t
shiftcmd(int32_t argc, cstring_t* argv)
{
	int32_t n;
	cstring_t* ap1, **ap2;
	n = 1;
	if (argc > 1)
		n = number(argv[1]);
	if (n > shellparam.nparam)
		return 1;
	INTOFF;
	shellparam.nparam -= n;
	for (ap1 = shellparam.p ; --n >= 0 ; ap1++)
	{
		if (shellparam.malloc)
			ckfree(*ap1);
	}
	ap2 = shellparam.p;
	while ((*ap2++ = *ap1++) != NULL);
	shellparam.reset = 1;
	INTON;
	return 0;
}



/*
 * The set command builtin.
 */

int32_t
setcmd(int32_t argc, cstring_t* argv)
{
	if (argc == 1)
		return showvarscmd(argc, argv);
	INTOFF;
	options(0);
	optschanged();
	if (*argptr != NULL)
	{
		setparam(argptr);
	}
	INTON;
	return 0;
}


void
getoptsreset(const_cstring_t value)
{
	while (*value == '0')
		value++;
	if (strcmp(value, "1") == 0)
		shellparam.reset = 1;
}

/*
 * The getopts builtin.  Shellparam.optnext points to the next argument
 * to be processed.  Shellparam.optptr points to the next character to
 * be processed in the current argument.  If shellparam.optnext is NULL,
 * then it's the first time getopts has been called.
 */

int32_t
getoptscmd(int32_t argc, cstring_t* argv)
{
	cstring_t* optbase = NULL;
	if (argc < 3)
		sherror("usage: getopts optstring var [arg]");
	else if (argc == 3)
		optbase = shellparam.p;
	else
		optbase = &argv[3];
	if (shellparam.reset == 1)
	{
		shellparam.optnext = optbase;
		shellparam.optptr = NULL;
		shellparam.reset = 0;
	}
	return getopts(argv[1], argv[2], optbase, &shellparam.optnext,
				   &shellparam.optptr);
}

static int32_t
getopts(cstring_t optstr, cstring_t optvar, cstring_t* optfirst, cstring_t** optnext,
		cstring_t* optptr)
{
	cstring_t p;
	cstring_t q;
	char32_t c = '?';
	int32_t done = 0;
	int32_t ind = 0;
	int32_t err = 0;
	char s[10];
	const_cstring_t optarg = NULL;

	if ((p = *optptr) == NULL || *p == '\0')
	{
		/* Current word is done, advance */
		if (*optnext == NULL)
			return 1;
		p = **optnext;
		if (p == NULL || *p != '-' || *++p == '\0')
		{
atend:
			ind = *optnext - optfirst + 1;
			*optnext = NULL;
			p = NULL;
			done = 1;
			goto out;
		}
		(*optnext)++;
		if (p[0] == '-' && p[1] == '\0')	/* check for "--" */
			goto atend;
	}
	c = *p++;
	for (q = optstr; *q != c;)
	{
		if (*q == '\0')
		{
			if (optstr[0] == ':')
			{
				s[0] = c;
				s[1] = '\0';
				optarg = s;
			}
			else
				out2fmt_flush("Illegal option -%c\n", c);
			c = '?';
			goto out;
		}
		if (*++q == ':')
			q++;
	}
	if (*++q == ':')
	{
		if (*p == '\0' && (p = **optnext) == NULL)
		{
			if (optstr[0] == ':')
			{
				s[0] = c;
				s[1] = '\0';
				optarg = s;
				c = ':';
			}
			else
			{
				out2fmt_flush("No arg for -%c option\n", c);
				c = '?';
			}
			goto out;
		}
		if (p == **optnext)
			(*optnext)++;
		optarg = p;
		p = NULL;
	}
out:
	if (*optnext != NULL)
		ind = *optnext - optfirst + 1;
	*optptr = p;
	if (optarg != NULL)
		err |= setvarsafe("OPTARG", optarg, 0);
	else
	{
		INTOFF;
		err |= unsetvar("OPTARG");
		INTON;
	}
	fmtstr(s, sizeof(s), "%d", ind);
	err |= setvarsafe("OPTIND", s, VNOFUNC);
	s[0] = c;
	s[1] = '\0';
	err |= setvarsafe(optvar, s, 0);
	if (err)
	{
		*optnext = NULL;
		*optptr = NULL;
		flushall();
		exraise(EXERROR);
	}
	return done;
}

/*
 * Standard option processing (a la getopt) for builtin routines.  The
 * only argument that is passed to nextopt is the option string; the
 * other arguments are unnecessary.  It return the character, or '\0' on
 * end of input.
 */

int32_t
nextopt(const_cstring_t optstring)
{
	cstring_t p;
	const_cstring_t q;
	char c;
	if ((p = nextopt_optptr) == NULL || *p == '\0')
	{
		p = *argptr;
		if (p == NULL || *p != '-' || *++p == '\0')
			return '\0';
		argptr++;
		if (p[0] == '-' && p[1] == '\0')	/* check for "--" */
			return '\0';
	}
	c = *p++;
	for (q = optstring ; *q != c ;)
	{
		if (*q == '\0')
			sherror("Illegal option -%c", c);
		if (*++q == ':')
			q++;
	}
	if (*++q == ':')
	{
		if (*p == '\0' && (p = *argptr++) == NULL)
			sherror("No arg for -%c option", c);
		shoptarg = p;
		p = NULL;
	}
	nextopt_optptr = p;
	return c;
}
