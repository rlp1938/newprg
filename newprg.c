/*     newprg.c
 *
 * Copyright 2019 Robert L (Bob) Parker rlp1938@gmail.com
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
#include <time.h>


typedef struct progid { /* vars to use in Makefile.am etc */
  char *dir;    // directory name.
  char *exe;    // program name.
  char *src;    // source code name.
  char *thr;    // three letter abbreviation.
  char *mpt;    // man page title
  char *man;    // manpage name.
  char *author; // program author name.
  char *email;  // author email address.
} progid;

static void progidfree(progid *pi);

typedef struct oplist_t {  /* var to use when generating options */
  char  *shoptname;   // short options name.
  char  *longoptname; // long options name.
  char  *dataname;    // name of the opts variable.
  char   datapurpose; /* what it's for, one of 'afnsdq'
                         a - accumulator ie for --verbosity; int
                         f - flag, set 0 or 1; int
                         n - interger to be input, default 0; int
                         s - c string to be input, default NULL; char *
                         d - a double to be input, default 0.0; double
                         0 - nothing set; this is the help option
                      */
  int    optarg;      // 0,1 or 2.
} oplist_t;

static void optlist_tfree(oplist_t *ol);

typedef struct prgvar_t { /* carries all vars needed to generate the
                              new program output.
                          */
  char **optsout;   // list of all the options in the new program.
  progid *pi;       // names made from the input project name.
  char **libswlist; // source library software list.
  char **extras;    // extradist files, eg config data files etc.
  char *newdir;     // full path to the dir of the new program
  char *linksdir;   // full path to the dir of the lib s/w to link in.
  char *stubsdir;   // full path to the dir of the lib s/w to copy in.
  int newdirctl;    // 'n' don't delete, 'y' delete, 'q' ask.
} prgvar_t;

static void prgvar_tfree(prgvar_t *pv);

typedef struct newopt_t { /* the options to be processed in the new
                            program. */
  char *dataname;   // Should be legal C var name, but I don't care.
  char *datatype;   // int, double, char *
  char *helptext;   // generated text, eg 'a flag, default 0'
  char *action;     // C code eg '= 1;' for a flag type.
  char *shortname;  // single char;
  char *longname;   // the option long name.
  char optargrqd;   // '0','1','2' no optarg, optarg, optarg optional.
} newopt_t;

static void newopt_tfree(newopt_t *no);

#include "dirs.h"
#include "files.h"
#include "gopt.h"
#include "firstrun.h"
static char *version;
static void is_this_first_run(void);
static prgvar_t *prog_args(options_t *optp, char **argv);

static progid *makeprogname(const char *);
static void makepaths(prgvar_t *pv);
static void makelists(prgvar_t *pv, options_t *optp);
static char **csv2strs(char *longstr, char sep);
static char *swdepends(char *optslist);
static newopt_t **makenewoptionslist(prgvar_t *pv); // the array
static newopt_t *makenewoptions(char *optsdescriptor);  // the struct
static char *getoptsdataname(char *optsdescriptor);
static int get_types_index(char *optsdescriptor);
static char *getCdatatype(int idx);
static char *gethelptext(int idx);
static char *getCaction(int idx);
static char *getoptshortname(char *optsdescriptor);
static char *getoptslongname(char *optsdescriptor);
static char getoptargrequired(char *optsdescriptor);
static int validateoptsdescriptor(char *optsdescriptor);
static void printerr(char *msg, char *var, int fatal);
static void maketargetdir(prgvar_t *pv);
static void makemain(prgvar_t *pv);
static void placelibs(prgvar_t *pv);
static void generatemakefile(prgvar_t *pv);
static void addautotools(prgvar_t *pv);
static char *xgetcwd(char *buf, size_t size);
static void generateoptionsheader(prgvar_t *pv, newopt_t **nopl);
static void generateoptionsfunction(prgvar_t *pv, newopt_t **nopl);
static char *genshortoptstring(newopt_t **nopl);
static char *genlongoptstrings(newopt_t **nopl);
static char *gencaseselector(newopt_t **nopl);
static void generatemanpage(prgvar_t *pv, newopt_t **nopl);
static char *ucstr(const char *str);
static char *get_today(void);
static char *genoptionshelp(newopt_t **nopl);
static void makehelperscripts(prgvar_t *pv);
static void ulstr(int, char *);

