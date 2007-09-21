# ----------------------------------------------------------------------------
#
# aicas GmbH, Karlsruhe
#
# $Source: /home/torsten/cvs/bar/Makefile,v $
# $Revision: 1.19 $
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
ECHO_NO_LF = echo -n
RMF  = rm -f
RMRF = rm -rf
PERL = perl
DIFF = diff

#------------------------ specific variables/settings ------------------------

CRYPT_NAMES    = none 3des cast5 blowfish aes128 aes192 aes256 twofish128 twofish256
COMPRESS_NAMES = none zip0 zip1 zip2 zip3 zip4 zip5 zip6 zip7 zip8 zip9

CRYPT=none
#CRYPT=aes128
COMPRESS=none

TEST_PASSWORD=franz_kafka

#---------------------------------- rules ------------------------------------

%.o:%.c
	$(CC) $(CC_FLAGS) $(CC_DEFINES) $(for z,$(CC_INCLUDES),-I$z) -c $*.c -o $*.o

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
                stringlists.c \
                filefragmentlists.c \

OBJECTS = 	$(foreach z,$(SOURCES),$(subst .c,.o,$z)) \
                archive_format.o \

INTERMEDIATE =  archive_format.c archive_format.h \

TARGETS = 	bar \
                destroyer \

#------------------------------ dependencies ---------------------------------

.PHONY: all clean distclean depend
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

# create Backup Archiver
bar: $(OBJECTS)

# create destroyer
destroyer: destroyer.o cmdoptions.o global.o lists.o strings.o
	$(LD) $(LD_FLAGS) -o $@ $^

archive_format.h: archive_format.def
	$(PERL) archive_format.pl < $^ -h $@
archive_format.c: archive_format.def
	$(PERL) archive_format.pl < $^ -c $@

# do test
.PHONY: test test1 test2 test3 test4 test_all
test:
	@$(MAKE) QUIET=1 -s test1 test2 test3 test4 test_all

test1: bar
	@$(ECHO_NO_LF) "Test 1: basic..."
	$(MAKE) COMPRESS=none CRYPT=none OPTIONS="" standard_test
	@$(ECHO) "ok"

test2: bar
	@$(ECHO_NO_LF) "Test 2: compress..."
	for compress in $(COMPRESS_NAMES); do \
          $(MAKE) OPTIONS="--compress=$$compress" standard_test; \
        done
	@$(ECHO) "ok"

test3: bar
	@$(ECHO_NO_LF) "Test 3: crypt..."
	for crypt in $(CRYPT_NAMES); do \
          $(MAKE) OPTIONS="--crypt=$$crypt --password=$(TEST_PASSWORD)" standard_test; \
        done
	@$(ECHO) "ok"

test4: bar
	@$(ECHO_NO_LF) "Test 4: split..."
	$(MAKE) OPTIONS="-s 100000" standard_test
	@$(ECHO) "ok"

test_all: bar
	@$(ECHO_NO_LF) "Test all..."
	for compress in $(COMPRESS_NAMES); do \
          for crypt in $(CRYPT_NAMES); do \
            $(MAKE) OPTIONS="--compress=$$compress --crypt=$$crypt --password=$(TEST_PASSWORD) -s 100000" standard_test; \
            rc=$$?; \
            if test $$rc -ne 0; then \
              exit 1; \
            fi; \
          done; \
        done
	@$(ECHO) "ok"

.PHONY: standard_test
standard_test: bar
	$(RMRF) test/test.bar* test/restore
	./bar -c test/test.bar test/data $(OPTIONS) $(if $(QUIET),1>/dev/null)
	./bar -l test/test.bar* $(OPTIONS) $(if $(QUIET),1>/dev/null)
	./bar -t test/test.bar* $(OPTIONS) $(if $(QUIET),1>/dev/null)
	./bar -x test/test.bar* $(OPTIONS) --directory test/restore $(if $(QUIET),1>/dev/null)
	$(DIFF) -r test/data test/restore/test/data
        
-include Makefile.depend

.PHONY: p0 pack0 l0 list0 r0 restore0 t0 c0 compare0
p0 pack0: bar
	./bar -c data/data.bar demo/1.dat --compress=$(COMPRESS) --crypt=$(CRYPT) --password=test

l0 list0: bar
	./bar -l data/data.bar --password=test

r0 restore0: bar
	./bar --directory data/restore -x data/data.bar --password=test

t0 c0 compare0: bar
	./bar -t data/data.bar --password=test

.PHONY: p1 pack1 l1 list1 r1 restore1 t1 c1 compare1
p1 pack1: bar
	./bar -c data/data.bar demo --compress=$(COMPRESS) --crypt=$(CRYPT) --password=test

l1 list1: bar
	./bar -l data/data.bar --password=test

r1 restore1: bar
	./bar  -x data/data.bar --directory data/restore --password=test

t1 c1 compare1: bar
	./bar -t data/data.bar --password=test

.PHONY: p2 pack2 l2 list2 r2 restore2
p2 pack2: bar
	./bar -c -s 1000000 data/data.bar demo --compress=$(COMPRESS) --crypt=$(CRYPT) --password=test

l2 list2: bar
	./bar -l data/data.bar.* --password=test

r2 restore2:
	./bar --directory data/restore -x data/data.bar.* --password=test

t2 c2 compare2: bar
	./bar -t data/data.bar.* --password=test

.PHONY: p3 pack3
p3 pack3: bar
	./bar -a data/data.bar -c "demo/abc*.dat"

clean_data:
	$(RMF) test/test.bar*
	$(RMRF) test/restore
