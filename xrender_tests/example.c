#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/render.h>
// This is for the wrappers
#include <xcb/xcb_renderutil.h>

#include <ft2build.h>
#include FT_FREETYPE_H
/*
 * Font rendering testing phase
 *
 * Documentation:
 * 
 * https://www.x.org/releases/X11R7.5/doc/libxcb/tutorial/#pixmaps
 * https://www.x.org/releases/current/doc/renderproto/renderproto.txt
 * https://keithp.com/~keithp/talks/usenix2001/xrender/
 *
 */


xcb_ewmh_connection_t *ewmh; // Ewmh Connection.
struct utf_holder {
	uint16_t *str;
	unsigned int length;
};


static void testCookie(xcb_void_cookie_t, xcb_connection_t *, char *);
static void ewmh_init(xcb_connection_t *c);
static xcb_render_picture_t create_pen(
	xcb_connection_t *c, int red, int green, int blue, int alpha);
static xcb_render_glyphset_t load_glyphset(
	xcb_connection_t *c, char *filename, int size, struct utf_holder);
static void load_glyph(
	xcb_connection_t *c, xcb_render_glyphset_t gs, FT_Face face, int charcode);
static int utf_len(char *str);
struct utf_holder char_to_uint16(char *str);

int
main(int argc, char **argv)
{
	char *fontfile;
	const char *text;
	xcb_connection_t *c;
	xcb_generic_event_t *e;
	xcb_screen_t *screen;
	xcb_void_cookie_t cookie;
	xcb_drawable_t win;
	xcb_drawable_t root;
	uint32_t mask = 0;
	uint32_t values[2];
	const char win_title[] = "Test XCB fonts";
	xcb_gcontext_t gc;

	// let's draw a simple rectangle on the window
	xcb_rectangle_t rectangles[] = {
		{
			.x = 0,
			.y = 0,
			.width = 300,
			.height = 300
		}
	};

	if (argc < 3) {
		fprintf (stderr, "usage: %s font.ttf text\n", argv[0]);
		exit (1);
	}
	fontfile = argv[1];
	text = argv[2];

	if (xcb_connection_has_error(c = xcb_connect(NULL, NULL))) {
		puts("error with initial connection");
		return 1;
	}
	ewmh_init(c);
	// get the first screen
	screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
	// root window
	root = screen->root;

	win = xcb_generate_id(c);
	// let's give this window a background and let it receive some events:
	// exposure and key press
	mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	values[0] = screen->white_pixel;
	values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS;

	xcb_create_window (c,                          /* connection    */
			XCB_COPY_FROM_PARENT,          /* depth         */
			win,                           /* window Id     */
			root,                          /* parent window */
			60, 60,                        /* x, y          */
			300, 300,                      /* width, height */
			0,                             /* border_width  */
			XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class         */
			screen->root_visual,           /* visual        */
			mask, values);                 /* masks         */

	// Mark as dialog
	xcb_change_property (c, XCB_PROP_MODE_REPLACE, win,
		ewmh->_NET_WM_WINDOW_TYPE, XCB_ATOM_CARDINAL, 32, 1,
		&(ewmh->_NET_WM_WINDOW_TYPE_DIALOG) );


	// just fun stuffs to disallow resize (you don't have to pay
	// too much attention)
	xcb_size_hints_t hints;
	hints.width = 
	hints.height = 
	hints.min_width = 
	hints.min_height = 
	hints.max_width = 
	hints.max_height = 
	hints.width_inc = 
	hints.height_inc = 
	hints.base_width = hints.base_height = 300;
	cookie = xcb_icccm_set_wm_normal_hints_checked(c, win, &hints);
	testCookie(cookie, c, "can't set normal hints");

	xcb_atom_t list[] = {
		ewmh->_NET_WM_ACTION_MOVE,
		ewmh->_NET_WM_ACTION_CHANGE_DESKTOP,
		ewmh->_NET_WM_ACTION_CLOSE,
		ewmh->_NET_WM_ACTION_ABOVE,
		ewmh->_NET_WM_ACTION_BELOW
	};
	cookie = xcb_ewmh_set_wm_allowed_actions(ewmh, win, 5, list);
	testCookie(cookie, c, "can't set ewmh allowed actions");

	// Give it a title
	xcb_ewmh_set_wm_name(ewmh, win, strlen(win_title), win_title);

	// graphic context with xyz color
	gc = xcb_generate_id(c);
	mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
	values[0] = 0xe6e3c6 | 0xff000000;
	values[1] = 0;
	xcb_create_gc(c, gc, win, mask, values);

	// pixmap to keep our drawing in memory
	xcb_pixmap_t pmap = xcb_generate_id(c);
	// One important note should be made: it is possible to create pixmaps
	// with different depths on the same screen. When we perform copy operations
	// (a pixmap onto a window, etc), we should make sure that both source and
	// target have the same depth. If they have a different depth, the operation
	// would fail. The exception to this is if we copy a specific bit plane of
	// the source pixmap using the xcb_copy_plane_t function. In such an event,
	// we can copy a specific plane to the target window (in actuality, setting
	// a specific bit in the color of each pixel copied). This can be used to
	// generate strange graphic effects in a window.
	xcb_create_pixmap(c,
		screen->root_depth,
		pmap,
		screen->root, // doesn't matter
		300,
		300
	);

	// Once we got a handle to a pixmap, we can draw on it
	xcb_poly_fill_rectangle(c, pmap, gc, 1, rectangles);

	/*
	 * START DEBUG
	 */
	// choose the appropriate picture format (depth and all colors)
	xcb_render_pictforminfo_t *fmt;
	const xcb_render_query_pict_formats_reply_t *fmt_rep =
		xcb_render_util_query_formats(c);
	fmt = xcb_render_util_find_standard_format(
		fmt_rep,
		XCB_PICT_STANDARD_RGB_24
	);

	// create the picture with its attribute and format
	xcb_render_picture_t picture = xcb_generate_id(c);
	values[0] = XCB_RENDER_POLY_MODE_IMPRECISE;
	values[1] = XCB_RENDER_POLY_EDGE_SMOOTH;
	cookie = xcb_render_create_picture_checked(c,
		picture, // pid
		pmap, // drawable
		fmt->id, // format
		XCB_RENDER_CP_POLY_MODE|XCB_RENDER_CP_POLY_EDGE,
		values); // make it smooth
	testCookie(cookie, c, "can't create source picture");

	xcb_render_color_t rect_color = {
		.red = 0x00ff,
		.green = 0xff0f,
		.blue = 0xf0ff,
		.alpha = 0xffff
	};

	xcb_rectangle_t window_rect = {
		.x = 100,
		.y = 50,
		.width = 30,
		.height = 60
	};

	// a test rectangle, just to make it pretty and show xrender power
	cookie = xcb_render_fill_rectangles_checked(c,
		XCB_RENDER_PICT_OP_OVER,
		picture,
		rect_color, 1, &window_rect);
	testCookie(cookie, c, "can't fill rectangles");

	// create a 1x1 pixel pen (on repeat mode) of a certain color
	xcb_render_picture_t fg_pen = create_pen(c, 0x7f0f, 0, 0xff00, 0xf000);

	// allow utf8 strings
	struct utf_holder holder;
	char *temp = malloc(strlen(text));
	strcpy(temp, text);
	holder = char_to_uint16(temp);

	// load the glyphs used in that string on the server
	int font_size = 30;
	xcb_render_glyphset_t font = load_glyphset(c, fontfile, font_size, holder);

	// we now have a text stream - a bunch of glyphs basically
	xcb_render_util_composite_text_stream_t *ts =
		xcb_render_util_composite_text_stream(font, holder.length, 0);

	/*
	xcb_render_util_glyphs_8 (
			ts,
			10, // dx
			100, // dy
			strlen(text), //
			text);
	*/

	// draw the text (in holder) at certain positions
	xcb_render_util_glyphs_16 (
			ts,
			10, // dx
			100, // dy
			holder.length, //
			holder.str);

	// hacky way, leave it for later to clean
	free(holder.str);
	free(temp);

	// finally render using the repeated pen color on the picture
	// (which is related to the pixmap)
	xcb_render_util_composite_text(
		c, // connection
		XCB_RENDER_PICT_OP_OVER, //op
		fg_pen, // src
		picture, // dst
		0, // fmt
		0, // src x
		0, // src y
		ts); // txt stream
	/*
	 * END DEBUG
	 */

	// show the window and start the event loop
	xcb_map_window_checked(c, win);
	testCookie(cookie, c, "can't map window");
	xcb_flush(c);

	while ((e = xcb_wait_for_event(c))) {
		xcb_generic_error_t *err = (xcb_generic_error_t *)e;
		switch (e->response_type & ~0x80) {
		case XCB_EXPOSE:
			// We draw the pixmap

	xcb_copy_area(c, /* xcb_connection_t */
		pmap, /* The Drawable we want to paste */
		win,  /* The Drawable on which we copy the previous Drawable */
		gc,   /* A Graphic Context */
		0,    /* Top left x coordinate of the region to copy */
		0,    /* Top left y coordinate of the region to copy */
		0,    /* Top left x coordinate of the region where to copy */
		0,    /* Top left y coordinate of the region where to copy */
		300,  /* Width of the region to copy */
		300); /* Height of the region to copy */

	// just for fun copy another part of the pixmap again
	xcb_copy_area(c, /* xcb_connection_t */
		pmap, /* The Drawable we want to paste */
		win,  /* The Drawable on which we copy the previous Drawable */
		gc,   /* A Graphic Context */
		0,    /* Top left x coordinate of the region to copy */
		70,    /* Top left y coordinate of the region to copy */
		100,    /* Top left x coordinate of the region where to copy */
		100,    /* Top left y coordinate of the region where to copy */
		110,  /* Width of the region to copy */
		60); /* Height of the region to copy */

			xcb_flush(c);
			break;
		case XCB_KEY_PRESS: {
			xcb_key_press_event_t *kr = (xcb_key_press_event_t *)e;
			switch (kr->detail) {
			case 9: /* escape */
			case 24: /* Q */
				goto endloop;
			}
		}
		case 0:
			printf("Received X11 error %d\n", err->error_code);
		}
		free (e);
	}
	endloop:
	puts("end");

	xcb_render_free_picture(c, picture);
	xcb_render_free_picture(c, fg_pen);
	xcb_free_pixmap(c, pmap);
	xcb_free_gc(c, gc);
	xcb_disconnect (c);

	return 0;
}

