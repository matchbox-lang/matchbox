BUILD := build
INCLUDE := include
OBJECT := object
SRC := src
MKDIR = if not exist "$1" mkdir "$1"
RMDIR = if exist "$1" rmdir /S /Q "$1"

SRCS := $(sort $(wildcard $(SRC)/*.c))
OBJS := $(patsubst $(SRC)/%.c, $(OBJECT)/%.o, $(SRCS))
DEPS := $(OBJS:.o=.d)
EXE := $(BUILD)/matchbox.exe
CC := gcc
CPPFLAGS += -I$(INCLUDE)
CFLAGS ?=
LDFLAGS ?=
LDLIBS += -lm

all: $(EXE)

$(EXE): $(OBJS) | $(BUILD)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(OBJECT)/%.o: $(SRC)/%.c | $(OBJECT)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD) $(OBJECT):
	$(call MKDIR,$@)

clean:
	$(call RMDIR,$(BUILD))
	$(call RMDIR,$(OBJECT))

.PHONY: all clean

-include $(DEPS)
