# ----------------------------------------------------------------------------
#
# aicas GmbH, Karlsruhe
#
# $Source: /home/torsten/cvs/bar/Makefile,v $
# $Revision: 1.3 $
# $Author: torsten $
# Contents: Makefile for
# Systems :
#			   
# ----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# Compiler options (gcc):
#
# basic options:
#  -ansi           support ANSI C standard programs
#  -nostdinc       don't search in host include files
#  -c              compile only (create object files)
#  -S              produce assembly source _only_
#  -g              produce debug information for debugger
#
# additional options:
#  -p              include profiler code statements (prof/gprof)
#  -a              include profiler code statements (tcov)
#
# special options:
#  -E              execute preprocessor _only_
#  -C              keep comments in preprocessor output
#  -M              generate dependency list (with system files)
#  -MM             generate dependency list (without system files)
#
# C++ options:
#  -fcheck-new     include check if new-operator returns NULL;
#                  constructors are called only if none-NULL
#
#-----------------------------------------------------------------------------

#--------------------------------- tool chain --------------------------------

CC         = gcc
CC_FLAGS   = -Wall -g -O0
CC_DEFINES = -D_LARGEFILE64_SOURCE

LD       = gcc
LD_FLAGS =

#---------------------------------- commands----------------------------------

ECHO = echo
RMF  = rm -f
PERL = perl

#------------------------ specific variables/settings ------------------------

#---------------------------------- rules ------------------------------------

%.o:%.c
	$(CC) $(CC_FLAGS) $(CC_DEFINES) -o $*.o -c $*.c

#--------------------------------- objects -----------------------------------

OBJECTS = 	bar.o \
                archive_create.o \
                archive_list.o \
                archive_restore.o \
                archive_format.o \
                files.o \
                chunks.o \
                \
                global.o \
                cmdoptions.o \
                lists.o \
                strings.o \

INTERMEDIATE =  archive_format.c archive_format.h

TARGETS = 	bar

#------------------------------ dependencies ---------------------------------

.PHONE: all clean
all:		$(TARGETS)

clean:
		$(RM) $(OBJECTS) $(INTERMEDIATE) $(TARGETS)

bar: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^ -lpthread

bar.o: bar.c bar.h archive.h archive_format.h

archive_create.o: archive_create.c archive.h archive_format.h strings.h

archive_list.o: archive_list.c archive.h archive_format.h strings.h

archive_restore.o: archive_restore.c archive.h archive_format.h strings.h

archive_format.o: archive_format.c archive_format.h

archive_format.h: archive_format.def
	$(PERL) archive_format.pl < $^ -h $@
archive_format.c: archive_format.def
	$(PERL) archive_format.pl < $^ -c $@

chunks.o: chunks.c chunks.h

files.o: files.c files.h

options.o: options.c options.h

.PHONE: pack1 list1 pack2 list2
pack1: bar
	./bar -a data/data.back -c demo

list1: bar
	./bar -l data/data.back

pack2: bar
	./bar -a data/data.back -c -s 1000000 demo

list2: bar
	./bar -l data/data.back.*

clean_data:
	$(RMF) data/data.back*