int main(int argc, char **argv)
{  /* newprogram - write the initial files for a new C program. */
  version = "1.0";
  is_this_first_run(); // check first run
  // data gathering
  options_t opt = process_options(argc, argv); // options processing
  prgvar_t *pv = prog_args(&opt, argv);
  makepaths(pv);  // full paths to newprogram dir and source libs.
  makelists(pv, &opt);  // space separated long strings to char **
  /* pv->optsout has the new options in coded form (as user input)
   * nopl has them expanded, ie ready for use. */
  newopt_t **nopl = makenewoptionslist(pv);
  maketargetdir(pv);  // generate the target dir.
  placelibs(pv);  // software source lib code.
  makemain(pv); // make the C source file.
  generatemakefile(pv); // convert makefile to suit the new program.
  addautotools(pv); // Create GNU file requirment
  generateoptionsheader(pv, nopl);  // fills in the struct.
  generateoptionsfunction(pv, nopl);  // gopt.c in newdir.
  generatemanpage(pv, nopl);
  makehelperscripts(pv);
  progidfree(pv->pi);
  //prgvar_tfree(pv);
  return 0;
} // main()

void
is_this_first_run(void) 
{ /* test for first run and take action if it is */
  char *names[2] = { "newprg.cfg", NULL };
  if (!checkfirstrun("newprg", names)) {
    firstrun("newprg", names);
    fprintf(stderr,
          "Please edit newprg.cfg in %s/.config/newprg"
          " to meet your needs.\n",
          getenv("HOME"));
    exit(EXIT_SUCCESS);
  }
} // is_this_first_run()

prgvar_t
*prog_args(options_t *optp, char **argv)
{/* Uses the global options vars, optind etc*/
  if (!argv[optind]) {
    fputs("No project name provided.\n", stderr);
    exit(EXIT_FAILURE);
  }
  char *pname = argv[optind];
  optind++;
  if (argv[optind]) printerr("Extraneous input:", argv[optind], 1);
  prgvar_t *pv = xmalloc(sizeof(struct prgvar_t));
  memset(pv, 0, sizeof(struct prgvar_t));
  pv->pi = makeprogname(pname);  // variations on the project name.
  return pv;
} // prog_args()

progid
*makeprogname(const char *pname)
{  /* Create and fill in the progid struct with the values needed in
   * Makefile.am, ->exe = name, ->src = name.c, ->man = name.1,
   * and ->thr = nam .
  */ 
  char name[NAME_MAX], lcname[NAME_MAX];
  progid *prid = xmalloc(sizeof(progid));
  strcpy(name, pname);
  ulstr('l', name);
  strcpy(lcname, name);  // keep pristine lower case copy
  prid->exe = xstrdup(name);
  strcat(name, ".c");
  prid->src = xstrdup(name);  // source code
  strcpy(name, lcname);
  strcat(name, ".1");
  prid->man = xstrdup(name);  // manpage name
  strcpy(name, lcname);
  name[3] = 0;
  prid->thr = xstrdup(name);  // 3 letter abbreviation for Makefile.am
  strcpy(name, lcname);
  name[0] = toupper(name[0]);
  prid->dir = xstrdup(name);  // project dir name
  strcpy(name, lcname);
  ulstr('u', name);
  prid->mpt = xstrdup(name);  // manpage title
  char ubuf[PATH_MAX];  // get the users name and config path.
  strcpy(ubuf, getenv("USER"));
  strjoin(ubuf, '/', ".config/newprg/newprg.cfg", PATH_MAX);
  mdata *md = initconfigread(ubuf);
  prid->author = cfg_getparameter("newprg", "newprg.cfg", "author");
  prid->email = cfg_getparameter("newprg", "newprg.cfg", "email");
  return prid;
} // makeprogname()

void
makepaths(prgvar_t *pv)
{ /* make full paths to the new program dir, source library files to
  link into the new dir, and source lib files to be copied. */
  char buf[PATH_MAX];
  char *home = xstrdup(getenv("HOME"));
  // software components - to link
  char *progdir = cfg_getparameter("newprg", "prdata.cfg",
   "progdir");
  char *compdir = cfg_getparameter("newprg", "prdata.cfg",
   "compdir");
  strcpy(buf, home);
  strjoin(buf, '/', progdir, PATH_MAX);
  strjoin(buf, '/', compdir, PATH_MAX);
  pv->linksdir = xstrdup(buf);
  free(compdir);  // keep progdir for next
  // software components - to copy
  char *stubdir = cfg_getparameter("newprg", "prdata.cfg",
   "stubdir");
  strcpy(buf, home);
  strjoin(buf, '/', progdir, PATH_MAX);
  strjoin(buf, '/', stubdir, PATH_MAX);
  pv->stubsdir = xstrdup(buf);
  free(stubdir);  // keep progdir
  // path to new program dir
  strcpy(buf, home);
  strjoin(buf, '/', progdir, PATH_MAX);
  strjoin(buf, '/', pv->pi->dir, PATH_MAX);
  pv->newdir = xstrdup(buf);
  free(home);
} // makepaths()

