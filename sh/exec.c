/*	$FreeBSD: head/bin/sh/exec.c 268920 2014-07-20 12:06:52Z jilles $	*/
/*	static char sccsid[] = "@(#)exec.c	8.4 (Berkeley) 6/8/95";	*/
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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <paths.h>
#include <stdlib.h>

/*
 * When commands are first encountered, they are entered in a hash table.
 * This ensures that a full path search will not have to be done for them
 * on each invocation.
 *
 * We should investigate converting to a linear search, even though that
 * would make the command name "hash" a misnomer.
 */

#include "shell.h"
#include "main.h"
#include "nodes.h"
#include "parser.h"
#include "redir.h"
#include "eval.h"
#include "exec.h"
#include "builtins.h"
#include "var.h"
#include "options.h"
#include "input.h"
#include "output.h"
#include "syntax.h"
#include "memalloc.h"
#include "sherror.h"
#include "mystring.h"
#include "show.h"
#include "jobs.h"
#include "alias.h"

//extern int32_t __cdecl access(const_cstring_t path, int32_t mode);
//extern int32_t __cdecl eaccess(const_cstring_t path, int32_t mode);	// nonstandard

#define CMDTABLESIZE 31		/* should be prime */



typedef struct tblentry
{
	struct tblentry*	next;		/* next entry in hash chain */
	union param			param;		/* definition of builtin function */
	int32_t				special;	/* flag for special builtin commands */
	uint32_t			cmdtype;	/* index identifying command */
	cstring_t				cmdname;	/* name of command */
} tblentry_t;
typedef tblentry_t* ptblentry_t;

static ptblentry_t cmdtable[CMDTABLESIZE];
static int32_t cmdtable_cd = 0;	/* cmdtable contains cd-dependent entries */
int32_t exerrno = 0;			/* Last exec error */


static void tryexec(cstring_t, cstring_t*, cstring_t*);
static void printentry(ptblentry_t, int32_t);
static ptblentry_t cmdlookup(const_cstring_t, int32_t);
static void delete_cmd_entry(void);
static void addcmdentry(const_cstring_t, struct cmdentry*);



/*
 * Exec a program.  Never returns.  If you change this routine, you may
 * have to change the find_command routine as well.
 *
 * The argv array may be changed and element argv[-1] should be writable.
 */

void
shellexec(cstring_t* argv, cstring_t* envp, const_cstring_t path, int32_t idx)
{
	cstring_t cmdname;
	int32_t e;
	if (strchr(argv[0], '/') != NULL)
	{
		tryexec(argv[0], argv, envp);
		e = errno;
	}
	else
	{
		e = ENOENT;
		while ((cmdname = padvance(&path, argv[0])) != NULL)
		{
			if (--idx < 0 && pathopt == NULL)
			{
				tryexec(cmdname, argv, envp);
				if (errno != ENOENT && errno != ENOTDIR)
					e = errno;
				if (e == ENOEXEC)
					break;
			}
			stunalloc(cmdname);
		}
	}
	/* Map to POSIX errors */
	if (e == ENOENT || e == ENOTDIR)
	{
		exerrno = 127;
		exerror(EXEXEC, "%s: not found", argv[0]);
	}
	else
	{
		exerrno = 126;
		exerror(EXEXEC, "%s: %s", argv[0], strerror(e));
	}
}


static void
tryexec(cstring_t cmd, cstring_t* argv, cstring_t* envp)
{
	int32_t e, in;
	ssize_t n;
	char buf[256] = {0};
	execve(cmd, argv, envp);
	e = errno;
	if (e == ENOEXEC)
	{
		INTOFF;
		in = open(cmd, O_RDONLY | O_NONBLOCK);
		if (in != -1)
		{
			n = pread(in, buf, sizeof buf, 0);
			close(in);
			if (n > 0 && memchr(buf, '\0', n) != NULL)
			{
				errno = ENOEXEC;
				return;
			}
		}
		*argv = cmd;
		*--argv = (cstring_t)(uintptr_t)(pvoid_t)_PATH_BSHELL;
		execve(_PATH_BSHELL, argv, envp);
	}
	errno = e;
}

