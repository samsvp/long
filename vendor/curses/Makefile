include ../module.mk

ifeq ($(PLATFORM),windows)
CURSES_LIBS ?= -lpdcurses
else
CURSES_LIBS ?= -lncursesw
endif

$(MOD_DIR)/curses$(MOD_EXT): curses.c keys.h $(ROOT)/src/long.h
	@mkdir -p $(@D)
	$(CC) $(MOD_FLAGS) $(CURSES_CFLAGS) $< $(LONG_LIB) $(MOD_LDFLAGS) $(CURSES_LIBS) -o $@
