# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/barcontrol/Makefile,v $
# $Revision: 1.6 $
# $Author: torsten $
# Contents: Makefile for barcontrol
# Systems: all
#			   
# ----------------------------------------------------------------------------

#------------------------------------ paths ----------------------------------

#--------------------------------- tool chain --------------------------------

JAVA         = /usr/local/bin/java
JAVA_FLAGS   = -enableassertions

JAVAC        = /usr/local/bin/javac
JAVAC_FLAGS  = -g
# -Xlint

JAR          = /usr/bin/jar

SWT_DIR      = /usr/local/swt
SWT_JAR      = $(SWT_DIR)/swt.jar

LAUNCH4J_DIR = @LAUNCH4J_DIR@

CLASSPATH    = classes:$(SWT_JAR)

#---------------------------------- commands----------------------------------

CAT        = cat
CD         = cd
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
UNZIP      = unzip

#------------------------ specific variables/settings ------------------------

#---------------------------------- rules ------------------------------------

classes/%.class:src/%.java
	$(INSTALL) -d classes
	$(JAVAC) $(JAVAC_FLAGS) -d classes -classpath $(CLASSPATH) src/*.java

#--------------------------------- objects -----------------------------------

SOURCES        = src/BARControl.java \
                 src/BARServer.java \
                 \
                 src/TableLayout.java \
                 src/TableLayoutData.java \
                 src/ProgressBar.java \
                 src/Widgets.java \
                 src/Dialogs.java \
                 src/StringParser.java \

OBJECTS        = $(foreach z,$(SOURCES),$(basename $(subst src/,classes/,$z)).class) \

INTERMEDIATE   = barcontrol.jar \

TARGETS        = barcontrol \
                 $(if $(LAUNCH4J),barcontrol.exe,barcontrol.bat) \

#------------------------------ dependencies ---------------------------------

.PHONY: all clean distclean depend
all: $(TARGETS)

clean:
	$(RMF) $(OBJECTS)
	$(RMF) $(INTERMEDIATE)
	$(RMF) classes/*.class
	$(RMF) $(TARGETS)

#	$(RMF) tmp/Images.java

distclean: clean
	$(RMRF) classes
	$(RMRF) tmp

.PHONE: jars
jars: barcontrol.jar barcontrol-linux.jar barcontrol-windows.jar

# create jars
barcontrol.jar: jar.txt $(OBJECTS) images/*.gif
	$(INSTALL) -d tmp/jar
	$(CP) classes/*.class tmp/jar
	$(INSTALL) -d tmp/jar/images
	$(CP) images/*.gif tmp/jar/images
	($(CD) tmp/jar; $(JAR) cmf ../../jar.txt ../../$@ *)
	$(RMRF) tmp/jar

barcontrol-linux.jar: jar.txt $(OBJECTS) images/*.gif jars/swt-3.4-linux.jar
	$(INSTALL) -d tmp/jar
	($(CD) tmp/jar; $(UNZIP) ../../jars/swt-3.4-linux.jar 1>/dev/null; $(RMRF) META-INF)
	$(CP) classes/*.class tmp/jar
	$(INSTALL) -d tmp/jar/images
	$(CP) images/*.gif tmp/jar/images
	($(CD) tmp/jar; $(JAR) cmf ../../jar.txt ../../$@ *)
	$(RMRF) tmp/jar

barcontrol-windows.jar: jar.txt $(OBJECTS) images/*.gif jars/swt-3.4-windows.jar
	$(INSTALL) -d tmp/jar
	($(CD) tmp/jar; $(UNZIP) ../../jars/swt-3.4-windows.jar 1>/dev/null; $(RMRF) META-INF)
	$(CP) classes/*.class tmp/jar
	$(INSTALL) -d tmp/jar/images
	$(CP) images/*.gif tmp/jar/images
	($(CD) tmp/jar; $(JAR) cmf ../../jar.txt ../../$@ *)
	$(RMRF) tmp/jar

# create Unix start script
barcontrol: barcontrol-linux.jar
	@$(ECHO) >$@  "#!/bin/sh"
	@$(ECHO) >>$@ ""
	@$(ECHO) >>$@ "java -jar barcontrol-linux.jar \"$$@\""
	@$(CHMOD) 775 $@

ifneq ($(LAUNCH4J),)
# create Windows executable
barcontrol.exe: barcontrol-windows.jar
	$(LAUNCH4J) barcontrol.xml
else
# create Windows start script
barcontrol.bat: barcontrol-windows.jar
	@$(ECHO) >>$@ "java -classpath swt.jar\;barcontrol.jar BARControl %*"
	@$(CHMOD) 775 $@
endif

# run
run: $(OBJECTS)
	$(JAVA) $(JAVA_FLAGS) -classpath $(CLASSPATH) BARControl

runjar: barcontrol
	./barcontrol

# end of file
