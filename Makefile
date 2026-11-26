# free — macOS için Linux free komutu / Linux free command for macOS

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -Wpedantic -std=c11
PREFIX  ?= /usr/local
BINDIR  := $(PREFIX)/bin
MANDIR  := $(PREFIX)/share/man/man1
VERSION := $(shell sed -n 's/^\#define FREE_VERSION "\(.*\)"/\1/p' free.c)
DIST    := free-$(VERSION)-macos-universal
# Hem Apple Silicon hem Intel için tek binary / One binary for both Apple Silicon and Intel
RELEASE_FLAGS := -arch arm64 -arch x86_64 -mmacosx-version-min=11.0

all: free

free: free.c
	$(CC) $(CFLAGS) -o $@ $<

test: free
	./tests/run_tests.sh ./free

install: free
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)
	install -m 0755 free $(DESTDIR)$(BINDIR)/free
	install -m 0644 free.1 $(DESTDIR)$(MANDIR)/free.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/free $(DESTDIR)$(MANDIR)/free.1

# GitHub release'e yüklenecek arşiv ve SHA-256 özeti
# Archive and SHA-256 checksum to attach to a GitHub release
release: free.c free.1 README.md LICENSE
	rm -rf dist && mkdir -p dist/$(DIST)
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) -o dist/$(DIST)/free free.c
	cp free.1 README.md LICENSE dist/$(DIST)/
	COPYFILE_DISABLE=1 tar --no-mac-metadata --uid 0 --gid 0 --uname root --gname wheel -C dist -czf dist/$(DIST).tar.gz $(DIST)
	cd dist && shasum -a 256 $(DIST).tar.gz > $(DIST).tar.gz.sha256

clean:
	rm -rf free dist

.PHONY: all test install uninstall release clean
