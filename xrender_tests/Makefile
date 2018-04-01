PKGS = xcb xcb-render xcb-ewmh xcb-renderutil
CFLAGS = -Wall -Werror -Wno-unused `pkg-config --cflags $(PKGS)` -g
LDFLAGS = `pkg-config --libs $(PKGS)` -lm

example: example

%: %.c
	$(CC) -std=c99 -o $@ $^ $(CFLAGS) $(LDFLAGS)
