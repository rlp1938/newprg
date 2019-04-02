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
  char *templates;  // full path to the dir of the templates files.
  /* The search order for named dependency files is, linksdir,
   * stubsdir, then templates.
   * */
} prgvar_t;

static void prgvar_tfree(prgvar_t *pv);

typedef struct newopt_t { /* the options to be processed in the new
                            program. */
  char *shortopt; // short option char with 0-2 ':' appended.
  char *longopt;  // long option name.
  char *varname;  // The C variable name, purpose decides the type.
  char *purpose;  /* Technical purpose of the option:
                    * flag, C int. No optarg.
                    * acc, accumulator, C int. No optarg. May set max.
                    * int, C int, Must have optarg. Conversion done.
                    * float, C double. Converts optarg.
                    * string, C char*.
                    * block, a file path.
                  */
  char *dflt_val;  // default value. May be 0 length, if so,
                  // "0", "0.0", or (char*) NULL will be used.
  char *max_val;  // If empty it will be ignored.
  char *help_txt; /* If empty, "FIXME" will be substituted.
                   * If the text needs to be long use a short headline
                   * with "FIXME" appended.
                  */ 
  char *runfunc;  // EG dohelp(0), runvsn(). "FIXME" or empty usually.
} newopt_t;

static void newopt_tfree(newopt_t *no);

#include "dirs.h"
#include "files.h"
#include "gopt.h"
#include "firstrun.h"
static char *version;
static void dohelp(int forced);
static void is_this_first_run(void);
static prgvar_t *action_options(options_t *optp);
static char **getlibsoftwarenames(const char *path, char *nameslist);
static char *expand_extensions(const char *list);
static char *read_defaults(const char *path);
static prgvar_t *prog_args(prgvar_t *pv, options_t *optp, char **argv);
static char **getextras(const char *path, char *nameslist);
static char **getoptionslist(const char *path, char *nameslist);
static prgvar_t *makepaths(char **configs, prgvar_t *pv);
static progid *makeprogname(const char *);
static void maketargetdir(prgvar_t *pv);
static newopt_t **makenewoptionslist(prgvar_t *pv); // the array
static newopt_t *makenewoption(char *optsdescriptor);  // the struct
static int validateoptsdescriptor(char *optsdescriptor);
static int validshortname(char *buf);
static int validlongname(char *buf);
static int validpurpose(char *buf);
static char *getdflt(char *purpose);
static void maketargetoptions(prgvar_t *pv, newopt_t **nopl);
static void maketoheader(prgvar_t *pv, newopt_t **nopl);
static void maketoCfile(prgvar_t *pv, newopt_t **nopl);
static mdata *gettargetfile(prgvar_t *pv, const char *fn);
static void setfileownertext(prgvar_t *pv, mdata *md);
static char *settargetfilename(prgvar_t *pv, const char *fn);
static char *purposetoCtype(const char *purpose);
static char *buildoptstring(newopt_t **nopl);
static char *builddefaults(newopt_t **nopl);
static char *buildlongopts(newopt_t **nopl);
static char *buildcases(newopt_t **nopl);
static char *getoptrval(const char *purpose);
static void placelibs(prgvar_t *pv);
static void makemain(prgvar_t *pv);




static int get_types_index(char *optsdescriptor);
static char *getCdatatype(int idx);
static char *gethelptext(int idx);
static char *getCaction(int idx);
static char *getoptshortname(char *optsdescriptor);
static char *getoptslongname(char *optsdescriptor);
static char getoptargrequired(char *optsdescriptor);
static void printerr(char *msg, char *var, int fatal);
static void generatemakefile(prgvar_t *pv);
static void addautotools(prgvar_t *pv);


static char *ucstr(const char *str);
static char *get_today(void);
static void makehelperscripts(prgvar_t *pv);
static void ulstr(int, char *);

