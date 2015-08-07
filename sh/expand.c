/*	$FreeBSD: head/bin/sh/expand.c 268576 2014-07-12 21:54:11Z jilles $	*/
/*	static char sccsid[] = "@(#)expand.c	8.5 (Berkeley) 5/15/95";	*/
/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1997-2005
 *	Herbert Xu <herbert@gondor.apana.org.au>.  All rights reserved.
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
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

/*
 * Routines to expand arguments to commands.  We have to deal with
 * backquotes, shell variables, and file metacharacters.
 */

#include "shell.h"
#include "main.h"
#include "nodes.h"
#include "eval.h"
#include "expand.h"
#include "syntax.h"
#include "parser.h"
#include "jobs.h"
#include "options.h"
#include "var.h"
#include "input.h"
#include "output.h"
#include "memalloc.h"
#include "sherror.h"
#include "mystring.h"
#include "arith.h"
#include "show.h"
#include "builtins.h"

/*
 * Structure specifying which parts of the string should be searched
 * for IFS characters.
 */

typedef struct ifsregion
{
	struct ifsregion*   next;       /* next region in list */
	size_t              begoff;     /* offset of start of region */
	size_t              endoff;     /* offset of end of region */
	size_t              inquotes;   /* search for nul bytes only */
} ifsregion_t;
typedef ifsregion_t* pifsregion_t;

static cstring_t expdest;			/* output of current string */
static struct nodelist* argbackq;	/* list of back quote expressions */
static ifsregion_t ifsfirst;	/* first struct in list of ifs regions */
static pifsregion_t ifslastp;	/* last struct in list */
static struct arglist exparg;		/* holds expanded arg list */

static cstring_t argstr(cstring_t, int32_t);
static cstring_t exptilde(cstring_t, int32_t);
static cstring_t expari(cstring_t);
static void expbackq(union node*, int32_t, int32_t);
static int32_t subevalvar(cstring_t p, cstring_t str, size_t strloc, uint32_t subtype, size_t startloc, uint32_t varflags, uint32_t quotes);
static cstring_t evalvar(cstring_t, int32_t);
static int32_t varisset(const_cstring_t, int32_t);
static void varvalue(const_cstring_t, int32_t, int32_t, int32_t);
static void recordregion(size_t start, size_t end, size_t inquotes);
static void removerecordregions(size_t endoff);
static void ifsbreakup(cstring_t, struct arglist*);
static void expandmeta(struct strlist*, int32_t);
static void expmeta(cstring_t, cstring_t);
static void addfname(cstring_t);
static struct strlist* expsort(struct strlist*);
static struct strlist* msort(struct strlist*, size_t len);
static int32_t patmatch(const_cstring_t, const_cstring_t, int32_t);
static cstring_t cvtnum(intptr_t num, cstring_t);
static int32_t collate_range_cmp(char32_t c1, char32_t c2);

static int32_t
collate_range_cmp(char32_t c1, char32_t c2)
{
	typedef union _nulltermchar {
		uint64_t	force8;
		char32_t	char32[2];
		wchar_t		wchar[sizeof(uint64_t)/sizeof(wchar_t)];
	} _nulltermchar_t;

	_nulltermchar_t s1;
	_nulltermchar_t s2;
	s1.force8 = c1;
	s2.force8 = c2;

	return (wcscoll(s1.wchar, s2.wchar));
}

static cstring_t
stputs_quotes(const_cstring_t data, const_cstring_t syntax, cstring_t p)
{
	while (*data)
	{
		CHECKSTRSPACE(2, p);
		if (syntax[(int32_t)*data] == CCTL)
			USTPUTC(CTLESC, p);
		USTPUTC(*data++, p);
	}
	return (p);
}
#define STPUTS_QUOTES(data, syntax, p) p = stputs_quotes((data), syntax, p)

/*
 * Perform expansions on an argument, placing the resulting list of arguments
 * in arglist.  Parameter expansion, command substitution and arithmetic
 * expansion are always performed; additional expansions can be requested
 * via flag (EXP_*).
 * The result is left in the stack string.
 * When arglist is NULL, perform here document expansion.
 *
 * Caution: this function uses global state and is not reentrant.
 * However, a new invocation after an interrupted invocation is safe
 * and will reset the global state for the new call.
 */
void
expandarg(union node* arg, struct arglist* arglist, int32_t flag)
{
	struct strlist* sp;
	cstring_t p;
	argbackq = arg->narg.backquote;
	STARTSTACKSTR(expdest);
	ifsfirst.next = NULL;
	ifslastp = NULL;
	argstr(arg->narg.text, flag);
	if (arglist == NULL)
	{
		STACKSTRNUL(expdest);
		return;			/* here document expanded */
	}
	STPUTC('\0', expdest);
	p = grabstackstr(expdest);
	exparg.lastp = &exparg.list;
	/*
	 * TODO - EXP_REDIR
	 */
	if (flag & EXP_FULL)
	{
		ifsbreakup(p, &exparg);
		*exparg.lastp = NULL;
		exparg.lastp = &exparg.list;
		expandmeta(exparg.list, flag);
	}
	else
	{
		if (flag & EXP_REDIR) /*XXX - for now, just remove escapes */
			rmescapes(p);
		sp = (struct strlist*)stalloc(sizeof(struct strlist));
		sp->text = p;
		*exparg.lastp = sp;
		exparg.lastp = &sp->next;
	}
	while (ifsfirst.next != NULL)
	{
		pifsregion_t ifsp;
		INTOFF;
		ifsp = ifsfirst.next->next;
		ckfree(ifsfirst.next);
		ifsfirst.next = ifsp;
		INTON;
	}
	*exparg.lastp = NULL;
	if (exparg.list)
	{
		*arglist->lastp = exparg.list;
		arglist->lastp = exparg.lastp;
	}
}



