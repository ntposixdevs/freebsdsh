/*	$FreeBSD: head/bin/sh/input.c 271593 2014-09-14 16:46:30Z jilles $	*/
/*	static char sccsid[] = "@(#)input.c	8.3 (Berkeley) 6/9/95";	*/
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
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/*
 * This file implements the input routines used by the parser.
 */

#include "shell.h"
#include "redir.h"
#include "syntax.h"
#include "input.h"
#include "output.h"
#include "options.h"
#include "memalloc.h"
#include "sherror.h"
#include "alias.h"
#include "parser.h"
#include "myhistedit.h"
#include "trap.h"

#define EOF_NLEFT SIZE_MAX		/* -99 value of parsenleft when EOF pushed back */

struct strpush
{
	struct strpush* prev;	/* preceding string on stack */
	const_cstring_t prevstring;
	size_t prevnleft;
	size_t prevlleft;
	struct alias* ap;	/* if push was associated with an alias */
};

/*
 * The parsefile structure pointed to by the global variable parsefile
 * contains information about the current file being read.
 */

struct parsefile
{
	struct parsefile* prev;	/* preceding file on stack */
	int32_t linno;		/* current line */
	int32_t fd;			/* file descriptor (or -1 if string) */
	size_t nleft;		/* number of chars left in this line */
	size_t lleft;		/* number of lines left in this buffer */
	const_cstring_t nextc;	/* next char in buffer */
	cstring_t buf;		/* input buffer */
	struct strpush* strpush; /* for pushing strings at this level */
	struct strpush basestrpush; /* so pushing one is fast */
};


int32_t plinno = 1;			/* input line number */
size_t parsenleft;			/* copy of parsefile->nleft */
static size_t parselleft;		/* copy of parsefile->lleft */
const_cstring_t parsenextc;		/* copy of parsefile->nextc */
static char basebuf[BUFSIZ + 1];/* buffer for top level input file */
static struct parsefile basepf =  	/* top level input file */
{
	.nextc = basebuf,
	.buf = basebuf
};
static struct parsefile* parsefile = &basepf;	/* current input file */
int32_t whichprompt;		/* 1 == PS1, 2 == PS2 */

EditLine* el;			/* cookie for editline package */

static void pushfile(void);
static ssize_t preadfd(void);
static void popstring(void);

void
resetinput(void)
{
	popallfiles();
	parselleft = parsenleft = 0;	/* clear input buffer */
}



/*
 * Read a character from the script, returning PEOF on end of file.
 * Nul characters in the input are silently discarded.
 */

int32_t
pgetc(void)
{
	// #define pgetc_macro()	(--parsenleft >= 0? *parsenextc++ : preadbuffer())
	if (parsenleft > 0)
	{
		--parsenleft;
		return *parsenextc++;
	}
	return preadbuffer();
}


static ssize_t
preadfd(void)
{
	ssize_t nr;
	parsenextc = parsefile->buf;
retry:
	if (parsefile->fd == 0 && el)
	{
		static const_cstring_t rl_cp;
		static ssize_t el_len;
		if (rl_cp == NULL)
		{
			el_resize(el);
			rl_cp = el_gets(el, &el_len);
		}
		if (rl_cp == NULL)
			nr = el_len == 0 ? 0 : -1;
		else
		{
			nr = el_len;
			if (nr > BUFSIZ)
				nr = BUFSIZ;
			memcpy(parsefile->buf, rl_cp, nr);
			if (nr != el_len)
			{
				el_len -= nr;
				rl_cp += nr;
			}
			else
				rl_cp = NULL;
		}
	}
	else
		nr = read(parsefile->fd, parsefile->buf, BUFSIZ);
	if (nr <= 0)
	{
		if (nr < 0)
		{
			if (errno == EINTR)
				goto retry;
			if (parsefile->fd == 0 && errno == EWOULDBLOCK)
			{
				int32_t flags = fcntl(0, F_GETFL, 0);
				if (flags >= 0 && flags & O_NONBLOCK)
				{
					flags &= ~ O_NONBLOCK;
					if (fcntl(0, F_SETFL, flags) >= 0)
					{
						out2fmt_flush("sh: turning off NDELAY mode\n");
						goto retry;
					}
				}
			}
		}
		nr = -1;
	}
	return nr;
}

