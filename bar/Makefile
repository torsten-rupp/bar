# ----------------------------------------------------------------------------
#
# aicas GmbH, Karlsruhe
#
# $Source: /home/torsten/cvs/bar/Makefile,v $
# $Revision: 1.5 $
# $Author: torsten $
# Contents: Makefile for bar
# Systems : all
#			   
# ----------------------------------------------------------------------------

#--------------------------------- tool chain --------------------------------

CC         = gcc
CC_FLAGS   = -Wall -g -O0
CC_DEFINES = -D_LARGEFILE64_SOURCE

LD         = gcc
LD_FLAGS   =

#---------------------------------- commands----------------------------------

ECHO = echo
RMF  = rm -f
PERL = perl

#------------------------ specific variables/settings ------------------------

#---------------------------------- rules ------------------------------------

%.o:%.c
	$(CC) $(CC_FLAGS) $(CC_DEFINES) -o $*.o -c $*.c

#--------------------------------- objects -----------------------------------

SOURCES =       bar.c \
                command_create.c \
                command_list.c \
                command_restore.c \
                command_test.c \
                files.c \
                archive.c \
                chunks.c \
                \
                global.c \
                cmdoptions.c \
                lists.c \
                strings.c \

OBJECTS = 	$(foreach z,$(SOURCES),$(subst .c,.o,$z)) \
                archive_format.o \

INTERMEDIATE =  archive_format.c archive_format.h \

TARGETS = 	bar \

#------------------------------ dependencies ---------------------------------

.PHONE: all clean distclean depend
all:		$(TARGETS)

clean:
		$(RMF) $(OBJECTS) $(INTERMEDIATE) $(TARGETS)
		$(RMF) $(OBJECTS) $(INTERMEDIATE) $(TARGETS)
		$(RMF) $(OBJECTS) $(INTERMEDIATE) $(TARGETS)

distclean: clean
	$(RMF) Makefile.depend

depend: archive_format.h
	$(ECHO) > Makefile.depend
	for z in $(SOURCES); do \
          $(CC) -MM >> Makefile.depend $$z; \
        done;

bar: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^ -lpthread

archive_format.h: archive_format.def
	$(PERL) archive_format.pl < $^ -h $@
archive_format.c: archive_format.def
	$(PERL) archive_format.pl < $^ -c $@

-include Makefile.depend

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
