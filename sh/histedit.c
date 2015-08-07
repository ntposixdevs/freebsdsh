/*	$FreeBSD: head/bin/sh/histedit.c 270113 2014-08-17 19:36:56Z jilles $	*/
/*	static char sccsid[] = "@(#)histedit.c	8.2 (Berkeley) 5/4/95";	*/
/*-
 * Copyright (c) 1993
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
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Editline and history functions (and glue).
 */
#include "shell.h"
#include "parser.h"
#include "var.h"
#include "options.h"
#include "main.h"
#include "output.h"
#include "mystring.h"
#ifndef NO_HISTORY
#include "myhistedit.h"
#include "sherror.h"
#include "eval.h"
#include "memalloc.h"
#include "builtins.h"

#define MAXHISTLOOPS	4	/* max recursions through fc */
#define DEFEDITOR	"ed"	/* default editor *should* be $EDITOR */

History* hist;	/* history cookie */
EditLine* el;	/* editline cookie */
int32_t displayhist;
static FILE* el_in, *el_out, *el_err;

static cstring_t fc_replace(const_cstring_t, cstring_t, cstring_t);
static int32_t not_fcnumber(const_cstring_t);
static int32_t str_to_event(const_cstring_t, int32_t);

/*
 * Set history and editing status.  Called whenever the status may
 * have changed (figures out what to do).
 */
void
histedit(void)
{
#define editing (Eflag || Vflag)
	if (iflag)
	{
		if (!hist)
		{
			/*
			 * turn history on
			 */
			INTOFF;
			hist = history_init();
			INTON;
			if (hist != NULL)
				sethistsize(histsizeval());
			else
				out2fmt_flush("sh: can't initialize history\n");
		}
		if (editing && !el && isatty(0))   /* && isatty(2) ??? */
		{
			/*
			 * turn editing on
			 */
			cstring_t term;
			INTOFF;
			if (el_in == NULL)
				el_in = fdopen(0, "r");
			if (el_err == NULL)
				el_err = fdopen(1, "w");
			if (el_out == NULL)
				el_out = fdopen(2, "w");
			if (el_in == NULL || el_err == NULL || el_out == NULL)
				goto bad;
			term = lookupvar("TERM");
			if (term)
				setenv("TERM", term, 1);
			else
				unsetenv("TERM");
			el = el_init(arg0, el_in, el_out, el_err);
			if (el != NULL)
			{
				if (hist)
					el_set(el, EL_HIST, history, hist);
				el_set(el, EL_PROMPT, getprompt);
				el_set(el, EL_ADDFN, "sh-complete",
					   "Filename completion",
					   _el_fn_complete);
			}
			else
			{
bad:
				out2fmt_flush("sh: can't initialize editing\n");
			}
			INTON;
		}
		else if (!editing && el)
		{
			INTOFF;
			el_end(el);
			el = NULL;
			INTON;
		}
		if (el)
		{
			if (Vflag)
				el_set(el, EL_EDITOR, "vi");
			else if (Eflag)
				el_set(el, EL_EDITOR, "emacs");
			el_set(el, EL_BIND, "^I", "sh-complete", NULL);
			el_source(el, NULL);
		}
	}
	else
	{
		INTOFF;
		if (el)  	/* no editing if not interactive */
		{
			el_end(el);
			el = NULL;
		}
		if (hist)
		{
			history_end(hist);
			hist = NULL;
		}
		INTON;
	}
}


void
sethistsize(const_cstring_t hs)
{
	int32_t histsize;
	HistEvent he;
	if (hist != NULL)
	{
		if (hs == NULL || !is_number(hs))
			histsize = 100;
		else
			histsize = atoi(hs);
		history(hist, &he, H_SETSIZE, histsize);
		history(hist, &he, H_SETUNIQUE, 1);
	}
}

void
setterm(const_cstring_t term)
{
	if (rootshell && el != NULL && term != NULL)
		el_set(el, EL_TERMINAL, term);
}

