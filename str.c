/*    str.c
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

#include "str.h"
#include "files.h"

size_t
countmemstr(mdata *md)
{ /* In memory block specified by md, count the number of C strings. */
	char *cp = md->fro;
	size_t c = 0;
	while (cp < md->to) {
		if (*cp == 0) c++;
		cp++;
	}
	return c;
} // countmemlines()

char
*mktmpfn(char *prname, char *extrafn, char *thename)
{/* Make a temporary file name.
  * The name will be the concatenation of: "/tmp/", prname, $USER, pid
  * and extrafn. Extrafn may be NULL if the application needs only one
  * filename. If thename is NULL a buffer of PATH_MAX will be created
  * and the returned result must be freed after use, otherwise thename
  * should be a buffer of PATH_MAX size allocated by the caller.
*/
	char *buf;
	if (thename) {
		buf = thename;
	} else {
		buf = xmalloc(PATH_MAX);
	}
	size_t len = 5/*"/tmp/"*/ + strlen(prname) + strlen(getenv("USER"))
				+ 5 /*"32767" (pid)*/ + strlen(extrafn);
	if (len >= PATH_MAX) {
		fprintf(stderr, "Name too long: %lu\n", len);
		exit(EXIT_FAILURE);
	}
	sprintf(buf, "/tmp/%s%s%d%s.lst", prname, getenv("USER"), getpid(),
				extrafn);
	return buf;
} // mktmpfn()

mdata
*init_mdata(void)
{
	mdata *md = xmalloc(sizeof(mdata));
	md->fro = md->to = md->limit = (char *)NULL;
	return md;
} // init_mdata()

void
meminsert(const char *line, mdata *dd, size_t meminc)
{	/* insert line into the data block described by dd, taking care of
	 * necessary memory reallocation as needed.
	*/
  const size_t fence = 8;
	size_t len = strlen(line);
	size_t safelen = len + fence;
	if (safelen > (unsigned)(dd->limit - dd->to)) { // >= 0 always
		/* Ensure that line always has room to fit. */
		size_t needed = (meminc > safelen) ? meminc : safelen;
		memresize(dd, needed);
	}
	strcpy(dd->to, line);
	dd->to += len+1;
} // meminsert()

/* There's a bug in memreplace(). It manifests as dofopen() segfaulting
 * when run immediately after a run of memreplace(). dofopen() should
 * never segfault, it can and should abort when there is some problem
 * with the file it's trying to open. So I am clobbering some memory, I
 * suppose with some kind of write beyond my malloc'd space.
 *
 * The problem happens when available space is == space needed or
 * available space is 1 more than space needed.
 * If available space is 1 less than needed, a resize is forced and all
 * is well. If available space is 2 more than needed it works as well.
 *
 * If I am off by 1 somewhere I can't see where. Is memmove() doing
 * something it shouldn't oughta?
 *
 * Workaround - I'll put an 8 byte fudge fence into the thing.
 * */

int
printstrlist(char **list)
{
	int i;
	for (i = 0; list[i]; i++) {
		printf("%s\n", list[i]);
	}
	return i;
} // printstrlist()

void
memreplace(mdata *md, char *find, char *repl, off_t meminc)
{/* Replace find with repl for every occurrence in the data block md. */
	size_t flen = strlen(find);
	size_t rlen = strlen(repl);
	off_t ldiff = rlen - flen;
	const off_t fudge_fence = 8;	// see bug commentary above.
	char *fp = md->fro;
	fp = memmem(fp, md->to - fp, find, flen);
	while (fp) {
		off_t avail = md->limit - md->to;
		if (ldiff + fudge_fence > avail) {
			off_t fpoffset = fp - md->fro;	// Handle relocation.
			// Ensure that the resized memory has space to fit new data.
			off_t actualinc = (meminc > ldiff)
								? meminc : ldiff + fudge_fence;
			/* Ensure that we never have available space within 0 or 1
			 * bytes of the needed space. */
			memresize(md, actualinc);
			fp = md->fro + fpoffset;
		}
		char *movefro = fp + flen;
		char *moveto = fp + rlen;
		size_t mlen = md->to - fp;
		memmove(moveto, movefro, mlen);
		memcpy(fp, repl, rlen);
		md->to += ldiff;
		fp = memmem(fp, md->to - fp, find, flen);
	} // while(fp)
} // memreplace()

