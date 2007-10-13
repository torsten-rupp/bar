# ----------------------------------------------------------------------------
#
# aicas GmbH, Karlsruhe
#
# $Source: /home/torsten/cvs/bar/Makefile,v $
# $Revision: 1.32 $
# $Author: torsten $
# Contents: Makefile for bar
# Systems : all
#			   
# ----------------------------------------------------------------------------

#--------------------------------- tool chain --------------------------------

CC                  = gcc
CC_FLAGS            = -g -O0
CC_WARN_FLAGS       = -Wall -Wpointer-arith -Wunused-label -Wunused-parameter -Wunused-variable -Wmissing-prototypes -Wmissing-declarations
CC_DEFINES          = -D_LARGEFILE64_SOURCE -D_GNU_SOURCE -D_LONG_LONG
CC_INCLUDES         =

LD                  = gcc
LD_FLAGS            =
LD_LIBRARY_PATHS    = /usr/local/lib
LD_LIBRARIES        = m pthread z bz2 gcrypt gpg-error ssh2 gnutls
LD_STATIC_LIBRARIES = 

YACC                = bison
YACC_FLAGS          =

#---------------------------------- commands----------------------------------

ECHO = echo
ECHO_NO_LF = echo -n
RMF  = rm -f
RMRF = rm -rf
PERL = perl
DIFF = diff

#------------------------ specific variables/settings ------------------------

CRYPT_NAMES    = none 3des cast5 blowfish aes128 aes192 aes256 twofish128 twofish256
COMPRESS_NAMES = none zip0 zip1 zip2 zip3 zip4 zip5 zip6 zip7 zip8 zip9 bzip1 bzip2 bzip3 bzip4 bzip5 bzip6 bzip7 bzip8 bzip9

CRYPT=none
COMPRESS=none

TEST_PASSWORD=franz_kafka

#---------------------------------- rules ------------------------------------

%.o:%.c
	$(CC) $(CC_FLAGS) $(CC_WARN_FLAGS) $(CC_DEFINES) $(for z,$(CC_INCLUDES),-I$z) -c $*.c -o $*.o

%:%.o
	$(LD) \
          $(LD_FLAGS) \
          -o $@ \
          $^ \
          $(foreach z,$(LD_LIBRARY_PATHS),-L$z) \
          $(foreach z,$(LD_LIBRARIES),-l$z) \

#          -Wl,-static $(foreach z,$(LD_STATIC_LIBRARIES),-l$z) -Wl,-dynamic

#--------------------------------- objects -----------------------------------

BAR_SOURCES        = bar.c \
                     commands_create.c \
                     commands_list.c \
                     commands_restore.c \
                     commands_test.c \
                     files.c \
                     archive.c \
                     chunks.c \
                     patterns.c \
                     compress.c \
                     crypt.c \
                     storage.c \
                     network.c \
                     server.c \
                     \
                     global.c \
                     cmdoptions.c \
                     lists.c \
                     strings.c \
                     stringlists.c \
                     msgqueues.c \
                     semaphores.c \
                     filefragmentlists.c \
                     arrays.c \

BAR_OBJECTS        = $(foreach z,$(BAR_SOURCES),$(subst .c,.o,$z)) \
                     archive_format.o \

DESTROYER_SOURCES  = destroyer.c \
                     global.c \
                     cmdoptions.c \
                     lists.c \
                     strings.c \

DESTROYER_OBJECTS  = $(foreach z,$(DESTROYER_SOURCES),$(subst .c,.o,$z))


INTERMEDIATE       = archive_format.c \
                     archive_format.h \

TARGETS            = bar \
                     destroyer \

#------------------------------ dependencies ---------------------------------

.PHONY: all clean distclean depend
all:		$(TARGETS)

clean:
		$(RMF) $(BAR_OBJECTS) $(DESTROYER_OBJECTS) $(INTERMEDIATE) $(TARGETS)

distclean: clean
	$(RMF) Makefile.depend

depend: archive_format.h
	$(ECHO) > Makefile.depend
	for z in $(BAR_SOURCES) $(DESTROYER_SOURCES); do \
          $(CC) -MM >> Makefile.depend $$z; \
        done;

# create Backup Archiver
bar: $(BAR_OBJECTS)

# create destroyer (for testing only!)
destroyer: $(DESTROYER_OBJECTS) 
	$(LD) $(LD_FLAGS) -o $@ $^ -lpthread

