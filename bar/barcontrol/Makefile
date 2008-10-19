# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/barcontrol/Makefile,v $
# $Revision: 1.1 $
# $Author: torsten $
# Contents: Makefile for bar
# Systems: all
#			   
# ----------------------------------------------------------------------------

#------------------------------------ paths ----------------------------------

#--------------------------------- tool chain --------------------------------

JAVA        = java
JAVA_FLAGS  = -enableassertions

JAVAC       = javac
JAVAC_FLAGS =

CLASSPATH = .:/usr/local/swt/swt.jar

#---------------------------------- commands----------------------------------

CAT        = cat
CHMOD      = chmod
CP         = cp
DIFF       = diff
ECHO       = echo
ECHO_NO_LF = echo -n
INSTALL    = /usr/bin/install -c
MKDIR      = mkdir
PERL       = /usr/bin/perl
RMDIR      = rmdir
RMF        = rm -f
RMRF       = rm -rf
TAR        = tar

#------------------------ specific variables/settings ------------------------

#---------------------------------- rules ------------------------------------

%.class:%.java
	$(JAVAC) $(JAVAC_FLAGS) -classpath $(CLASSPATH) -d . $*.java

#--------------------------------- objects -----------------------------------

SOURCES        = BARControl.java \

OBJECTS        = BARControl.class

TARGETS        = barcontrol \

#------------------------------ dependencies ---------------------------------

.PHONY: all clean distclean depend
all: $(TARGETS)

clean:
	$(RMF) $(OBJECTS)

distclean: clean

# create BARControl
barcontrol: $(OBJECTS)

# run
run: BARControl.class
	$(JAVA) $(JAVA_FLAGS) -classpath $(CLASSPATH) BARControl

# end of file