void
makelists(prgvar_t *pv, options_t *optp)
{ /* with input C strings with elements separated by ' ', return
  * string lists as char **, NULL terminated. NULL inputs may be
  * possible and will cause NULL returns.
  */
  if (optp->extra_data) {
    pv->extras = csv2strs(optp->extra_data, ' ');
  } else pv->extras = NULL;
  if (optp->software_deps) {
    optp->software_deps = swdepends(optp->software_deps);
    // expands anything having fn.x+y as the extension into fn.x, fn.y
    pv->libswlist = csv2strs(optp->software_deps, ' ');
  } else pv->libswlist = NULL;
  if (optp->options_list) {
    pv->optsout = csv2strs(optp->options_list, ' ');
  } else pv->optsout = NULL;
  pv->newdirctl = optp->del_target;
} // makelists()

char **csv2strs(char *longstr, char sep)
{ /* break the string into shorter pieces separated by sep and return
  * a NULL terminated string array.
  */
  int count = 0;
  size_t themax = PATH_MAX;
  char buf[PATH_MAX];
  if (strlen(longstr) >= themax)
    printerr("String too long in csv2strs:\n", longstr, 1);
  char *cp = longstr; // Possible to have longstr starting with ' '
  if (*cp == ' ') {
    while (*cp == ' ') cp++;
  }
  strcpy(buf, cp);
  cp = buf;
  char *ep = cp + strlen(cp);
  while (cp < ep) {
    if (*cp == sep) {
      *cp = 0;
      count++;
    }
    cp++;
  }
  count++;  // the last chunk won't have 'sep' on the end
  char **ret = xmalloc((count + 1) * sizeof(char *));
  memset(ret, 0, (count + 1) * sizeof(char *));
  ret[count] = NULL;
  int i;
  cp = buf;
  for (i = 0; i < count; i++) {
    ret[i] = xstrdup(cp);
    int l = strlen(cp);
    cp += l + 1;
  }
  return ret;
} // csv2strs()

char
*swdepends(char *optslist)
{/* optslist may have software names in the form of name.h+c.
  * Any such names will be expanded to name.h and name.c
*/
  if (!optslist) return NULL;

  size_t olen = strlen(optslist);
  if(!olen) return NULL;
  char *ibuf = xmalloc(olen + 1);
  strcpy(ibuf, optslist);
  olen = strlen(ibuf);
  char *fnp = ibuf;
  char *guard = fnp + olen;
  
  size_t blen = 2*olen + 1;
  char *obuf = xmalloc(blen);  // enough for every item name.h+c
  fnp = ibuf;
  while (fnp < guard) {
    char *sp = strchr(fnp, ' ');
    if (sp) *sp = 0;
    fnp += strlen(fnp) + 1;
  }

  fnp = ibuf;
  obuf[0] = 0;
  while (fnp < guard) {
    char name[NAME_MAX];
    strcpy(name, fnp);
    int obl = strlen(obuf);
    char *plusp = strchr(name, '+');
    if (plusp) {  // good for single char xtns only, eg name.h, name.c
      *plusp = 0;
      if (obl) strjoin(obuf, ' ', name, blen);
      else strcpy(obuf, name);
      *(plusp - 1) = *(plusp + 1);
      strjoin(obuf, ' ', name, blen);
    } else {
      if (obl) strjoin(obuf, ' ', name, blen);
      else strcpy(obuf, name);
    }
    fnp += strlen(fnp) + 1;
  }
  free(optslist);  // this was strdup()'d
  char *ret = xstrdup(obuf);
  free(obuf);
  free(ibuf);
  return ret;
} // swdepends()

newopt_t
**makenewoptionslist(prgvar_t *pv)  // the array of structs
{ /* Using the array of chars describing each option as input, this
  returns any array of structs with all the expanded option data. */
  int count = 0;
  while (pv->optsout[count]) count++; // array is null terminated.
  newopt_t **nopl = xmalloc((count+1) * sizeof(newopt_t *));
  int i;
  for (i = 0; i < count; i++) {
    nopl[i] = makenewoptions(pv->optsout[i]);
  }
  return nopl;
}
 