/*
 * Perform parameter expansion, command substitution and arithmetic
 * expansion, and tilde expansion if requested via EXP_TILDE/EXP_VARTILDE.
 * Processing ends at a CTLENDVAR or CTLENDARI character as well as '\0'.
 * This is used to expand word in ${var+word} etc.
 * If EXP_FULL, EXP_CASE or EXP_REDIR are set, keep and/or generate CTLESC
 * characters to allow for further processing.
 * If EXP_FULL is set, also preserve CTLQUOTEMARK characters.
 */
static cstring_t
argstr(cstring_t p, int32_t flag)
{
	char c;
	int32_t quotes = flag & (EXP_FULL | EXP_CASE | EXP_REDIR);	/* do CTLESC */
	int32_t firsteq = 1;
	int32_t split_lit;
	int32_t lit_quoted;
	split_lit = flag & EXP_SPLIT_LIT;
	lit_quoted = flag & EXP_LIT_QUOTED;
	flag &= ~(EXP_SPLIT_LIT | EXP_LIT_QUOTED);
	if (*p == '~' && (flag & (EXP_TILDE | EXP_VARTILDE)))
		p = exptilde(p, flag);
	for (;;)
	{
		CHECKSTRSPACE(2, expdest);
		switch (c = *p++)
		{
			case '\0':
				return (p - 1);
			case CTLENDVAR:
			case CTLENDARI:
				return (p);
			case CTLQUOTEMARK:
				lit_quoted = 1;
				/* "$@" syntax adherence hack */
				if (p[0] == CTLVAR && p[2] == '@' && p[3] == '=')
					break;
				if ((flag & EXP_FULL) != 0)
					USTPUTC(c, expdest);
				break;
			case CTLQUOTEEND:
				lit_quoted = 0;
				break;
			case CTLESC:
				if (quotes)
					USTPUTC(c, expdest);
				c = *p++;
				USTPUTC(c, expdest);
				if (split_lit && !lit_quoted)
					recordregion(expdest - stackblock() -
								 (quotes ? 2 : 1),
								 expdest - stackblock(), 0);
				break;
			case CTLVAR:
				p = evalvar(p, flag);
				break;
			case CTLBACKQ:
			case CTLBACKQ|CTLQUOTE:
				expbackq(argbackq->n, c & CTLQUOTE, flag);
				argbackq = argbackq->next;
				break;
			case CTLARI:
				p = expari(p);
				break;
			case ':':
			case '=':
				/*
				 * sort of a hack - expand tildes in variable
				 * assignments (after the first '=' and after ':'s).
				 */
				USTPUTC(c, expdest);
				if (split_lit && !lit_quoted)
					recordregion(expdest - stackblock() - 1,
								 expdest - stackblock(), 0);
				if (flag & EXP_VARTILDE && *p == '~' &&
						(c != '=' || firsteq))
				{
					if (c == '=')
						firsteq = 0;
					p = exptilde(p, flag);
				}
				break;
			default:
				USTPUTC(c, expdest);
				if (split_lit && !lit_quoted)
					recordregion(expdest - stackblock() - 1,
								 expdest - stackblock(), 0);
		}
	}
}

/*
 * Perform tilde expansion, placing the result in the stack string and
 * returning the next position in the input string to process.
 */
static cstring_t
exptilde(cstring_t p, int32_t flag)
{
	char c, *startp = p;
	struct passwd* pw;
	cstring_t home;
	int32_t quotes = flag & (EXP_FULL | EXP_CASE | EXP_REDIR);
	while ((c = *p) != '\0')
	{
		switch (c)
		{
			case CTLESC: /* This means CTL* are always considered quoted. */
			case CTLVAR:
			case CTLBACKQ:
			case CTLBACKQ | CTLQUOTE:
			case CTLARI:
			case CTLENDARI:
			case CTLQUOTEMARK:
				return (startp);
			case ':':
				if (flag & EXP_VARTILDE)
					goto done;
				break;
			case '/':
			case CTLENDVAR:
				goto done;
		}
		p++;
	}
done:
	*p = '\0';
	if (*(startp + 1) == '\0')
	{
		if ((home = lookupvar("HOME")) == NULL)
			goto lose;
	}
	else
	{
		if ((pw = getpwnam(startp + 1)) == NULL)
			goto lose;
		home = pw->pw_dir;
	}
	if (*home == '\0')
		goto lose;
	*p = c;
	if (quotes)
		STPUTS_QUOTES(home, SQSYNTAX, expdest);
	else
		STPUTS(home, expdest);
	return (p);
lose:
	*p = c;
	return (startp);
}


static void
removerecordregions(size_t endoff)
{
	if (ifslastp == NULL)
		return;
	if (ifsfirst.endoff > endoff)
	{
		while (ifsfirst.next != NULL)
		{
			pifsregion_t ifsp;
			INTOFF;
			ifsp = ifsfirst.next->next;
			ckfree(ifsfirst.next);
			ifsfirst.next = ifsp;
			INTON;
		}
		if (ifsfirst.begoff > endoff)
			ifslastp = NULL;
		else
		{
			ifslastp = &ifsfirst;
			ifsfirst.endoff = endoff;
		}
		return;
	}
	ifslastp = &ifsfirst;
	while (ifslastp->next && ifslastp->next->begoff < endoff)
		ifslastp = ifslastp->next;
	while (ifslastp->next != NULL)
	{
		pifsregion_t ifsp;
		INTOFF;
		ifsp = ifslastp->next->next;
		ckfree(ifslastp->next);
		ifslastp->next = ifsp;
		INTON;
	}
	if (ifslastp->endoff > endoff)
		ifslastp->endoff = endoff;
}