archive_format.h: archive_format.def
	$(PERL) archive_format.pl < $^ -h $@
archive_format.c: archive_format.def
	$(PERL) archive_format.pl < $^ -c $@

# create SSL certificates
.PHONY: cert clean_cert
cert: bar-ca.pem bar-key.pem bar-server-cert.pem bar-server-key.pem

clean_cert:
	$(RMF) bar-ca.pem bar-key.pem bar-server-cert.pem bar-server-key.pem

bar-key.pem:
	certtool \
          --generate-privkey \
          --outfile $@ \
          1>/dev/null 2>/dev/null

bar-ca.pem: bar-key.pem
	certtool \
          --generate-self-signed \
          --template ssl/ca.tmpl \
          --load-privkey bar-key.pem \
          --outfile $@ \
          1>/dev/null 2>/dev/null

bar-server-key.pem:
	certtool \
          --generate-privkey \
          --outfile $@ \
          1>/dev/null 2>/dev/null

bar-server-cert.pem: bar-server-key.pem
	certtool \
          --generate-certificate \
          --template ssl/server.tmpl \
          --load-privkey bar-server-key.pem \
          --load-ca-certificate bar-ca.pem \
          --load-ca-privkey bar-key.pem \
          --outfile $@ \
          1>/dev/null 2>/dev/null

#	certtool \
#          --generate-crl \
#          --template ssl/server.tmpl \
#          --load-privkey key-server.pem \
#          --load-ca-certificate ca.pem \
#          --load-ca-privkey key-ca.pem \
#          --outfile server-crl.pem
#	certtool --generate-privkey --outfile client-key.pem
#	certtool --generate-certificate --template doc/credentials/client.tmpl --load-privkey client-key.pem --load-ca-certificate ca.pem --load-ca-privkey ca-key.pem --outfile client-cert.pem

# run server
.PHONY: barserver barserver_ssl
barserver: bar
	./bar \
          --daemon

barserver_ssl: bar
	./bar \
          --daemon \
          --ssl \
          --server-ca-file=bar-ca.pem \
          --server-cert-file=bar-server-cert.pem \
          --server-key-file=bar-server-key.pem

# do test
.PHONY: test test1 test2 test3 test4 test_all
test:
	@$(MAKE) QUIET=1 -s test1 test2 test3 test4 test_all

test1: bar
	@$(ECHO_NO_LF) "Test 1: basic..."
	$(MAKE) BAR_FILE="test.bar" OPTIONS="--compress-algorithm=none --crypt-algorithm=none" standard_test
	@$(ECHO) "ok"

test2 test_compress: bar
	@$(ECHO_NO_LF) "Test 2: compress..."
	for compress in $(COMPRESS_NAMES); do \
          $(MAKE) BAR_FILE="test.bar" OPTIONS="--compress-algorithm=$$compress" standard_test; \
        done
	@$(ECHO) "ok"

test3 test_crypt: bar
	@$(ECHO_NO_LF) "Test 3: crypt..."
	for crypt in $(CRYPT_NAMES); do \
          $(MAKE) BAR_FILE="test-###.bar" OPTIONS="--crypt-algorithm=$$crypt --crypt-password=$(TEST_PASSWORD)" standard_test; \
        done
	@$(ECHO) "ok"

test4 test_split: bar
	@$(ECHO_NO_LF) "Test 4: split..."
	$(MAKE) BAR_FILE="test-###.bar" OPTIONS="-s 100000" standard_test
	@$(ECHO) "ok"

test_all: bar
	@$(ECHO_NO_LF) "Test all..."
	for compress in $(COMPRESS_NAMES); do \
          for crypt in $(CRYPT_NAMES); do \
            $(MAKE) BAR_FILE=test-###.bar OPTIONS="--compress-algorithm=$$compress --crypt-algorithm=$$crypt --crypt-password=$(TEST_PASSWORD) -s 100000" standard_test; \
            rc=$$?; \
            if test $$rc -ne 0; then \
              exit 1; \
            fi; \
          done; \
        done
	@$(ECHO) "ok"

