# ----------------------------------------------------------------------------
#
# $Source$
# $Revision$
# $Author$
# Contents: Makefile for using Open SuSE build server (osc)
# Systems: Unix
#			   
# ----------------------------------------------------------------------------

#--------------------------------- tool chain --------------------------------

#---------------------------------- commands----------------------------------

CD      = cd
CP      = cp
ECHO    = echo
FIND    = find
GREP    = grep
INSTALL = install -c
MV      = mv
RMF     = rm -f
RMRF    = rm -rf

OSC     = osc

#------------------------ specific variables/settings ------------------------

OSC_PATH = home:torsten20:BAR/BAR

#--------------------------------- objects -----------------------------------

#------------------------------ dependencies ---------------------------------

.PHONY: default
default: \
  help

.PHONY: all rebuild changelog
all:

.PHONY: clean
clean:

.PHONY: help
help:
	@$(ECHO) "Help:"
	@$(ECHO) ""
	@$(ECHO) "  changelog  - create debian.changelog "
	@$(ECHO) "  commit     - commit changes to osc; MSG=<commit message>"
	@$(ECHO) "  rebuild    - start rebuild"
	@$(ECHO) "  download   - download built packages"

changelog:
	./changelog.pl < ../ChangeLog > $(OSC_PATH)/debian.changelog

commit:
	($(CD) $(OSC_PATH); $(OSC) up)
	($(CD) $(OSC_PATH); $(OSC) commit -m "$(MSG)")
	($(CD) $(OSC_PATH); $(OSC) rebuildpac home\:torsten20\:BAR/BAR)

rebuild:
	($(CD) $(OSC_PATH); $(OSC) rebuildpac home\:torsten20\:BAR/BAR)

download:
	./download-packages.sh
