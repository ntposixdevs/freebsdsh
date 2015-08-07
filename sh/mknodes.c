/*	$FreeBSD: head/bin/sh/mknodes.c 196634 2009-08-28 22:41:25Z jilles $	*/
/*	static char sccsid[] = "@(#)mknodes.c	8.2 (Berkeley) 5/4/95";	*/
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

/*
 * This program reads the nodetypes file and nodes.c.pat file.  It generates
 * the files nodes.h and nodes.c.
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#define MAXTYPES 50		/* max number of node types */
#define MAXFIELDS 20		/* max fields in a structure */
#define BUFLEN 100		/* size of character buffers */

/* field types */
#define T_NODE 1		/* union node *field */
#define T_NODELIST 2		/* struct nodelist *field */
#define T_STRING 3
#define T_INT 4			/* int32_t field */
#define T_OTHER 5		/* other */
#define T_TEMP 6		/* don't copy this field */


struct field  			/* a structure field */
{
	cstring_t name;		/* name of field */
	int32_t type;			/* type of field */
	cstring_t decl;		/* declaration of field */
};


struct str  			/* struct representing a node structure */
{
	cstring_t tag;		/* structure tag */
	int32_t nfields;		/* number of fields in the structure */
	struct field field[MAXFIELDS];	/* the fields of the structure */
	int32_t done;			/* set if fully parsed */
};


static int32_t ntypes;			/* number of node types */
static cstring_t nodename[MAXTYPES];	/* names of the nodes */
static struct str* nodestr[MAXTYPES];	/* type of structure used by the node */
static int32_t nstr;			/* number of structures */
static struct str str[MAXTYPES];	/* the structures */
static struct str* curstr;		/* current structure */
static FILE* infp;
static char line[1024];
static int32_t linno;
static cstring_t linep;

static void parsenode(void);
static void parsefield(void);
static void output(cstring_t);
static void outsizes(FILE*);
static void outfunc(FILE*, int32_t);
static void indent(int32_t, FILE*);
static int32_t nextfield(cstring_t);
static void skipbl(void);
static int32_t readline(void);
static void sherror(const_cstring_t, ...) /*__printf0like(1, 2) __dead2*/;
static cstring_t savestr(const_cstring_t);


int32_t
main(int32_t argc, cstring_t argv[])
{
	if (argc != 3)
		sherror("usage: mknodes file");
	infp = stdin;
	if ((infp = fopen(argv[1], "r")) == NULL)
		sherror("Can't open %s: %s", argv[1], strerror(errno));
	while (readline())
	{
		if (line[0] == ' ' || line[0] == '\t')
			parsefield();
		else if (line[0] != '\0')
			parsenode();
	}
	output(argv[2]);
	exit(0);
}



static void
parsenode(void)
{
	char name[BUFLEN];
	char tag[BUFLEN];
	struct str* sp;
	if (curstr && curstr->nfields > 0)
		curstr->done = 1;
	nextfield(name);
	if (! nextfield(tag))
		sherror("Tag expected");
	if (*linep != '\0')
		sherror("Garbage at end of line");
	nodename[ntypes] = savestr(name);
	for (sp = str ; sp < str + nstr ; sp++)
	{
		if (strcmp(sp->tag, tag) == 0)
			break;
	}
	if (sp >= str + nstr)
	{
		sp->tag = savestr(tag);
		sp->nfields = 0;
		curstr = sp;
		nstr++;
	}
	nodestr[ntypes] = sp;
	ntypes++;
}


