/*	$FreeBSD: head/bin/sh/cd.c 258776 2013-11-30 21:27:11Z jilles $	*/
/*	static char sccsid[] = "@(#)cd.c	8.2 (Berkeley) 5/4/95";	*/
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
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

/*
 * The cd and pwd commands.
 */

#include "shell.h"
#include "var.h"
#include "nodes.h"	/* for jobs.h */
#include "jobs.h"
#include "options.h"
#include "output.h"
#include "memalloc.h"
#include "sherror.h"
#include "exec.h"
#include "redir.h"
#include "mystring.h"
#include "show.h"
#include "cd.h"
#include "builtins.h"

static int32_t cdlogical(cstring_t);
static int32_t cdphysical(cstring_t);
static int32_t docd(cstring_t, int32_t, int32_t);
static cstring_t getcomponent(void);
static cstring_t findcwd(cstring_t);
static void updatepwd(cstring_t);
static cstring_t getpwd(void);
static cstring_t getpwd2(void);

static cstring_t curdir = NULL;	/* current working directory */
static cstring_t prevdir;		/* previous working directory */
static cstring_t cdcomppath;

int32_t
cdcmd(int32_t argc __unused, cstring_t* argv __unused)
{
	const_cstring_t dest;
	const_cstring_t path;
	cstring_t p;
	struct stat statb;
	int32_t ch, phys, print = 0, getcwderr = 0;
	int32_t rc;
	int32_t errno1 = ENOENT;
	(void)argc; (void)argv;

	phys = Pflag;
	while ((ch = nextopt("eLP")) != '\0')
	{
		switch (ch)
		{
			case 'e':
				getcwderr = 1;
				break;
			case 'L':
				phys = 0;
				break;
			case 'P':
				phys = 1;
				break;
		}
	}
	if (*argptr != NULL && argptr[1] != NULL)
		sherror("too many arguments");
	if ((dest = *argptr) == NULL && (dest = bltinlookup("HOME", 1)) == NULL)
		sherror("HOME not set");
	if (*dest == '\0')
		dest = ".";
	if (dest[0] == '-' && dest[1] == '\0')
	{
		dest = prevdir ? prevdir : curdir;
		if (dest)
			print = 1;
		else
			dest = ".";
	}
	if (dest[0] == '/' ||
			(dest[0] == '.' && (dest[1] == '/' || dest[1] == '\0')) ||
			(dest[0] == '.' && dest[1] == '.' && (dest[2] == '/' || dest[2] == '\0')) ||
			(path = bltinlookup("CDPATH", 1)) == NULL)
		path = nullstr;
	while ((p = padvance(&path, dest)) != NULL)
	{
		if (stat(p, &statb) < 0)
		{
			if (errno != ENOENT)
				errno1 = errno;
		}
		else if (!S_ISDIR(statb.st_mode))
			errno1 = ENOTDIR;
		else
		{
			if (!print)
			{
				/*
				 * XXX - rethink
				 */
				if (p[0] == '.' && p[1] == '/' && p[2] != '\0')
					print = strcmp(p + 2, dest);
				else
					print = strcmp(p, dest);
			}
			rc = docd(p, print, phys);
			if (rc >= 0)
				return getcwderr ? rc : 0;
			if (errno != ENOENT)
				errno1 = errno;
		}
	}
	sherror("%s: %s", dest, strerror(errno1));
	NOTREACHED;	// this is bad
	return 0;
}


/*
 * Actually change the directory.  In an interactive shell, print the
 * directory name if "print" is nonzero.
 */
static int32_t
docd(cstring_t dest, int32_t print, int32_t phys)
{
	int32_t rc;
	TRACE(("docd(\"%s\", %d, %d) called\n", dest, print, phys));
	/* If logical cd fails, fall back to physical. */
	if ((phys || (rc = cdlogical(dest)) < 0) && (rc = cdphysical(dest)) < 0)
		return (-1);
	if (print && iflag && curdir)
		out1fmt("%s\n", curdir);
	return (rc);
}

static int32_t
cdlogical(cstring_t dest)
{
	cstring_t p;
	cstring_t q;
	cstring_t component;
	struct stat statb;
	int32_t first;
	int32_t badstat;
	size_t len;
	/*
	 *  Check each component of the path. If we find a symlink or
	 *  something we can't stat, clear curdir to force a getcwd()
	 *  next time we get the value of the current directory.
	 */
	badstat = 0;
	len = strlen(dest);
	cdcomppath = stalloc(len + 1);
	memcpy(cdcomppath, dest, len + 1);
	STARTSTACKSTR(p);
	if (*dest == '/')
	{
		STPUTC('/', p);
		cdcomppath++;
	}
	first = 1;
	while ((q = getcomponent()) != NULL)
	{
		if (q[0] == '\0' || (q[0] == '.' && q[1] == '\0'))
			continue;
		if (! first)
			STPUTC('/', p);
		first = 0;
		component = q;
		STPUTS(q, p);
		if (equal(component, ".."))
			continue;
		STACKSTRNUL(p);
		if (lstat(stackblock(), &statb) < 0)
		{
			badstat = 1;
			break;
		}
	}
	INTOFF;
	if ((p = findcwd(badstat ? NULL : dest)) == NULL || chdir(p) < 0)
	{
		INTON;
		return (-1);
	}
	updatepwd(p);
	INTON;
	return (0);
}