int main(int argc, char **argv)
{  /* newprogram - write the initial files for a new C program. */
  version = "1.0";
  is_this_first_run(); // check first run
  // data gathering
  options_t opt = process_options(argc, argv);
  prgvar_t *pv = action_options(&opt);
  pv = prog_args(pv, &opt, argv);
  char **configs = loadconfigs("newprg");
  pv = makepaths(configs, pv);
  maketargetdir(pv);  // generate the target dir.
  newopt_t **nopl = makenewoptionslist(pv);
  placelibs(pv);  // software source library code.
  maketargetoptions(pv, nopl);
  makemain(pv); // make the C source file.

  exit(0);


  
  //makelists(pv, &opt);  // space separated long strings to char **
  /* pv->optsout has the new options in coded form (as user input)
   * nopl has them expanded, ie ready for use. */
  
  generatemakefile(pv); // convert makefile to suit the new program.
  addautotools(pv); // Create GNU file requirment

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
*prog_args(prgvar_t *pv, options_t *optp, char **argv)
{/* Uses the global options vars, optind etc*/
  if (!argv[optind]) {
    fputs("No project name provided.\n", stderr);
    exit(EXIT_FAILURE);
  }
  char *pname = argv[optind];
  optind++;
  if (argv[optind]) printerr("Extraneous input:", argv[optind], 1);
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
  strcpy(ubuf, getenv("HOME"));
  strjoin(ubuf, '/', ".config/newprg/newprg.cfg", PATH_MAX);
  mdata *md = initconfigread(ubuf);
  prid->author = cfg_getparameter("newprg", "newprg.cfg", "author");
  prid->email = cfg_getparameter("newprg", "newprg.cfg", "email");
  return prid;
} // makeprogname()

prgvar_t
*makepaths(char **configs, prgvar_t *pv)
{ /* make full paths to the new program dir, source library files to
  link into the new dir, and source lib files to be copied. */
  char buf[PATH_MAX];
  char *home = getenv("HOME");
  strcpy(buf, home);
  char *cfg = getconfig(configs, "progdir");
  if (cfg) {
    strcpy(buf, home);
    strjoin(buf, '/', cfg, PATH_MAX);
  } else {
    fputs("Couldn't find document root", stderr);
    exit(EXIT_FAILURE);
  }
  char *docroot = xstrdup(buf);
  strcpy(buf, docroot); // the new programs dir
  strjoin(buf, '/', pv->pi->dir, PATH_MAX);
  pv->newdir = xstrdup(buf);
  strcpy(buf, docroot); // path to linked in s/w libs.
  cfg = getconfig(configs, "compdir");
  strjoin(buf, '/', cfg, PATH_MAX);
  pv->linksdir = xstrdup(buf);
  strcpy(buf, docroot); // path to copied s/w libs.
  cfg = getconfig(configs, "stubdir");
  strjoin(buf, '/', cfg, PATH_MAX);
  pv->stubsdir = xstrdup(buf);
  strcpy(buf, docroot); // path to template files.
  cfg = getconfig(configs, "templates");
  strjoin(buf, '/', cfg, PATH_MAX);
  pv->templates = xstrdup(buf);
  free(docroot);
  return pv;
} // makepaths()

void
maketargetdir(prgvar_t *pv)
{ /* Make the target dir. Kill it if pre-existing. */
  if (exists_dir(pv->newdir)) {
    char command[PATH_MAX];
    sprintf(command, "rm -rf %s", pv->newdir);
    xsystem(command, 1);
    sync();
  }
  newdir(pv->newdir, 0);
} // maketargetdir()

newopt_t
**makenewoptionslist(prgvar_t *pv)  // the array of structs
{ /* Using the array of chars describing each option as input, this
  returns any array of structs with all the expanded option data. */
  int count = 0;
  while (pv->optsout[count]) count++; // array is null terminated.
  newopt_t **nopl = xmalloc((count+1) * sizeof(newopt_t *));
  int i;
  for (i = 0; i < count; i++) {
    nopl[i] = makenewoption(pv->optsout[i]);
  }
  return nopl;
} // makenewoptionslist()
 
newopt_t
*makenewoption(char *optsdescriptor) // the struct
{ /* */
  validateoptsdescriptor(optsdescriptor); // exits on error.
  newopt_t *nop = xmalloc(sizeof(newopt_t));
  char buf[PATH_MAX];
  char objbuf[NAME_MAX];
  strcpy(buf, optsdescriptor);  // protect the source.
  char *obj = buf;  // field # 1, short option name.
  char *comma = obj;
  while (*comma != ',') comma++;
  *comma = 0;
  strcpy(objbuf, obj);
  if (validshortname(objbuf)) nop->shortopt = xstrdup(objbuf);
  obj = comma + 1;  // field # 2, long option name.
  while (*comma != ',') comma++;
  *comma = 0;
  strcpy(objbuf, obj);
  if (validlongname(objbuf)) nop->longopt = xstrdup(objbuf);
  obj = comma + 1;  // field # 3, option variable name.
  while (*comma != ',') comma++;
  *comma = 0;
  strcpy(objbuf, obj);
  if (strlen(objbuf)) nop->varname = xstrdup(objbuf);
    else nop->varname = "FIXME";
  obj = comma + 1;  // field # 4, purpose.
  while (*comma != ',') comma++;
  *comma = 0;
  strcpy(objbuf, obj);
  if (validpurpose(objbuf)) nop->purpose = xstrdup(objbuf);
  obj = comma + 1;  // field # 5, dflt_val.
  while (*comma != ',') comma++;
  *comma = 0;
  strcpy(objbuf, obj);
  if (strlen(objbuf)) nop->dflt_val = xstrdup(objbuf);
    else nop->dflt_val = getdflt(nop->purpose);
  obj = comma + 1;  // field # 6, max_val.
  while (*comma != ',') comma++;
  *comma = 0;
  strcpy(objbuf, obj);
  if (strlen(objbuf)) nop->max_val = xstrdup(objbuf);
    else nop->max_val = (char*)NULL;
  obj = comma + 1;  // field # 7, help_txt.
  while (*comma != ',') comma++;
  *comma = 0;
  strcpy(objbuf, obj);
  if (strlen(objbuf)) nop->help_txt = xstrdup(objbuf);
    else nop->help_txt = xstrdup("FIXME");
  obj = comma + 1;  // field # 8, runfunc.
  strcpy(objbuf, obj);
  if (strlen(objbuf)) nop->runfunc = xstrdup(objbuf);
    else nop->runfunc = xstrdup("FIXME");
  return nop;
} // makenewoption()

int
validateoptsdescriptor(char *optsdescriptor)
{ /* The optsdescriptor has 8 fields, comma separated.
   * So there must be 7 commas, more or fewer is an error.
  */
  size_t i;
  size_t count = 0;
  for (i = 0; optsdescriptor[i]; i++) {
    if (optsdescriptor[i] == ',') count++;
  }
  if (count != 7) {
    fprintf(stderr, "Option descriptor string must have 7 commas.\n"
                    "%s\n", optsdescriptor);
    exit(EXIT_FAILURE);
  }
  // Can have a terminal ';' on the last item of a list. Get rid of it.
  char *sc = strrchr(optsdescriptor, ';');
  if (sc) *sc = 0;
  return 1; // didn't fail count test.
} // validateoptsdescriptor()

int
validshortname(char *buf)
{ /* a valid shortname is 1,2 or 3 chars long
   * buf[0] must be alpha or a number.
   * buf[1] and buf[2] must be == ':' if they exist.
  */
  int yes;
  size_t len = strlen(buf);
  if (len > 0 && len < 4) yes = 1; else yes = 0;
  if ((yes) && isalnum(buf[0])) yes = 1; else yes = 0;
  switch(len) {
    case 2:
      if ((yes) && buf[1] == ':') yes = 1; else yes = 0;
    break;
    case 3:
      if ((yes) && buf[1] == ':' && buf[2] == ':') yes = 1;
      else yes = 0;
    break;
  } // switch()
  if (yes) return yes;
  fprintf(stderr, "Malformed short option specifier: %s\n", buf);
  exit(EXIT_FAILURE);
} // validshortname()

int
validlongname(char *buf)
{ /* Must be > 1 char long and all chars are to be isalnum() */
  size_t len = strlen(buf);
  int res;
  if (len > 1) res = 1; else res = 0;
  if (res) {
    size_t i;
    for (i = 0; i < len; i++) {
      if (!isalnum(buf[i])) res = 0;
    } // for()
  } // if()
  if (res) {
    return 1;
  } else {
    fprintf(stderr, "Invalid option long name: %s\n", buf);
    exit(EXIT_FAILURE);
  }
} // validlongname()

int
validpurpose(char *buf)
{ /* must be in a list, if not abort */
  char *list[7] = { "flag","acc","int","float", "string", "file",
                    NULL };
  int ret = instrlist(buf, list);
  if (!ret) {
    fprintf(stderr, "Purpose, not in list:\n");
    size_t i;
    for (i = 0; list[i]; i++) {
      fprintf(stderr, "%s, ", list[i]);
    }
    fputs("\n", stderr);
    exit(EXIT_FAILURE);
  } // if()
  return 1;
} // validpurpose()

char
*getdflt(char *purpose)
{ /* do a dictionary lookup to find the default zero value. */
  char *list[] = { "flag=0", "acc=0", "int=0", "float=0.0",
                  "string=(char*)NULL", "file=(char*)NULL", NULL};
  static char buf[PATH_MAX];
  char *cp = dictionary(list, purpose);
  if (!cp) {
    fprintf(stderr, "Invalid option purpose: %s\n", purpose);
    exit(EXIT_FAILURE);
  }
  strcpy(buf, cp);
  return buf;
} // getdflt()

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
maketargetoptions(prgvar_t *pv, newopt_t **nopl)
{ /* make the gopt.c+h for the target program.*/
  maketoheader(pv, nopl);
  maketoCfile(pv, nopl);
} // maketargetoptions()

void
maketoheader(prgvar_t *pv, newopt_t **nopl)
{ /* generates the options_t struct */
  mdata *md = gettargetfile(pv, "gopt.h");
  setfileownertext(pv, md);
  char buf[PATH_MAX];
  buf[0] = 0;
  size_t i;
  for (i = 0; nopl[i]; i++) {
    char *purpose = nopl[i]->purpose;
    char *ctype = purposetoCtype(purpose);
    char *vname = nopl[i]->varname;
    char *cmnt = nopl[i]->runfunc;
    char line[NAME_MAX];
    sprintf(line, "\t%s\t%s\t// %s, %s", ctype, vname, purpose, cmnt);
    strjoin(buf, '\n', line, PATH_MAX);
  }
  memreplace(md, "<struct>", buf, PATH_MAX);
  char *path = settargetfilename(pv, "gopt.h");
  writefile(path, md->fro, md->to, "w" );
  free_mdata(md);
} // maketoheader()

void
maketoCfile(prgvar_t *pv, newopt_t **nopl)
{ /* Deals with <file owner>, <optstring>, <longopt>, and
   * <cases>, in template file.
  * */
  mdata *md = gettargetfile(pv, "gopt.c");
  setfileownertext(pv, md);
  char *cp = buildoptstring(nopl);
  memreplace(md, "<optstring>", cp, NAME_MAX);
  cp = builddefaults(nopl);
  if (cp) {
    memreplace(md, "<defaults>", cp, PATH_MAX);
  } else memdel(md, "<defaults>");
  cp = buildlongopts(nopl);
  if (cp) memreplace(md, "<longopt>", cp, PATH_MAX);
  cp = buildcases(nopl);
  if (cp) memreplace(md, "<cases>", cp, PATH_MAX);
  char *path = settargetfilename(pv, "gopt.c");
  writefile(path, md->fro, md->to, "w" );
  free_mdata(md);
} // maketoCfile()

char *buildoptstring(newopt_t **nopl)
{
  static char buf[PATH_MAX];
  strcpy(buf, ":");
  size_t i;
  for (i = 0; nopl[i]; i++) {
    strcat(buf, nopl[i]->shortopt);
  } // for()
  return buf;
} // buildoptstring()

char
*builddefaults(newopt_t **nopl)
{ /* Deals with presence or absence of non-zero default values. */
  static char buf[PATH_MAX];
  buf[0] = 0;
  size_t i;
  for (i = 0; nopl[i]; i++) {
    char *cp = nopl[i]->dflt_val;
    if (strcmp(cp, "0") == 0) continue;
    if (strcmp(cp, "0.0") == 0) continue;
    if (strstr(cp, "NULL")) continue;
    char name[NAME_MAX];
    sprintf(name, "\topts.%s = %s;\n", nopl[i]->varname, cp);
    strcat(buf, name);
  } // for()
  if (strlen(buf)) return buf; else return (char*)NULL;
} // builddefaults()

char
*buildlongopts(newopt_t **nopl)
{
  static char buf[PATH_MAX];
  buf[0] = 0;
  size_t i;
  for (i = 0; nopl[i]; i++) {
    char *cp = nopl[i]->longopt;
    char *sp = nopl[i]->shortopt;
    int colon = 0;
    if (strstr(sp, "::")) colon = 2;
    else if (strstr(sp, ":")) colon = 1;
    char name[NAME_MAX];
    sprintf(name, "\t\t{\"%s\",\t%d,\t0,\t\'%c\' },",
              cp, colon, sp[0]);
    strjoin(buf, '\n', name, PATH_MAX);
  } // for()
  return buf;
} // buildlongopts()

char
*buildcases(newopt_t **nopl)
{
  static char buf[PATH_MAX];
  buf[0] = 0;
  size_t i;
  for (i = 0; nopl[i]; i++) {
    char *so = nopl[i]->shortopt;
    char *vn = nopl[i]->varname;
    char *rv = getoptrval(nopl[i]->purpose);
    char name[NAME_MAX];
    sprintf(name, "\t\tcase \'%c\':\n\t\t\topts.%s = %s;",
              so[0], vn, rv);
    strjoin(buf, '\n', name, PATH_MAX);
    // to deal with maxval later
    strjoin(buf, '\n', "\t\tbreak;", PATH_MAX);
  } // for()
  return buf;
} // buildcases()

char
*getoptrval(const char *purpose)
{ /* From options purpose, return the value to the varname. */
  char *list[7] = {
    "flag= 1",
    "acc= 1",
    "int= atol(argv, NULL, 10)",
    "float= atod(argv, NULL)",
    "string= xstrdup(argv)",
    "file= xstrdup(argv)",
    NULL
  };
  return dictionary(list, purpose);
} // getoptrval()

mdata
*gettargetfile(prgvar_t *pv, const char *fn)
{ /* readfile to take care of errors. */
  char *path = settargetfilename(pv, fn);
  mdata *md = readfile(path, 0, 1);
  return md;
} // gettargetfile()

void
setfileownertext(prgvar_t *pv, mdata *md)
{ /* Writes the copyright ownership text near the top of a file. */
  char buf[NAME_MAX];
  time_t now = time(NULL);
  struct tm *lt = localtime(&now);
  int yy = lt->tm_year + 1900;
  sprintf(buf, "%d %s %s", yy, pv->pi->author, pv->pi->email);
  memreplace(md, "<file owner>", buf, NAME_MAX);
} // setfileownertext()

char
*settargetfilename(prgvar_t *pv, const char *fn)
{
  static char path[PATH_MAX];
  strcpy(path, pv->newdir);
  strjoin(path, '/', fn, PATH_MAX);
  return path;
} // settargetfilename()

char
*purposetoCtype(const char *purpose)
{
  char *list[7] = { "flag=int", "acc=int", "int=int", "float=double",
    "string=char*", "file=char*", NULL };
    return dictionary(list, purpose);
} // purposetoCtype()

void
makemain(prgvar_t *pv)
{ /*  Copy main.c template to source file name and fill in targets. */
  char *pathto = settargetfilename(pv, pv->pi->src);
  copyfile("./templates/main.c", pathto);
  mdata *md = gettargetfile(pv, pv->pi->src);
  setfileownertext(pv, md);
  memreplace(md, "<exename>", pv->pi->exe, NAME_MAX);

  char *path = settargetfilename(pv, pv->pi->src);
  writefile(path, md->fro, md->to, "w" );
  free_mdata(md);
} // makemain()




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
} // addautotools()

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
prgvar_tfree(prgvar_t *pv)
{ /* allow that any object to free may be NULL */
  if (!pv) return;
  if (pv->optsout)    freestringlist(pv->optsout, 0);
  if (pv->pi)         progidfree(pv->pi);
  if (pv->libswlist)  freestringlist(pv->libswlist, 0);
  if (pv->extras)     freestringlist(pv->extras, 0);
  if (pv->newdir)     free(pv->newdir);
  if (pv->linksdir)   free(pv->linksdir);
  if (pv->stubsdir)   free(pv->stubsdir);
  free(pv);
}  // prgvar_tfree()

