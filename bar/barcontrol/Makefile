# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/barcontrol/Makefile,v $
# $Revision: 1.2 $
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
JAVAC_FLAGS = -g

CLASSPATH = classes:/usr/local/swt/swt.jar

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

classes/%.class:%.java
	$(INSTALL) -d classes
	$(JAVAC) $(JAVAC_FLAGS) -d classes -classpath $(CLASSPATH) *.java

#--------------------------------- objects -----------------------------------

SOURCES        = BARControl.java \
                 StringParser.java \

OBJECTS        = classes/BARControl.class \
                 classes/StringParser.class \

TARGETS        = barcontrol \

#------------------------------ dependencies ---------------------------------

.PHONY: all clean distclean depend
all: $(TARGETS)

clean:
	$(RMF) $(OBJECTS)
	$(RMF) classes/*.class

distclean: clean

# create BARControl
barcontrol: $(OBJECTS)

# run
run: $(OBJECTS)
	$(JAVA) $(JAVA_FLAGS) -classpath $(CLASSPATH) BARControl

# end of file