/*
 * Expand arithmetic expression.
 * Note that flag is not required as digits never require CTLESC characters.
 */
static cstring_t
expari(cstring_t p)
{
	cstring_t q;
	cstring_t start;
	arith_t result;
	size_t begoff;
	int32_t quoted;
	size_t adj;

	quoted = *p++ == '"';
	begoff = expdest - stackblock();
	p = argstr(p, 0);
	removerecordregions(begoff);
	STPUTC('\0', expdest);
	start = stackblock() + begoff;
	q = grabstackstr(expdest);
	result = arith(start);
	ungrabstackstr(q, expdest);
	start = stackblock() + begoff;
	adj = start - expdest;
	STADJUST(adj, expdest);
	CHECKSTRSPACE((int32_t)(DIGITS(result) + 1), expdest);
	fmtstr(expdest, DIGITS(result), ARITH_FORMAT_STR, result);
	adj = strlen(expdest);
	STADJUST(adj, expdest);
	if (!quoted)
		recordregion(begoff, expdest - stackblock(), 0);
	return p;
}


/*
 * Perform command substitution.
 */
static void
expbackq(union node* cmd, int32_t quoted, int32_t flag)
{
	struct backcmd in;
	ssize_t nreadbytes;
	char buf[128] = {0};
	cstring_t p;
	cstring_t dest = expdest;
	ifsregion_t saveifs;
	pifsregion_t savelastp;
	struct nodelist* saveargbackq;
	char lastc;
	size_t startloc = dest - stackblock();
	char const* syntax = quoted ? DQSYNTAX : BASESYNTAX;
	int32_t quotes = flag & (EXP_FULL | EXP_CASE | EXP_REDIR);
	size_t nnl;

	INTOFF;
	saveifs = ifsfirst;
	savelastp = ifslastp;
	saveargbackq = argbackq;
	p = grabstackstr(dest);
	evalbackcmd(cmd, &in);
	ungrabstackstr(p, dest);
	ifsfirst = saveifs;
	ifslastp = savelastp;
	argbackq = saveargbackq;
	p = in.buf;
	lastc = '\0';
	nnl = 0;
	/* Don't copy trailing newlines */
	for (;;)
	{
		if (--in.nleft < 0)
		{
			if (in.fd < 0)
				break;
			while ((nreadbytes = read(in.fd, buf, sizeof(buf))) < 0 && errno == EINTR);
			TRACE(("expbackq: read returns %d\n", nreadbytes));
			if (nreadbytes <= 0)
				break;
			p = buf;
			in.nleft = nreadbytes - 1;
		}
		lastc = *p++;
		if (lastc != '\0')
		{
			if (lastc == '\n')
			{
				nnl++;
			}
			else
			{
				CHECKSTRSPACE(nnl + 2, dest);
				while (nnl > 0)
				{
					nnl--;
					USTPUTC('\n', dest);
				}
				if (quotes && syntax[(int32_t)lastc] == CCTL)
					USTPUTC(CTLESC, dest);
				USTPUTC(lastc, dest);
			}
		}
	}
	if (in.fd >= 0)
		close(in.fd);
	if (in.buf)
		ckfree(in.buf);
	if (in.jp)
		exitstatus = waitforjob(in.jp, (int32_t*)NULL);
	if (quoted == 0)
		recordregion(startloc, dest - stackblock(), 0);
	TRACE(("expbackq: size=%td: \"%.*s\"\n",
		   ((dest - stackblock()) - startloc),
		   (int32_t)((dest - stackblock()) - startloc),
		   stackblock() + startloc));
	expdest = dest;
	INTON;
}



static int32_t
subevalvar(cstring_t p, cstring_t str, size_t strloc, uint32_t subtype, size_t startloc,
		   uint32_t varflags, uint32_t quotes)
{
	cstring_t startp;
	cstring_t loc = NULL;
	cstring_t q;
	char32_t c = 0;
	struct nodelist* saveargbackq = argbackq;
	size_t amount;
	argstr(p, (subtype == VSTRIMLEFT || subtype == VSTRIMLEFTMAX ||
			   subtype == VSTRIMRIGHT || subtype == VSTRIMRIGHTMAX ?
			   EXP_CASE : 0) | EXP_TILDE);
	STACKSTRNUL(expdest);
	argbackq = saveargbackq;
	startp = stackblock() + startloc;
	if (str == NULL)
		str = stackblock() + strloc;
	switch (subtype)
	{
		case VSASSIGN:
			setvar(str, startp, 0);
			amount = startp - expdest;
			STADJUST(amount, expdest);
			varflags &= ~VSNUL;
			return 1;
		case VSQUESTION:
			if (*p != CTLENDVAR)
			{
				outfmt(out2, "%s\n", startp);
				sherror((cstring_t)NULL);
			}
			sherror("%.*s: parameter %snot set", (int32_t)(p - str - 1),
				  str, (varflags & VSNUL) ? "null or "
				  : nullstr);
			NOTREACHED;
		case VSTRIMLEFT:
			for (loc = startp; loc < str; loc++)
			{
				c = *loc;
				*loc = '\0';
				if (patmatch(str, startp, quotes))
				{
					*loc = (cchar_t)c;
					goto recordleft;
				}
				*loc = (cchar_t)c;
				if (quotes && *loc == CTLESC)
					loc++;
			}
			return 0;
		case VSTRIMLEFTMAX:
			for (loc = str - 1; loc >= startp;)
			{
				c = *loc;
				*loc = '\0';
				if (patmatch(str, startp, quotes))
				{
					*loc = (cchar_t)c;
					goto recordleft;
				}
				*loc = (cchar_t)c;
				loc--;
				if (quotes && loc > startp && *(loc - 1) == CTLESC)
				{
					for (q = startp; q < loc; q++)
						if (*q == CTLESC)
							q++;
					if (q > loc)
						loc--;
				}
			}
			return 0;
		case VSTRIMRIGHT:
			for (loc = str - 1; loc >= startp;)
			{
				if (patmatch(str, loc, quotes))
				{
					amount = loc - expdest;
					STADJUST(amount, expdest);
					return 1;
				}
				loc--;
				if (quotes && loc > startp && *(loc - 1) == CTLESC)
				{
					for (q = startp; q < loc; q++)
						if (*q == CTLESC)
							q++;
					if (q > loc)
						loc--;
				}
			}
			return 0;
		case VSTRIMRIGHTMAX:
			for (loc = startp; loc < str - 1; loc++)
			{
				if (patmatch(str, loc, quotes))
				{
					amount = loc - expdest;
					STADJUST(amount, expdest);
					return 1;
				}
				if (quotes && *loc == CTLESC)
					loc++;
			}
			return 0;
		default:
			abort();
	}

recordleft:
	amount = ((str - 1) - (loc - startp)) - expdest;
	STADJUST(amount, expdest);
	while (loc != str - 1)
		*startp++ = *loc++;

	return 1;
}


