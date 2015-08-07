/*	$FreeBSD: head/bin/sh/output.c 253649 2013-07-25 13:09:17Z jilles $	*/
/*	static char sccsid[] = "@(#)output.c	8.2 (Berkeley) 5/4/95";	*/
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
#include <stdio.h>	/* defines BUFSIZ */
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "shell.h"
#include "syntax.h"
#include "output.h"
#include "memalloc.h"
#include "sherror.h"
#include "var.h"

/*
 * Shell output routines.  We use our own output routines because:
 *	When a builtin command is interrupted we have to discard
 *		any pending output.
 *	When a builtin command appears in back quotes, we want to
 *		save the output of the command in a region obtained
 *		via malloc, rather than doing a fork and reading the
 *		output of the command via a pipe.
 */

#define OUTBUFSIZ BUFSIZ
#define MEM_OUT -2		/* output to dynamically allocated memory */
#define OUTPUT_ERR 01		/* error occurred on output */

static size_t doformat_wr(pvoid_t, const_cstring_t, size_t len);

output_t output = {NULL, 0, NULL, OUTBUFSIZ, 1, 0};
output_t errout = {NULL, 0, NULL, 256, 2, 0};
output_t memout = {NULL, 0, NULL, 0, MEM_OUT, 0};
poutput_t out1 = &output;
poutput_t out2 = &errout;

void
outcslow(int32_t c, poutput_t file)
{
	outc(c, file);
}

void
out1str(const_cstring_t p)
{
	outstr(p, out1);
}

void
out1qstr(const_cstring_t p)
{
	outqstr(p, out1);
}

void
out2str(const_cstring_t p)
{
	outstr(p, out2);
}

void
out2qstr(const_cstring_t p)
{
	outqstr(p, out2);
}

void
outstr(const_cstring_t p, poutput_t file)
{
	outbin(p, strlen(p), file);
}

/* Like outstr(), but quote for re-input into the shell. */
void
outqstr(const_cstring_t p, poutput_t file)
{
	char ch;
	int32_t inquotes;
	if (p[0] == '\0')
	{
		outstr("''", file);
		return;
	}
	/* Caller will handle '=' if necessary */
	if (p[strcspn(p, "|&;<>()$`\\\"' \t\n*?[~#")] == '\0' ||
			strcmp(p, "[") == 0)
	{
		outstr(p, file);
		return;
	}
	inquotes = 0;
	while ((ch = *p++) != '\0')
	{
		switch (ch)
		{
			case '\'':
				/* Can't quote single quotes inside single quotes. */
				if (inquotes)
					outcslow('\'', file);
				inquotes = 0;
				outstr("\\'", file);
				break;
			default:
				if (!inquotes)
					outcslow('\'', file);
				inquotes = 1;
				outc(ch, file);
		}
	}
	if (inquotes)
		outcslow('\'', file);
}

void
outbin(const_pvoid_t data, size_t len, poutput_t file)
{
	const_cstring_t p;
	p = data;
	while (len-- > 0)
		outc(*p++, file);
}

void
emptyoutbuf(poutput_t dest)
{
	int32_t offset;
	if (dest->buf == NULL)
	{
		INTOFF;
		dest->buf = ckmalloc(dest->bufsize);
		dest->nextc = dest->buf;
		dest->nleft = dest->bufsize;
		INTON;
	}
	else if (dest->fd == MEM_OUT)
	{
		offset = dest->bufsize;
		INTOFF;
		dest->bufsize <<= 1;
		dest->buf = ckrealloc(dest->buf, dest->bufsize);
		dest->nleft = dest->bufsize - offset;
		dest->nextc = dest->buf + offset;
		INTON;
	}
	else
	{
		flushout(dest);
	}
	dest->nleft--;
}


void
flushall(void)
{
	flushout(&output);
	flushout(&errout);
}


void
flushout(poutput_t dest)
{
	if (dest->buf == NULL || dest->nextc == dest->buf || dest->fd < 0)
		return;
	if (xwrite(dest->fd, dest->buf, dest->nextc - dest->buf) < 0)
		dest->flags |= OUTPUT_ERR;
	dest->nextc = dest->buf;
	dest->nleft = dest->bufsize;
}


void
freestdout(void)
{
	INTOFF;
	if (output.buf)
	{
		ckfree(output.buf);
		output.buf = NULL;
		output.nleft = 0;
	}
	INTON;
}


int32_t
outiserror(poutput_t file)
{
	return (file->flags & OUTPUT_ERR);
}


void
outclearerror(poutput_t file)
{
	file->flags &= ~OUTPUT_ERR;
}


void
outfmt(poutput_t file, const_cstring_t fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	doformat(file, fmt, ap);
	va_end(ap);
}


void
out1fmt(const_cstring_t fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	doformat(out1, fmt, ap);
	va_end(ap);
}

void
out2fmt_flush(const_cstring_t fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	doformat(out2, fmt, ap);
	va_end(ap);
	flushout(out2);
}

void
fmtstr(cstring_t outbuf, int32_t length, const_cstring_t fmt, ...)
{
	va_list ap;
	INTOFF;
	va_start(ap, fmt);
	vsnprintf(outbuf, length, fmt, ap);
	va_end(ap);
	INTON;
}

static size_t
doformat_wr(pvoid_t cookie, const_cstring_t buf, size_t len)
{
	poutput_t o;
	o = (poutput_t)cookie;
	outbin(buf, len, o);
	return (len);
}

void
doformat(poutput_t dest, const_cstring_t f, va_list ap)
{
	FILE* fp;
	if ((fp = fwopen(dest, doformat_wr)) != NULL)
	{
		vfprintf(fp, f, ap);
		fclose(fp);
	}
}

/*
 * Version of write which resumes after a signal is caught.
 */

int32_t
xwrite(int32_t fd, const_cstring_t buf, int32_t nbytes)
{
	int32_t ntry;
	int32_t i;
	int32_t n;
	n = nbytes;
	ntry = 0;
	for (;;)
	{
		i = write(fd, buf, n);
		if (i > 0)
		{
			if ((n -= i) <= 0)
				return nbytes;
			buf += i;
			ntry = 0;
		}
		else if (i == 0)
		{
			if (++ntry > 10)
				return nbytes - n;
		}
		else if (errno != EINTR)
		{
			return -1;
		}
	}
}
