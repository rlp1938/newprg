/*
 * <exename>.c
 * 
 * Copyright <file owner>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
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

// structs

#include "dirs.h"
#include "files.h"
#include "gopt.h"
#include "firstrun.h"

// Globals
static char *vsn;
// headers
static void
dohelp(int forced);
static void
dovsn(void);
static void
is_this_first_run(void);

int main(int argc, char **argv)
{ /* main */
  vsn = "1.0";
  is_this_first_run(); // check first run
  return 0;
} // main()

void
dohelp(int forced)
{ /* runs the manpage and then quits. */
  char command[PATH_MAX];
  char *dev = "./<exename>.1";
  char *prd = "<exename>";
  if (exists_file(dev)) {
    sprintf(command, "man %s", dev);
  } else {
    sprintf(command, "man 1 %s", prd);
  }
  xsystem(command, 1);
  exit(forced);
} // dohelp()

void
dovsn(void)
{ /* print version number and quit. */
  fprintf(stderr, "<exename>, version %s\n", vsn);
  exit(0);
} // dovsn()

void
is_this_first_run(void) 
{ /* test for first run and take action if it is */
  char *names[2] = { "<exename>.cfg", NULL };
  if (!checkfirstrun("<exename>", names)) {
    firstrun("<exename>", names);
    fprintf(stderr,
          "Please edit <exename>.cfg in %s/.config/<exename>"
          " to meet your needs.\n",
          getenv("HOME"));
    exit(EXIT_SUCCESS);
  }
} // is_this_first_run()