/*
 * Expand a variable, and return a pvoid_t to the next character in the
 * input string.
 */

static cstring_t
evalvar(cstring_t p, int32_t flag)
{
	uint32_t subtype;
	uint32_t varflags;
	cstring_t var;
	const_cstring_t val;
	size_t patloc;
	size_t c;
	int32_t set;
	int32_t special;
	size_t startloc;
	size_t varlen;
	size_t varlenb;
	int32_t easy;
	uint32_t quotes = flag & (EXP_FULL | EXP_CASE | EXP_REDIR);

	varflags = (uint32_t)*p++;
	subtype = varflags & VSTYPE;
	var = p;
	special = 0;
	if (!is_name(*p))
		special = 1;
	p = strchr(p, '=') + 1;
again: /* jump here after setting a variable with ${var=text} */
	if (varflags & VSLINENO)
	{
		set = 1;
		special = 1;
		val = NULL;
	}
	else if (special)
	{
		set = varisset(var, varflags & VSNUL);
		val = NULL;
	}
	else
	{
		val = bltinlookup(var, 1);
		if (val == NULL || ((varflags & VSNUL) && val[0] == '\0'))
		{
			val = NULL;
			set = 0;
		}
		else
			set = 1;
	}
	varlen = 0;
	startloc = expdest - stackblock();
	if (!set && uflag && *var != '@' && *var != '*')
	{
		switch (subtype)
		{
			case VSNORMAL:
			case VSTRIMLEFT:
			case VSTRIMLEFTMAX:
			case VSTRIMRIGHT:
			case VSTRIMRIGHTMAX:
			case VSLENGTH:
				sherror("%.*s: parameter not set", (int32_t)(p - var - 1),
					  var);
		}
	}
	if (set && subtype != VSPLUS)
	{
		/* insert the value of the variable */
		if (special)
		{
			if (varflags & VSLINENO)
				STPUTBIN(var, p - var - 1, expdest);
			else
				varvalue(var, varflags & VSQUOTE, subtype, flag);
			if (subtype == VSLENGTH)
			{
				varlenb = expdest - stackblock() - startloc;
				varlen = varlenb;
				if (localeisutf8)
				{
					val = stackblock() + startloc;
					for (; val != expdest; val++)
						if ((*val & 0xC0) == 0x80)
							varlen--;
				}
				expdest -= varlenb;	// STADJUST(-varlenb, expdest);
			}
		}
		else
		{
			char const* syntax = (varflags & VSQUOTE) ? DQSYNTAX
								 : BASESYNTAX;
			if (subtype == VSLENGTH)
			{
				for (; *val; val++)
					if (!localeisutf8 ||
							(*val & 0xC0) != 0x80)
						varlen++;
			}
			else
			{
				if (quotes)
					STPUTS_QUOTES(val, syntax, expdest);
				else
					STPUTS(val, expdest);
			}
		}
	}
	if (subtype == VSPLUS)
		set = ! set;
	easy = ((varflags & VSQUOTE) == 0 ||
			(*var == '@' && shellparam.nparam != 1));
	switch (subtype)
	{
		case VSLENGTH:
			expdest = cvtnum(varlen, expdest);
			goto record;
		case VSNORMAL:
			if (!easy)
				break;
record:
			recordregion(startloc, expdest - stackblock(),
						 varflags & VSQUOTE || (ifsset() && ifsval()[0] == '\0' &&
												(*var == '@' || *var == '*')));
			break;
		case VSPLUS:
		case VSMINUS:
			if (!set)
			{
				argstr(p, flag | (flag & EXP_FULL ? EXP_SPLIT_LIT : 0) |
					   (varflags & VSQUOTE ? EXP_LIT_QUOTED : 0));
				break;
			}
			if (easy)
				goto record;
			break;
		case VSTRIMLEFT:
		case VSTRIMLEFTMAX:
		case VSTRIMRIGHT:
		case VSTRIMRIGHTMAX:
			if (!set)
				break;
			/*
			 * Terminate the string and start recording the pattern
			 * right after it
			 */
			STPUTC('\0', expdest);
			patloc = expdest - stackblock();
			if (subevalvar(p, NULL, patloc, subtype, startloc, varflags, quotes) == 0)
			{
				size_t amount = (expdest - stackblock() - patloc) + 1;
				expdest -= amount;	// STADJUST(-amount, expdest);
			}
			/* Remove any recorded regions beyond start of variable */
			removerecordregions(startloc);
			goto record;
		case VSASSIGN:
		case VSQUESTION:
			if (!set)
			{
				if (subevalvar(p, var, 0, subtype, startloc, varflags, quotes) == 1)
				{
					varflags &= ~VSNUL;
					/*
					 * Remove any recorded regions beyond
					 * start of variable
					 */
					removerecordregions(startloc);
					goto again;
				}
				break;
			}
			if (easy)
				goto record;
			break;
		case VSERROR:
			c = p - var - 1;
			sherror("${%.*s%s}: Bad substitution", c, var,
				  (c > 0 && *p != CTLENDVAR) ? "..." : "");
		default:
			abort();
	}
	if (subtype != VSNORMAL)  	/* skip to end of alternative */
	{
		int32_t nesting = 1;
		for (;;)
		{
			if ((c = *p++) == CTLESC)
				p++;
			else if (c == CTLBACKQ || c == (CTLBACKQ | CTLQUOTE))
			{
				if (set)
					argbackq = argbackq->next;
			}
			else if (c == CTLVAR)
			{
				if ((*p++ & VSTYPE) != VSNORMAL)
					nesting++;
			}
			else if (c == CTLENDVAR)
			{
				if (--nesting == 0)
					break;
			}
		}
	}
	return p;
}



