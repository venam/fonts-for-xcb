#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/render.h>
// This is for the wrappers
#include <xcb/xcb_renderutil.h>
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

static void testCookie(xcb_void_cookie_t, xcb_connection_t *, char *);
static void ewmh_init(xcb_connection_t *c);

int
main(int argc, char **argv)
{
	const char *fontfile;
	const char *text;
	xcb_connection_t *c;
	xcb_generic_event_t *e;
	xcb_screen_t *screen;
	xcb_void_cookie_t cookie;
	xcb_drawable_t win;
	xcb_drawable_t root;
	xcb_atom_t atom_wm_name;
	uint32_t mask = 0;
	uint32_t values[2];
	const char win_title[] = "Test example fonts";
	xcb_gcontext_t gc;

	// let's draw a simple rectangle on the window
	xcb_rectangle_t rectangles[] = {
		{
			.x = 10,
			.y = 10,
			.width = 30,
			.height = 30
		}
	};

	xcb_rectangle_t rectangles2[] = {
		{
			.x = 40,
			.y = 80,
			.width = 50,
			.height = 30
		},
		{
			.x = 90,
			.y = 20,
			.width = 30,
			.height = 90
		}
	};

	/*
	if (argc < 3) {
		fprintf (stderr, "usage: %s font.ttf text\n", argv[0]);
		exit (1);
	}
	fontfile = argv[1];
	text = argv[2];
	*/

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

	// Give it a title
	xcb_ewmh_set_wm_name(ewmh, win, strlen(win_title), win_title);

	// graphic context with #8022ff color
	gc = xcb_generate_id(c);
	mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
	values[0] = 0x8022ff | 0xff000000;
	values[1] = 0;
	xcb_create_gc(c, gc, win, mask, values);

	// pixmap to keep our drawing
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
		100,
		100
	);

	// Once we got a handle to a pixmap, we can draw it
	xcb_poly_fill_rectangle(c, pmap, gc, 1, rectangles);
	xcb_poly_fill_rectangle(c, pmap, gc, 1, rectangles2);
	xcb_flush(c);

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
		.x = 0,
		.y = 0,
		.width = 30,
		.height = 60
	};

	cookie = xcb_render_fill_rectangles_checked(c,
		XCB_RENDER_PICT_OP_DIFFERENCE,
		picture,
		rect_color, 1, &window_rect);
	testCookie(cookie, c, "can't fill rectangles");
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
		100,  /* Top left x coordinate of the region where to copy */
		50,   /* Top left y coordinate of the region where to copy */
		100,  /* Width of the region to copy */
		100); /* Height of the region to copy */

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

