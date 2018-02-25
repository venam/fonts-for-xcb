# Freetype fun project Makefile

CPPFLAGS=
CFLAGS= -Wall -pedantic -g `pkg-config --cflags 'freetype2'` -fstack-protector-all
LDFLAGS=-Wl,-rpath,"freetype fun" --no-leading-underscore #make symbols look funny
LDLIBS= `pkg-config --libs 'freetype2'`

all: main

clean:
	rm -f main