.PHONY: standard_test
standard_test: bar
	$(RMRF) test/test*.bar test/restore
	./bar -c test/$(BAR_FILE) test/data $(OPTIONS) $(if $(QUIET),1>/dev/null)
	./bar -l test/test*.bar $(OPTIONS) $(if $(QUIET),1>/dev/null)
	./bar -t test/test*.bar $(OPTIONS) $(if $(QUIET),1>/dev/null)
	./bar -x test/test*.bar $(OPTIONS) --directory test/restore $(if $(QUIET),1>/dev/null)
	$(DIFF) -r test/data test/restore/test/data

.PHONY: standard_test_destroy
standard_test_destroy: bar destroyer
	$(RMRF) test/test*.bar test/restore
	./bar -c test/$(BAR_FILE) test/data/zero1024.dat $(OPTIONS) $(if $(QUIET),1>/dev/null)
	./destroyer test/test.bar m:0 > test/test.bar_destroyed
	./bar -l test/test.bar_destroyed $(OPTIONS) $(if $(QUIET),1>/dev/null)
	./bar -t test/test.bar_destroyed $(OPTIONS) $(if $(QUIET),1>/dev/null)
	./bar -x test/test.bar_destroyed $(OPTIONS) --directory test/restore $(if $(QUIET),1>/dev/null)
	$(DIFF) -r test/data test/restore/test/data

-include Makefile.depend

.PHONY: p0 pack0 l0 list0 r0 restore0 t0 c0 compare0
p0 pack0: bar
	./bar -c data/data.bar demo/1.dat --compress-algorithm=$(COMPRESS) --crypt-algorithm=$(CRYPT) --crypt-password=test

l0 list0: bar
	./bar -l data/data.bar --crypt-password=test

r0 restore0: bar
	./bar --directory data/restore -x data/data.bar --crypt-password=test

t0 c0 compare0: bar
	./bar -t data/data.bar --crypt-password=test

.PHONY: p1 pack1 l1 list1 r1 restore1 t1 c1 compare1
p1 pack1: bar
	./bar -c data/data.bar demo --compress-algorithm=$(COMPRESS) --crypt-algorithm=$(CRYPT) --crypt-password=test

l1 list1: bar
	./bar -l data/data.bar --crypt-password=test

r1 restore1: bar
	./bar  -x data/data.bar --directory data/restore --crypt-password=test

t1 c1 compare1: bar
	./bar -t data/data.bar --crypt-password=test

.PHONY: p2 pack2 l2 list2 r2 restore2
p2 pack2: bar
	./bar -c -s 1000000 data/data.bar demo --compress-algorithm=$(COMPRESS) --crypt-algorithm=$(CRYPT) --crypt-password=test

l2 list2: bar
	./bar -l data/data.bar.* --crypt-password=test

r2 restore2:
	./bar --directory data/restore -x data/data.bar.* --crypt-password=test

t2 c2 compare2: bar
	./bar -t data/data.bar.* --crypt-password=test

.PHONY: p3 pack3
p3 pack3: bar
	./bar -a data/data.bar -c "demo/abc*.dat"

clean_data:
	$(RMF) test/test.bar*
	$(RMRF) test/restore

.PHONY: backup-home
backup-tooru: bar
	./bar \
          -c scp:torsten@tooru.dyndns.org:backup-torsten-###.bar \
          --ssh-port=49856 \
          --compress-algorithm=bzip9 \
          --crypt-algorithm=aes256 \
          --crypt-password=franz \
          -s 128M \
          --max-tmp-size=256M \
          --skip-unreadable \
          -! /home/torsten/tmp \
          -! /home/torsten/aicas \
          -! /home/torsten/public \
          -! /home/torsten/trash \
          -! /home/torsten/other \
          -! /home/torsten/Mail/trash \
          -! "*.mov" -! "*.avi" \
          /home/torsten

backup-itzo: bar
	./bar \
          -c scp:torsten@it-zo.de:backup/backup-torsten-###.bar \
          --ssh-port=2222 \
          --compress-algorithm=bzip9 \
          --crypt-algorithm=aes256 \
          --crypt-password=franz \
          -s 128M \
          --max-tmp-size=256M \
          --skip-unreadable \
          --verbose=1 \
          -! /home/torsten/tmp \
          -! /home/torsten/aicas \
          -! /home/torsten/public \
          -! /home/torsten/trash \
          -! /home/torsten/other \
          -! /home/torsten/Mail/trash \
          -! "*.mov" -! "*.avi" \
          /home/torsten
