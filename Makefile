MKDIR := mkdir -p
RMDIR := rm -rf
BIN := bin
INCLUDE := include
OBJECT := object
SRC := src
SRCS := $(wildcard $(SRC)/*.c)
OBJS := $(patsubst $(SRC)/%.c, $(OBJECT)/%.o, $(SRCS))
EXE := $(BIN)/matchbox
CC := gcc
CFLAGS := -I$(INCLUDE)
LDLIBS := -lm

all: $(EXE)

$(EXE): $(OBJS) | $(BIN)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(OBJECT)/%.o: $(SRC)/%.c | $(OBJECT)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN) $(OBJECT):
	$(MKDIR) $@

clean:
	$(RMDIR) $(BIN) $(OBJECT)

.PHONY: all clean
