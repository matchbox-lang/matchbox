MKDIR = if not exist "$1" mkdir "$1"
RMDIR = if exist "$1" rmdir /S /Q "$1"

BUILD_DIR := build
INCLUDE_DIR := include
SRC_DIR := src

SRCS := $(sort $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/builtin/*.c $(SRC_DIR)/common/*.c $(SRC_DIR)/compiler/*.c \
    $(SRC_DIR)/cli/*.c $(SRC_DIR)/runtime/*.c $(SRC_DIR)/vm/*.c))
EXE := $(BUILD_DIR)/matchbox.exe
LIBRARY := $(BUILD_DIR)/libmatchbox.a
CC := gcc
AR := gcc-ar
CPPFLAGS += -I$(INCLUDE_DIR) -I$(SRC_DIR)
CFLAGS += -Wall -Wextra
LDFLAGS ?=
LDLIBS += -lm

DEBUG_OBJ_DIR := $(BUILD_DIR)/debug
DEBUG_OBJ_DIRS := $(DEBUG_OBJ_DIR) $(DEBUG_OBJ_DIR)/builtin $(DEBUG_OBJ_DIR)/compiler \
    $(DEBUG_OBJ_DIR)/common $(DEBUG_OBJ_DIR)/cli $(DEBUG_OBJ_DIR)/runtime $(DEBUG_OBJ_DIR)/vm
DEBUG_OBJECTS := $(patsubst $(SRC_DIR)/%.c, $(DEBUG_OBJ_DIR)/%.o, $(SRCS))
DEBUG_DEPS := $(DEBUG_OBJECTS:.o=.d)
DEBUG_CFLAGS := $(CFLAGS) -O0 -g

RELEASE_OBJ_DIR := $(BUILD_DIR)/release
RELEASE_OBJ_DIRS := $(RELEASE_OBJ_DIR) $(RELEASE_OBJ_DIR)/builtin $(RELEASE_OBJ_DIR)/compiler \
    $(RELEASE_OBJ_DIR)/common $(RELEASE_OBJ_DIR)/cli $(RELEASE_OBJ_DIR)/runtime $(RELEASE_OBJ_DIR)/vm
RELEASE_OBJECTS := $(patsubst $(SRC_DIR)/%.c, $(RELEASE_OBJ_DIR)/%.o, $(SRCS))
LIBRARY_SRCS := $(filter-out $(SRC_DIR)/main.c $(SRC_DIR)/cli/%.c,$(SRCS))
DEBUG_LIBRARY_OBJ_DIR := $(BUILD_DIR)/lib-debug
DEBUG_LIBRARY_OBJ_DIRS := $(DEBUG_LIBRARY_OBJ_DIR) $(DEBUG_LIBRARY_OBJ_DIR)/builtin \
    $(DEBUG_LIBRARY_OBJ_DIR)/common $(DEBUG_LIBRARY_OBJ_DIR)/compiler \
    $(DEBUG_LIBRARY_OBJ_DIR)/runtime $(DEBUG_LIBRARY_OBJ_DIR)/vm
DEBUG_LIBRARY_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(DEBUG_LIBRARY_OBJ_DIR)/%.o,$(LIBRARY_SRCS))
DEBUG_LIBRARY_DEPS := $(DEBUG_LIBRARY_OBJECTS:.o=.d)

RELEASE_LIBRARY_OBJ_DIR := $(BUILD_DIR)/lib-release
RELEASE_LIBRARY_OBJ_DIRS := $(RELEASE_LIBRARY_OBJ_DIR) $(RELEASE_LIBRARY_OBJ_DIR)/builtin \
    $(RELEASE_LIBRARY_OBJ_DIR)/common $(RELEASE_LIBRARY_OBJ_DIR)/compiler \
    $(RELEASE_LIBRARY_OBJ_DIR)/runtime $(RELEASE_LIBRARY_OBJ_DIR)/vm
RELEASE_LIBRARY_OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(RELEASE_LIBRARY_OBJ_DIR)/%.o,$(LIBRARY_SRCS))
RELEASE_LIBRARY_DEPS := $(RELEASE_LIBRARY_OBJECTS:.o=.d)
RELEASE_DEPS := $(RELEASE_OBJECTS:.o=.d)
RELEASE_CFLAGS := $(CFLAGS) -Os -ffunction-sections -fdata-sections
RELEASE_LDFLAGS := $(LDFLAGS) -Wl,--gc-sections -s

all: debug

lib: lib-debug

lib-debug: $(DEBUG_LIBRARY_OBJECTS) | $(BUILD_DIR)
	$(AR) rcs $(LIBRARY) $^

lib-release: $(RELEASE_LIBRARY_OBJECTS) | $(BUILD_DIR)
	$(AR) rcs $(LIBRARY) $^

debug: $(DEBUG_OBJECTS) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) $^ -o $(EXE) $(LDLIBS)

release: $(RELEASE_OBJECTS) | $(BUILD_DIR)
	$(CC) $(RELEASE_LDFLAGS) $^ -o $(EXE) $(LDLIBS)

$(DEBUG_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(DEBUG_OBJ_DIRS)
	$(CC) $(CPPFLAGS) $(DEBUG_CFLAGS) -MMD -MP -c $< -o $@

$(RELEASE_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(RELEASE_OBJ_DIRS)
	$(CC) $(CPPFLAGS) $(RELEASE_CFLAGS) -MMD -MP -c $< -o $@

$(DEBUG_LIBRARY_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(DEBUG_LIBRARY_OBJ_DIRS)
	$(CC) $(CPPFLAGS) $(DEBUG_CFLAGS) -MMD -MP -c $< -o $@

$(RELEASE_LIBRARY_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(RELEASE_LIBRARY_OBJ_DIRS)
	$(CC) $(CPPFLAGS) $(RELEASE_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR) $(DEBUG_OBJ_DIRS) $(RELEASE_OBJ_DIRS) $(DEBUG_LIBRARY_OBJ_DIRS) $(RELEASE_LIBRARY_OBJ_DIRS):
	$(call MKDIR,$@)

clean:
	$(call RMDIR,$(BUILD_DIR))

.PHONY: all clean debug release lib lib-debug lib-release

-include $(DEBUG_DEPS) $(RELEASE_DEPS) $(DEBUG_LIBRARY_DEPS) $(RELEASE_LIBRARY_DEPS)