newopt_t
*makenewoptions(char *optsdescriptor) // the struct
{ /* xextra:;Toptdataname afnsd */
  int hasoptsdataname = validateoptsdescriptor(optsdescriptor);
  newopt_t *nop = xmalloc(sizeof(newopt_t));
  int idx;
  if (hasoptsdataname) nop->dataname = getoptsdataname(optsdescriptor);
  if (hasoptsdataname) idx = get_types_index(optsdescriptor);
  if (hasoptsdataname) nop->datatype = getCdatatype(idx);
  if (hasoptsdataname) nop->helptext = gethelptext(idx);
  if (hasoptsdataname) nop->action = getCaction(idx);
  if (hasoptsdataname)
    nop->optargrqd = getoptargrequired(optsdescriptor);
  nop->shortname = getoptshortname(optsdescriptor);
  nop->longname = getoptslongname(optsdescriptor);
  
  return nop;
} // makenewoptions()

int
validateoptsdescriptor(char *optsdescriptor)
{ /* The optsdescriptor must contain or terminate with ';'
   * If there is nothing after ';' then some fields of newopt_t
   * will be left NULL/0 when setting up this struct.
  */
  char *cp = strchr(optsdescriptor, ';');
  if (!cp) printerr("Malformed options descriptor", optsdescriptor, 1);
  int ret;
  cp++; // what comes next
  if (*cp == 0) ret = 0; else ret = 1;
  return ret;
} // validateoptsdescriptor()

char
*getoptsdataname(char *optsdescriptor)
{ /* Extract the data name from the descriptor and return a pointer
     to it on the heap. */
  char *cp = strchr(optsdescriptor, ';');
  cp += 2;
  char *ret = xstrdup(cp);
  return ret;
} // getoptsdataname()

int
get_types_index(char *optsdescriptor)
{/* find what the type is in 'afnsd' */
  char *cp = strchr(optsdescriptor, ';');
  cp += 1;
  char t = *cp;
  char *begin = "afnsd";
  cp = strchr(begin, t);
  if (!cp) printerr("The option type must be in 'afnsd'",
                      optsdescriptor, 1);
  int ret = cp - begin;
  return ret;
} // get_types_index()

char
*getCdatatype(int idx)
{ /* returns pointer on heap to the indexed string. */
  char *dtypes[6] = {"int", "int", "int", "char *", "double", NULL};
  char *ret = xstrdup(dtypes[idx]);
  return ret;
} // getCdatatype()

char
*gethelptext(int idx)
{ /* returns pointer on heap to the indexed string. */
  char *ht[6] = {"increases by 1 every time it's invoked. "
    "Default is 0.",
     "is set to 1. Default is 0.", "the optarg is converted to a long."
     " Default is 0.", "is set to a copy of \\f[I]optarg\\f[]. "
     "Default is NULL.",
     "the \\f[I]optarg\\f[] is converted to a double. "
     "Default is 0.0.", NULL};
  char *ret = xstrdup(ht[idx]);
  return ret;
} // gethelptext()

char
*getCaction(int idx)
{ /* returns pointer on heap to the indexed string. */
  char *dactions[6] = {" += 1;", " = 1;",
                        " = strtol(optarg, NULL, 10);",
                        " = xstrdup(optarg);",
                        " = strtod(optarg, NULL);", NULL};
  char *ret = xstrdup(dactions[idx]);
  return ret;
} // getCaction()

char
*getoptshortname(char *optsdescriptor)
{ /* return the options short name char as a string. */
  char two[2];
  two[0] = optsdescriptor[0];
  two[1] = 0;
  char *ret = xstrdup(two);
  return ret;
} // getoptshortname()

char
*getoptslongname(char *optsdescriptor)
{ /* return the options long name as a string. */
  char buf[NAME_MAX];
  strcpy(buf, optsdescriptor+1);
  char *cp = strchr(buf, ';');
  *cp = 0;
  cp = strchr(buf, ':');  // check for opt arg marker;
  if (cp) *cp = 0;
  char *ret = xstrdup(buf);
  return ret;
} // getoptslongname()

char
getoptargrequired(char *optsdescriptor)
{
  char ret;
  char *cp = strchr(optsdescriptor, ':');
  if (!cp) {
    ret = '0';
  } else if (*(cp+1) == ':') {
    ret = '2';
  } else {
    ret = '1';
  }
  return ret;
} // getoptargrequired()

