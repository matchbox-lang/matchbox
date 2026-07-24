MKDIR = if not exist "$1" mkdir "$1"
RMDIR = if exist "$1" rmdir /S /Q "$1"

BUILD_DIR := build
INCLUDE_DIR := include
SRC_DIR := src

SRCS := $(sort $(wildcard $(SRC_DIR)/*.c))
EXE := $(BUILD_DIR)/matchbox.exe
CC := gcc
CPPFLAGS += -I$(INCLUDE_DIR)
CFLAGS += -Wall -Wextra
LDFLAGS ?=
LDLIBS += -lm

DEBUG_OBJ_DIR := $(BUILD_DIR)/debug
DEBUG_OBJECTS := $(patsubst $(SRC_DIR)/%.c, $(DEBUG_OBJ_DIR)/%.o, $(SRCS))
DEBUG_DEPS := $(DEBUG_OBJECTS:.o=.d)
DEBUG_CFLAGS := $(CFLAGS) -O0 -g

RELEASE_OBJ_DIR := $(BUILD_DIR)/release
RELEASE_OBJECTS := $(patsubst $(SRC_DIR)/%.c, $(RELEASE_OBJ_DIR)/%.o, $(SRCS))
RELEASE_DEPS := $(RELEASE_OBJECTS:.o=.d)
RELEASE_CFLAGS := $(CFLAGS) -O2 -ffunction-sections -fdata-sections -flto
RELEASE_LDFLAGS := $(LDFLAGS) -flto -Wl,--gc-sections -s

all: debug

debug: $(DEBUG_OBJECTS) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) $^ -o $(EXE) $(LDLIBS)

release: $(RELEASE_OBJECTS) | $(BUILD_DIR)
	$(CC) $(RELEASE_LDFLAGS) $^ -o $(EXE) $(LDLIBS)

$(DEBUG_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(DEBUG_OBJ_DIR)
	$(CC) $(CPPFLAGS) $(DEBUG_CFLAGS) -MMD -MP -c $< -o $@

$(RELEASE_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(RELEASE_OBJ_DIR)
	$(CC) $(CPPFLAGS) $(RELEASE_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR) $(DEBUG_OBJ_DIR) $(RELEASE_OBJ_DIR):
	$(call MKDIR,$@)

clean:
	$(call RMDIR,$(BUILD_DIR))

.PHONY: all clean debug release

-include $(DEBUG_DEPS) $(RELEASE_DEPS)
