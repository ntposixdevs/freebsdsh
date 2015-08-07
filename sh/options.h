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
 *
 *	@(#)options.h	8.2 (Berkeley) 5/4/95
 * $FreeBSD: head/bin/sh/options.h 223281 2011-06-18 23:43:28Z jilles $
 */

struct shparam
{
	int32_t nparam;		/* # of positional parameters (without $0) */
	uint8_t malloc;	/* if parameter list dynamically allocated */
	uint8_t reset;	/* if getopts has been reset */
	cstring_t* p;		/* parameter list */
	cstring_t* optnext;		/* next parameter to be processed by getopts */
	cstring_t optptr;		/* used by getopts */
};



#define eflag optlist[0].val
#define fflag optlist[1].val
#define Iflag optlist[2].val
#define iflag optlist[3].val
#define mflag optlist[4].val
#define nflag optlist[5].val
#define sflag optlist[6].val
#define xflag optlist[7].val
#define vflag optlist[8].val
#define Vflag optlist[9].val
#define	Eflag optlist[10].val
#define	Cflag optlist[11].val
#define	aflag optlist[12].val
#define	bflag optlist[13].val
#define	uflag optlist[14].val
#define	privileged optlist[15].val
#define	Tflag optlist[16].val
#define	Pflag optlist[17].val
#define	hflag optlist[18].val

#define NOPTS	19

struct optent
{
	const_cstring_t name;
	const char letter;
	char val;
};

#ifdef DEFINE_OPTIONS
struct optent optlist[NOPTS] =
{
	{ "errexit",	'e',	0 },
	{ "noglob",	'f',	0 },
	{ "ignoreeof",	'I',	0 },
	{ "interactive", 'i',	0 },
	{ "monitor",	'm',	0 },
	{ "noexec",	'n',	0 },
	{ "stdin",	's',	0 },
	{ "xtrace",	'x',	0 },
	{ "verbose",	'v',	0 },
	{ "vi",		'V',	0 },
	{ "emacs",	'E',	0 },
	{ "noclobber",	'C',	0 },
	{ "allexport",	'a',	0 },
	{ "notify",	'b',	0 },
	{ "nounset",	'u',	0 },
	{ "privileged",	'p',	0 },
	{ "trapsasync",	'T',	0 },
	{ "physical",	'P',	0 },
	{ "trackall",	'h',	0 },
};
#else
extern struct optent optlist[NOPTS];
#endif


extern cstring_t minusc;		/* argument to -c option */
extern cstring_t arg0;		/* $0 */
extern struct shparam shellparam;  /* $@ */
extern cstring_t* argptr;		/* argument list for builtin commands */
extern cstring_t shoptarg;		/* set by nextopt */
extern cstring_t nextopt_optptr;	/* used by nextopt */

void procargs(int32_t, cstring_t*);
void optschanged(void);
void setparam(cstring_t*);
void freeparam(struct shparam*);
int32_t nextopt(const_cstring_t);
void getoptsreset(const_cstring_t);
