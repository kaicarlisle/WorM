CFLAGS+= -Wall
LDADD+= -lX11
LDFLAGS=
EXEC=worm

PREFIX?= /usr/local
BINDIR?= $(PREFIX)/bin

CC=gcc

all: $(EXEC)

worm: worm.o
	$(CC) $(LDFLAGS) -s -Os -o $@ $+ $(LDADD)

install: all
	install -Dm 755 worm $(DESTDIR)$(BINDIR)/worm

clean:
	rm -fv worm *.o
