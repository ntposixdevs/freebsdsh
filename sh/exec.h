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
 *	@(#)exec.h	8.3 (Berkeley) 6/8/95
 * $FreeBSD: head/bin/sh/exec.h 238468 2012-07-15 10:19:43Z jilles $
 */

/* values of cmdtype */
typedef enum __sh_cmdtype_const
{
	CMDUNKNOWN,		/* no entry in table for command */
	CMDNORMAL,		/* command is an executable program */
	CMDBUILTIN,		/* command is a shell builtin */
	CMDFUNCTION,	/* command is a shell function */
};

/* values for typecmd_impl's third parameter */
typedef enum __typecmd_impl_param
{
	TYPECMD_SMALLV,		/* command -v */
	TYPECMD_BIGV,		/* command -V */
	TYPECMD_TYPE		/* type */
};

union node;
struct cmdentry
{
	int32_t cmdtype;
	union param
	{
		int32_t index;
		struct funcdef* func;
	} u;
	int32_t special;
};


/* action to find_command() */
#define DO_ERR		0x01	/* prints errors */
#define DO_NOFUNC	0x02	/* don't return shell functions, for command */

extern const_cstring_t pathopt;	/* set by padvance */
extern int32_t exerrno;		/* last exec error */

DECLSPEC_NORETURN void shellexec(cstring_t*, cstring_t*, const_cstring_t, int32_t);
cstring_t padvance(const_cstring_t*, const_cstring_t);
void find_command(const_cstring_t, struct cmdentry*, int32_t, const_cstring_t);
int32_t find_builtin(const_cstring_t, int32_t*);
void hashcd(void);
void changepath(const_cstring_t);
void defun(const_cstring_t, union node*);
int32_t unsetfunc(const_cstring_t);
int32_t isfunc(const_cstring_t);
int32_t typecmd_impl(int32_t, cstring_t*, int32_t, const_cstring_t);
void clearcmdentry(void);