/*
 * Do a path search.  The variable path (passed by reference) should be
 * set to the start of the path before the first call; padvance will update
 * this value as it proceeds.  Successive calls to padvance will return
 * the possible path expansions in sequence.  If an option (indicated by
 * a percent sign) appears in the path entry then the global variable
 * pathopt will be set to point to it; otherwise pathopt will be set to
 * NULL.
 */

const_cstring_t pathopt;

cstring_t
padvance(const_cstring_t* path, const_cstring_t name)
{
	const_cstring_t p;
	const_cstring_t start;
	cstring_t q;
	size_t len, namelen;
	if (*path == NULL)
		return NULL;
	start = *path;
	for (p = start; *p && *p != ':' && *p != '%'; p++)
		; /* nothing */
	namelen = strlen(name);
	len = p - start + namelen + 2;	/* "2" is for '/' and '\0' */
	STARTSTACKSTR(q);
	CHECKSTRSPACE(len, q);
	if (p != start)
	{
		memcpy(q, start, p - start);
		q += p - start;
		*q++ = '/';
	}
	memcpy(q, name, namelen + 1);
	pathopt = NULL;
	if (*p == '%')
	{
		pathopt = ++p;
		while (*p && *p != ':')  p++;
	}
	if (*p == ':')
		*path = p + 1;
	else
		*path = NULL;
	return stalloc(len);
}



/*** Command hashing code ***/


int32_t
hashcmd(int32_t argc __unused, cstring_t* argv __unused)
{
	ptblentry_t* pp;
	ptblentry_t cmdp;
	int32_t c;
	int32_t verbose;
	struct cmdentry entry;
	cstring_t name;
	int32_t errors;
	(void)argc; (void)argv;

	errors = 0;
	verbose = 0;
	while ((c = nextopt("rv")) != '\0')
	{
		if (c == 'r')
		{
			clearcmdentry();
		}
		else if (c == 'v')
		{
			verbose++;
		}
	}
	if (*argptr == NULL)
	{
		for (pp = cmdtable ; pp < &cmdtable[CMDTABLESIZE] ; pp++)
		{
			for (cmdp = *pp ; cmdp ; cmdp = cmdp->next)
			{
				if (cmdp->cmdtype == CMDNORMAL)
					printentry(cmdp, verbose);
			}
		}
		return 0;
	}
	while ((name = *argptr) != NULL)
	{
		if ((cmdp = cmdlookup(name, 0)) != NULL
				&& cmdp->cmdtype == CMDNORMAL)
			delete_cmd_entry();
		find_command(name, &entry, DO_ERR, pathval());
		if (entry.cmdtype == CMDUNKNOWN)
			errors = 1;
		else if (verbose)
		{
			cmdp = cmdlookup(name, 0);
			if (cmdp != NULL)
				printentry(cmdp, verbose);
			else
			{
				outfmt(out2, "%s: not found\n", name);
				errors = 1;
			}
			flushall();
		}
		argptr++;
	}
	return errors;
}


static void
printentry(ptblentry_t cmdp, int32_t verbose)
{
	int32_t idx;
	const_cstring_t path;
	cstring_t name;
	if (cmdp->cmdtype == CMDNORMAL)
	{
		idx = cmdp->param.index;
		path = pathval();
		do
		{
			name = padvance(&path, cmdp->cmdname);
			stunalloc(name);
		}
		while (--idx >= 0);
		out1str(name);
	}
	else if (cmdp->cmdtype == CMDBUILTIN)
	{
		out1fmt("builtin %s", cmdp->cmdname);
	}
	else if (cmdp->cmdtype == CMDFUNCTION)
	{
		out1fmt("function %s", cmdp->cmdname);
		if (verbose)
		{
			INTOFF;
			name = commandtext(getfuncnode(cmdp->param.func));
			out1c(' ');
			out1str(name);
			ckfree(name);
			INTON;
		}
#ifdef DEBUG
	}
	else
	{
		sherror("internal error: cmdtype %d", cmdp->cmdtype);
#endif
	}
	out1c('\n');
}