static void
testCookie(xcb_void_cookie_t cookie,
	xcb_connection_t *connection,
	char *errMessage)
{
	xcb_generic_error_t *error = xcb_request_check(connection, cookie);
	if (error) {
		printf("ERROR: %s : %"PRIu8"\n",
			errMessage,
			error->error_code);
		xcb_disconnect(connection);
		exit(-1);
	}
}

static void
ewmh_init(xcb_connection_t *c)
{
	if (!(ewmh = calloc(1, sizeof(xcb_ewmh_connection_t))))
		printf("Fail\n");

	xcb_intern_atom_cookie_t *cookie = xcb_ewmh_init_atoms(c, ewmh);
	xcb_ewmh_init_atoms_replies(ewmh, cookie, (void *)0);
}


static xcb_render_picture_t
create_pen(xcb_connection_t *c, int red, int green, int blue, int alpha)
{
	xcb_render_color_t color = {
		.red = red,
		.green = green,
		.blue = blue,
		.alpha = alpha
	};

	xcb_render_pictforminfo_t *fmt;
	const xcb_render_query_pict_formats_reply_t *fmt_rep =
		xcb_render_util_query_formats(c);
	// alpha can only be used with a picture containing a pixmap
	fmt = xcb_render_util_find_standard_format(
		fmt_rep,
		XCB_PICT_STANDARD_ARGB_32
	);

	xcb_drawable_t root = xcb_setup_roots_iterator(
			xcb_get_setup(c)
		).data->root;

	xcb_pixmap_t pm = xcb_generate_id(c);
	xcb_create_pixmap(c, 32, pm, root, 1, 1);

	uint32_t values[1];
	values[0] = XCB_RENDER_REPEAT_NORMAL;

	xcb_render_picture_t picture = xcb_generate_id(c);
	xcb_render_create_picture(c,
		picture,
		pm,
		fmt->id,
		XCB_RENDER_CP_REPEAT,
		values);

	xcb_rectangle_t rect = {
		.x = 0,
		.y = 0,
		.width = 1,
		.height = 1
	};

	xcb_render_fill_rectangles(c,
		XCB_RENDER_PICT_OP_OVER,
		picture,
		color, 1, &rect);

	xcb_free_pixmap(c, pm);
	return picture;
}