void
newopt_tfree(newopt_t *no)
{ /* allow that any object to free may be NULL */
} // newopt_tfree()

void
printerr(char *msg, char *var, int fatal)
{ /* many errors will be suitable for this format. */
  fprintf(stderr, "%s: %s\n", msg, var);
  if (fatal) exit(EXIT_FAILURE);
} // printerr()

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

void
dohelp(int forced)
{
  char command[PATH_MAX];
  char *dev = "./newprg.1";
  char *prd = "newprg";
  if (exists_file(dev)) {
    sprintf(command, "man %s", dev);
  } else {
    sprintf(command, "man 1 %s", prd);
  }
  xsystem(command, 1);
  exit(forced);
} // dohelp()

prgvar_t
*action_options(options_t *optp)
{
/*
typedef struct options_t {
	char *options_list;   // output options description text.
} options_t;
*/
  if (optp->runhelp) dohelp(0);  // exits, no return;
  // if (optp->runvsn) dovsn(0);  // exits, no return;
  prgvar_t *pv = xmalloc(sizeof(struct prgvar_t));
  pv->libswlist = getlibsoftwarenames("./defaults/lsw.dflt",
                                        optp->software_deps);
  pv->extras = getextras("./defaults/extra.dflt", optp->extra_data);
  pv->optsout = getoptionslist("./defaults/options.dflt",
                                optp->options_list);
  return pv;
} // action_options()