static int32_t
cdphysical(cstring_t dest)
{
	cstring_t p;
	int32_t rc = 0;
	INTOFF;
	if (chdir(dest) < 0)
	{
		INTON;
		return (-1);
	}
	p = findcwd(NULL);
	if (p == NULL)
	{
		warning("warning: failed to get name of current directory");
		rc = 1;
	}
	updatepwd(p);
	INTON;
	return (rc);
}

/*
 * Get the next component of the path name pointed to by cdcomppath.
 * This routine overwrites the string pointed to by cdcomppath.
 */
static cstring_t
getcomponent(void)
{
	cstring_t p;
	cstring_t start;
	if ((p = cdcomppath) == NULL)
		return NULL;
	start = cdcomppath;
	while (*p != '/' && *p != '\0')
		p++;
	if (*p == '\0')
	{
		cdcomppath = NULL;
	}
	else
	{
		*p++ = '\0';
		cdcomppath = p;
	}
	return start;
}


static cstring_t
findcwd(cstring_t dir)
{
	cstring_t newdir;
	cstring_t p;
	size_t len;
	/*
	 * If our argument is NULL, we don't know the current directory
	 * any more because we traversed a symbolic link or something
	 * we couldn't stat().
	 */
	if (dir == NULL || curdir == NULL)
		return getpwd2();
	len = strlen(dir);
	cdcomppath = stalloc(len + 1);
	memcpy(cdcomppath, dir, len + 1);
	STARTSTACKSTR(newdir);
	if (*dir != '/')
	{
		STPUTS(curdir, newdir);
		if (STTOPC(newdir) == '/')
			STUNPUTC(newdir);
	}
	while ((p = getcomponent()) != NULL)
	{
		if (equal(p, ".."))
		{
			while (newdir > stackblock() && (STUNPUTC(newdir), *newdir) != '/');
		}
		else if (*p != '\0' && ! equal(p, "."))
		{
			STPUTC('/', newdir);
			STPUTS(p, newdir);
		}
	}
	if (newdir == stackblock())
		STPUTC('/', newdir);
	STACKSTRNUL(newdir);
	return stackblock();
}

/*
 * Update curdir (the name of the current directory) in response to a
 * cd command.  We also call hashcd to let the routines in exec.c know
 * that the current directory has changed.
 */
static void
updatepwd(cstring_t dir)
{
	hashcd();				/* update command hash table */
	if (prevdir)
		ckfree(prevdir);
	prevdir = curdir;
	curdir = dir ? savestr(dir) : NULL;
	setvar("PWD", curdir, VEXPORT);
	setvar("OLDPWD", prevdir, VEXPORT);
}

int32_t
pwdcmd(int32_t argc __unused, cstring_t* argv __unused)
{
	cstring_t p;
	int32_t ch, phys;
	(void)argc; (void)argv;

	phys = Pflag;
	while ((ch = nextopt("LP")) != '\0')
	{
		switch (ch)
		{
			case 'L':
				phys = 0;
				break;
			case 'P':
				phys = 1;
				break;
		}
	}
	if (*argptr != NULL)
		sherror("too many arguments");
	if (!phys && getpwd())
	{
		out1str(curdir);
		out1c('\n');
	}
	else
	{
		if ((p = getpwd2()) == NULL)
			sherror(".: %s", strerror(errno));
		out1str(p);
		out1c('\n');
	}
	return 0;
}

/*
 * Get the current directory and cache the result in curdir.
 */
static cstring_t
getpwd(void)
{
	cstring_t p;
	if (curdir)
		return curdir;
	p = getpwd2();
	if (p != NULL)
		curdir = savestr(p);
	return curdir;
}

#define MAXPWD 256

/*
 * Return the current directory.
 */
static cstring_t
getpwd2(void)
{
	cstring_t pwd;
	size_t i;
	for (i = MAXPWD;; i *= 2)
	{
		pwd = stalloc(i);
		if (getcwd(pwd, i) != NULL)
			return pwd;
		stunalloc(pwd);
		if (errno != ERANGE)
			break;
	}
	return NULL;
}

/*
 * Initialize PWD in a new shell.
 * If the shell is interactive, we need to warn if this fails.
 */
void
pwd_init(int32_t warn)
{
	cstring_t pwd;
	struct stat stdot, stpwd;
	pwd = lookupvar("PWD");
	if (pwd && *pwd == '/' && stat(".", &stdot) != -1 &&
			stat(pwd, &stpwd) != -1 &&
			stdot.st_dev == stpwd.st_dev &&
			stdot.st_ino == stpwd.st_ino)
	{
		if (curdir)
			ckfree(curdir);
		curdir = savestr(pwd);
	}
	if (getpwd() == NULL && warn)
		out2fmt_flush("sh: cannot determine working directory\n");
	setvar("PWD", curdir, VEXPORT);
}