void
memresize(mdata *dd, off_t change)
{	/* Alter the the size of a malloc'd memory block.
	 * Takes care of any relocation of the original pointer.
	 * Writes 0 over the new space when increasing size.
	 * Handles memory allocation failure.
	*/
	size_t now = dd->limit - dd->fro;
	size_t dlen = dd->to - dd->fro;
	size_t newsize = now + change;	// change can be negative
	dd->fro = realloc(dd->fro, newsize);
	if (!dd->fro) {
		fputs("Out of memory\n", stderr);
		exit(EXIT_FAILURE);
	}
	dd->limit = dd->fro + newsize;
	dd->to = dd->fro + dlen;
	if (dd->limit > dd->to) {	// will be the case after an increase.
		memset(dd->to, 0, dd->limit - dd->to);
	}
} // memresize()

int
memlinestostr(mdata *md)
{ /* In the block of memory enumerated by md, replace all '\n' with
   * '\0' and return the number of replacements done.
  */
	size_t lcount = 0;
	char *cp = md->fro;
	while (cp < md->to) {
		if (*cp == '\n') {
			*cp = 0;
			lcount++;
		}
		cp++;
	}
	return lcount;
} // memlinestostr()

size_t
memstrtolines(mdata *md)
{ /* In the block of memory enumerated by md, replace all '\0' with
   * '\n' and return the number of replacements done.
  */
	size_t scount = 0;
	char *cp = md->fro;
	while (cp < md->to) {
		if (*cp == 0) {
			*cp = '\n';
			scount++;
		}
		cp++;
	}
	return scount;
} // memstrtolines()

void
strjoin(char *left, char sep, char *right, size_t max)
{/*	Join right onto left, ensuring that sep is between left and right.
	* Left is a buffer of length max bytes. However strlen(left) may be
	* 0 and also sep may be '\0' and if it is this will have the effect
	* of a simple strcat().
	* If the total length of the join would exceed max, quit with
	* an error message.
*/
	size_t llen = strlen(left);
	if (!right) return;
	size_t rlen = strlen(right);
  if (rlen == 0) return;
  if (sep == 0) {
    fputs("Seperator may not be 0", stderr);
    exit(EXIT_FAILURE);
  }
	size_t tlen = llen + rlen + 1;
	if ( tlen >= max) {
		fprintf(stderr, "String length %lu, to big for buffer %lu\n",
				tlen, max);
		exit(EXIT_FAILURE);
	}
  if (llen == 0) {
    strcpy(left, right);
    return;
  }
  char sepstr[2];
  sepstr[1] = 0;
  sepstr[0] = sep;
  strcat(left, sepstr);
  strcat(left, right);
} // strjoin()

char
*xstrdup(char *s)
{	/* strdup() with error handling */
	char *p = strdup(s);
	if (!p) {
		fputs("Out of memory.\n", stderr);
		exit(EXIT_FAILURE);
	}
	return p;
} // xstrdup()

void
*xmalloc(size_t s)
{	// malloc with error handling
	void *p = malloc(s);
	if (!p) {	// Better forget perror in this circumstance.
		fputs("Out of memory.\n", stderr);
		exit(EXIT_FAILURE);
	}
  memset(p, 0, s);  // init memory block
	return p;
} // xmalloc()

char
*getcfgdata(mdata *cfdat, char *cfgid)
{/* Read selected line */
	static char buf[NAME_MAX];
	buf[0] = 0;
	char *cp = cfdat->fro;
	size_t slen = strlen(cfgid);
	cp = memmem(cp, cfdat->to - cp, cfgid, slen);
	if (!cp) {
		fprintf(stderr, "No such parameter in config: %s\n", cfgid);
		exit(EXIT_FAILURE);
	}
	cp = memchr(cp, '=', cfdat->to - cp);
	if (!cp) {
		fprintf(stderr, "Malformed line in config: %s\n", cfgid);
		exit(EXIT_FAILURE);
	}
	cp++;	// step past '='
	char *ep = memchr(cp, '\n', cfdat->to - cp);
	if (!cp) {
		fprintf(stderr, "WTF??? no line feed in config: %s\n", cfgid);
		exit(EXIT_FAILURE);
	}
	size_t dlen = ep - cp;
	strncpy(buf, cp, dlen);
	buf[dlen] = 0;
	return buf;
} // getgfgdata()

void
free_mdata(mdata *md)
{/* Free the data pointed to by md, then free md itself. */
	free(md->fro);
	free(md);
} // freemdata()

void
vfree(void *p, ...)
{/* free as many as are listed, terminate at NULL */
	va_list ap;
	va_start(ap, p);
	free(p);
	void *vp;
	while (1) {
		vp = va_arg(ap, void *);
		if (!vp) break;	// terminating NULL
		free(vp);
	}
	va_end(ap);
}

