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

.PHONY: all
all:

.PHONY: clean
clean:

.PHONY: help
help:
	@$(ECHO) "Help:"
	@$(ECHO) ""
	@$(ECHO) "  changelogs            - create changelogs "
	@$(ECHO) "  checkout              - checkout"
	@$(ECHO) "  commit                - commit changes to osc; MSG=<commit message>"
	@$(ECHO) "  rebuild               - start rebuild"
	@$(ECHO) "  download VERSION=<n>  - download built packages"

.PHONY: changelog changelogs
changelog changelogs:
	(./changelog.pl --type debian < ../ChangeLog) > $(OSC_PATH)/debian.changelog
	for fileName in $(wildcard $(OSC_PATH)/*.spec); do \
          sed -i '/^%changelog/q1' $$fileName; \
          if test $$? -eq 1; then \
             ./changelog.pl --type spec < ../ChangeLog >> $$fileName; \
          fi; \
        done

.PHONY: checkout
checkout:
	($(OSC) checkout $(OSC_PATH))

.PHONY: commit
commit:
	($(CD) $(OSC_PATH); $(OSC) up)
	($(CD) $(OSC_PATH); $(OSC) commit -m "$(MSG)")
	($(CD) $(OSC_PATH); $(OSC) rebuildpac home\:torsten20\:BAR/BAR)

.PHONY: rebuild
rebuild:
	($(CD) $(OSC_PATH); $(OSC) rebuildpac home\:torsten20\:BAR/BAR)

.PHONY: download
download:
	./download-packages.sh $(VERSION)