void
maketargetdir(prgvar_t *pv)
{ /* Conditionally make the target dir.
   * Non-existent, just make it.
   * Exists, if pv->newdirctl == 'n' - error
   *          if pv->newdirctl == 'y', delete and recreate it
   *          if pv->newdirctl == 'q', ask what to do
   */
  if (exists_dir(pv->newdir)) {
    int killit;
    char command[PATH_MAX];
    switch (pv->newdirctl) {
    case 'q':
      fputs("Dir exists, replace [Yn] ",stderr);
      killit = getchar();
      if (killit == 'y' || killit == 'Y') {
        sprintf(command, "rm -rf %s", pv->newdir);
        xsystem(command, 1);
        sync();
        newdir(pv->newdir, 0);
      } else {
        exit(EXIT_FAILURE);
      }
        break;
    case 'n':
      fprintf(stderr, "Target directory exists: %s\n", pv->newdir);
      exit(EXIT_FAILURE);         
        break;
    case 'y':
      sprintf(command, "rm -rf %s", pv->newdir);
      xsystem(command, 1);
      sync();
      newdir(pv->newdir, 0);
        break;
    }
  } else newdir(pv->newdir, 0);
/*
  char **optsout;   // list of all the options in the new program.
  progid *pi;       // names made from the input project name.
  char **libswlist; // source library software list.
  char **extras;    // extradist files, eg config data files etc.
  char *newdir;     // full path to the dir of the new program
  char *linksdir;   // full path to the dir of the lib s/w to link in.
  char *stubsdir;   // full path to the dir of the lib s/w to copy in.
  int newdirctl;    // 'n' don't delete, 'y' delete, 'q' ask.
  */
} // maketargetdir()

void
makemain(prgvar_t *pv)
{ /* open the main template and write the named C program using it. */
  mdata *md = readfile("./templates/main.c", 1, 1024);
  memreplace(md, "<prname>", pv->pi->exe, 1024);  // program name
  char buf[PATH_MAX];
  // TODO compute the year for copyright
  sprintf(buf, "Copyright 2018 %s %s", pv->pi->author, pv->pi->email);
  memreplace(md, "<copyright>", buf, 1024); // author, email
  sprintf(buf, "%s/%s", pv->newdir, pv->pi->src); // target C file.
  writefile(buf, md->fro, md->to, "w");
  free_mdata(md);
} // makemain()

void placelibs(prgvar_t *pv)
{ /* Link source libraries (linksdir), copy the same as needed
   * (stubsdir), copy gopt.? ./templates from ./, or print warning
   *  messages as needed.
  */
  int count = 0;
  while (pv->libswlist[count]) count++;
  int *ilist = xmalloc(count * sizeof(int));
  int i;
  for (i = 0; i < count; i++) ilist[i] = 1;
  char frbuf[PATH_MAX], tobuf[PATH_MAX];
  for (i = 0; i < count; i++) { // to be linked
    sprintf(frbuf, "%s/%s", pv->linksdir, pv->libswlist[i]);
    sprintf(tobuf, "%s/%s", pv->newdir, pv->libswlist[i]);
    int res = link(frbuf, tobuf); // may not succeed
    if (res == 0) ilist[i] = 0; // success.
  }
  for (i = 0; i < count; i++) { // to be copied
    if (ilist[i]) {
      sprintf(frbuf, "%s/%s", pv->stubsdir, pv->libswlist[i]);
      sprintf(tobuf, "%s/%s", pv->newdir, pv->libswlist[i]);
      if (exists_file(frbuf)) { // the file does not have to exist
        copyfile(frbuf, tobuf);
        ilist[i] = 0;
      }
    }
  } // for()
  for (i = 0; i < count; i++) { // gopt.c|h from templates dir
    if (ilist[i]) {
      sprintf(frbuf, "%s/%s", "./templates", pv->libswlist[i]);
      sprintf(tobuf, "%s/%s", pv->newdir, pv->libswlist[i]);
      if (exists_file(frbuf)) { // the file does not have to exist
        copyfile(frbuf, tobuf);
        ilist[i] = 0;
      }
    }
  } // for()
  for (i = 0; i < count; i++) { // dependencies for the makefile.
    if (ilist[i]) {
      fprintf(stderr, "File: %s does not exist.\n", pv->libswlist[i]);
    }
  } // for()
} // placelibs()

void
generatemakefile(prgvar_t *pv)
{ /* the makefile stub is to be copied into the new dir. */
  char mfname[PATH_MAX];
  sprintf(mfname, "%s/Makefile.am", pv->newdir);
  copyfile("./templates/Makefile.am", mfname);
  mdata *md = readfile(mfname, 1, 1024);
  memreplace(md, "progname", pv->pi->exe, 1024);
  char joinbuf[NAME_MAX];
  int i;
  strcpy(joinbuf, pv->libswlist[0]);
  for (i = 1; pv->libswlist[i]; i++) {  // NULL terminated list
    strjoin(joinbuf, ' ', pv->libswlist[i], NAME_MAX);
  }
  memreplace(md, "SWLIBS", joinbuf, 1024);
  memreplace(md, "TLA", pv->pi->thr, 1024);
  writefile(mfname, md->fro, md->to, "w");
  free_mdata(md);
} // generatemakefile()

