PKGS = xcb xcb-render xcb-ewmh xcb-renderutil xcb-icccm freetype2 fontconfig
CFLAGS = -Wall -Werror -Wno-unused `pkg-config --cflags $(PKGS)` -g
LDLIBS = `pkg-config --libs $(PKGS)` -lm

ex: example

