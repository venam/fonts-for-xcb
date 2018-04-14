
PKGS = freetype2 fontconfig
CPPFLAGS=
CFLAGS= -Wall -pedantic -g `pkg-config --cflags $(PKGS)` -fstack-protector-all
LDFLAGS= `pkg-config --libs $(PKGS)`

all: main

clean:
	rm -f main

