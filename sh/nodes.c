/*
 * This file was generated by the mknodes program.
 */

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
 *	@(#)nodes.c.pat	8.2 (Berkeley) 5/4/95
 * $FreeBSD: head/bin/sh/nodes.c.pat 249235 2013-04-07 16:28:36Z jilles $
 */

#include <sys/param.h>
#include <stdlib.h>
#include <stddef.h>
/*
 * Routine for dealing with parsed shell commands.
 */

#include "shell.h"
#include "nodes.h"
#include "memalloc.h"
#include "mystring.h"


static int     funcblocksize;	/* size of structures in function */
static int     funcstringsize;	/* size of strings in node */
static pointer funcblock;	/* block to allocate function from */
static char*   funcstring;	/* block to allocate strings from */

static const short nodesize[27] =
{
	ALIGN(sizeof(struct nbinary)),
	ALIGN(sizeof(struct ncmd)),
	ALIGN(sizeof(struct npipe)),
	ALIGN(sizeof(struct nredir)),
	ALIGN(sizeof(struct nredir)),
	ALIGN(sizeof(struct nredir)),
	ALIGN(sizeof(struct nbinary)),
	ALIGN(sizeof(struct nbinary)),
	ALIGN(sizeof(struct nif)),
	ALIGN(sizeof(struct nbinary)),
	ALIGN(sizeof(struct nbinary)),
	ALIGN(sizeof(struct nfor)),
	ALIGN(sizeof(struct ncase)),
	ALIGN(sizeof(struct nclist)),
	ALIGN(sizeof(struct nclist)),
	ALIGN(sizeof(struct narg)),
	ALIGN(sizeof(struct narg)),
	ALIGN(sizeof(struct nfile)),
	ALIGN(sizeof(struct nfile)),
	ALIGN(sizeof(struct nfile)),
	ALIGN(sizeof(struct nfile)),
	ALIGN(sizeof(struct nfile)),
	ALIGN(sizeof(struct ndup)),
	ALIGN(sizeof(struct ndup)),
	ALIGN(sizeof(struct nhere)),
	ALIGN(sizeof(struct nhere)),
	ALIGN(sizeof(struct nnot)),
};


static void calcsize(union node*);
static void sizenodelist(struct nodelist*);
static union node* copynode(union node*);
static struct nodelist* copynodelist(struct nodelist*);
static char* nodesavestr(const char*);


struct funcdef
{
	unsigned int refcount;
	union node n;
};

/*
 * Make a copy of a parse tree.
 */

struct funcdef*
copyfunc(union node* n)
{
	struct funcdef* fn;
	if (n == NULL)
		return NULL;
	funcblocksize = offsetof(struct funcdef, n);
	funcstringsize = 0;
	calcsize(n);
	fn = ckmalloc(funcblocksize + funcstringsize);
	fn->refcount = 1;
	funcblock = (char*)fn + offsetof(struct funcdef, n);
	funcstring = (char*)fn + funcblocksize;
	copynode(n);
	return fn;
}


union node*
		getfuncnode(struct funcdef* fn)
{
	return fn == NULL ? NULL : &fn->n;
}


static void
calcsize(union node* n)
{
	if (n == NULL)
		return;
	funcblocksize += nodesize[n->type];
	switch (n->type)
	{
		case NSEMI:
		case NAND:
		case NOR:
		case NWHILE:
		case NUNTIL:
			calcsize(n->nbinary.ch2);
			calcsize(n->nbinary.ch1);
			break;
		case NCMD:
			calcsize(n->ncmd.redirect);
			calcsize(n->ncmd.args);
			break;
		case NPIPE:
			sizenodelist(n->npipe.cmdlist);
			break;
		case NREDIR:
		case NBACKGND:
		case NSUBSHELL:
			calcsize(n->nredir.redirect);
			calcsize(n->nredir.n);
			break;
		case NIF:
			calcsize(n->nif.elsepart);
			calcsize(n->nif.ifpart);
			calcsize(n->nif.test);
			break;
		case NFOR:
			funcstringsize += strlen(n->nfor.var) + 1;
			calcsize(n->nfor.body);
			calcsize(n->nfor.args);
			break;
		case NCASE:
			calcsize(n->ncase.cases);
			calcsize(n->ncase.expr);
			break;
		case NCLIST:
		case NCLISTFALLTHRU:
			calcsize(n->nclist.body);
			calcsize(n->nclist.pattern);
			calcsize(n->nclist.next);
			break;
		case NDEFUN:
		case NARG:
			sizenodelist(n->narg.backquote);
			funcstringsize += strlen(n->narg.text) + 1;
			calcsize(n->narg.next);
			break;
		case NTO:
		case NFROM:
		case NFROMTO:
		case NAPPEND:
		case NCLOBBER:
			calcsize(n->nfile.fname);
			calcsize(n->nfile.next);
			break;
		case NTOFD:
		case NFROMFD:
			calcsize(n->ndup.vname);
			calcsize(n->ndup.next);
			break;
		case NHERE:
		case NXHERE:
			calcsize(n->nhere.doc);
			calcsize(n->nhere.next);
			break;
		case NNOT:
			calcsize(n->nnot.com);
			break;
	};
}



static void
sizenodelist(struct nodelist* lp)
{
	while (lp)
	{
		funcblocksize += ALIGN(sizeof(struct nodelist));
		calcsize(lp->n);
		lp = lp->next;
	}
}



static union node*
		copynode(union node* n)
{
	union node* new;
	if (n == NULL)
		return NULL;
	new = funcblock;
	funcblock = (char*)funcblock + nodesize[n->type];
	switch (n->type)
	{
		case NSEMI:
		case NAND:
		case NOR:
		case NWHILE:
		case NUNTIL:
			new->nbinary.ch2 = copynode(n->nbinary.ch2);
			new->nbinary.ch1 = copynode(n->nbinary.ch1);
			break;
		case NCMD:
			new->ncmd.redirect = copynode(n->ncmd.redirect);
			new->ncmd.args = copynode(n->ncmd.args);
			break;
		case NPIPE:
			new->npipe.cmdlist = copynodelist(n->npipe.cmdlist);
			new->npipe.backgnd = n->npipe.backgnd;
			break;
		case NREDIR:
		case NBACKGND:
		case NSUBSHELL:
			new->nredir.redirect = copynode(n->nredir.redirect);
			new->nredir.n = copynode(n->nredir.n);
			break;
		case NIF:
			new->nif.elsepart = copynode(n->nif.elsepart);
			new->nif.ifpart = copynode(n->nif.ifpart);
			new->nif.test = copynode(n->nif.test);
			break;
		case NFOR:
			new->nfor.var = nodesavestr(n->nfor.var);
			new->nfor.body = copynode(n->nfor.body);
			new->nfor.args = copynode(n->nfor.args);
			break;
		case NCASE:
			new->ncase.cases = copynode(n->ncase.cases);
			new->ncase.expr = copynode(n->ncase.expr);
			break;
		case NCLIST:
		case NCLISTFALLTHRU:
			new->nclist.body = copynode(n->nclist.body);
			new->nclist.pattern = copynode(n->nclist.pattern);
			new->nclist.next = copynode(n->nclist.next);
			break;
		case NDEFUN:
		case NARG:
			new->narg.backquote = copynodelist(n->narg.backquote);
			new->narg.text = nodesavestr(n->narg.text);
			new->narg.next = copynode(n->narg.next);
			break;
		case NTO:
		case NFROM:
		case NFROMTO:
		case NAPPEND:
		case NCLOBBER:
			new->nfile.fname = copynode(n->nfile.fname);
			new->nfile.next = copynode(n->nfile.next);
			new->nfile.fd = n->nfile.fd;
			break;
		case NTOFD:
		case NFROMFD:
			new->ndup.vname = copynode(n->ndup.vname);
			new->ndup.dupfd = n->ndup.dupfd;
			new->ndup.next = copynode(n->ndup.next);
			new->ndup.fd = n->ndup.fd;
			break;
		case NHERE:
		case NXHERE:
			new->nhere.doc = copynode(n->nhere.doc);
			new->nhere.next = copynode(n->nhere.next);
			new->nhere.fd = n->nhere.fd;
			break;
		case NNOT:
			new->nnot.com = copynode(n->nnot.com);
			break;
	};
	new->type = n->type;
	return new;
}


static struct nodelist*
copynodelist(struct nodelist* lp)
{
	struct nodelist* start;
	struct nodelist** lpp;
	lpp = &start;
	while (lp)
	{
		*lpp = funcblock;
		funcblock = (char*)funcblock + ALIGN(sizeof(struct nodelist));
		(*lpp)->n = copynode(lp->n);
		lp = lp->next;
		lpp = &(*lpp)->next;
	}
	*lpp = NULL;
	return start;
}



static char*
nodesavestr(const char* s)
{
	const char* p = s;
	char* q = funcstring;
	char*   rtn = funcstring;
	while ((*q++ = *p++) != '\0')
		continue;
	funcstring = q;
	return rtn;
}


void
reffunc(struct funcdef* fn)
{
	if (fn)
		fn->refcount++;
}


/*
 * Decrement the reference count of a function definition, freeing it
 * if it falls to 0.
 */

void
unreffunc(struct funcdef* fn)
{
	if (fn)
	{
		fn->refcount--;
		if (fn->refcount > 0)
			return;
		ckfree(fn);
	}
}