static xcb_render_glyphset_t
load_glyphset(
	xcb_connection_t *c, char *filename, int size, struct utf_holder holder)
{
	static int ft_lib_initialized=0;
	static FT_Library library;
	int n;

	xcb_render_pictforminfo_t *fmt_a8;
	const xcb_render_query_pict_formats_reply_t *fmt_rep =
		xcb_render_util_query_formats(c);
	fmt_a8 = xcb_render_util_find_standard_format(
		fmt_rep,
		XCB_PICT_STANDARD_A_8
	);

	xcb_render_glyphset_t gs = xcb_generate_id (c);
	xcb_render_create_glyph_set(c, gs, fmt_a8->id);

	if (!ft_lib_initialized)
		FT_Init_FreeType(&library);

	// TODO: leave that aside from now and do a simple loading
	FT_Face face;

	FT_New_Face(library,
		filename,
		0, &face);

	FT_Set_Char_Size(
		face, 0, size*64,
		90, 90);

	for (n = 0; n < holder.length; n++)
		load_glyph(c, gs, face, holder.str[n]);

	FT_Done_Face(face);

	return gs;
}

static void
load_glyph(
	xcb_connection_t *c, xcb_render_glyphset_t gs, FT_Face face, int charcode)
{
	uint32_t gid;
	xcb_render_glyphinfo_t ginfo;

	FT_Select_Charmap(face , ft_encoding_unicode);
	int glyph_index = FT_Get_Char_Index(face, charcode);
	if (glyph_index == 0) {
		// TODO use fallback font
		// http://www.unicode.org/policies/lastresortfont_eula.html
		printf("character %d not found\n", charcode);
	}
	FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT);
	
	FT_Bitmap *bitmap = &face->glyph->bitmap;
	ginfo.x = -face->glyph->bitmap_left;
	ginfo.y = face->glyph->bitmap_top;
	ginfo.width = bitmap->width;
	ginfo.height = bitmap->rows;
	ginfo.x_off = face->glyph->advance.x/64;
	ginfo.y_off = face->glyph->advance.y/64;

	//glyph_id = charcode;

	gid=charcode;
	
	int stride=(ginfo.width+3)&~3;
	uint8_t tmpbitmap[stride*ginfo.height];
	int y;
	for(y=0; y<ginfo.height; y++)
		memcpy(tmpbitmap+y*stride, bitmap->buffer+y*ginfo.width, ginfo.width);
	
	xcb_render_add_glyphs_checked(c,
		gs, 1, &gid, &ginfo, stride*ginfo.height, tmpbitmap);

	xcb_flush(c);
}