/*
 * Refill the input buffer and return the next input character:
 *
 * 1) If a string was pushed back on the input, pop it;
 * 2) If an EOF was pushed back (parsenleft == EOF_NLEFT) or we are reading
 *    from a string so we can't refill the buffer, return EOF.
 * 3) If there is more in this buffer, use it else call read to fill it.
 * 4) Process input up to the next newline, deleting nul characters.
 */
int32_t
preadbuffer(void)
{
	cstring_t p;
	cstring_t q;
	int32_t more;
	char savec;

/* !!!! NOTE: This code needs to be reimplemented !!!! */
	while (parsefile->strpush)
	{
		/*
		 * Add a space to the end of an alias to ensure that the
		 * alias remains in use while parsing its last word.
		 * This avoids alias recursions.
		 */
		if (parsenleft == 0 /*-1*/ && parsefile->strpush->ap != NULL)
			return ' ';
		popstring();
		if (parsenleft > 0)
		{
			--parsenleft;
			return (*parsenextc++);
		}
	}
	if (parsenleft == EOF_NLEFT || parsefile->buf == NULL)
		return PEOF;
	flushout(&output);
	flushout(&errout);
again:
	if (parselleft <= 0)
	{
		if ((parselleft = preadfd()) == -1)
		{
			parselleft = parsenleft = EOF_NLEFT;
			return PEOF;
		}
	}
	q = p = parsefile->buf + (parsenextc - parsefile->buf);
	/* delete nul characters */
	for (more = 1; more;)
	{
		switch (*p)
		{
			case '\0':
				p++;	/* Skip nul */
				goto check;
			case '\n':
				parsenleft = (q - parsenextc);
				more = 0; /* Stop processing here */
				break;
			default:
				break;
		}
		*q++ = *p++;
check:
		if (--parselleft <= 0)
		{
			parsenleft = (q - parsenextc - 1);
			if (parsenleft < 0)
				goto again;
			*q = '\0';
			more = 0;
		}
	}
	savec = *q;
	*q = '\0';

	if (parsefile->fd == 0 && hist &&
			parsenextc[strspn(parsenextc, " \t\n")] != '\0')
	{
		HistEvent he;
		INTOFF;
		history(hist, &he, whichprompt == 1 ? H_ENTER : H_ADD,
				parsenextc);
		INTON;
	}

	if (vflag)
	{
		out2str(parsenextc);
		flushout(out2);
	}
	*q = savec;
	return *parsenextc++;
}

/*
 * Returns if we are certain we are at EOF. Does not cause any more input
 * to be read from the outside world.
 */

int32_t
preadateof(void)
{
	if (parsenleft > 0)
		return 0;
	if (parsefile->strpush)
		return 0;
	if (parsenleft == EOF_NLEFT || parsefile->buf == NULL)
		return 1;
	return 0;
}

/*
 * Undo the last call to pgetc.  Only one character may be pushed back.
 * PEOF may be pushed back.
 */

void
pungetc(void)
{
	parsenleft++;
	parsenextc--;
}

/*
 * Push a string back onto the input at this current parsefile level.
 * We handle aliases this way.
 */
void
pushstring(const_cstring_t s, size_t len, struct alias* ap)
{
	struct strpush* sp;
	INTOFF;
	/*out2fmt_flush("*** calling pushstring: %s, %d\n", s, len);*/
	if (parsefile->strpush)
	{
		sp = ckmalloc(sizeof(struct strpush));
		sp->prev = parsefile->strpush;
		parsefile->strpush = sp;
	}
	else
		sp = parsefile->strpush = &(parsefile->basestrpush);
	sp->prevstring = parsenextc;
	sp->prevnleft = parsenleft;
	sp->prevlleft = parselleft;
	sp->ap = ap;
	if (ap)
		ap->flag |= ALIASINUSE;
	parsenextc = s;
	parsenleft = len;
	INTON;
}

