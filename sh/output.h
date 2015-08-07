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
 *	@(#)output.h	8.2 (Berkeley) 5/4/95
 * $FreeBSD: head/bin/sh/output.h 244162 2012-12-12 22:01:10Z jilles $
 */

#ifndef OUTPUT_INCL
#define OUTPUT_INCL

#include <stdarg.h>
#include <stddef.h>

typedef struct output
{
	cstring_t nextc;
	size_t nleft;
	cstring_t buf;
	size_t bufsize;
	int16_t fd;
	int16_t flags;
} output_t;
typedef output_t* poutput_t;

extern output_t output; /* to fd 1 */
extern output_t errout; /* to fd 2 */
extern output_t memout;
extern poutput_t out1; /* &memout if backquote, otherwise &output */
extern poutput_t out2; /* &memout if backquote with 2>&1, otherwise
			       &errout */

void outcslow(int32_t, poutput_t);
void out1str(const_cstring_t);
void out1qstr(const_cstring_t);
void out2str(const_cstring_t);
void out2qstr(const_cstring_t);
void outstr(const_cstring_t, poutput_t);
void outqstr(const_cstring_t, poutput_t);
void outbin(const_pvoid_t, size_t, poutput_t);
void emptyoutbuf(poutput_t);
void flushall(void);
void flushout(poutput_t);
void freestdout(void);
int32_t outiserror(poutput_t);
void outclearerror(poutput_t);
void outfmt(poutput_t, const_cstring_t, ...) __printflike(2, 3);
void out1fmt(const_cstring_t, ...) __printflike(1, 2);
void out2fmt_flush(const_cstring_t, ...) __printflike(1, 2);
void fmtstr(cstring_t, int32_t, const_cstring_t, ...) __printflike(3, 4);
void doformat(poutput_t, const_cstring_t, va_list) __printflike(2, 0);
int32_t xwrite(int32_t, const_cstring_t, int32_t);

// #define outc(c, file) \
// (--(file)->nleft < 0? (emptyoutbuf(file), *(file)->nextc++ = (c)) : (*(file)->nextc++ = (c)))
FORCEINLINE void outc(char32_t ch, poutput_t file)
{
	if (file->nleft == 0)
		emptyoutbuf(file);
#if _UNSURE_IMPLEMENTATION
	else
		--(file->nleft);
#endif
	*(file->nextc++) = (cchar_t)ch;
}

#define out1c(c)	outc(c, out1);
#define out2c(c)	outcslow(c, out2);

#endif
