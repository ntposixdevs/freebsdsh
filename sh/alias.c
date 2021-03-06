/*	$FreeBSD: head/bin/sh/alias.c 263777 2014-03-26 20:43:40Z jilles $	*/
/*	static char sccsid[] = "@(#)alias.c	8.3 (Berkeley) 5/4/95";	*/
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
#include <sys/cdefs.h>
#include <sys/types.h>
#include <stdlib.h>
#include "shell.h"
#include "output.h"
#include "sherror.h"
#include "memalloc.h"
#include "mystring.h"
#include "alias.h"
#include "options.h"	/* XXX for argptr (should remove?) */
#include "builtins.h"

#define ATABSIZE 39

static struct alias* atab[ATABSIZE];
static int32_t aliases;

static void setalias(const_cstring_t, const_cstring_t);
static int32_t unalias(const_cstring_t);
static struct alias** hashalias(const_cstring_t);

static
void
setalias(const_cstring_t name, const_cstring_t val)
{
	struct alias* ap, **app;
	app = hashalias(name);
	for (ap = *app; ap; ap = ap->next)
	{
		if (equal(name, ap->name))
		{
			INTOFF;
			ckfree(ap->val);
			ap->val	= savestr(val);
			INTON;
			return;
		}
	}
	/* not found */
	INTOFF;
	ap = ckmalloc(sizeof(struct alias));
	ap->name = savestr(name);
	ap->val = savestr(val);
	ap->flag = 0;
	ap->next = *app;
	*app = ap;
	aliases++;
	INTON;
}

static int32_t
unalias(const_cstring_t name)
{
	struct alias* ap, **app;
	app = hashalias(name);
	for (ap = *app; ap; app = &(ap->next), ap = ap->next)
	{
		if (equal(name, ap->name))
		{
			/*
			 * if the alias is currently in use (i.e. its
			 * buffer is being used by the input routine) we
			 * just null out the name instead of freeing it.
			 * We could clear it out later, but this situation
			 * is so rare that it hardly seems worth it.
			 */
			if (ap->flag & ALIASINUSE)
				*ap->name = '\0';
			else
			{
				INTOFF;
				*app = ap->next;
				ckfree(ap->name);
				ckfree(ap->val);
				ckfree(ap);
				INTON;
			}
			aliases--;
			return (0);
		}
	}
	return (1);
}

static void
rmaliases(void)
{
	struct alias* ap, *tmp;
	int32_t i;
	INTOFF;
	for (i = 0; i < ATABSIZE; i++)
	{
		ap = atab[i];
		atab[i] = NULL;
		while (ap)
		{
			ckfree(ap->name);
			ckfree(ap->val);
			tmp = ap;
			ap = ap->next;
			ckfree(tmp);
		}
	}
	aliases = 0;
	INTON;
}

struct alias*
lookupalias(const_cstring_t name, int32_t check)
{
	struct alias* ap = *hashalias(name);
	for (; ap; ap = ap->next)
	{
		if (equal(name, ap->name))
		{
			if (check && (ap->flag & ALIASINUSE))
				return (NULL);
			return (ap);
		}
	}
	return (NULL);
}

static int32_t __cdecl
comparealiases(const_pvoid_t p1, const_pvoid_t p2)
{
	const struct alias* const* a1 = p1;
	const struct alias* const* a2 = p2;
	return strcmp((*a1)->name, (*a2)->name);
}

static void
printalias(const struct alias* a)
{
	out1fmt("%s=", a->name);
	out1qstr(a->val);
	out1c('\n');
}

static void
printaliases(void)
{
	int32_t i, j;
	struct alias** sorted, *ap;
	INTOFF;
	sorted = ckmalloc(aliases * sizeof(*sorted));
	j = 0;
	for (i = 0; i < ATABSIZE; i++)
		for (ap = atab[i]; ap; ap = ap->next)
			if (*ap->name != '\0')
				sorted[j++] = ap;
	qsort(sorted, aliases, sizeof(*sorted), comparealiases);
	for (i = 0; i < aliases; i++)
	{
		printalias(sorted[i]);
		if (int_pending())
			break;
	}
	ckfree(sorted);
	INTON;
}

int32_t
aliascmd(int32_t argc __unused, cstring_t* argv __unused)
{
	cstring_t n;
	cstring_t v;
	int32_t ret = 0;
	struct alias* ap;
	(void)argc; (void)argv;

	nextopt("");
	if (*argptr == NULL)
	{
		printaliases();
		return (0);
	}
	while ((n = *argptr++) != NULL)
	{
		if ((v = strchr(n + 1, '=')) == NULL) /* n+1: funny ksh stuff */
			if ((ap = lookupalias(n, 0)) == NULL)
			{
				warning("%s: not found", n);
				ret = 1;
			}
			else
				printalias(ap);
		else
		{
			*v++ = '\0';
			setalias(n, v);
		}
	}
	return (ret);
}

int32_t
unaliascmd(int32_t argc __unused, cstring_t* argv __unused)
{
	char32_t i;
	(void)argc; (void)argv;

	while ((i = nextopt("a")) != '\0')
	{
		if (i == 'a')
		{
			rmaliases();
			return (0);
		}
	}
	for (i = 0; *argptr; argptr++)
		i |= unalias(*argptr);
	return (i);
}

static struct alias**
hashalias(const_cstring_t p)
{
	uint32_t hashval;
	hashval = *p << 4;
	while (*p)
		hashval += *p++;
	return &atab[hashval % ATABSIZE];
}