/*
 * Test whether a specialized variable is set.
 */

static int32_t
varisset(const_cstring_t name, int32_t nulok)
{
	if (*name == '!')
		return backgndpidset();
	else if (*name == '@' || *name == '*')
	{
		if (*shellparam.p == NULL)
			return 0;
		if (nulok)
		{
			cstring_t* av;
			for (av = shellparam.p; *av; av++)
				if (**av != '\0')
					return 1;
			return 0;
		}
	}
	else if (is_digit(*name))
	{
		cstring_t ap;
		intptr_t num;
		errno = 0;
		num = (intptr_t)strtol(name, NULL, 10);
		if (errno != 0 || num > shellparam.nparam)
			return 0;
		if (num == 0)
			ap = arg0;
		else
			ap = shellparam.p[num - 1];
		if (nulok && (ap == NULL || *ap == '\0'))
			return 0;
	}
	return 1;
}

static void
strtodest(const_cstring_t p, int32_t flag, int32_t subtype, int32_t quoted)
{
	if (flag & (EXP_FULL | EXP_CASE) && subtype != VSLENGTH)
		STPUTS_QUOTES(p, quoted ? DQSYNTAX : BASESYNTAX, expdest);
	else
		STPUTS(p, expdest);
}

/*
 * Add the value of a specialized variable to the stack string.
 */

static void
varvalue(const_cstring_t name, int32_t quoted, int32_t subtype, int32_t flag)
{
	int32_t num;
	cstring_t p;
	int32_t i;
	char sep;
	cstring_t* ap;
	switch (*name)
	{
		case '$':
			num = rootpid;
			goto numvar;
		case '?':
			num = oexitstatus;
			goto numvar;
		case '#':
			num = shellparam.nparam;
			goto numvar;
		case '!':
			num = backgndpidval();
numvar:
			expdest = cvtnum(num, expdest);
			break;
		case '-':
			for (i = 0 ; i < NOPTS ; i++)
			{
				if (optlist[i].val)
					STPUTC(optlist[i].letter, expdest);
			}
			break;
		case '@':
			if (flag & EXP_FULL && quoted)
			{
				for (ap = shellparam.p ; (p = *ap++) != NULL ;)
				{
					strtodest(p, flag, subtype, quoted);
					if (*ap)
						STPUTC('\0', expdest);
				}
				break;
			}
		/* FALLTHROUGH */
		case '*':
			if (ifsset())
				sep = ifsval()[0];
			else
				sep = ' ';
			for (ap = shellparam.p ; (p = *ap++) != NULL ;)
			{
				strtodest(p, flag, subtype, quoted);
				if (!*ap)
					break;
				if (sep || (flag & EXP_FULL && !quoted &&** ap != '\0'))
					STPUTC(sep, expdest);
			}
			break;
		default:
			if (is_digit(*name))
			{
				num = atoi(name);
				if (num == 0)
					p = arg0;
				else if (num > 0 && num <= shellparam.nparam)
					p = shellparam.p[num - 1];
				else
					break;
				strtodest(p, flag, subtype, quoted);
			}
			break;
	}
}



/*
 * Record the fact that we have to scan this region of the
 * string for IFS characters.
 */

static void
recordregion(size_t start, size_t end, size_t inquotes)
{
	pifsregion_t ifsp;
	INTOFF;
	if (ifslastp == NULL)
	{
		ifsp = &ifsfirst;
	}
	else
	{
		if (ifslastp->endoff == start
				&& ifslastp->inquotes == inquotes)
		{
			/* extend previous area */
			ifslastp->endoff = end;
			INTON;
			return;
		}
		ifsp = (pifsregion_t)ckmalloc(sizeof(*ifsp));
		ifslastp->next = ifsp;
	}
	ifslastp = ifsp;
	ifslastp->next = NULL;
	ifslastp->begoff = start;
	ifslastp->endoff = end;
	ifslastp->inquotes = inquotes;
	INTON;
}



/*
 * Break the argument string into pieces based upon IFS and add the
 * strings to the argument list.  The regions of the string to be
 * searched for IFS characters have been stored by recordregion.
 * CTLESC characters are preserved but have little effect in this pass
 * other than escaping CTL* characters.  In particular, they do not escape
 * IFS characters: that should be done with the ifsregion mechanism.
 * CTLQUOTEMARK characters are used to preserve empty quoted strings.
 * This pass treats them as a regular character, making the string non-empty.
 * Later, they are removed along with the other CTL* characters.
 */
