VERSION = 1.0
INSTALL_PATH = /usr/local

CC                  = gcc
CC_FLAGS            = -Wall -g -O0
CC_DEFINES          = -DHAVE_UNISTD_H=1 -DHAVE_LIMITS_H=1 -DHAVE_GETCWD=1 -DHAVE_OPENDIR=1 -DHAVE_STRSTR=1 -DHAVE_STRTOL=1 -DHAVE_TMPNAM=1 -DHAVE_WAITPID=1 -DHAVE_UNISTD_H=1 -DHAVE_SYS_PARAM_H=1 -DUSE_TERMIOS=1 -DHAVE_SYS_TIME_H=1 -DTIME_WITH_SYS_TIME=1 -DHAVE_TZNAME=1 -DHAVE_TIMEZONE_VAR=1 -DHAVE_ST_BLKSIZE=1 -DSTDC_HEADERS=1 -DNO_UNION_WAIT=1 -DNEED_MATHERR=1 -DHAVE_SIGNED_CHAR=1 -DHAVE_SYS_IOCTL_H=1 -DHAVE_SYS_FILIO_H=1  -fPIC $(TCL_INCLUDE)
CC_INCLUDES         =

LD                  = ld
LD_FLAGS            = -G -z text
LD_LIBRARY_PATHS    = /usr/local/lib /usr/lib
LD_LIBRARIES        = 
LD_STATIC_LIBRARIES = 

%.o:%.c
	$(CC) $(CC_FLAGS) $(CC_DEFINES) $(for z,$(CC_INCLUDES),-I$z) -c $*.c -o $*.o

.PHONY: all clean
all: libscanx.so pkgIndex.tcl

clean:
	rm -f scanx.o tclScanx.o libscanx.so
	rm -f pkgIndex.tcl

scanx.o: scanx.c

tclScanx.o: tclScanx.c

libscanx.so: scanx.o tclScanx.o
	$(LD) \
          $(LD_FLAGS) \
          -o $@ \
          $^ \
          $(foreach z,$(LD_LIBRARY_PATHS),-L$z) \
          $(foreach z,$(LD_LIBRARIES),-l$z)

.PHONY: test
test: libscanx.so
	tclsh scanx_test.tcl

pkgIndex.tcl:
	@echo >$@  '# pkgIndex.tcl - scanx'
	@echo >>$@ ''
	@echo >>$@ 'package ifneeded scanx $(VERSION) "[list load [file join $$dir .. libscanx$(VERSION).so]]"'

install: libscanx.so pkgIndex.tcl
	install -d $(INSTALL_PATH)/lib/scanx$(VERSION)
	install libscanx.so $(INSTALL_PATH)/lib/libscanx$(VERSION).so
	install pkgIndex.tcl $(INSTALL_PATH)/lib/scanx$(VERSION)
