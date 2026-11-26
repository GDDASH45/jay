CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Iinclude -Isrc
LDLIBS += -lm

TARGET := build/jay
SOURCES := $(shell find src -type f -name '*.c' -print)
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
	install -Dm644 TheUltimateJayManual.md $(DESTDIR)/usr/share/doc/jay/TheUltimateJayManual.md
	install -d $(DESTDIR)/usr/share/doc/jay/examples
	cp -a examples/. $(DESTDIR)/usr/share/doc/jay/examples/

clean:
	rm -rf build
