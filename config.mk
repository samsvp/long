# Compiler and platform settings, shared by the root Makefile and vendor/module.mk.
CC ?= cc
STD = -std=c17
WARN = -Wall -Wextra -Werror -Wswitch-enum -pedantic
TARGET ?= long

ifeq ($(OS),Windows_NT)
PLATFORM = windows
LIB_FILE = $(TARGET).dll
LIB_LDFLAGS = -Wl,--out-implib,$(@D)/lib$(TARGET).dll.a
EXE_LDFLAGS =
PIC =
LDLIBS = -lm
MOD_EXT = .dll
MOD_LDFLAGS =
else ifeq ($(shell uname -s),Darwin)
PLATFORM = darwin
LIB_FILE = lib$(TARGET).dylib
LIB_LDFLAGS = -install_name @rpath/lib$(TARGET).dylib
EXE_LDFLAGS = -Wl,-rpath,@executable_path
PIC = -fPIC
LDLIBS = -lm
MOD_EXT = .so
MOD_LDFLAGS = -Wl,-rpath,@loader_path/..
else
PLATFORM = linux
LIB_FILE = lib$(TARGET).so
LIB_LDFLAGS =
EXE_LDFLAGS = -Wl,-rpath,'$$ORIGIN'
PIC = -fPIC
LDLIBS = -lm -ldl
MOD_EXT = .so
MOD_LDFLAGS = -Wl,-rpath,'$$ORIGIN/..' -Wl,--exclude-libs,ALL
endif
