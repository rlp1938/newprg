
#AM_CFLAGS=-Wall -Wextra -O2 -D_GNU_SOURCE=1
# Set up initially to use GDB, change to optimised afterward.
AM_CFLAGS=-Wall -Wextra -g -O0 -D_GNU_SOURCE=1

bin_PROGRAMS=newprg

newprg_SOURCES=newprg.c dirs.c dirs.h files.c files.h str.c \
str.h firstrun.h firstrun.c gopt.h gopt.c

man_MANS=newprg.1

# next lines to be hand edited
# send <whatever> to $(prefix)/share/
newdir=$(datadir)/newprg
new_DATA=newprg.cfg
# ensure that newprg.1 and any other config files get put in the
# tarball. Also stops `make distcheck` bringing an error.
EXTRA_DIST=newprg.1 prdata.cfg