char
**list2array(char *items, char *sep)
{ /* Operates on a list of items, separated by sep, and returns a NULL
   * terminated array of strings.
  */
  size_t il = strlen(items);
  size_t sl = strlen(sep);
	il++;
  char *buf = xmalloc(il * sizeof(char*));
	strcpy(buf, items);
  char *line = buf;
  size_t lcount = 0;
  while (1) {
    char *sep_p = strstr(line, sep);
    if (!sep_p) break;
    lcount++;
    line = sep_p + sl;
  }
  lcount += 2;  // count the last item + NULL pointer at end.
  line = buf;
  char **res = xmalloc(lcount * sizeof(char*));
  size_t i;
  for (i = 0; i < lcount; i++) {
    char *sep_p = strstr(line, sep);
    if (sep_p) *sep_p = 0;
    res[i] = xstrdup(line);
    if (sep_p) line = sep_p + sl;
  }
  lcount--;
  res[lcount] = 0;
  free(buf);
	return res;
} // list2array()

void
trimspace(char *buf)
{/* Lops any isspace(char) off the front and back of buf. */
	size_t buflen = strlen(buf);
	if (buflen > PATH_MAX) {
		fputs("Input string too long, quitting!\n", stderr);
		exit(EXIT_FAILURE);
	}
	char work[PATH_MAX];
	char *begin = buf;
	char *end = begin + buflen;
	while (isspace(*begin) && begin < end) begin++;
	strcpy(work, begin);
	size_t worklen = strlen(work);
	begin = work;
	end = begin + worklen;
	while (isspace(*end) && end > begin) end--;
	strcpy(buf, work);
} // trimws()

void
freestringlist(char **wordlist, size_t count)
{/* Frees list of strings. If count is non-zero, frees count strings,
  * otherwise it assumes wordlist has a NULL terminator.
*/
	size_t i = 0;
	if (count) {
		for (i = 0; i < count; i++) free(wordlist[count]);
	} else {
		while (wordlist[i]) {
			free(wordlist[i]);
			i++;
		}
	}
	free(wordlist);
} // freestringlist()

int
instrlist(const char *name, char **list)
{	/* return 1 if name is found in list, 0 otherwise. */
	if (!list) return 0;	// list may not have been made.
	int i = 0;
	while (list[i]) {
		if(strcmp(name, list[i]) == 0) return 1;
		i++;
	}
	return 0;
} // instrlist()

int
in_uch_array(const unsigned char c, unsigned char *buf)
{ /* Return 1 if c is in buf, 0 otherwise. Buf must be 0 terminated. */
	unsigned char *cp = buf;
	while (*cp != 0) {
		if(*cp == c) return 1;
		cp++;
	}
	return 0;
} // in_uch_array()

char
**memblocktoarray(mdata *md, int n)
{/* returns an array of char *, NULL terminated */
	char *cp = md->fro;
	char **res = xmalloc((n+1) * sizeof(char *));
	int i;
	for (i = 0; i < n; i++) {
		res[i] = cp;
		cp += strlen(cp) + 1;
	}
	res[n] = (char *)NULL;
	return res;
}

void
memdel(mdata *md, const char *tofind)
{ /* if the string tofind exists delete the first instance, else do
   * nothing.
  */
  size_t len = strlen(tofind);
  if (!(len)) return;
  char *cp = memmem(md->fro, md->to - md->fro, tofind, len);
  if (cp) {
    char *end = cp + len;
    memmove(cp, end, md->to - end);
    md->to -= len;
  }
} // memdel()

void
stripcomment(mdata *md, const char *opn, const char *cls, int lopcls)
{ /* Remove all comments from the data space identified by md.
   * The strings to be removed are identified by opn at the start and
   * cls at the end. If lopcls is non-zero delete the closing string
   * also, otherwise don't.
   * EG stripcomment(md "#", "\n", 0), stripcomment(md, "//", "\n", 0)
   * stripcomment(md, "!<--", "-->", 1, removal of C style comments
   * is similar.)
  */
  for (;;) {
    size_t stlen = strlen(opn);
    if (!(stlen)) return;
    size_t enlen = strlen(cls);
    if (!(enlen)) return;
    char cmntbuf[PATH_MAX];
    char *st = memmem(md->fro, md->to - md->fro, opn, stlen);
    if (!st) break;
    char *en = memmem(st, md->to - st, cls, enlen);
    if (!en) break; // Any editors that don't put '\n' at end of file?
    if (lopcls) en += enlen;
    size_t cplen = en - st;
    if (cplen >= PATH_MAX) {
      fputs("Comment length is too big for buffer, quitting.\n",
            stderr);
      exit(EXIT_FAILURE);
    }
    strncpy(cmntbuf, st, cplen);
    cmntbuf[cplen] = 0;
    memdel(md, cmntbuf);
  }
} // stripcomment()