void
addautotools(prgvar_t *pv)
{/* adds files needed by autotools, runs programs and amends files
  * as required. NB `automake --add-missing --copy` no longer makes
  * copies of some files required by a GNU standard build so I create
  * them here.
  */
  // Create files required.
  char textbuf[NAME_MAX];
  char pathbuf[PATH_MAX];
  strcpy(pathbuf, pv->newdir);
  size_t len = strlen(pathbuf);
  pathbuf[len] = '/';
  char *fn = pathbuf + (len + 1); // copy the filename to here.
  char *author = pv->pi->author;
  char *email = pv->pi->email;
  sprintf(textbuf, "README for %s", pv->pi->exe);
  sprintf(pathbuf, "%s/%s", pv->newdir, "README");
  str2file(pathbuf, textbuf, "w");

  sprintf(textbuf, "NOTES for %s", pv->pi->exe);
  sprintf(pathbuf, "%s/%s", pv->newdir, "NOTES");
  str2file(pathbuf, textbuf, "w");

  sprintf(textbuf, "ChangeLog for %s", pv->pi->exe);
  sprintf(pathbuf, "%s/%s", pv->newdir, "ChangeLog");
  str2file(pathbuf, textbuf, "w");

  sprintf(textbuf, "NEWS for %s", pv->pi->exe);
  sprintf(pathbuf, "%s/%s", pv->newdir, "NEWS");
  str2file(pathbuf, textbuf, "w");

  sprintf(textbuf, "Author for %s", pv->pi->exe);
  strjoin(textbuf, '\n', author, NAME_MAX);
  strjoin(textbuf, ' ', email, NAME_MAX);
  sprintf(pathbuf, "%s/%s", pv->newdir, "AUTHORS");
  str2file(pathbuf, textbuf, "w");

  strcpy(fn, "COPYING");
  char *copying = "/usr/share/automake-1.15/COPYING";
  if (exists_file(copying)) copyfile(copying, pathbuf);

  // run the autotools stuff
  char *at = xgetcwd(pathbuf, PATH_MAX);  // must change into new dir.
  xchdir(pv->newdir);
  xsystem("autoscan", 1);
  mdata *cfd = readfile("configure.scan", 1, 128);
  memreplace(cfd, "FULL-PACKAGE-NAME", pv->pi->exe, 128);
  memreplace(cfd, "VERSION", "1.0", 128);  // Hard wired? OK I think.
  memreplace(cfd, "BUG-REPORT-ADDRESS", email, 128);
  // Lazy way to stop infinite loop.
  memreplace(cfd, "AC_CONFIG_SRCDIR",
            "AM_INIT_AUTOMAKE\nac_config_srcdir", 128);
  // Original value of search target restored.
  memreplace(cfd, "ac_config_srcdir", "AC_CONFIG_SRCDIR", 128);
  writefile("configure.ac", cfd->fro, cfd->to, "w");
  xsystem("autoheader", 1);
  xsystem("aclocal", 1);
  xsystem("automake --add-missing --copy", 1);
  xsystem("autoconf", 1);
  xchdir(at);
} // addautotools()

void
generateoptionsheader(prgvar_t *pv, newopt_t **nopl)
{ /* replace '<struct>' in options_t with the list of opts variables */
  char outbuf[PATH_MAX] = {0};
  int i = 0;
  while ((nopl[i])) {
    if (nopl[i]->datatype) {  // options may exist with no data.
      char wrk[NAME_MAX];
      sprintf(wrk, "\t%s %s;\n", nopl[i]->datatype, nopl[i]->dataname);
      strjoin(outbuf, 0, wrk, PATH_MAX);
    }
    i++;
  }
  char *cp = strrchr(outbuf, '\n');
  *cp = 0;  // don't need the last '\n'
  mdata *md = readfile("./templates/gopt.h", 1, 1024);
  memreplace(md, "<struct>", outbuf, 1024);
  memreplace(md, "char * ", "char *", 1024);  // tidy up o/p format.
  char fnbuf[PATH_MAX];
  sprintf(fnbuf, "%s/%s", pv->newdir, "gopt.h");
  writefile(fnbuf, md->fro, md->to, "w");
  free_mdata(md);
} // generateoptionsheader()

