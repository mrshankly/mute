.DEFAULT_GOAL = all

BIN := mute
OBJ := build/mute.o
DEP := build/mute.dep

CC      = cc
CFLAGS  = -std=c11 -pedantic -Wall -Wextra -Wno-unused-parameter -MMD -MP -MF build/$*.dep
LDFLAGS = -lpulse

.SUFFIXES:
.SECONDARY:
.PHONY: all debug clean

all: CFLAGS += -O3
all: LDFLAGS += -s
all: $(BIN)

debug: CFLAGS += -O0 -g
debug: LDFLAGS += -g
debug: $(BIN)

clean:
	rm -f $(BIN)
	rm -rf build

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

build/%.o: %.c build/sentinel
	$(CC) -c $(CFLAGS) -o $@ $<

%/sentinel:
	@mkdir -p $(@D)
	@touch $@

-include $(DEP)
