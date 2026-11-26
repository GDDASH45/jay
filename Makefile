CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Iinclude
LDLIBS += -lm

TARGET := build/jay
SOURCES := src/jay.c
OBJECTS := $(patsubst src/%.c,build/%.o,$(SOURCES))

.PHONY: all clean install test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

build/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	@output=`$(TARGET) tests/control_flow.jay`; expected=`printf '12\ncontrol flow: ok\nelse: ok'`; test "$$output" = "$$expected" || { echo 'Jay tests failed'; exit 1; }

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)/usr/bin/jay

clean:
	rm -rf build