static void generateoptionsfunction(prgvar_t *pv, newopt_t **nopl)
{ /* fills in the gaps in the stub file, ./templates/gopt.c */
  mdata *md = readfile("./templates/gopt.c", 1, 1024);
  memreplace(md, "progname", pv->pi->exe, 1024);  // manpage name.
  char *sostr = genshortoptstring(nopl);
  memreplace(md, "/* short options target */", sostr, 1024);
  free(sostr);  // short options string.
  char *lostr = genlongoptstrings(nopl);
  memreplace(md, "/* long options target */", lostr, 1024);
  free(lostr);  // long options string.
  char *casestr = gencaseselector(nopl);
  memreplace(md, "/* option proc target */", casestr, 1024);
  free(casestr);
  char fnbuf[PATH_MAX] = {0}; // write gopt.c in newdir.
  sprintf(fnbuf, "%s/%s", pv->newdir, "gopt.c");
  memreplace(md, "char * ", "char *", 1024);
  writefile(fnbuf, md->fro, md->to, "w");
  free_mdata(md);
} // generateoptionsfunction()

char
*genshortoptstring(newopt_t **nopl)
{
  char so[NAME_MAX] = {0};
  int i = 0;
  while ((nopl[i])) {
    if (nopl[i]->dataname) {  // ignore options w/o data.
      strcat(so, nopl[i]->shortname);
      switch(nopl[i]->optargrqd) {
        case '1':
          strcat(so, ":");
        break;
        case '2':
          strcat(so, "::");
        break;
      } // switch()
    } // if()
    i++;
  } // while()
  char *ret = strdup(so); // caller to free
  return ret;
} // genshortoptstring()

char
*genlongoptstrings(newopt_t **nopl)
{ /* build the additions to the long options array. */
  char str[NAME_MAX] = {0};
  char outbuf[PATH_MAX] = {0};
  int i = 0;
  while ((nopl[i])) {
    if (nopl[i]->dataname) {
      sprintf(str, "\t\t{\"%s\",\t%c,\t0,\t'%s'},\n",
      nopl[i]->longname, nopl[i]->optargrqd, nopl[i]->shortname);
      strcat(outbuf, str);
    }  // if()
    i++;
  } // while()
  char *cp = strrchr(outbuf, '\n');
  *cp = '\0'; // don't need the last line end.
  char *ret = xstrdup(outbuf);  // caller is to free this
  return ret;
} // genlongoptstrings()

char
*gencaseselector(newopt_t **nopl)
{ /* make up the case statements */
  char str[NAME_MAX] = {0};
  char outbuf[PATH_MAX] = {0};
  int i = 0;
  while ((nopl[i])) {
    if (nopl[i]->action) {
      sprintf(str, "\t\tcase '%s':\n\t\t\t%s %s%s\n\t\tbreak;\n",
      nopl[i]->shortname, nopl[i]->datatype, nopl[i]->dataname,
      nopl[i]->action);
      strcat(outbuf, str);
    }  // if()
    i++;
  } // while()
  char *cp = strrchr(outbuf, '\n');
  *cp = '\0'; // don't need the last line end.
  char *ret = xstrdup(outbuf);  // caller is to free this
  return ret;
} // gencaseselector()

void
generatemanpage(prgvar_t *pv, newopt_t **nopl)
{ /* man page in markdown format for pandoc to convert */
  mdata *md = readfile("./templates/manpage.1", 1, 1024);
  char *cp = ucstr(pv->pi->exe);
  memreplace(md, "PRGNAME", cp, 1024);
  free(cp);
  memreplace(md, "prgname", pv->pi->exe, 1024);
  memreplace(md, "yyyy-mm-dd", get_today(), 1024);
  cp = genoptionshelp(nopl);
  memreplace(md, "<next>", cp, 1024);
  char fnbuf[PATH_MAX] = {0}; // write prgname.md in newdir.
  sprintf(fnbuf, "%s/%s", pv->newdir, pv->pi->man);
  //memreplace(md, "char * ", "char *", 1024);
  writefile(fnbuf, md->fro, md->to, "w");
  free_mdata(md);
} // generatemanpage()

char
*genoptionshelp(newopt_t **nopl)
{ /* make a string of options help for markdown */
  char str[NAME_MAX] = {0};
  char outbuf[PATH_MAX] = {0};
  int i = 0;
  while ((nopl[i])) {
    if (nopl[i]->action) {
      sprintf(str, ".TP\n.B -%s, --%s\n%s %s FIXME\n",
      nopl[i]->shortname, nopl[i]->longname, nopl[i]->dataname,
      nopl[i]->helptext);
      strcat(outbuf, str);
    }  // if()
    i++;
  } // while()
  char *cp = strrchr(outbuf, '\n');
  *cp = '\0'; // don't need the last line end.
  char *ret = xstrdup(outbuf);  // caller is to free this
  return ret;

} // genoptionshelp()

char *ucstr(const char *str)
{ /* return uppercase of str */
  char buf[PATH_MAX] = {0};
  char *cp = buf;
  strcpy(buf, str);
  while ((*cp)) {
    *cp = toupper(*cp);
    cp++;
  }
  char *ret = xstrdup(buf);
  return ret; // caller must free.
} // ucstr()