static void
ifsbreakup(cstring_t string, struct arglist* arglist)
{
	pifsregion_t ifsp;
	struct strlist* sp;
	cstring_t start;
	cstring_t p;
	cstring_t q;
	const_cstring_t ifs;
	const_cstring_t ifsspc;
	int32_t had_param_ch = 0;
	start = string;
	if (ifslastp == NULL)
	{
		/* Return entire argument, IFS doesn't apply to any of it */
		sp = (struct strlist*)stalloc(sizeof * sp);
		sp->text = start;
		*arglist->lastp = sp;
		arglist->lastp = &sp->next;
		return;
	}
	ifs = ifsset() ? ifsval() : " \t\n";
	for (ifsp = &ifsfirst; ifsp != NULL; ifsp = ifsp->next)
	{
		p = string + ifsp->begoff;
		while (p < string + ifsp->endoff)
		{
			q = p;
			if (*p == CTLESC)
				p++;
			if (ifsp->inquotes)
			{
				/* Only NULs (should be from "$@") end args */
				had_param_ch = 1;
				if (*p != 0)
				{
					p++;
					continue;
				}
				ifsspc = NULL;
			}
			else
			{
				if (!strchr(ifs, *p))
				{
					had_param_ch = 1;
					p++;
					continue;
				}
				ifsspc = strchr(" \t\n", *p);
				/* Ignore IFS whitespace at start */
				if (q == start && ifsspc != NULL)
				{
					p++;
					start = p;
					continue;
				}
				had_param_ch = 0;
			}
			/* Save this argument... */
			*q = '\0';
			sp = (struct strlist*)stalloc(sizeof * sp);
			sp->text = start;
			*arglist->lastp = sp;
			arglist->lastp = &sp->next;
			p++;
			if (ifsspc != NULL)
			{
				/* Ignore further trailing IFS whitespace */
				for (; p < string + ifsp->endoff; p++)
				{
					q = p;
					if (*p == CTLESC)
						p++;
					if (strchr(ifs, *p) == NULL)
					{
						p = q;
						break;
					}
					if (strchr(" \t\n", *p) == NULL)
					{
						p++;
						break;
					}
				}
			}
			start = p;
		}
	}
	/*
	 * Save anything left as an argument.
	 * Traditionally we have treated 'IFS=':'; set -- x$IFS' as
	 * generating 2 arguments, the second of which is empty.
	 * Some recent clarification of the Posix spec say that it
	 * should only generate one....
	 */
	if (had_param_ch || *start != 0)
	{
		sp = (struct strlist*)stalloc(sizeof * sp);
		sp->text = start;
		*arglist->lastp = sp;
		arglist->lastp = &sp->next;
	}
}


static char expdir[PATH_MAX];
#define expdir_end (expdir + sizeof(expdir))

/*
 * Perform pathname generation and remove control characters.
 * At this point, the only control characters should be CTLESC and CTLQUOTEMARK.
 * The results are stored in the list exparg.
 */
static void
expandmeta(struct strlist* str, int32_t flag __unused)
{
	cstring_t p;
	struct strlist** savelastp;
	struct strlist* sp;
	char c;
	(void)flag;
	/* TODO - EXP_REDIR */
	while (str)
	{
		if (fflag)
			goto nometa;
		p = str->text;
		for (;;)  			/* fast check for meta chars */
		{
			if ((c = *p++) == '\0')
				goto nometa;
			if (c == '*' || c == '?' || c == '[')
				break;
		}
		savelastp = exparg.lastp;
		INTOFF;
		expmeta(expdir, str->text);
		INTON;
		if (exparg.lastp == savelastp)
		{
			/*
			 * no matches
			 */
nometa:
			*exparg.lastp = str;
			rmescapes(str->text);
			exparg.lastp = &str->next;
		}
		else
		{
			*exparg.lastp = NULL;
			*savelastp = sp = expsort(*savelastp);
			while (sp->next != NULL)
				sp = sp->next;
			exparg.lastp = &sp->next;
		}
		str = str->next;
	}
}


/*
 * Do metacharacter (i.e. *, ?, [...]) expansion.
 */

