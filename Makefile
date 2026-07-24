MKDIR = if not exist "$1" mkdir "$1"
RMDIR = if exist "$1" rmdir /S /Q "$1"

BUILD := build
INCLUDE := include
OBJECT := object
SRC := src

SRCS := $(sort $(wildcard $(SRC)/*.c))
EXE := $(BUILD)/matchbox.exe
CC := gcc
CPPFLAGS += -I$(INCLUDE)
CFLAGS += -Wall -Wextra
LDFLAGS ?=
LDLIBS += -lm

DEBUG_OBJECT := $(OBJECT)/debug
DEBUG_OBJS := $(patsubst $(SRC)/%.c, $(DEBUG_OBJECT)/%.o, $(SRCS))
DEBUG_DEPS := $(DEBUG_OBJS:.o=.d)
DEBUG_CFLAGS := $(CFLAGS) -O0 -g

RELEASE_OBJECT := $(OBJECT)/release
RELEASE_OBJS := $(patsubst $(SRC)/%.c, $(RELEASE_OBJECT)/%.o, $(SRCS))
RELEASE_DEPS := $(RELEASE_OBJS:.o=.d)
RELEASE_CFLAGS := $(CFLAGS) -O2 -ffunction-sections -fdata-sections -flto
RELEASE_LDFLAGS := $(LDFLAGS) -flto -Wl,--gc-sections -s

all: release

debug: $(DEBUG_OBJS) | $(BUILD)
	$(CC) $(LDFLAGS) $^ -o $(EXE) $(LDLIBS)

release: $(RELEASE_OBJS) | $(BUILD)
	$(CC) $(RELEASE_LDFLAGS) $^ -o $(EXE) $(LDLIBS)

$(DEBUG_OBJECT)/%.o: $(SRC)/%.c | $(DEBUG_OBJECT)
	$(CC) $(CPPFLAGS) $(DEBUG_CFLAGS) -MMD -MP -c $< -o $@

$(RELEASE_OBJECT)/%.o: $(SRC)/%.c | $(RELEASE_OBJECT)
	$(CC) $(CPPFLAGS) $(RELEASE_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD) $(DEBUG_OBJECT) $(RELEASE_OBJECT):
	$(call MKDIR,$@)

clean:
	$(call RMDIR,$(BUILD))
	$(call RMDIR,$(OBJECT))

.PHONY: all clean debug release

-include $(DEBUG_DEPS) $(RELEASE_DEPS)