void
makehelperscripts(prgvar_t *pv)
{ /* The new programs dir needs a sub dir ./bin to have helper scripts*/
  char fro[PATH_MAX] = {0};
  char to[PATH_MAX] = {0};
  xgetcwd(fro, PATH_MAX);
  strjoin(fro, '/', "templates/", PATH_MAX);
  char *frp = fro + strlen(fro);
  strcpy(to, pv->newdir);
  strjoin(to, '/', "bin/", PATH_MAX);
  char *top = to + strlen(to);
  newdir(to, 1); // bin sub dir in target dir
  strcpy(top, "findfixme"); // findfixme, a script
  strcpy(frp, "findfixme");
  mdata *md = readfile(fro, 0, 128);
  char *cmnt = "# this is a template, prgname to be replaced with a "
  "target program name.";
  memreplace(md, cmnt, " ", 128);  // zap the comment
  memreplace(md, "prgname", pv->pi->exe, 128);
  writefile(to, md->fro, md->to, "w");
  if (chmod(to, 0775) == -1) {
    perror(to);
    exit(EXIT_FAILURE);
  }
  
  // workaround auto tools bugs
} // makehelperscripts()

void
ulstr(int ul, char *b)
{  /* convert b to upper or lower case depending on value of ul */
  size_t i;
  switch (ul)
  {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-sign"
    case 'u':
      for (i = 0; i < strlen(b); i++) {
        b[i] = toupper(b[i]);
      }
      break;
    case 'l':
      for (i = 0; i < strlen(b); i++) {
        b[i] = tolower(b[i]);
      }
      break;
#pragma GCC diagnostic pop
    default:
      b[0] = 0;  // trash the input string if ul is rubbish.
      break;
  }
} // ulstr()

/* destructors */
void
progidfree(progid *pi)
{ /* allow that any object to free may be NULL */
  if (!pi) return;
  if (pi->dir)    free (pi->dir);
  if (pi->exe)    free (pi->exe);
  if (pi->src)    free (pi->src);
  if (pi->thr)    free (pi->thr);
  if (pi->man)    free (pi->man);
  if (pi->author) free (pi->author);
  if (pi->email)  free (pi->email);
  free(pi);
} // progidfree()

void
optlist_tfree(oplist_t *ol)
{ /* allow that any object to free may be NULL */
  if (!ol) return;
  if (ol->shoptname)    free(ol->shoptname);
  if (ol->longoptname)  free(ol->longoptname);
  if (ol->dataname)     free(ol->dataname);
  free(ol);
} // optlist_tfree()

void
prgvar_tfree(prgvar_t *pv)
{ /* allow that any object to free may be NULL */
  if (!pv) return;
  if (pv->optsout)    destroystrarray(pv->optsout, 0);
  if (pv->pi)         progidfree(pv->pi);
  if (pv->libswlist)  destroystrarray(pv->libswlist, 0);
  if (pv->extras)     destroystrarray(pv->extras, 0);
  if (pv->newdir)     free(pv->newdir);
  if (pv->linksdir)   free(pv->linksdir);
  if (pv->stubsdir)   free(pv->stubsdir);
  free(pv);
}  // prgvar_tfree()

void
newopt_tfree(newopt_t *no)
{ /* allow that any object to free may be NULL */
  if (!no) return;
  if (no->action)     free(no->action);
  if (no->dataname)   free(no->dataname);
  if (no->datatype)   free(no->datatype);
  if (no->helptext)   free(no->helptext);
  if (no->longname)   free(no->longname);
  if (no->shortname)  free(no->shortname);
  free(no);
} // newopt_tfree()

void
printerr(char *msg, char *var, int fatal)
{ /* many errors will be suitable for this format. */
  fprintf(stderr, "%s: %s\n", msg, var);
  if (fatal) exit(EXIT_FAILURE);
} // printerr()

char
*xgetcwd(char *buf, size_t size)
{ /* getcwd with error handling */
  char *res = getcwd(buf, size);
  if (!res) {
    perror("getcwd()");
    exit(EXIT_FAILURE);
  }
  return res;
} // xgetcwd()

char
*get_today(void)
{ /* return result as yyyy-mm-dd (iso 8601) */
  time_t now = time(NULL);
  struct tm *lt;
  lt = localtime(&now);
  static char buf[NAME_MAX] = {0};
  sprintf(buf, "%d-%.2d-%.2d", lt->tm_year + 1900, lt->tm_mon + 1,
          lt->tm_mday);
  return buf;
} // get_today()