static void
expmeta(cstring_t enddir, cstring_t name)
{
	const_cstring_t p;
	const_cstring_t q;
	const_cstring_t start;
	cstring_t endname;
	int32_t metaflag;
	struct stat statb;
	DIR* dirp;
	struct dirent* dp;
	int32_t atend;
	int32_t matchdot;
	int32_t esc;
	size_t namlen;
	metaflag = 0;
	start = name;
	for (p = name; esc = 0, *p; p += esc + 1)
	{
		if (*p == '*' || *p == '?')
			metaflag = 1;
		else if (*p == '[')
		{
			q = p + 1;
			if (*q == '!' || *q == '^')
				q++;
			for (;;)
			{
				while (*q == CTLQUOTEMARK)
					q++;
				if (*q == CTLESC)
					q++;
				if (*q == '/' || *q == '\0')
					break;
				if (*++q == ']')
				{
					metaflag = 1;
					break;
				}
			}
		}
		else if (*p == '\0')
			break;
		else if (*p == CTLQUOTEMARK)
			continue;
		else
		{
			if (*p == CTLESC)
				esc++;
			if (p[esc] == '/')
			{
				if (metaflag)
					break;
				start = p + esc + 1;
			}
		}
	}
	if (metaflag == 0)  	/* we've reached the end of the file name */
	{
		if (enddir != expdir)
			metaflag++;
		for (p = name ; ; p++)
		{
			if (*p == CTLQUOTEMARK)
				continue;
			if (*p == CTLESC)
				p++;
			*enddir++ = *p;
			if (*p == '\0')
				break;
			if (enddir == expdir_end)
				return;
		}
		if (metaflag == 0 || lstat(expdir, &statb) >= 0)
			addfname(expdir);
		return;
	}
	endname = name + (p - name);
	if (start != name)
	{
		p = name;
		while (p < start)
		{
			while (*p == CTLQUOTEMARK)
				p++;
			if (*p == CTLESC)
				p++;
			*enddir++ = *p++;
			if (enddir == expdir_end)
				return;
		}
	}
	if (enddir == expdir)
	{
		p = ".";
	}
	else if (enddir == expdir + 1 && *expdir == '/')
	{
		p = "/";
	}
	else
	{
		p = expdir;
		enddir[-1] = '\0';
	}
	if ((dirp = opendir(p)) == NULL)
		return;
	if (enddir != expdir)
		enddir[-1] = '/';
	if (*endname == 0)
	{
		atend = 1;
	}
	else
	{
		atend = 0;
		*endname = '\0';
		endname += esc + 1;
	}
	matchdot = 0;
	p = start;
	while (*p == CTLQUOTEMARK)
		p++;
	if (*p == CTLESC)
		p++;
	if (*p == '.')
		matchdot++;
	while (! int_pending() && (dp = readdir(dirp)) != NULL)
	{
		if (dp->d_name[0] == '.' && ! matchdot)
			continue;
		if (patmatch(start, dp->d_name, 0))
		{
			namlen = dp->d_namlen;
			if (enddir + namlen + 1 > expdir_end)
				continue;
			memcpy(enddir, dp->d_name, namlen + 1);
			if (atend)
				addfname(expdir);
			else
			{
#if _CONSIDERED_DISABLED
				if (dp->d_type != DT_UNKNOWN &&
						dp->d_type != DT_DIR &&
						dp->d_type != DT_LNK)
					continue;
#endif
				if (enddir + namlen + 2 > expdir_end)
					continue;
				enddir[namlen] = '/';
				enddir[namlen + 1] = '\0';
				expmeta(enddir + namlen + 1, endname);
			}
		}
	}
	closedir(dirp);
	if (! atend)
		endname[-esc - 1] = esc ? CTLESC : '/';
}


/*
 * Add a file name to the list.
 */

static void
addfname(cstring_t name)
{
	cstring_t p;
	struct strlist* sp;
	size_t len;
	len = strlen(name);
	p = stalloc(len + 1);
	memcpy(p, name, len + 1);
	sp = (struct strlist*)stalloc(sizeof * sp);
	sp->text = p;
	*exparg.lastp = sp;
	exparg.lastp = &sp->next;
}


/*
 * Sort the results of file name expansion.  It calculates the number of
 * strings to sort and then calls msort (short for merge sort) to do the
 * work.
 */

static struct strlist*
expsort(struct strlist* str)
{
	size_t len;
	struct strlist* sp;
	len = 0;
	for (sp = str ; sp ; sp = sp->next)
		len++;
	return msort(str, len);
}


static struct strlist*
msort(struct strlist* list, size_t len)
{
	struct strlist* p;
	struct strlist* q = NULL;
	struct strlist** lpp;
	size_t half;
	size_t n;

	if (len <= 1)
		return list;
	half = len >> 1;
	p = list;
	for (n = half; n-- > 0 ;)
	{
		q = p;
		p = p->next;
	}
	if (q) q->next = NULL;		/* terminate first half of list */
	q = msort(list, half);		/* sort first half of list */
	p = msort(p, len - half);	/* sort second half */
	lpp = &list;
	for (;;)
	{
		if (strcmp(p->text, q->text) < 0)
		{
			*lpp = p;
			lpp = &p->next;
			if ((p = *lpp) == NULL)
			{
				*lpp = q;
				break;
			}
		}
		else
		{
			*lpp = q;
			lpp = &q->next;
			if ((q = *lpp) == NULL)
			{
				*lpp = p;
				break;
			}
		}
	}
	return list;
}



static wchar_t
get_wc(const_cstring_t* p)
{
	wchar_t c;
	int32_t chrlen;
	chrlen = mbtowc(&c, *p, 4);
	if (chrlen == 0)
		return 0;
	else if (chrlen == -1)
		c = 0;
	else
		*p += chrlen;
	return c;
}


/*
 * See if a character matches a character class, starting at the first colon
 * of "[:class:]".
 * If a valid character class is recognized, a pvoid_t to the next character
 * after the final closing bracket is stored into *end, otherwise a null
 * pvoid_t is stored into *end.
 */
static int32_t
match_charclass(const_cstring_t p, wchar_t chr, const_cstring_t* end)
{
	char name[20];
	const_cstring_t nameend;
	wctype_t cclass;
	*end = NULL;
	p++;
	nameend = strstr(p, ":]");
	if (nameend == NULL || (size_t)(nameend - p) >= sizeof(name) ||
			nameend == p)
		return 0;
	memcpy(name, p, nameend - p);
	name[nameend - p] = '\0';
	*end = nameend + 2;
	cclass = wctype(name);
	/* An unknown class matches nothing but is valid nevertheless. */
	if (cclass == 0)
		return 0;
	return iswctype(chr, cclass);
}


/*
 * Returns true if the pattern matches the string.
 */

