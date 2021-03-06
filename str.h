/*    str.h
 *
 * Copyright 2017 Robert L (Bob) Parker rlp1938@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

/* The purpose of str.[h|c] is to provide some utility functions for
 * manipulating memory. Consequently many of the functions will simply
 * be wrappers around the C library functions str*() and mem*().
 * */
#ifndef _STR_H
#define _STR_H
#define _GNU_SOURCE 1
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <getopt.h>
#include <ctype.h>
#include <limits.h>
#include <linux/limits.h>
#include <libgen.h>
#include <errno.h>

typedef struct mdata {
	char *fro;
	char *to;
	char *limit;
} mdata;

int
printstrlist(char **list);

size_t
countmemstr(mdata *md);

char
*mktmpfn(char *prname, char *extrafn, char *thename);

mdata
*init_mdata(void);

void
free_mdata(mdata *);

void
*xmalloc(size_t n);

void
meminsert(const char *line, mdata *md, size_t meminc);

void
memreplace(mdata *md, char *find , char *repl, off_t meminc);

void
memresize(mdata *md, off_t meminc);

int
memlinestostr(mdata *md);

size_t
memstrtolines(mdata *md);

void
strjoin(char *buf, char sep, char *tojoin, size_t bufsize);

char
*xstrdup(char *s);

char
*getcfgdata(mdata *md, char *configid);

void
vfree(void *, ...);

char
**list2array(char *items, char *sep);

void
trimspace(char *buf);

void
freestringlist(char **str_array, size_t count);

int
instrlist(const char *find, char **list);

int
in_uch_array(const unsigned char, unsigned char *);

char
**memblocktoarray(mdata *md, int n);

void
memdel(mdata *md, const char *tofind);

void
stripcomment(mdata *md, const char *opn, const char *cls, int lop);

char
**loadconfigs(const char *prgname);

size_t
countchar(mdata *md, const char ch);

char
**findconfigs(mdata *md, char ** cfglist, size_t nrconfigs);

char
**mdatatostringlist(mdata *md);

mdata
*append_eol(mdata *md);

char
*getconfig(char **configs, char *cfgname);

char
*dictionary(char **list, char *key);

#endif
