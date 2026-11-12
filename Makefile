CC ?= cc
STD = -std=c17
WARN = -Wall -Wextra -Werror -Wswitch-enum -pedantic
SAN ?= -fsanitize=address,undefined
TARGET ?= long

ifeq ($(OS),Windows_NT)
LIB = build/$(TARGET).dll
LIB_LDFLAGS = -Wl,--out-implib,build/lib$(TARGET).dll.a
EXE_LDFLAGS =
PIC =
LDLIBS = -lm
MOD_EXT = .dll
MOD_LDFLAGS =
RAYLIB_LIBS ?= -lraylib -lopengl32 -lgdi32 -lwinmm
else ifeq ($(shell uname -s),Darwin)
LIB = build/lib$(TARGET).dylib
LIB_LDFLAGS = -install_name @rpath/lib$(TARGET).dylib
EXE_LDFLAGS = -Wl,-rpath,@executable_path
PIC = -fPIC
LDLIBS = -lm
MOD_EXT = .so
MOD_LDFLAGS = -Wl,-rpath,@loader_path/..
RAYLIB_LIBS ?= -Wl,-hidden-lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
else
LIB = build/lib$(TARGET).so
LIB_LDFLAGS =
EXE_LDFLAGS = -Wl,-rpath,'$$ORIGIN'
PIC = -fPIC
LDLIBS = -lm -ldl
MOD_EXT = .so
MOD_LDFLAGS = -Wl,-rpath,'$$ORIGIN/..' -Wl,--exclude-libs,ALL
RAYLIB_LIBS ?= -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

RAYLIB_CFLAGS ?=

RELEASE_FLAGS = $(STD) $(WARN) -O3 -flto=auto -fvisibility=hidden $(PIC)
TEST_FLAGS = $(STD) $(WARN) -O0 -g -fno-omit-frame-pointer $(SAN) -DSV_ARENA_POISON
DEBUG_FLAGS = $(STD) $(WARN) -O0 -g -fno-omit-frame-pointer $(SAN) -DSV_ARENA_POISON
MOD_FLAGS = $(STD) $(WARN) -O2 -shared -fvisibility=hidden $(PIC)
DEPFLAGS = -MMD -MP

LIB_SRC = $(wildcard src/*.c) $(wildcard src/obj/*.c) $(wildcard src/deps/*.c) $(wildcard src/modules/*.c)
TEST_SRC = $(wildcard tests/*.c)

LIB_OBJ = $(LIB_SRC:src/%.c=build/release/%.o)
OBJ = $(LIB_OBJ) build/release/main.o
TEST_OBJ = $(LIB_SRC:src/%.c=build/test/%.o) \
       $(patsubst tests/%.c,build/test/tests_%.o,$(TEST_SRC))
DEBUG_OBJ = $(LIB_SRC:src/%.c=build/debug/%.o) build/debug/main.o

BIN = build/$(TARGET)
TEST_BIN = build/$(TARGET)-test
DEBUG_BIN = build/$(TARGET)-debug

.PHONY: build run test debug clean static shared vendor

build: $(BIN)

run: $(BIN)
	./$(BIN) $(ARGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

debug: $(DEBUG_BIN)

static: $(LIB_OBJ)
	rm -f $(TARGET).a
	ar rcs $(TARGET).a $^

shared: $(LIB)

vendor: build/vendor/raylib$(MOD_EXT)

clean:
	rm -rf build

$(LIB): $(LIB_OBJ)
	$(CC) $(RELEASE_FLAGS) -shared $(LIB_LDFLAGS) $^ -o $@ $(LDLIBS)

$(BIN): build/release/main.o $(LIB)
	$(CC) $(RELEASE_FLAGS) $(EXE_LDFLAGS) $< -Lbuild -l$(TARGET) -o $@ $(LDLIBS)

build/vendor/raylib$(MOD_EXT): vendor/raylib/raylib.c src/long.h $(LIB)
	@mkdir -p $(@D)
	$(CC) $(MOD_FLAGS) $(RAYLIB_CFLAGS) -Isrc $< -Lbuild -l$(TARGET) $(MOD_LDFLAGS) $(RAYLIB_LIBS) -o $@

$(TEST_BIN): $(TEST_OBJ)
	$(CC) $(TEST_FLAGS) $^ -o $@ $(LDLIBS)

$(DEBUG_BIN): $(DEBUG_OBJ)
	$(CC) $(DEBUG_FLAGS) $^ -o $@ $(LDLIBS)

build/release/main.o: main.c
	@mkdir -p $(@D)
	$(CC) $(RELEASE_FLAGS) $(DEPFLAGS) -Isrc -c $< -o $@

build/release/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(RELEASE_FLAGS) -DLONG_BUILD $(DEPFLAGS) -c $< -o $@

build/test/tests_%.o: tests/%.c
	@mkdir -p $(@D)
	$(CC) $(TEST_FLAGS) $(DEPFLAGS) -Isrc -c $< -o $@

build/test/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(TEST_FLAGS) -DLONG_BUILD $(DEPFLAGS) -c $< -o $@

build/debug/main.o: main.c
	@mkdir -p $(@D)
	$(CC) $(DEBUG_FLAGS) -DLONG_BUILD $(DEPFLAGS) -Isrc -c $< -o $@

build/debug/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(DEBUG_FLAGS) -DLONG_BUILD $(DEPFLAGS) -c $< -o $@

-include $(OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(DEBUG_OBJ:.o=.d)
