# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/barcontrol/Makefile,v $
# $Revision: 1.4 $
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

classes/%.class:%.java Images.java
	$(INSTALL) -d classes
	$(JAVAC) $(JAVAC_FLAGS) -d classes -classpath $(CLASSPATH) *.java

#--------------------------------- objects -----------------------------------

SOURCES        = BARControl.java \
                 StringParser.java \
                 TableLayout.java \
                 TableLayoutData.java \
                 \
                 ImagesToClass.java \

OBJECTS        = classes/BARControl.class \
                 classes/StringParser.class \
                 classes/TableLayout.class \
                 classes/TableLayoutData.class \
                 \
                 classes/ImagesToClass.class \
                 \
                 classes/Images.class \

TARGETS        = barcontrol \

#------------------------------ dependencies ---------------------------------

.PHONY: all clean distclean depend
all: $(TARGETS)

clean:
	$(RMF) $(OBJECTS)
	$(RMF) Images.java
	$(RMF) classes/*.class

distclean: clean

# create images class
Images.java: ImagesToClass.java $(wildcard images/*.gif)
	$(JAVAC) $(JAVAC_FLAGS) -d classes -classpath $(CLASSPATH) ImagesToClass.java
	$(JAVA) $(JAVA_FLAGS) -classpath $(CLASSPATH) ImagesToClass $(wildcard images/*.gif) > $@

# create BARControl
barcontrol: $(OBJECTS)

# run
run: $(OBJECTS)
	$(JAVA) $(JAVA_FLAGS) -classpath $(CLASSPATH) BARControl

# end of file