char
**getlibsoftwarenames(const char *path, char *nameslist)
{ /* consolidate all names from defaults and/or options.
   * either or both sources may be NULL.
  */
  char buf[PATH_MAX];
  buf[0] = 0;
  if (exists_file(path)) {
    char *dfp = read_defaults(path);
    strjoin(buf, ' ', dfp, PATH_MAX);
  }
  if (nameslist) strjoin(buf, ' ', nameslist, PATH_MAX);
  if (!(strlen(buf))) return (char**)NULL; 
  char *expanded = expand_extensions(buf);
  char **swlist = list2array(expanded, " ");
  free(expanded);
  return swlist;
} // getlibsoftwarenames()

char
*expand_extensions(const char *list)
{ /* some filenames may look like example.h+c as shorthand for
   * example.h and example.c. This expands any such shorthand into
   * the named pairs of files.
  */
  char inbuf[PATH_MAX]; // I know that list is < PATH_MAX
  const int twoby = PATH_MAX * 2;
  char outbuf[2 * PATH_MAX];
  outbuf[0] = 0;  // strjoin demands this.
  strcpy (inbuf, list);
  char *cp = inbuf;
  while (*cp) {
    char name[FILENAME_MAX];
    char *ep = strchr(cp, ' ');
    if (ep) *ep = 0;  // the last word is already terminated by 0.
    strcpy(name, cp);
    char *exp = strrchr(name, '+'); // shorthand naming?
    if (exp) {
      *exp = 0;
      strjoin(outbuf, ' ', name, twoby);  // first named file
      *(exp-1) = *(exp+1);  // prepare second named file
    }
    strjoin(outbuf, ' ', name, twoby);
    cp += strlen(cp) + 1;
  } // while()
  return xstrdup(outbuf);
} // expand_extensions()

