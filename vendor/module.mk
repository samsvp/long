# Settings for a vendored module, included first by each vendor/<module>/Makefile.
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
include $(ROOT)/config.mk

# BUILD_DIR is where liblong is, and the module goes in its vendor directory
BUILD_DIR ?= $(ROOT)/build
MOD_OPT ?= -O2
MOD_FLAGS = $(STD) $(WARN) $(MOD_OPT) -shared -fvisibility=hidden $(PIC) -I$(ROOT)/src
MOD_DIR = $(BUILD_DIR)/vendor
LONG_LIB = -L$(BUILD_DIR) -l$(TARGET)