static void
parsefield(void)
{
	char name[BUFLEN];
	char type[BUFLEN];
	char decl[2 * BUFLEN];
	struct field* fp;
	if (curstr == NULL || curstr->done)
		sherror("No current structure to add field to");
	if (! nextfield(name))
		sherror("No field name");
	if (! nextfield(type))
		sherror("No field type");
	fp = &curstr->field[curstr->nfields];
	fp->name = savestr(name);
	if (strcmp(type, "nodeptr") == 0)
	{
		fp->type = T_NODE;
		sprintf(decl, "union node *%s", name);
	}
	else if (strcmp(type, "nodelist") == 0)
	{
		fp->type = T_NODELIST;
		sprintf(decl, "struct nodelist *%s", name);
	}
	else if (strcmp(type, "string") == 0)
	{
		fp->type = T_STRING;
		sprintf(decl, "char *%s", name);
	}
	else if (strcmp(type, "int32_t") == 0)
	{
		fp->type = T_INT;
		sprintf(decl, "int32_t %s", name);
	}
	else if (strcmp(type, "other") == 0)
	{
		fp->type = T_OTHER;
	}
	else if (strcmp(type, "temp") == 0)
	{
		fp->type = T_TEMP;
	}
	else
	{
		sherror("Unknown type %s", type);
	}
	if (fp->type == T_OTHER || fp->type == T_TEMP)
	{
		skipbl();
		fp->decl = savestr(linep);
	}
	else
	{
		if (*linep)
			sherror("Garbage at end of line");
		fp->decl = savestr(decl);
	}
	curstr->nfields++;
}


char writer[] = "\
/*\n\
 * This file was generated by the mknodes program.\n\
 */\n\
\n";

static void
output(cstring_t file)
{
	FILE* hfile;
	FILE* cfile;
	FILE* patfile;
	int32_t i;
	struct str* sp;
	struct field* fp;
	cstring_t p;
	if ((patfile = fopen(file, "r")) == NULL)
		sherror("Can't open %s: %s", file, strerror(errno));
	if ((hfile = fopen("nodes.h", "w")) == NULL)
		sherror("Can't create nodes.h: %s", strerror(errno));
	if ((cfile = fopen("nodes.c", "w")) == NULL)
		sherror("Can't create nodes.c");
	fputs(writer, hfile);
	for (i = 0 ; i < ntypes ; i++)
		fprintf(hfile, "#define %s %d\n", nodename[i], i);
	fputs("\n\n\n", hfile);
	for (sp = str ; sp < &str[nstr] ; sp++)
	{
		fprintf(hfile, "struct %s {\n", sp->tag);
		for (i = sp->nfields, fp = sp->field ; --i >= 0 ; fp++)
		{
			fprintf(hfile, "      %s;\n", fp->decl);
		}
		fputs("};\n\n\n", hfile);
	}
	fputs("union node {\n", hfile);
	fprintf(hfile, "      int32_t type;\n");
	for (sp = str ; sp < &str[nstr] ; sp++)
	{
		fprintf(hfile, "      struct %s %s;\n", sp->tag, sp->tag);
	}
	fputs("};\n\n\n", hfile);
	fputs("struct nodelist {\n", hfile);
	fputs("\tstruct nodelist *next;\n", hfile);
	fputs("\tunion node *n;\n", hfile);
	fputs("};\n\n\n", hfile);
	fputs("struct funcdef;\n", hfile);
	fputs("struct funcdef *copyfunc(union node *);\n", hfile);
	fputs("union node *getfuncnode(struct funcdef *);\n", hfile);
	fputs("void reffunc(struct funcdef *);\n", hfile);
	fputs("void unreffunc(struct funcdef *);\n", hfile);
	fputs(writer, cfile);
	while (fgets(line, sizeof line, patfile) != NULL)
	{
		for (p = line ; *p == ' ' || *p == '\t' ; p++);
		if (strcmp(p, "%SIZES\n") == 0)
			outsizes(cfile);
		else if (strcmp(p, "%CALCSIZE\n") == 0)
			outfunc(cfile, 1);
		else if (strcmp(p, "%COPY\n") == 0)
			outfunc(cfile, 0);
		else
			fputs(line, cfile);
	}
}



static void
outsizes(FILE* cfile)
{
	int32_t i;
	fprintf(cfile, "static const int16_t nodesize[%d] = {\n", ntypes);
	for (i = 0 ; i < ntypes ; i++)
	{
		fprintf(cfile, "      ALIGN(sizeof (struct %s)),\n", nodestr[i]->tag);
	}
	fprintf(cfile, "};\n");
}