struct utf_holder
char_to_uint16(char *str)
{
	struct utf_holder holder;
	char *utf = str;
	uint16_t *output = NULL;
	int length = 0;
	output = (uint16_t *)malloc(sizeof(uint16_t)*strlen(str));

	while (*utf) {
		uint16_t chr;
		uint8_t *u8 = (uint8_t *) utf;

		switch (utf_len(utf)) {
		case 1:
			chr = u8[0];
			break;
		case 2:
			chr = (u8[0] & 0x1f) << 6 | (u8[1] & 0x3f);
			break;
		case 3:
			chr = (u8[0] & 0xf) << 12 | (u8[1] & 0x3f) << 6 | (u8[2] & 0x3f);
			break;
		case 4:
		case 5:
		case 6:
			chr = 0xfffd;
			break;
		}

		//chr = chr >> 8 | chr << 8;
		output[length] = chr;
		utf += utf_len(utf);
		length++;
	}

	holder.length = length;
	holder.str = output;

	return holder;
}

static int
utf_len(char *str) {
	uint8_t *utf = (uint8_t *)str;

	if (utf[0] < 0x80)
		return 1;
	else if ((utf[0] & 0xe0) == 0xc0)
		return 2;
	else if ((utf[0] & 0xf0) == 0xe0)
		return 3;
	else if ((utf[0] & 0xf8) == 0xf0)
		return 4;
	else if ((utf[0] & 0xfc) == 0xf8)
		return 5;
	else if ((utf[0] & 0xfe) == 0xfc)
		return 6;
	else
		return 1;
}

