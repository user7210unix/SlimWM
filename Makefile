CFLAGS += -std=c99 -Wall -Wextra -pedantic -Wold-style-declaration
CFLAGS += -Wmissing-prototypes -Wno-unused-parameter
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
CC     ?= gcc

all: slimwm

slimwm: slimwm.c slimwm.h config.h Makefile
	$(CC) -O3 $(CFLAGS) -o $@ $< -lX11 $(LDFLAGS)

install: all
	install -Dm755 slimwm $(DESTDIR)$(BINDIR)/slimwm

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/slimwm

clean:
	rm -f slimwm *.o

.PHONY: all install uninstall clean
