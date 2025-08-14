MKDIR := mkdir -p
RMDIR := rm -rf
BUILD := build
INCLUDE := include
OBJECT := object
SRC := src
SRCS := $(wildcard $(SRC)/*.c)
OBJS := $(patsubst $(SRC)/%.c, $(OBJECT)/%.o, $(SRCS))
EXE := $(BUILD)/matchbox
CC := gcc
CFLAGS := -I$(INCLUDE)
LDLIBS := -lm

all: $(EXE)

$(EXE): $(OBJS) | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(OBJECT)/%.o: $(SRC)/%.c | $(OBJECT)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD) $(OBJECT):
	$(MKDIR) $@

clean:
	$(RMDIR) $(BUILD) $(OBJECT)

.PHONY: all clean
