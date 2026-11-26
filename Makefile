CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Iinclude -Isrc -Ijaystdlib
LDLIBS += -lm

TARGET := build/jay
SOURCES := $(shell find src jaystdlib -type f -name '*.c' -print)
OBJECTS := $(patsubst %.c,build/%.o,$(SOURCES))

.PHONY: all clean install test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	@$(TARGET) --compile tests/stdlib.jay -o build/stdlib.jbc
	@output=`$(TARGET) build/stdlib.jbc`; expected=`printf 'Jay stdlib\\nvalue: 7\\nJay\\n42'`; test "$output" = "$expected" || { echo 'Jay bytecode tests failed'; exit 1; }
	@output=`JAY_MODULE_DIR=tests/modules $(TARGET) tests/umport.jay 2>build/umport.err`; status=$?; test "$output" = "73" && test "$status" -ne 0 && grep -q "undefined function 'module_value'" build/umport.err || { echo 'Jay umport tests failed'; cat build/umport.err; exit 1; }
	@output=`JAY_MODULE_DIR=tests/modules $(TARGET) tests/import.jay`; expected=`printf 'module says:73'`; test "$output" = "$expected" || { echo 'Jay import tests failed'; exit 1; }
	@output=`$(TARGET) tests/control_flow.jay`; expected=`printf '12\ncontrol flow: ok\nelse: ok'`; test "$$output" = "$$expected" || { echo 'Jay tests failed'; exit 1; }
	@output=`$(TARGET) tests/stdlib.jay`; expected=`printf 'Jay stdlib\nvalue: 7\nJay\n42'`; test "$$output" = "$$expected" || { echo 'Jay standard-library tests failed'; exit 1; }

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)/usr/bin/jay
	install -Dm644 TheUltimateJayManual.md $(DESTDIR)/usr/share/doc/jay/TheUltimateJayManual.md
	install -Dm644 jaystdlib/README.md $(DESTDIR)/usr/share/doc/jay/jaystdlib.md
	install -d $(DESTDIR)/lib/jay/modules
	@if [ -d modules ]; then cp -a modules/. "$(DESTDIR)/lib/jay/modules/"; fi
	install -d $(DESTDIR)/usr/share/doc/jay/examples
	cp -a examples/. $(DESTDIR)/usr/share/doc/jay/examples/

clean:
	rm -rf build

.PHONY: debpkg

PACKAGE := jay
VERSION := 0.8.0
ARCH := $(shell dpkg --print-architecture)
DEB_DIR := ../debian

debpkg: $(TARGET)
	rm -rf $(DEB_DIR)
	mkdir -p $(DEB_DIR)/DEBIAN
	install -Dm755 $(TARGET) $(DEB_DIR)/usr/bin/jay
	install -Dm644 LICENSE $(DEB_DIR)/usr/share/doc/jay/copyright
	install -Dm644 TheUltimateJayManual.md $(DEB_DIR)/usr/share/doc/jay/TheUltimateJayManual.md
	install -Dm644 jaystdlib/README.md $(DEB_DIR)/usr/share/doc/jay/jaystdlib.md
	install -d $(DEB_DIR)/lib/jay/modules
	@if [ -d modules ]; then cp -a modules/. "$(DEB_DIR)/lib/jay/modules/"; fi
	mkdir -p $(DEB_DIR)/usr/share/doc/jay/examples
	cp -a examples/. $(DEB_DIR)/usr/share/doc/jay/examples/
	printf '%s\n' \
		'Package: $(PACKAGE)' \
		'Version: $(VERSION)' \
		'Section: devel' \
		'Priority: optional' \
		'Architecture: $(ARCH)' \
		'Maintainer: Jay Developers' \
		'Depends: libc6' \
		'Description: Jay interpreted programming language' \
		' An interpreted programming language written in C.' \
		> $(DEB_DIR)/DEBIAN/control
	dpkg-deb --root-owner-group --build $(DEB_DIR) ../$(PACKAGE)_$(VERSION)_$(ARCH).deb