char
*read_defaults(const char *path)
{ /* read the named path, get rid of comments, and return a space
   * separated list of non-zero length strings.
  */
  char buf[PATH_MAX];
  buf[0] = 0; // strjoin requires this.
  mdata *md = readfile(path, 0, 1);
  stripcomment(md, "#", "\n", 0);
  char **strlist = mdatatostringlist(md);
  size_t idx = 0;
  while (strlist[idx]) {
    strjoin(buf, ' ', strlist[idx], PATH_MAX);
    idx++;
  }
  freestringlist(strlist, 0);
  free_mdata(md);
  return xstrdup(buf);
} // read_defaults()

char
**getextras(const char *path, char *nameslist)
{ 
  char buf[PATH_MAX];
  buf[0] = 0;
  if (exists_file(path)) {
    char *dfp = read_defaults(path);
    strjoin(buf, ' ', dfp, PATH_MAX);
  }
  if (nameslist) strjoin(buf, ' ', nameslist, PATH_MAX);
  if (!(strlen(buf))) return (char**)NULL; 
  char **extralist = list2array(buf, " ");
  return extralist;
} // getextras()

char
**getoptionslist(const char *path, char *nameslist)
{ /* consolidate all names from defaults and/or options.
   * either or both sources may be NULL.
  */
  char buf[PATH_MAX];
  buf[0] = 0;
  if (exists_file(path)) {
    char *dfp = read_defaults(path);
    strjoin(buf, ' ', dfp, PATH_MAX);
  }
  if (nameslist) strjoin(buf, ' ', nameslist, PATH_MAX);
  if (!(strlen(buf))) return (char**)NULL; 
  char **optionslist = list2array(buf, "; ");
  return optionslist;
} // getoptionslist()
