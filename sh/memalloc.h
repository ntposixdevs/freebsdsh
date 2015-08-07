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
 *	@(#)memalloc.h	8.2 (Berkeley) 5/4/95
 * $FreeBSD: head/bin/sh/memalloc.h 250527 2013-05-11 20:51:00Z jilles $
 */

#include <string.h>

struct stackmark
{
	struct stack_block* stackp;
	cstring_t stacknxt;
	int32_t stacknleft;
};


extern cstring_t stacknxt;
extern int32_t stacknleft;
extern cstring_t sstrend;

pvoid_t ckmalloc(size_t);
pvoid_t ckrealloc(pvoid_t, int32_t);
void ckfree(pvoid_t);
cstring_t savestr(const_cstring_t);
pvoid_t stalloc(size_t nbytes);
void stunalloc(pvoid_t);
void setstackmark(struct stackmark*);
void popstackmark(struct stackmark*);
cstring_t growstackstr(void);
cstring_t makestrspace(size_t, cstring_t);
cstring_t stputbin(const_cstring_t data, size_t len, cstring_t p);
cstring_t stputs(const_cstring_t data, cstring_t p);



#define stackblock() stacknxt
#define stackblocksize() stacknleft
#define grabstackblock(n) stalloc(n)
#define STARTSTACKSTR(p)	p = stackblock()
#define STPUTC(c, p)	do { if (p == sstrend) p = growstackstr(); *p++ = (c); } while(0)
#define CHECKSTRSPACE(n, p)	{ if ((size_t)(sstrend - p) < n) p = makestrspace(n, p); }
#define USTPUTC(c, p)	(*p++ = (c))
/*
 * STACKSTRNUL's use is where we want to be able to turn a stack
 * (non-sentinel, character counting string) into a C string,
 * and later pretend the NUL is not there.
 * Note: Because of STACKSTRNUL's semantics, STACKSTRNUL cannot be used
 * on a stack that will grabstackstr()ed.
 */
#define STACKSTRNUL(p)	(p == sstrend ? (p = growstackstr(), *p = '\0') : (*p = '\0'))
#define STUNPUTC(p)	(--p)
#define STTOPC(p)	p[-1]
#define STADJUST(amount, p)	(p += (amount))
#define grabstackstr(p)	stalloc((char *)p - stackblock())
#define ungrabstackstr(s, p)	stunalloc((s))
#define STPUTBIN(s, len, p)	p = stputbin((s), (len), p)
#define STPUTS(s, p)	p = stputs((s), p)
