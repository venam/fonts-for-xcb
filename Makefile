CC= cc
CFLAGS= -Wall -pedantic
LDLIBS= -lfontconfig
PREFIX=

all: font_test

#_font_test: _font_compile _font_link
#
#_font_compile:
#	$(CC) $(CFLAGS) -c -o font_test.o font_test.c
#
#_font_link:
#	$(CC) $(LDFLAGS) $(LDLIBS) -o font_test font_test.o

clean:
	rm -f font_test font_test.o 2> /dev/null