char
**loadconfigs(const char *prgname)
{ /* produces a NULL terminated list of strings of the form
   * name1=param1, ... ,nameN=paramN, (char *)NULL
  */
  char buf[PATH_MAX]; // path to config file.
  sprintf(buf, "%s/.config/%s/%s.cfg", getenv("HOME"),prgname, prgname);
  mdata *md = initconfigread(buf);
  size_t prmcount = countchar(md, '=');
  char **cfgs = xmalloc((prmcount+1) * sizeof(char *));
  cfgs = findconfigs(md, cfgs, prmcount);
  return cfgs;
} // loadconfigs()

size_t
countchar(mdata *md, const char ch)
{ /* count given char, useful rarely. */
  size_t count = 0;
  char *loc = memchr(md->fro, ch, md->to - md->fro);
  while ((loc = memchr(loc, ch, md->to - loc))) {
    count++;
    loc++;
  }
  return count;
} // countchar()

char
**findconfigs(mdata *md, char **cfglist, size_t nrconfigs)
{ /* duplicate the config strings onto the cfglist items */
  char *eol = md->fro;
  size_t i;
  for (i = 0; i < nrconfigs; i++) {
    char *eq = memchr(eol, '=', md->to - eol);  // next config line
    char *bol = eq;
    while (*bol != '\n') bol--;
    bol++;
    eol = eq; while (*eol != '\n') eol++;
    char buf[NAME_MAX];
    size_t len = eol - bol;
    strncpy(buf, bol, len);
    buf[len] = 0;
    cfglist[i] = xstrdup(buf);
  }
  return cfglist;
} // findconfigs()

char
**mdatatostringlist(mdata *md)
{ /* mdata is always a block of lines from a text file.
   * This returns an array of C strings, NULL terminated.
   * Leading and trailing spaces are removed.
   * Zero length strings are skipped.
  */
  char *eod = md->to - 1; // check file is terminated with '\n'.
  if (*eod != '\n') {
    append_eol(md);
  }
  size_t linecount = countchar(md, '\n'); // size of output array
  char **retval = xmalloc((linecount+1) * sizeof(char*));
  size_t idx = 0;
  char *line = md->fro;
  while (line < md->to) {
    char *eol = memchr(line, '\n', md->to - line);
    *eol = 0;
    char *begin = line;
      while(isspace(*begin) && begin < eol) { // leading space
        *begin = 0;
        begin++;
      } // while()
        char *end = eol - 1;
      while((isspace(*end)) && (end > begin)) {  // trailing space
        *end = 0;
        end--;
      } // while()
    size_t ll = strlen(begin); // ll may be 0
    if (ll) {
      retval[idx] = xstrdup(begin);
      idx++;
    }
    line = eol + 1;
  } // while()
  retval[idx] = 0;
  return retval;  // some wasted char* likely due 0 length lines.
} // mdatatostringlist()

mdata
*append_eol(mdata *md)
{ /* If the last char in a text file is not '\n' append one. */
  char *eod = md->to - 1;
  if (*eod == '\n') return md;
  if (md->limit > md->to) {
    eod++;
    *eod = '\n';
    md->to++;
  } else {
    memresize(md, 8);
    eod++;
    *eod = '\n';
    md->to++;
  }
  return md;
} // append_eol()

char
*getconfig(char **configs, char *cfgname)
{ /* configs is an array of strings of form key=data, NULL terminated.
    * returns data if found, other wise NULL.
  */
  static char buf[PATH_MAX];
  size_t len = strlen(cfgname);
  size_t i;
  for (i = 0; configs[i]; i++) {
    if (strncmp(configs[i], cfgname, len) == 0) {
      char *eq = strchr(configs[i], '=');
      if (eq) {
        strcpy(buf, eq+1);
        return buf;
      } else {
        fprintf(stderr, "Badly formed config item: %s\n", configs[i]);
        exit(EXIT_FAILURE);
      }
    } // if(strncmp ...)
  } // for (i = 0 ...)
  return (char *)NULL;
} // getconfig()

char
*dictionary(char **list, char *key)
{ /* List must be NULL terminated. Returns NULL if key not found. */
  size_t i;
  char *ret = NULL;
  static char buf[PATH_MAX];
  for (i = 0; list[i]; i++) {
    strcpy(buf, list[i]);
    char *eq = strchr(buf, '=');
    *eq = 0;
    if (strcmp(key, buf) == 0) {
      ret = eq + 1;
      break;
    } // if()
  } // for()
  return ret;
} // dictionary()