int32_t
histcmd(int32_t argc, cstring_t* argv __unused)
{
	int32_t ch;
	const_cstring_t editor = NULL;
	HistEvent he;
	int32_t lflg = 0, nflg = 0, rflg = 0, sflg = 0;
	int32_t i, retval;
	const_cstring_t firststr;
	const_cstring_t laststr;
	int32_t first, last, direction;
	cstring_t pat = NULL;
	cstring_t repl = NULL;
	static int32_t active = 0;
	struct jmploc jmploc;
	struct jmploc* savehandler;
	char editfilestr[PATH_MAX];
	cstring_t volatile editfile = NULL;
	FILE* efp = NULL;
	int32_t oldhistnum;
	(void)argv;

	if (hist == NULL)
		sherror("history not active");
	if (argc == 1)
		sherror("missing history argument");
	while (not_fcnumber(*argptr) && (ch = nextopt("e:lnrs")) != '\0')
		switch ((char)ch)
		{
			case 'e':
				editor = shoptarg;
				break;
			case 'l':
				lflg = 1;
				break;
			case 'n':
				nflg = 1;
				break;
			case 'r':
				rflg = 1;
				break;
			case 's':
				sflg = 1;
				break;
		}
	savehandler = handler;
	/*
	 * If executing...
	 */
	if (lflg == 0 || editor || sflg)
	{
		lflg = 0;	/* ignore */
		editfile = NULL;
		/*
		 * Catch interrupts to reset active counter and
		 * cleanup temp files.
		 */
		if (setjmp(jmploc.loc))
		{
			active = 0;
			if (editfile)
				unlink(editfile);
			handler = savehandler;
			longjmp(handler->loc, 1);
		}
		handler = &jmploc;
		if (++active > MAXHISTLOOPS)
		{
			active = 0;
			displayhist = 0;
			sherror("called recursively too many times");
		}
		/*
		 * Set editor.
		 */
		if (sflg == 0)
		{
			if (editor == NULL &&
					(editor = bltinlookup("FCEDIT", 1)) == NULL &&
					(editor = bltinlookup("EDITOR", 1)) == NULL)
				editor = DEFEDITOR;
			if (editor[0] == '-' && editor[1] == '\0')
			{
				sflg = 1;	/* no edit */
				editor = NULL;
			}
		}
	}
	/*
	 * If executing, parse [old=new] now
	 */
	if (lflg == 0 && *argptr != NULL &&
			((repl = strchr(*argptr, '=')) != NULL))
	{
		pat = *argptr;
		*repl++ = '\0';
		argptr++;
	}
	/*
	 * determine [first] and [last]
	 */
	if (*argptr == NULL)
	{
		firststr = lflg ? "-16" : "-1";
		laststr = "-1";
	}
	else if (argptr[1] == NULL)
	{
		firststr = argptr[0];
		laststr = lflg ? "-1" : argptr[0];
	}
	else if (argptr[2] == NULL)
	{
		firststr = argptr[0];
		laststr = argptr[1];
	}
	else
		sherror("too many arguments");
	/*
	 * Turn into event numbers.
	 */
	first = str_to_event(firststr, 0);
	last = str_to_event(laststr, 1);
	if (rflg)
	{
		i = last;
		last = first;
		first = i;
	}
	/*
	 * XXX - this should not depend on the event numbers
	 * always increasing.  Add sequence numbers or offset
	 * to the history element in next (diskbased) release.
	 */
	direction = first < last ? H_PREV : H_NEXT;
	/*
	 * If editing, grab a temp file.
	 */
	if (editor)
	{
		int32_t fd;
		INTOFF;		/* easier */
		sprintf(editfilestr, "%s/_shXXXXXX", _PATH_TMP);
		if ((fd = mkstemp(editfilestr)) < 0)
			sherror("can't create temporary file %s", editfile);
		editfile = editfilestr;
		if ((efp = fdopen(fd, "w")) == NULL)
		{
			close(fd);
			sherror("Out of space");
		}
	}
	/*
	 * Loop through selected history events.  If listing or executing,
	 * do it now.  Otherwise, put into temp file and call the editor
	 * after.
	 *
	 * The history interface needs rethinking, as the following
	 * convolutions will demonstrate.
	 */
	history(hist, &he, H_FIRST);
	retval = history(hist, &he, H_NEXT_EVENT, first);
	for (; retval != -1; retval = history(hist, &he, direction))
	{
		if (lflg)
		{
			if (!nflg)
				out1fmt("%5d ", he.num);
			out1str(he.str);
		}
		else
		{
			cstring_t s = pat ?
					  fc_replace(he.str, pat, repl) : (cstring_t)he.str;
			if (sflg)
			{
				if (displayhist)
				{
					out2str(s);
					flushout(out2);
				}
				evalstring(s, 0);
				if (displayhist && hist)
				{
					/*
					 *  XXX what about recursive and
					 *  relative histnums.
					 */
					oldhistnum = he.num;
					history(hist, &he, H_ENTER, s);
					/*
					 * XXX H_ENTER moves the internal
					 * cursor, set it back to the current
					 * entry.
					 */
					retval = history(hist, &he,
									 H_NEXT_EVENT, oldhistnum);
				}
			}
			else
				fputs(s, efp);
		}
		/*
		 * At end?  (if we were to lose last, we'd sure be
		 * messed up).
		 */
		if (he.num == last)
			break;
	}
	if (editor)
	{
		cstring_t editcmd;
		fclose(efp);
		editcmd = stalloc(strlen(editor) + strlen(editfile) + 2);
		sprintf(editcmd, "%s %s", editor, editfile);
		evalstring(editcmd, 0);	/* XXX - should use no JC command */
		INTON;
		readcmdfile(editfile);	/* XXX - should read back - quick tst */
		unlink(editfile);
	}
	if (lflg == 0 && active > 0)
		--active;
	if (displayhist)
		displayhist = 0;
	handler = savehandler;
	return 0;
}