static void
popstring(void)
{
	struct strpush* sp = parsefile->strpush;
	INTOFF;
	if (sp->ap)
	{
		if (parsenextc != sp->ap->val &&
				(parsenextc[-1] == ' ' || parsenextc[-1] == '\t'))
			forcealias();
		sp->ap->flag &= ~ALIASINUSE;
	}
	parsenextc = sp->prevstring;
	parsenleft = sp->prevnleft;
	parselleft = sp->prevlleft;
	/*out2fmt_flush("*** calling popstring: restoring to '%s'\n", parsenextc);*/
	parsefile->strpush = sp->prev;
	if (sp != &(parsefile->basestrpush))
		ckfree(sp);
	INTON;
}

/*
 * Set the input to take input from a file.  If push is set, push the
 * old input onto the stack first.
 */

void
setinputfile(const_cstring_t fname, int32_t push)
{
	int32_t fd;
	int32_t fd2;
	INTOFF;
	if ((fd = open(fname, O_RDONLY/* | O_CLOEXEC*/)) < 0)
		sherror("cannot open %s: %s", fname, strerror(errno));
	if (fd < 10)
	{
		fd2 = fcntl(fd, F_DUPFD /*F_DUPFD_CLOEXEC*/, 10);
		close(fd);
		if (fd2 < 0)
			sherror("Out of file descriptors");
		fd = fd2;
	}
	setinputfd(fd, push);
	INTON;
}


/*
 * Like setinputfile, but takes an open file descriptor (which should have
 * its FD_CLOEXEC flag already set).  Call this with interrupts off.
 */

void
setinputfd(int32_t fd, int32_t push)
{
	if (push)
	{
		pushfile();
		parsefile->buf = ckmalloc(BUFSIZ + 1);
	}
	if (parsefile->fd > 0)
		close(parsefile->fd);
	parsefile->fd = fd;
	if (parsefile->buf == NULL)
		parsefile->buf = ckmalloc(BUFSIZ + 1);
	parselleft = parsenleft = 0;
	plinno = 1;
}


/*
 * Like setinputfile, but takes input from a string.
 */

void
setinputstring(const_cstring_t string, int32_t push)
{
	INTOFF;
	if (push)
		pushfile();
	parsenextc = string;
	parselleft = parsenleft = strlen(string);
	parsefile->buf = NULL;
	plinno = 1;
	INTON;
}



/*
 * To handle the "." command, a stack of input files is used.  Pushfile
 * adds a new entry to the stack and popfile restores the previous level.
 */

static void
pushfile(void)
{
	struct parsefile* pf;
	parsefile->nleft = parsenleft;
	parsefile->lleft = parselleft;
	parsefile->nextc = parsenextc;
	parsefile->linno = plinno;
	pf = (struct parsefile*)ckmalloc(sizeof(struct parsefile));
	pf->prev = parsefile;
	pf->fd = -1;
	pf->strpush = NULL;
	pf->basestrpush.prev = NULL;
	parsefile = pf;
}


void
popfile(void)
{
	struct parsefile* pf = parsefile;
	INTOFF;
	if (pf->fd >= 0)
		close(pf->fd);
	if (pf->buf)
		ckfree(pf->buf);
	while (pf->strpush)
		popstring();
	parsefile = pf->prev;
	ckfree(pf);
	parsenleft = parsefile->nleft;
	parselleft = parsefile->lleft;
	parsenextc = parsefile->nextc;
	plinno = parsefile->linno;
	INTON;
}


/*
 * Return current file (to go back to it later using popfilesupto()).
 */

struct parsefile*
getcurrentfile(void)
{
	return parsefile;
}


/*
 * Pop files until the given file is on top again. Useful for regular
 * builtins that read shell commands from files or strings.
 * If the given file is not an active file, an error is raised.
 */

void
popfilesupto(struct parsefile* file)
{
	while (parsefile != file && parsefile != &basepf)
		popfile();
	if (parsefile != file)
		sherror("popfilesupto() misused");
}

/*
 * Return to top level.
 */

void
popallfiles(void)
{
	while (parsefile != &basepf)
		popfile();
}



/*
 * Close the file(s) that the shell is reading commands from.  Called
 * after a fork is done.
 */

void
closescript(void)
{
	popallfiles();
	if (parsefile->fd > 0)
	{
		close(parsefile->fd);
		parsefile->fd = 0;
	}
}
