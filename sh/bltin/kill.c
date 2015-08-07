/*	$FreeBSD: head/bin/kill/kill.c 263206 2014-03-15 14:58:48Z jilles $	*/
/*	static char sccsid[] = "@(#)kill.c	8.4 (Berkeley) 4/28/95";	*/
/*-
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
/*
 * Important: This file is used both as a standalone program /bin/kill and
 * as a builtin for /bin/sh (#define SHELL).
 */

#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bltin.h"
#include "sherror.h"

static void nosig(const_cstring_t);
static void printsignals(FILE*);
static int32_t signame_to_signum(const_cstring_t);
static void usage(void);

int32_t
killcmd(int32_t argc, cstring_t argv[])
{
	int32_t errors, ret;
	intptr_t numsig;
	pid_t pid;
	cstring_t ep;
	if (argc < 2)
		usage();
	numsig = SIGTERM;
	argc--, argv++;
	if (!strcmp(*argv, "-l"))
	{
		argc--, argv++;
		if (argc > 1)
			usage();
		if (argc == 1)
		{
			if (!isdigit(**argv))
				usage();
			numsig = strtol(*argv, &ep, 10);
			if (!** argv || *ep)
				errx(2, "illegal signal number: %s", *argv);
			if (numsig >= 128)
				numsig -= 128;
			if (numsig <= 0 || numsig >= sys_nsig)
				nosig(*argv);
			printf("%s\n", sys_signame[numsig]);
			return (0);
		}
		printsignals(stdout);
		return (0);
	}
	if (!strcmp(*argv, "-s"))
	{
		argc--, argv++;
		if (argc < 1)
		{
			warnx("option requires an argument -- s");
			usage();
		}
		if (strcmp(*argv, "0"))
		{
			if ((numsig = signame_to_signum(*argv)) < 0)
				nosig(*argv);
		}
		else
			numsig = 0;
		argc--, argv++;
	}
	else if (**argv == '-' && *(*argv + 1) != '-')
	{
		++*argv;
		if (isalpha(**argv))
		{
			if ((numsig = signame_to_signum(*argv)) < 0)
				nosig(*argv);
		}
		else if (isdigit(**argv))
		{
			numsig = strtol(*argv, &ep, 10);
			if (!** argv || *ep)
				errx(2, "illegal signal number: %s", *argv);
			if (numsig < 0)
				nosig(*argv);
		}
		else
			nosig(*argv);
		argc--, argv++;
	}
	if (argc > 0 && strncmp(*argv, "--", 2) == 0)
		argc--, argv++;
	if (argc == 0)
		usage();
	for (errors = 0; argc; argc--, argv++)
	{
		if (**argv == '%')
			ret = killjob(*argv, numsig);
		else
		{
			pid = strtol(*argv, &ep, 10);
			if (!** argv || *ep)
				errx(2, "illegal process id: %s", *argv);
			ret = kill(pid, numsig);
		}
		if (ret == -1)
		{
			warn("%s", *argv);
			errors = 1;
		}
	}
	return (errors);
}

static int32_t
signame_to_signum(const_cstring_t sig)
{
	int32_t n;
	if (strncasecmp(sig, "SIG", 3) == 0)
		sig += 3;
	for (n = 1; n < sys_nsig; n++)
	{
		if (!strcasecmp(sys_signame[n], sig))
			return (n);
	}
	return (-1);
}

static void
nosig(const_cstring_t name)
{
	warnx("unknown signal %s; valid signals:", name);
	printsignals(stderr);
	sherror(NULL);
}

static void
printsignals(FILE* fp)
{
	int32_t n;
	for (n = 1; n < sys_nsig; n++)
	{
		(void)fprintf(fp, "%s", sys_signame[n]);
		if (n == (sys_nsig / 2) || n == (sys_nsig - 1))
			(void)fprintf(fp, "\n");
		else
			(void)fprintf(fp, " ");
	}
}

static void
usage(void)
{
	(void)fprintf(stderr, "%s\n%s\n%s\n%s\n",
				  "usage: kill [-s signal_name] pid ...",
				  "       kill -l [exit_status]",
				  "       kill -signal_name pid ...",
				  "       kill -signal_number pid ...");
	sherror(NULL);
}
