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
 *	@(#)var.h	8.2 (Berkeley) 5/4/95
 * $FreeBSD: head/bin/sh/var.h 259874 2013-12-25 16:14:02Z jilles $
 */

/*
 * Shell variables.
 */

/* flags */
#define VEXPORT		0x01	/* variable is exported */
#define VREADONLY	0x02	/* variable cannot be modified */
#define VSTRFIXED	0x04	/* variable struct is statically allocated */
#define VTEXTFIXED	0x08	/* text is statically allocated */
#define VSTACK		0x10	/* text is allocated on the stack */
#define VUNSET		0x20	/* the variable is not set */
#define VNOFUNC		0x40	/* don't call the callback function */
#define VNOSET		0x80	/* do not set variable - just readonly test */
#define VNOLOCAL	0x100	/* ignore forcelocal */


struct var
{
	struct var* next;		/* next entry in hash list */
	int32_t flags;			/* flags are defined above */
	int32_t name_len;			/* length of name */
	cstring_t text;			/* name=value */
	void (*func)(const_cstring_t);
	/* function to be called when  */
	/* the variable gets set/unset */
};


struct localvar
{
	struct localvar* next;		/* next local variable in list */
	struct var* vp;			/* the variable that was made local */
	int32_t flags;			/* saved flags */
	cstring_t text;			/* saved text */
};


struct localvar* localvars;
extern int32_t forcelocal;

extern struct var vifs;
extern struct var vmail;
extern struct var vmpath;
extern struct var vpath;
extern struct var vps1;
extern struct var vps2;
extern struct var vps4;
extern struct var vdisvfork;
#ifndef NO_HISTORY
extern struct var vhistsize;
extern struct var vterm;
#endif

extern int32_t localeisutf8;
/* The parser uses the locale that was in effect at startup. */
extern int32_t initial_localeisutf8;

/*
 * The following macros access the values of the above variables.
 * They have to skip over the name.  They return the null string
 * for unset variables.
 */

#define ifsval()	(vifs.text + 4)
#define ifsset()	((vifs.flags & VUNSET) == 0)
#define mailval()	(vmail.text + 5)
#define mpathval()	(vmpath.text + 9)
#define pathval()	(vpath.text + 5)
#define ps1val()	(vps1.text + 4)
#define ps2val()	(vps2.text + 4)
#define ps4val()	(vps4.text + 4)
#define optindval()	(voptind.text + 7)
#ifndef NO_HISTORY
#define histsizeval()	(vhistsize.text + 9)
#define termval()	(vterm.text + 5)
#endif

#define mpathset()	((vmpath.flags & VUNSET) == 0)
#define disvforkset()	((vdisvfork.flags & VUNSET) == 0)

void initvar(void);
void setvar(const_cstring_t, const_cstring_t, int32_t);
void setvareq(cstring_t, int32_t);
struct strlist;
void listsetvar(struct strlist*, int32_t);
cstring_t lookupvar(const_cstring_t);
cstring_t bltinlookup(const_cstring_t, int32_t);
void bltinsetlocale(void);
void bltinunsetlocale(void);
void updatecharset(void);
void initcharset(void);
cstring_t* environment(void);
int32_t showvarscmd(int32_t, cstring_t*);
void mklocal(cstring_t);
void poplocalvars(void);
int32_t unsetvar(const_cstring_t);
int32_t setvarsafe(const_cstring_t, const_cstring_t, int32_t);