/*
 * Resolve a command name.  If you change this routine, you may have to
 * change the shellexec routine as well.
 */

void
find_command(const_cstring_t name, struct cmdentry* entry, int32_t act,
			 const_cstring_t path)
{
	ptblentry_t cmdp;
	tblentry_t loc_cmd;
	int32_t idx;
	cstring_t fullname;
	struct stat statb;
	int32_t e;
	int32_t i;
	int32_t spec;
	int32_t cd;
	/* If name contains a slash, don't use the hash table */
	if (strchr(name, '/') != NULL)
	{
		entry->cmdtype = CMDNORMAL;
		entry->u.index = 0;
		return;
	}
	cd = 0;
	/* If name is in the table, and not invalidated by cd, we're done */
	if ((cmdp = cmdlookup(name, 0)) != NULL)
	{
		if (cmdp->cmdtype == CMDFUNCTION && act & DO_NOFUNC)
			cmdp = NULL;
		else
			goto success;
	}
	/* Check for builtin next */
	if ((i = find_builtin(name, &spec)) >= 0)
	{
		INTOFF;
		cmdp = cmdlookup(name, 1);
		if (cmdp->cmdtype == CMDFUNCTION)
			cmdp = &loc_cmd;
		cmdp->cmdtype = CMDBUILTIN;
		cmdp->param.index = i;
		cmdp->special = spec;
		INTON;
		goto success;
	}
	/* We have to search path. */
	e = ENOENT;
	idx = -1;
	for (; (fullname = padvance(&path, name)) != NULL; stunalloc(fullname))
	{
		idx++;
		if (pathopt)
		{
			if (strncmp(pathopt, "func", 4) == 0)
			{
				/* handled below */
			}
			else
			{
				continue; /* ignore unimplemented options */
			}
		}
		if (fullname[0] != '/')
			cd = 1;
		if (stat(fullname, &statb) < 0)
		{
			if (errno != ENOENT && errno != ENOTDIR)
				e = errno;
			continue;
		}
		e = EACCES;	/* if we fail, this will be the error */
		if (!S_ISREG(statb.st_mode))
			continue;
		if (pathopt)  		/* this is a %func directory */
		{
			readcmdfile(fullname);
			if ((cmdp = cmdlookup(name, 0)) == NULL || cmdp->cmdtype != CMDFUNCTION)
				sherror("%s not defined in %s", name, fullname);
			stunalloc(fullname);
			goto success;
		}
#ifdef notdef
		if (statb.st_uid == geteuid())
		{
			if ((statb.st_mode & 0100) == 0)
				goto loop;
		}
		else if (statb.st_gid == getegid())
		{
			if ((statb.st_mode & 010) == 0)
				goto loop;
		}
		else
		{
			if ((statb.st_mode & 01) == 0)
				goto loop;
		}
#endif
		TRACE(("searchexec \"%s\" returns \"%s\"\n", name, fullname));
		INTOFF;
		stunalloc(fullname);
		cmdp = cmdlookup(name, 1);
		if (cmdp->cmdtype == CMDFUNCTION)
			cmdp = &loc_cmd;
		cmdp->cmdtype = CMDNORMAL;
		cmdp->param.index = idx;
		INTON;
		goto success;
	}
	if (act & DO_ERR)
	{
		if (e == ENOENT || e == ENOTDIR)
			outfmt(out2, "%s: not found\n", name);
		else
			outfmt(out2, "%s: %s\n", name, strerror(e));
	}
	entry->cmdtype = CMDUNKNOWN;
	entry->u.index = 0;
	return;
success:
	if (cd)
		cmdtable_cd = 1;
	entry->cmdtype = cmdp->cmdtype;
	entry->u = cmdp->param;
	entry->special = cmdp->special;
}



/*
 * Search the table of builtin commands.
 */

int32_t
find_builtin(const_cstring_t name, int32_t* special)
{
	const struct builtincmd* bp;
	for (bp = builtincmd ; bp->name ; bp++)
	{
		if (*bp->name == *name && equal(bp->name, name))
		{
			*special = bp->special;
			return bp->code;
		}
	}
	return -1;
}



