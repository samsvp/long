include ../module.mk

ifeq ($(PLATFORM),windows)
RAYLIB_LIBS ?= -lraylib -lopengl32 -lgdi32 -lwinmm
else ifeq ($(PLATFORM),darwin)
RAYLIB_LIBS ?= -Wl,-hidden-lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
else
RAYLIB_LIBS ?= -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

$(MOD_DIR)/raylib$(MOD_EXT): raylib.c $(ROOT)/src/long.h
	@mkdir -p $(@D)
	$(CC) $(MOD_FLAGS) $(RAYLIB_CFLAGS) $< $(LONG_LIB) $(MOD_LDFLAGS) $(RAYLIB_LIBS) -o $@
