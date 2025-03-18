CFLAGS+= -Wall -Wextra -pedantic
LDADD+= -lX11 -lXext
LDFLAGS=
PREFIX?= /usr
BINDIR?= $(PREFIX)/bin

CC ?= gcc

all: slimwm

slimwm: slimwm.o
	$(CC) $(LDFLAGS) -O3 -o $@ $+ $(LDADD)

slimwm.o: slimwm.c
	$(CC) $(CFLAGS) -c slimwm.c

install: all 
	install -Dm 755 slimwm $(DESTDIR)$(BINDIR)/slimwm

uninstall:
	rm $(BINDIR)/slimwm

clean:
	rm -f slimwm *.o
