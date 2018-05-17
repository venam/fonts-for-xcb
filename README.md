# Good fonts for XCB #

A project about creating something similar to Xft but for the XCB.

Project roadmap:

* Font matching phase (fontconfig): creation of a string that can be used.

* Font rasterizing phase (freetype): use the pattern given from fontconfig
  to rasterize from the font file given

* Font rendering using xrender extension: use both `xcb/render.h` and
  `xcb/xcb_renderutil.h` to render and cache the glyphs used in the font

* Join both inside an XCB wrapper

## TODOs ##

- Add documentation
- Add dpi support via xrm
- Fallback support for search similar to initial fontquery
- Check if bold is working properly
- Check return codes of functions and comments
- Maybe add vertical font support
- Maybe add Kerning
- Maybe cache the glyphset
- Maybe load more settings from xrm (hinting, antialias, subpixel, etc..)


## Usage ##

```C
#include "../xcbft/xcbft.h"

/* ... */

FcStrSet *fontsearch;
struct xcbft_patterns_holder font_patterns;
struct utf_holder text;
struct xcbft_face_holder faces;
xcb_render_color_t text_color;

// The pixmap we want to draw over
xcb_pixmap_t pmap = xcb_generate_id(c);

/* ... pmap stuffs fill and others ... */

// The fonts to use and the text in unicode
char *searchlist = "times:style=bold:pixelsize=30,monospace:pixelsize=40\n";
text = char_to_uint32("Héllo ༃𐤋𐤊탄ཀ𐍊");

// extract the fonts in a list
fontsearch = xcbft_extract_fontsearch_list(searchlist);
// do the search and it returns all the matching fonts
font_patterns = xcbft_query_fontsearch_all(fontsearch);
// no need for the fonts list anymore
FcStrSetDestroy(fontsearch);
// load the faces related to the matching fonts patterns
faces = xcbft_load_faces(font_patterns);
// no need for the matching fonts patterns
xcbft_patterns_holder_destroy(font_patterns);

// select a specific color
text_color.red =  0x4242;
text_color.green = 0x4242;
text_color.blue = 0x4242;
text_color.alpha = 0xFFFF;

// draw on the drawable (pixmap here) pmap at position (50,60) the text
with the color we chose and the faces we chose
xcbft_draw_text(
	c, // X connection
	pmap, // win or pixmap
	50, 60, // x, y
	text, // text
	text_color,
	faces); // faces

// no need for the text and the faces
utf_holder_destroy(text);
xcbft_face_holder_destroy(faces);

/* ... */

```

Depends on : `xcb xcb-render xcb-renderutil freetype2 fontconfig`  