static cstring_t
fc_replace(const_cstring_t s, cstring_t p, cstring_t r)
{
	cstring_t dest;
	size_t len = strlen(p);
	STARTSTACKSTR(dest);
	while (*s)
	{
		if (*s == *p && strncmp(s, p, len) == 0)
		{
			STPUTS(r, dest);
			s += len;
			*p = '\0';	/* so no more matches */
		}
		else
			STPUTC(*s++, dest);
	}
	STPUTC('\0', dest);
	dest = grabstackstr(dest);
	return (dest);
}

static int32_t
not_fcnumber(const_cstring_t s)
{
	if (s == NULL)
		return (0);
	if (*s == '-')
		s++;
	return (!is_number(s));
}

static int32_t
str_to_event(const_cstring_t str, int32_t last)
{
	HistEvent he;
	const_cstring_t s = str;
	int32_t relative = 0;
	int32_t i, retval;
	retval = history(hist, &he, H_FIRST);
	switch (*s)
	{
		case '-':
			relative = 1;
		/*FALLTHROUGH*/
		case '+':
			s++;
	}
	if (is_number(s))
	{
		i = atoi(s);
		if (relative)
		{
			while (retval != -1 && i--)
			{
				retval = history(hist, &he, H_NEXT);
			}
			if (retval == -1)
				retval = history(hist, &he, H_LAST);
		}
		else
		{
			retval = history(hist, &he, H_NEXT_EVENT, i);
			if (retval == -1)
			{
				/*
				 * the notion of first and last is
				 * backwards to that of the history package
				 */
				retval = history(hist, &he, last ? H_FIRST : H_LAST);
			}
		}
		if (retval == -1)
			sherror("history number %s not found (internal error)",
				  str);
	}
	else
	{
		/*
		 * pattern
		 */
		retval = history(hist, &he, H_PREV_STR, str);
		if (retval == -1)
			sherror("history pattern not found: %s", str);
	}
	return (he.num);
}

int32_t
bindcmd(int32_t argc, cstring_t* argv)
{
	if (el == NULL)
		sherror("line editing is disabled");
	return (el_parse(el, argc, (const_cstring_t*)argv));
}

#else
#include "sherror.h"

int32_t
histcmd(int32_t argc, cstring_t* argv)
{
	sherror("not compiled with history support");
	/*NOTREACHED*/
	return (0);
}

int32_t
bindcmd(int32_t argc, cstring_t* argv)
{
	sherror("not compiled with line editing support");
	return (0);
}
#endif