/*
 * Called when a cd is done.  If any entry in cmdtable depends on the current
 * directory, simply clear cmdtable completely.
 */

void
hashcd(void)
{
	if (cmdtable_cd)
		clearcmdentry();
}



/*
 * Called before PATH is changed.  The argument is the new value of PATH;
 * pathval() still returns the old value at this point.  Called with
 * interrupts off.
 */

void
changepath(const_cstring_t newval __unused)
{
	(void)newval;
	clearcmdentry();
}


/*
 * Clear out command entries.  The argument specifies the first entry in
 * PATH which has changed.
 */

void
clearcmdentry(void)
{
	ptblentry_t* tblp;
	ptblentry_t* pp;
	ptblentry_t cmdp;
	INTOFF;
	for (tblp = cmdtable ; tblp < &cmdtable[CMDTABLESIZE] ; tblp++)
	{
		pp = tblp;
		while ((cmdp = *pp) != NULL)
		{
			if (cmdp->cmdtype == CMDNORMAL)
			{
				*pp = cmdp->next;
				ckfree(cmdp);
			}
			else
			{
				pp = &cmdp->next;
			}
		}
	}
	cmdtable_cd = 0;
	INTON;
}


/*
 * Locate a command in the command hash table.  If "add" is nonzero,
 * add the command to the table if it is not already present.  The
 * variable "lastcmdentry" is set to point to the address of the link
 * pointing to the entry, so that delete_cmd_entry can delete the
 * entry.
 */

static ptblentry_t* lastcmdentry;


static ptblentry_t
cmdlookup(const_cstring_t name, int32_t add)
{
	int32_t hashval;
	const_cstring_t p;
	ptblentry_t cmdp;
	ptblentry_t* pp;
	size_t len;

	p = name;
	hashval = *p << 4;
	while (*p)
		hashval += *p++;
	hashval &= 0x7FFF;
	pp = &cmdtable[hashval % CMDTABLESIZE];
	for (cmdp = *pp ; cmdp ; cmdp = cmdp->next)
	{
		if (equal(cmdp->cmdname, name))
			break;
		pp = &cmdp->next;
	}
	if (add && cmdp == NULL)
	{
		INTOFF;
		len = strlen(name);
		cmdp = *pp = ckmalloc(sizeof(struct tblentry) + len + 1);
		cmdp->next = NULL;
		cmdp->cmdtype = CMDUNKNOWN;
		memcpy(cmdp->cmdname, name, len + 1);
		INTON;
	}
	lastcmdentry = pp;

	return cmdp;
}

/*
 * Delete the command entry returned on the last lookup.
 */

static void
delete_cmd_entry(void)
{
	ptblentry_t cmdp;
	INTOFF;
	cmdp = *lastcmdentry;
	*lastcmdentry = cmdp->next;
	ckfree(cmdp);
	INTON;
}



/*
 * Add a new command entry, replacing any existing command entry for
 * the same name.
 */

static void
addcmdentry(const_cstring_t name, struct cmdentry* entry)
{
	ptblentry_t cmdp;
	INTOFF;
	cmdp = cmdlookup(name, 1);
	if (cmdp->cmdtype == CMDFUNCTION)
	{
		unreffunc(cmdp->param.func);
	}
	cmdp->cmdtype = entry->cmdtype;
	cmdp->param = entry->u;
	INTON;
}


/*
 * Define a shell function.
 */

void
defun(const_cstring_t name, union node* func)
{
	struct cmdentry entry;
	INTOFF;
	entry.cmdtype = CMDFUNCTION;
	entry.u.func = copyfunc(func);
	addcmdentry(name, &entry);
	INTON;
}


/*
 * Delete a function if it exists.
 * Called with interrupts off.
 */

int32_t
unsetfunc(const_cstring_t name)
{
	ptblentry_t cmdp;
	if ((cmdp = cmdlookup(name, 0)) != NULL && cmdp->cmdtype == CMDFUNCTION)
	{
		unreffunc(cmdp->param.func);
		delete_cmd_entry();
		return (0);
	}
	return (0);
}


/*
 * Check if a function by a certain name exists.
 */
