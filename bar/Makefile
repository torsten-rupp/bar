# ----------------------------------------------------------------------------
#
# aicas GmbH, Karlsruhe
#
# $Source: /home/torsten/cvs/bar/Makefile,v $
# $Revision: 1.7 $
# $Author: torsten $
# Contents: Makefile for bar
# Systems : all
#			   
# ----------------------------------------------------------------------------

#--------------------------------- tool chain --------------------------------

CC                  = gcc
CC_FLAGS            = -Wall -g -O0
CC_DEFINES          = -D_LARGEFILE64_SOURCE
CC_INCLUDES         =

LD                  = gcc
LD_FLAGS            =
LD_LIBRARY_PATHS    = /usr/local/lib
LD_LIBRARIES        = pthread z
LD_STATIC_LIBRARIES = gcrypt gpg-error

#---------------------------------- commands----------------------------------

ECHO = echo
RMF  = rm -f
PERL = perl

#------------------------ specific variables/settings ------------------------

CRYPT=none
#CRYPT=aes128
COMPRESS=none

#---------------------------------- rules ------------------------------------

%.o:%.c
	$(CC) $(CC_FLAGS) $(CC_DEFINES) $(for z,$(CC_INCLUDES),-I$z) -o $*.o -c $*.c

%:%.o
	$(LD) \
          $(LD_FLAGS) \
          -o $@ \
          $^ \
          $(foreach z,$(LD_LIBRARY_PATHS),-L$z) \
          $(foreach z,$(LD_LIBRARIES),-l$z) \
          -Wl,-static $(foreach z,$(LD_STATIC_LIBRARIES),-l$z)

#--------------------------------- objects -----------------------------------

SOURCES =       bar.c \
                command_create.c \
                command_list.c \
                command_restore.c \
                command_test.c \
                files.c \
                archive.c \
                chunks.c \
                patterns.c \
                compress.c \
                crypt.c \
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

distclean: clean
	$(RMF) Makefile.depend

depend: archive_format.h
	$(ECHO) > Makefile.depend
	for z in $(SOURCES); do \
          $(CC) -MM >> Makefile.depend $$z; \
        done;

bar: $(OBJECTS)

archive_format.h: archive_format.def
	$(PERL) archive_format.pl < $^ -h $@
archive_format.c: archive_format.def
	$(PERL) archive_format.pl < $^ -c $@

-include Makefile.depend

.PHONE: p0 pack0 l0 list0 r0 restore0 t0 c0 compare0
p0 pack0: bar
	./bar -a data/data.back -c demo/abc.dat --compress=$(COMPRESS) --crypt=$(CRYPT) --password=test

l0 list0: bar
	./bar -l data/data.back --password=test

r0 restore0: bar
	./bar --directory data/restore -x data/data.back --password=test

t0 c0 compare0: bar
	./bar -t data/data.back --password=test

.PHONE: p1 pack1 l1 list1 r1 restore1 t1 c1 compare1
p1 pack1: bar
	./bar -a data/data.back -c demo

l1 list1: bar
	./bar -l data/data.back

r1 restore1: bar
	./bar --directory data/restore -x data/data.back

t1 c1 compare1: bar
	./bar -t data/data.back

.PHONE: p2 pack2 l2 list2 r2 restore2
p2 pack2: bar
	./bar -a data/data.back -c -s 1000000 demo

l2 list2: bar
	./bar -l data/data.back.*

r2 restore2:
	./bar --directory data/restore -x data/data.back.*

c2 compare2: bar
	./bar -t data/data.back.*

.PHONE: p3 pack3
p3 pack3: bar
	./bar -a data/data.back -c "demo/abc*.dat"

clean_data:
	$(RMF) data/data.back*
