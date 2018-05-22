PKGS = xcb xcb-render xcb-ewmh xcb-renderutil xcb-icccm freetype2 fontconfig
CFLAGS = -Wall -Werror -pedantic `pkg-config --cflags $(PKGS)` -g -fstack-protector-all
#LDFLAGS=-Wl,--no-as-needed
LDLIBS = `pkg-config --libs $(PKGS)` -lm

ex: example

clean:
	rm -f example

