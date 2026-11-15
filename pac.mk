# Shared by the Makefiles of the pac tools:   include $(PACOMMON)/pac.mk
#
# make install puts what a tool needs to work out of the box where it looks for it: the program,
# and the configuration and assets it starts with. A file that is already there is the person's
# (they may have edited it): it is kept, with a warning. `make install REPLACE=1` replaces it on
# purpose, and saves the old one next to it as NAME.bak.DATE, so that nothing is lost.

REPLACE ?= 0
SYSCONFDIR ?= /etc

# $(call pac_install_keep,SOURCE,DESTINATION,MODE)
# Installs SOURCE as DESTINATION unless DESTINATION exists. A file that is the same is left
# alone, quietly; one that differs is kept with a warning, or replaced if REPLACE=1.
define pac_install_keep
@if [ -e "$(2)" ] && [ "$(REPLACE)" != 1 ]; then \
	if ! cmp -s "$(1)" "$(2)"; then \
		echo "warning: $(2) already exists and is different, so it was kept." >&2; \
		echo "         To replace it with the new one, run: make install REPLACE=1 (the old one is saved as $(2).bak.DATE)" >&2; \
	fi; \
else \
	if [ -e "$(2)" ] && ! cmp -s "$(1)" "$(2)"; then \
		backup="$(2).bak.`date +%Y%m%d-%H%M%S`"; \
		cp -p "$(2)" "$$backup" && echo "replaced $(2); the old one is saved as $$backup"; \
	fi; \
	install -Dm$(3) "$(1)" "$(2)"; \
fi
endef

# $(call pac_uninstall_keep,SOURCE,DESTINATION)
# Removes DESTINATION only if it is still the file that was installed: one the person has edited
# is theirs, and stays.
define pac_uninstall_keep
@if [ -e "$(2)" ]; then \
	if cmp -s "$(1)" "$(2)"; then rm -f "$(2)"; \
	else echo "warning: $(2) was changed, so it was kept." >&2; fi; \
fi
endef

# The settings every pac tool shares (pac.conf): installed by each tool that reads it, and kept
# when it is already there, so that installing a second tool does not touch it.
define pac_install_shared_conf
$(call pac_install_keep,$(PACOMMON)/pac.conf,$(DESTDIR)$(SYSCONFDIR)/pac/pac.conf,644)
endef

# Speech (speech.c): SPEECH=speechd (default) speaks through speech-dispatcher, SPEECH=none links no
# speech library. A tool adds $(PAC_SPEECH_CFLAGS) to its compiler flags (every file, speech.c too)
# and $(PAC_SPEECH_PKGS) to the packages it asks pkg-config for.
SPEECH ?= speechd
ifeq ($(filter $(SPEECH),none speechd),)
$(error SPEECH=$(SPEECH) is not supported: use none or speechd)
endif
PAC_SPEECH_CFLAGS = $(if $(filter speechd,$(SPEECH)),-DSPEECH_SPEECHD)
PAC_SPEECH_PKGS = $(if $(filter speechd,$(SPEECH)),speech-dispatcher)
