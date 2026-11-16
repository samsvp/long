include config.mk

SAN ?= -fsanitize=address,undefined

RELEASE_FLAGS = $(STD) $(WARN) -O3 -flto=auto -fvisibility=hidden $(PIC)
TEST_FLAGS = $(STD) $(WARN) -O0 -g -fno-omit-frame-pointer $(SAN) -DSV_ARENA_POISON
DEBUG_FLAGS = $(TEST_FLAGS) -fvisibility=hidden $(PIC)
DEPFLAGS = -MMD -MP

LIB_SRC = $(wildcard src/*.c) $(wildcard src/obj/*.c) $(wildcard src/deps/*.c) $(wildcard src/modules/*.c)
TEST_SRC = $(wildcard tests/*.c)

LIB_OBJ = $(LIB_SRC:src/%.c=build/release/%.o)
OBJ = $(LIB_OBJ) build/release/main.o
TEST_OBJ = $(LIB_SRC:src/%.c=build/test/%.o) \
       $(patsubst tests/%.c,build/test/tests_%.o,$(TEST_SRC))
DEBUG_LIB_OBJ = $(LIB_SRC:src/%.c=build/debug/%.o)
DEBUG_OBJ = $(DEBUG_LIB_OBJ) build/debug/main.o

LIB = build/$(LIB_FILE)
BIN = build/$(TARGET)
TEST_BIN = build/$(TARGET)-test
DEBUG_LIB = build/debug/$(LIB_FILE)
DEBUG_BIN = build/debug/$(TARGET)
VENDOR = $(patsubst vendor/%/Makefile,vendor-%,$(wildcard vendor/*/Makefile))
VENDOR_DEBUG = $(VENDOR:%=%-debug)

.PHONY: build run test debug clean static shared vendor vendor-debug $(VENDOR) $(VENDOR_DEBUG)

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

vendor: $(VENDOR)

$(VENDOR): vendor-%: $(LIB)
	$(MAKE) --no-print-directory -C vendor/$*

vendor-debug: $(VENDOR_DEBUG)

$(VENDOR_DEBUG): vendor-%-debug: $(DEBUG_LIB)
	$(MAKE) --no-print-directory -C vendor/$* BUILD_DIR=$(CURDIR)/build/debug MOD_OPT="-O0 -g"

clean:
	rm -rf build

$(LIB): $(LIB_OBJ)
	$(CC) $(RELEASE_FLAGS) -shared $(LIB_LDFLAGS) $^ -o $@ $(LDLIBS)

$(BIN): build/release/main.o $(LIB)
	$(CC) $(RELEASE_FLAGS) $(EXE_LDFLAGS) $< -Lbuild -l$(TARGET) -o $@ $(LDLIBS)

$(TEST_BIN): $(TEST_OBJ)
	$(CC) $(TEST_FLAGS) $^ -o $@ $(LDLIBS)

$(DEBUG_LIB): $(DEBUG_LIB_OBJ)
	$(CC) $(DEBUG_FLAGS) -shared $(LIB_LDFLAGS) $^ -o $@ $(LDLIBS)

$(DEBUG_BIN): build/debug/main.o $(DEBUG_LIB)
	$(CC) $(DEBUG_FLAGS) $(EXE_LDFLAGS) $< -Lbuild/debug -l$(TARGET) -o $@ $(LDLIBS)

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
	$(CC) $(DEBUG_FLAGS) $(DEPFLAGS) -Isrc -c $< -o $@

build/debug/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(DEBUG_FLAGS) -DLONG_BUILD $(DEPFLAGS) -c $< -o $@

-include $(OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(DEBUG_OBJ:.o=.d)
