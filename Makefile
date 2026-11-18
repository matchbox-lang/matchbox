# Toolchain and flags
CC := gcc
AR := gcc-ar

CPPFLAGS += -I$(INCLUDE_DIR) -I$(SRC_DIR)
CFLAGS += -Wall -Wextra
LDFLAGS ?=
LDLIBS += -lm

# Directories and cleanup
ifeq ($(OS),Windows_NT)
MKDIR = if not exist "$1" mkdir "$1"
RMDIR = if exist "$1" rmdir /S /Q "$1"
else
MKDIR = mkdir -p "$1"
RMDIR = rm -rf "$1"
endif

# Paths and sources
BUILD_DIR := build
INCLUDE_DIR := include
SRC_DIR := src
EXE := $(BUILD_DIR)/matchbox.exe
LIBRARY := $(BUILD_DIR)/libmatchbox.a

# Recursive source discovery
SOURCE_ENTRIES = $(wildcard $1/*)
FIND_SRCS = $(wildcard $1/*.c) $(foreach source_path,$(SOURCE_ENTRIES),$(call FIND_SRCS,$(source_path)))

SRCS := $(call FIND_SRCS,$(SRC_DIR))
SRCS := $(sort $(SRCS))
LIBRARY_SRCS := $(filter-out $(SRC_DIR)/main.c $(SRC_DIR)/cli/%.c,$(SRCS))

# Build options
DEBUG_CFLAGS := $(CFLAGS) -O0 -g
RELEASE_CFLAGS := $(CFLAGS) -Os -flto -ffunction-sections -fdata-sections
RELEASE_LDFLAGS := $(LDFLAGS) -Os -flto -Wl,--gc-sections -s

DEBUG_OBJ_DIR := $(BUILD_DIR)/debug
DEBUG_OBJECTS := $(patsubst $(SRC_DIR)/%.c, $(DEBUG_OBJ_DIR)/%.o, $(SRCS))
DEBUG_OBJ_DIRS := $(dir $(DEBUG_OBJECTS))
DEBUG_DEPS := $(DEBUG_OBJECTS:.o=.d)

RELEASE_OBJ_DIR := $(BUILD_DIR)/release
RELEASE_OBJECTS := $(patsubst $(SRC_DIR)/%.c, $(RELEASE_OBJ_DIR)/%.o, $(SRCS))
RELEASE_OBJ_DIRS := $(dir $(RELEASE_OBJECTS))
RELEASE_DEPS := $(RELEASE_OBJECTS:.o=.d)

DEBUG_LIBRARY_OBJ_DIR := $(BUILD_DIR)/lib-debug
DEBUG_LIBRARY_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(DEBUG_LIBRARY_OBJ_DIR)/%.o,$(LIBRARY_SRCS))
DEBUG_LIBRARY_OBJ_DIRS := $(dir $(DEBUG_LIBRARY_OBJECTS))
DEBUG_LIBRARY_DEPS := $(DEBUG_LIBRARY_OBJECTS:.o=.d)

RELEASE_LIBRARY_OBJ_DIR := $(BUILD_DIR)/lib-release
RELEASE_LIBRARY_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(RELEASE_LIBRARY_OBJ_DIR)/%.o,$(LIBRARY_SRCS))
RELEASE_LIBRARY_OBJ_DIRS := $(dir $(RELEASE_LIBRARY_OBJECTS))
RELEASE_LIBRARY_DEPS := $(RELEASE_LIBRARY_OBJECTS:.o=.d)

OBJ_DIRS := $(DEBUG_OBJ_DIRS) $(RELEASE_OBJ_DIRS) $(DEBUG_LIBRARY_OBJ_DIRS) $(RELEASE_LIBRARY_OBJ_DIRS)
OBJ_DIRS := $(sort $(OBJ_DIRS))

# Build targets
all: debug

lib: lib-debug

debug: $(DEBUG_OBJECTS) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) $^ -o $(EXE) $(LDLIBS)

release: $(RELEASE_OBJECTS) | $(BUILD_DIR)
	$(CC) $(RELEASE_LDFLAGS) $^ -o $(EXE) $(LDLIBS)

lib-debug: $(DEBUG_LIBRARY_OBJECTS) | $(BUILD_DIR)
	$(AR) rcs $(LIBRARY) $^

lib-release: $(RELEASE_LIBRARY_OBJECTS) | $(BUILD_DIR)
	$(AR) rcs $(LIBRARY) $^

# Object compilation
$(DEBUG_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(DEBUG_OBJ_DIRS)
	$(CC) $(CPPFLAGS) $(DEBUG_CFLAGS) -MMD -MP -c $< -o $@

$(RELEASE_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(RELEASE_OBJ_DIRS)
	$(CC) $(CPPFLAGS) $(RELEASE_CFLAGS) -MMD -MP -c $< -o $@

$(DEBUG_LIBRARY_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(DEBUG_LIBRARY_OBJ_DIRS)
	$(CC) $(CPPFLAGS) $(DEBUG_CFLAGS) -MMD -MP -c $< -o $@

$(RELEASE_LIBRARY_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(RELEASE_LIBRARY_OBJ_DIRS)
	$(CC) $(CPPFLAGS) $(RELEASE_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR) $(OBJ_DIRS):
	$(call MKDIR,$@)

clean:
	$(call RMDIR,$(BUILD_DIR))

.PHONY: all clean debug release lib lib-debug lib-release

-include $(DEBUG_DEPS) $(RELEASE_DEPS) $(DEBUG_LIBRARY_DEPS) $(RELEASE_LIBRARY_DEPS)
