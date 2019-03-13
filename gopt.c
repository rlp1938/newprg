/*      gopt.c
 *
 *  Copyright 2019 Robert L (Bob) Parker rlp1938@gmail.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
*/
#include "str.h"
#include "files.h"
#include "gopt.h"


options_t process_options(int argc, char **argv)
{
  optstring = ":hd:x:n:";  // initialise

  options_t opts;
  opts.runhelp         = 0;
  opts.software_deps  = NULL;
  opts.extra_data     = NULL;
  opts.options_list   = NULL;

  int c;
  const int max = PATH_MAX;
  char joinbuffer[PATH_MAX];    // collects list of extra software.
  joinbuffer[0] = 0;
  char databuffer[PATH_MAX];    // collects list of other data.
  databuffer[0] = 0;
  char optionsbuffer[PATH_MAX]; // collects list of options codes.
  optionsbuffer[0] = 0;

  while(1) {
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] = {
    {"help",          0,  0,  'h' },
    {"depends",       1,  0,  'd' },
    {"extra-dist",    1,  0,  'x' },
    {"options-list",  1,  0,  'n' },
    {0,  0,  0,  0 }
    };

    c = getopt_long(argc, argv, optstring,
                    long_options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0:
      switch (option_index) {
      } // switch()
    break;
    case 'h':
      opts.runhelp = 1;
    break;
    case 'd':  // output software dependencies for Makefile.am
      strjoin(joinbuffer, ' ',optarg, max);
    break;
    case 'n':  // output options descriptor strings.
      strjoin(optionsbuffer, ';', optarg, max);
    break;
    case 'x':  // other data for Makefile.am
      strjoin(databuffer, ' ',optarg, max);
    break;
    case ':':
      fprintf(stderr, "Option %s requires an argument\n",
          argv[this_option_optind]);
      opts.runhelp = 1;
    break;
    case '?':
      fprintf(stderr, "Unknown option: %s\n",
           argv[this_option_optind]);
      opts.runhelp = 1;
    break;
    } // switch()
  } // while()
  if (strlen(joinbuffer)) {
    opts.software_deps = xstrdup(joinbuffer);
  }
  if (strlen(databuffer)) {
    opts.extra_data = xstrdup(databuffer);
  }
  if (strlen(optionsbuffer)) {
    opts.options_list = xstrdup(optionsbuffer);    
  }
  return opts;
} // process_options()