static int32_t
patmatch(const_cstring_t pattern, const_cstring_t string, int32_t squoted)
{
	const_cstring_t p;
	const_cstring_t q;
	const_cstring_t end;
	const_cstring_t bt_p;
	const_cstring_t bt_q;
	char32_t c;
	char32_t wc;
	char32_t wc2;

	p = pattern;
	q = string;
	bt_p = NULL;
	bt_q = NULL;
	for (;;)
	{
		switch (c = *p++)
		{
			case '\0':
				if (*q != '\0')
					goto backtrack;
				return 1;
			case CTLESC:
				if (squoted && *q == CTLESC)
					q++;
				if (*q++ != *p++)
					goto backtrack;
				break;
			case CTLQUOTEMARK:
				continue;
			case '?':
				if (squoted && *q == CTLESC)
					q++;
				if (*q == '\0')
					return 0;
				if (localeisutf8)
				{
					wc = get_wc(&q);
					/*
					 * A '?' does not match invalid UTF-8 but a
					 * '*' does, so backtrack.
					 */
					if (wc == 0)
						goto backtrack;
				}
				else
					wc = (char32_t)*q++;
				break;
			case '*':
				c = *p;
				while (c == CTLQUOTEMARK || c == '*')
					c = *++p;
				/*
				 * If the pattern ends here, we know the string
				 * matches without needing to look at the rest of it.
				 */
				if (c == '\0')
					return 1;
				/*
				 * First try the shortest match for the '*' that
				 * could work. We can forget any earlier '*' since
				 * there is no way having it match more characters
				 * can help us, given that we are already here.
				 */
				bt_p = p;
				bt_q = q;
				break;
			case '[':
			{
				const_cstring_t endp;
				int32_t invert, found;
				wchar_t chr;
				endp = p;
				if (*endp == '!' || *endp == '^')
					endp++;
				for (;;)
				{
					while (*endp == CTLQUOTEMARK)
						endp++;
					if (*endp == 0)
						goto dft;		/* no matching ] */
					if (*endp == CTLESC)
						endp++;
					if (*++endp == ']')
						break;
				}
				invert = 0;
				if (*p == '!' || *p == '^')
				{
					invert++;
					p++;
				}
				found = 0;
				if (squoted && *q == CTLESC)
					q++;
				if (*q == '\0')
					return 0;
				if (localeisutf8)
				{
					chr = get_wc(&q);
					if (chr == 0)
						goto backtrack;
				}
				else
					chr = (uint8_t) * q++;
				c = *p++;
				do
				{
					if (c == CTLQUOTEMARK)
						continue;
					if (c == '[' && *p == ':')
					{
						found |= match_charclass(p, chr, &end);
						if (end != NULL)
							p = end;
					}
					if (c == CTLESC)
						c = *p++;
					if (localeisutf8 && c & 0x80)
					{
						p--;
						wc = get_wc(&p);
						if (wc == 0) /* bad utf-8 */
							return 0;
					}
					else
						wc = (char32_t)c;
					if (*p == '-' && p[1] != ']')
					{
						p++;
						while (*p == CTLQUOTEMARK)
							p++;
						if (*p == CTLESC)
							p++;
						if (localeisutf8)
						{
							wc2 = get_wc(&p);
							if (wc2 == 0) /* bad utf-8 */
								return 0;
						}
						else
							wc2 = (char32_t)*p++;
						if (collate_range_cmp(chr, wc) >= 0
								&& collate_range_cmp(chr, wc2) <= 0
						   )
							found = 1;
					}
					else
					{
						if (chr == wc)
							found = 1;
					}
				}
				while ((c = *p++) != ']');
				if (found == invert)
					goto backtrack;
				break;
			}
dft:
			default:
				if (squoted && *q == CTLESC)
					q++;
				if (*q == '\0')
					return 0;
				if (*q++ == (cchar_t)c)
					break;
backtrack:
				/*
				 * If we have a mismatch (other than hitting the end
				 * of the string), go back to the last '*' seen and
				 * have it match one additional character.
				 */
				if (bt_p == NULL)
					return 0;
				if (squoted && *bt_q == CTLESC)
					bt_q++;
				if (*bt_q == '\0')
					return 0;
				bt_q++;
				p = bt_p;
				q = bt_q;
				break;
		}
	}
}



/*
 * Remove any CTLESC and CTLQUOTEMARK characters from a string.
 */

void
rmescapes(cstring_t str)
{
	cstring_t p;
	cstring_t q;

	p = str;
	while (*p != CTLESC && *p != CTLQUOTEMARK && *p != CTLQUOTEEND)
	{
		if (*p++ == '\0')
			return;
	}
	q = p;
	while (*p)
	{
		if (*p == CTLQUOTEMARK || *p == CTLQUOTEEND)
		{
			p++;
			continue;
		}
		if (*p == CTLESC)
			p++;
		*q++ = *p++;
	}
	*q = '\0';
}



/*
 * See if a pattern matches in a case statement.
 */

int32_t
casematch(union node* pattern, const_cstring_t val)
{
	struct stackmark smark;
	int32_t result;
	cstring_t p;
	setstackmark(&smark);
	argbackq = pattern->narg.backquote;
	STARTSTACKSTR(expdest);
	ifslastp = NULL;
	argstr(pattern->narg.text, EXP_TILDE | EXP_CASE);
	STPUTC('\0', expdest);
	p = grabstackstr(expdest);
	result = patmatch(p, val, 0);
	popstackmark(&smark);
	return result;
}

/*
 * Our own itoa().
 */

static cstring_t
cvtnum(intptr_t num, cstring_t buf)
{
	char temp[32];
	intptr_t neg = num < 0;
	cstring_t p = temp + 31;
	temp[31] = '\0';
	do
	{
		*--p = num % 10 + '0';
	}
	while ((num /= 10) != 0);
	if (neg)
		*--p = '-';
	STPUTS(p, buf);
	return buf;
}

/*
 * Do most of the work for wordexp(3).
 */

int32_t
wordexpcmd(int32_t argc, cstring_t* argv)
{
	size_t len;
	int32_t i;
	out1fmt("%08x", argc - 1);
	for (i = 1, len = 0; i < argc; i++)
		len += strlen(argv[i]);
	out1fmt("%08x", (int32_t)len);
	for (i = 1; i < argc; i++)
		outbin(argv[i], strlen(argv[i]) + 1, out1);
	return (0);
}