int32_t
isfunc(const_cstring_t name)
{
	ptblentry_t cmdp;
	cmdp = cmdlookup(name, 0);
	return (cmdp != NULL && cmdp->cmdtype == CMDFUNCTION);
}


/*
 * Shared code for the following builtin commands:
 *    type, command -v, command -V
 */

int32_t
typecmd_impl(int32_t argc, cstring_t* argv, int32_t cmd, const_cstring_t path)
{
	struct cmdentry entry;
	ptblentry_t cmdp;
	const_cstring_t const* pp;
	struct alias* ap;
	int32_t i;
	int32_t error1 = 0;
	if (path != pathval())
		clearcmdentry();
	for (i = 1; i < argc; i++)
	{
		/* First look at the keywords */
		for (pp = parsekwd; *pp; pp++)
			if (**pp == *argv[i] && equal(*pp, argv[i]))
				break;
		if (*pp)
		{
			if (cmd == TYPECMD_SMALLV)
				out1fmt("%s\n", argv[i]);
			else
				out1fmt("%s is a shell keyword\n", argv[i]);
			continue;
		}
		/* Then look at the aliases */
		if ((ap = lookupalias(argv[i], 1)) != NULL)
		{
			if (cmd == TYPECMD_SMALLV)
			{
				out1fmt("alias %s=", argv[i]);
				out1qstr(ap->val);
				outcslow('\n', out1);
			}
			else
				out1fmt("%s is an alias for %s\n", argv[i],
						ap->val);
			continue;
		}
		/* Then check if it is a tracked alias */
		if ((cmdp = cmdlookup(argv[i], 0)) != NULL)
		{
			entry.cmdtype = cmdp->cmdtype;
			entry.u = cmdp->param;
			entry.special = cmdp->special;
		}
		else
		{
			/* Finally use brute force */
			find_command(argv[i], &entry, 0, path);
		}
		switch (entry.cmdtype)
		{
			case CMDNORMAL:
			{
				if (strchr(argv[i], '/') == NULL)
				{
					const_cstring_t path2 = path;
					cstring_t name;
					int32_t j = entry.u.index;
					do
					{
						name = padvance(&path2, argv[i]);
						stunalloc(name);
					}
					while (--j >= 0);
					if (cmd == TYPECMD_SMALLV)
						out1fmt("%s\n", name);
					else
						out1fmt("%s is%s %s\n", argv[i],
								(cmdp && cmd == TYPECMD_TYPE) ?
								" a tracked alias for" : "",
								name);
				}
				else
				{
					if (access(argv[i], X_OK) == 0)
					{
						if (cmd == TYPECMD_SMALLV)
							out1fmt("%s\n", argv[i]);
						else
							out1fmt("%s is %s\n", argv[i],
									argv[i]);
					}
					else
					{
						if (cmd != TYPECMD_SMALLV)
							outfmt(out2, "%s: %s\n",
								   argv[i], strerror(errno));
						error1 |= 127;
					}
				}
				break;
			}
			case CMDFUNCTION:
				if (cmd == TYPECMD_SMALLV)
					out1fmt("%s\n", argv[i]);
				else
					out1fmt("%s is a shell function\n", argv[i]);
				break;
			case CMDBUILTIN:
				if (cmd == TYPECMD_SMALLV)
					out1fmt("%s\n", argv[i]);
				else if (entry.special)
					out1fmt("%s is a special shell builtin\n",
							argv[i]);
				else
					out1fmt("%s is a shell builtin\n", argv[i]);
				break;
			default:
				if (cmd != TYPECMD_SMALLV)
					outfmt(out2, "%s: not found\n", argv[i]);
				error1 |= 127;
				break;
		}
	}
	if (path != pathval())
		clearcmdentry();
	return error1;
}

/*
 * Locate and print what a word is...
 */

int32_t
typecmd(int32_t argc, cstring_t* argv)
{
	if (argc > 2 && strcmp(argv[1], "--") == 0)
		argc--, argv++;
	return typecmd_impl(argc, argv, TYPECMD_TYPE, bltinlookup("PATH", 1));
}
