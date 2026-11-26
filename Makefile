# free — macOS için Linux free komutu / Linux free command for macOS

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -Wpedantic -std=c11
PREFIX  ?= /usr/local
BINDIR  := $(PREFIX)/bin
MANDIR  := $(PREFIX)/share/man/man1

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

clean:
	rm -f free

.PHONY: all test install uninstall clean
