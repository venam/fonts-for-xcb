
PKGS = freetype2 fontconfig
CPPFLAGS=
CFLAGS= -Wall -pedantic -g `pkg-config --cflags $(PKGS)` -fstack-protector-all
#LDFLAGS=-Wl,--no-as-needed
LDLIBS= `pkg-config --libs $(PKGS)`

all: main

clean:
	rm -f main