static void
outfunc(FILE* cfile, int32_t calcsize)
{
	struct str* sp;
	struct field* fp;
	int32_t i;
	fputs("      if (n == NULL)\n", cfile);
	if (calcsize)
		fputs("	    return;\n", cfile);
	else
		fputs("	    return NULL;\n", cfile);
	if (calcsize)
		fputs("      funcblocksize += nodesize[n->type];\n", cfile);
	else
	{
		fputs("      new = funcblock;\n", cfile);
		fputs("      funcblock = (char *)funcblock + nodesize[n->type];\n", cfile);
	}
	fputs("      switch (n->type) {\n", cfile);
	for (sp = str ; sp < &str[nstr] ; sp++)
	{
		for (i = 0 ; i < ntypes ; i++)
		{
			if (nodestr[i] == sp)
				fprintf(cfile, "      case %s:\n", nodename[i]);
		}
		for (i = sp->nfields ; --i >= 1 ;)
		{
			fp = &sp->field[i];
			switch (fp->type)
			{
				case T_NODE:
					if (calcsize)
					{
						indent(12, cfile);
						fprintf(cfile, "calcsize(n->%s.%s);\n",
								sp->tag, fp->name);
					}
					else
					{
						indent(12, cfile);
						fprintf(cfile, "new->%s.%s = copynode(n->%s.%s);\n",
								sp->tag, fp->name, sp->tag, fp->name);
					}
					break;
				case T_NODELIST:
					if (calcsize)
					{
						indent(12, cfile);
						fprintf(cfile, "sizenodelist(n->%s.%s);\n",
								sp->tag, fp->name);
					}
					else
					{
						indent(12, cfile);
						fprintf(cfile, "new->%s.%s = copynodelist(n->%s.%s);\n",
								sp->tag, fp->name, sp->tag, fp->name);
					}
					break;
				case T_STRING:
					if (calcsize)
					{
						indent(12, cfile);
						fprintf(cfile, "funcstringsize += strlen(n->%s.%s) + 1;\n",
								sp->tag, fp->name);
					}
					else
					{
						indent(12, cfile);
						fprintf(cfile, "new->%s.%s = nodesavestr(n->%s.%s);\n",
								sp->tag, fp->name, sp->tag, fp->name);
					}
					break;
				case T_INT:
				case T_OTHER:
					if (! calcsize)
					{
						indent(12, cfile);
						fprintf(cfile, "new->%s.%s = n->%s.%s;\n",
								sp->tag, fp->name, sp->tag, fp->name);
					}
					break;
			}
		}
		indent(12, cfile);
		fputs("break;\n", cfile);
	}
	fputs("      };\n", cfile);
	if (! calcsize)
		fputs("      new->type = n->type;\n", cfile);
}


static void
indent(int32_t amount, FILE* fp)
{
	while (amount >= 8)
	{
		putc('\t', fp);
		amount -= 8;
	}
	while (--amount >= 0)
	{
		putc(' ', fp);
	}
}


static int32_t
nextfield(cstring_t buf)
{
	cstring_t p;
	cstring_t q;
	p = linep;
	while (*p == ' ' || *p == '\t')
		p++;
	q = buf;
	while (*p != ' ' && *p != '\t' && *p != '\0')
		*q++ = *p++;
	*q = '\0';
	linep = p;
	return (q > buf);
}


static void
skipbl(void)
{
	while (*linep == ' ' || *linep == '\t')
		linep++;
}


static int32_t
readline(void)
{
	cstring_t p;
	if (fgets(line, 1024, infp) == NULL)
		return 0;
	for (p = line ; *p != '#' && *p != '\n' && *p != '\0' ; p++);
	while (p > line && (p[-1] == ' ' || p[-1] == '\t'))
		p--;
	*p = '\0';
	linep = line;
	linno++;
	if (p - line > BUFLEN)
		sherror("Line too long");
	return 1;
}



static void
sherror(const_cstring_t msg, ...)
{
	va_list va;
	va_start(va, msg);
	(void) fprintf(stderr, "line %d: ", linno);
	(void) vfprintf(stderr, msg, va);
	(void) fputc('\n', stderr);
	va_end(va);
	exit(2);
}



static cstring_t
savestr(const_cstring_t s)
{
	cstring_t p;
	if ((p = malloc(strlen(s) + 1)) == NULL)
		sherror("Out of space");
	(void) strcpy(p, s);
	return p;
}
