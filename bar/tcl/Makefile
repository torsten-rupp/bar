TCL_PATH    = /tools/tk-8.4a1
TCL_LIB     = 
#TCL_LIB     = -ltcl8.4
TCL_INCLUDE = -I$(TCL_PATH)/include -I$(TCL_PATH)/generic

CC                  = gcc
CC_FLAGS            = -Wall -g -O0
CC_DEFINES          = -DHAVE_UNISTD_H=1 -DHAVE_LIMITS_H=1 -DHAVE_GETCWD=1 -DHAVE_OPENDIR=1 -DHAVE_STRSTR=1 -DHAVE_STRTOL=1 -DHAVE_TMPNAM=1 -DHAVE_WAITPID=1 -DHAVE_UNISTD_H=1 -DHAVE_SYS_PARAM_H=1 -DUSE_TERMIOS=1 -DHAVE_SYS_TIME_H=1 -DTIME_WITH_SYS_TIME=1 -DHAVE_TZNAME=1 -DHAVE_TIMEZONE_VAR=1 -DHAVE_ST_BLKSIZE=1 -DSTDC_HEADERS=1 -DNO_UNION_WAIT=1 -DNEED_MATHERR=1 -DHAVE_SIGNED_CHAR=1 -DHAVE_SYS_IOCTL_H=1 -DHAVE_SYS_FILIO_H=1  -fPIC $(TCL_INCLUDE)
CC_INCLUDES         =

LD                  = ld
LD_FLAGS            = -G -z text
LD_LIBRARY_PATHS    = /usr/lib /usr/local/lib
LD_LIBRARIES        = 
#tcl8.4 dl nsl m
LD_STATIC_LIBRARIES = 

%.o:%.c
	$(CC) $(CC_FLAGS) $(CC_DEFINES) $(for z,$(CC_INCLUDES),-I$z) -c $*.c -o $*.o

.PHONY: all clean
all: scanx.so

clean:
	rm -f scanx.so scanx.o tclScanx.o

scanx.o: scanx.c

tclScanx.o: tclScanx.c

scanx.so: scanx.o tclScanx.o
	$(LD) \
          $(LD_FLAGS) \
          -o $@ \
          $^ \
          $(foreach z,$(LD_LIBRARY_PATHS),-L$z) \
          $(foreach z,$(LD_LIBRARIES),-l$z)

.PHONY: test
test: scanx.so
	tclsh scanx_test.tcl

install:
	install -d $(TCL_PATH)/lib/scanx
	install scanx.so $(TCL_PATH)/lib/scanx
	install pkgIndex.tcl $(TCL_PATH)/lib/scanx
