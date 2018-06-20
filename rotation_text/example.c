#include <xcb/xcb.h>

#include "../xcbft/xcbft.h"


static xcb_render_fixed_t
float_to_fixed(float f)
{
	return f * 0x10000;
}

static xcb_pixmap_t
create_same_size_pixmap(xcb_connection_t *c, xcb_drawable_t drawable)
{
	xcb_get_geometry_cookie_t cookie;
	xcb_generic_error_t *e;
	xcb_get_geometry_reply_t *geom;
	xcb_pixmap_t pmap = xcb_generate_id(c);

	cookie = xcb_get_geometry(c, drawable);
	geom = xcb_get_geometry_reply(c, cookie, &e);
	xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
	xcb_create_pixmap(c,
		geom->depth,
		pmap,
		screen->root,
		geom->width,
		geom->height
	);

	return pmap;
}

int
main(int argc, char **argv)
{
	xcb_connection_t *c;
	xcb_generic_event_t *e;
	xcb_screen_t *screen;
	xcb_drawable_t win;
	xcb_drawable_t root;
	uint32_t mask = 0;
	uint32_t values[2];
	xcb_gcontext_t gc;
	xcb_render_color_t text_color;

	FcStrSet *fontsearch;
	struct xcbft_patterns_holder font_patterns;
	struct utf_holder text;
	struct xcbft_face_holder faces;

	// let's draw a simple rectangle on the window
	xcb_rectangle_t rectangles[] = {
		{
			.x = 0,
			.y = 0,
			.width = 300,
			.height = 300
		}
	};


	if (xcb_connection_has_error(c = xcb_connect(NULL, NULL))) {
		puts("error with initial connection");
		return 1;
	}

	xcbft_init();
    char *searchlist = "times:style=bold:pixelsize=30,monospace:pixelsize=40\n";
	fontsearch = xcbft_extract_fontsearch_list(searchlist);
	// test fallback support also
	text = char_to_uint32("Héllo ༃𐤋𐤊탄ཀ𐍊");
	font_patterns = xcbft_query_fontsearch_all(fontsearch);
	FcStrSetDestroy(fontsearch);
	long dpi = xcbft_get_dpi(c);
	faces = xcbft_load_faces(font_patterns, dpi);
	xcbft_patterns_holder_destroy(font_patterns);

	// XXX: DEBUG

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

	xcb_create_window(c,                          /* connection    */
			XCB_COPY_FROM_PARENT,          /* depth         */
			win,                           /* window Id     */
			root,                          /* parent window */
			120, 120,                        /* x, y          */
			300, 300,                      /* width, height */
			0,                             /* border_width  */
			XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class         */
			screen->root_visual,           /* visual        */
			mask, values);                 /* masks         */

	// graphic context with xyz color
	gc = xcb_generate_id(c);
	mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
	values[0] = 0xFFFFFF | 0xff000000;
	values[1] = 0;
	xcb_create_gc(c, gc, win, mask, values);

	// pixmap to keep our drawing in memory
	xcb_pixmap_t pmap = xcb_generate_id(c);

	xcb_create_pixmap(c,
		screen->root_depth,
		pmap,
		screen->root, // doesn't matter
		300,
		300
	);

	// draw a rectangle filling the whole pixmap with a single color
	xcb_poly_fill_rectangle(c, pmap, gc, 1, rectangles);

	// TODO: tricky part start
	text_color.red =  0x4242;
	text_color.green = 0x4242;
	text_color.blue = 0x4242;
	text_color.alpha = 0xFFFF;

	FT_Vector advance = xcbft_draw_text(
		c,
		pmap, // win or pixmap
		50, 60, // x, y
		text, // text
		text_color,
		faces,
		dpi);
	printf("advance: %ld\n", advance.x);
	// draw a rectangle at that place to know the advance was
	// calculated properly
	mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
	values[0] = 0xFF0000 | 0xff000000;
	values[1] = 0;
	xcb_change_gc(c, gc, mask, values);
	rectangles[0].x = 50+advance.x;
	rectangles[0].y = 60;
	rectangles[0].width = 10;
	rectangles[0].height = 10;
	xcb_poly_fill_rectangle(c, pmap, gc, 1, rectangles);

	// rotation
	xcb_render_picture_t picture, pic_mask, back_pix;
	xcb_render_pictforminfo_t *fmt;
	const xcb_render_query_pict_formats_reply_t *fmt_rep =
		xcb_render_util_query_formats(c);
	fmt = xcb_render_util_find_standard_format(
		fmt_rep,
		XCB_PICT_STANDARD_RGB_24
	);

	// create the picture with its attribute and format
	picture = xcb_generate_id(c);
	pic_mask = xcb_generate_id(c);
	back_pix = xcb_generate_id(c);
	values[0] = XCB_RENDER_POLY_MODE_IMPRECISE;
	values[1] = XCB_RENDER_POLY_EDGE_SMOOTH;

	xcb_pixmap_t temp_px = create_same_size_pixmap(c, pmap);

	xcb_render_create_picture_checked(c,
		picture, // pid
		pmap, // drawable from the user
		fmt->id, // format
		XCB_RENDER_CP_POLY_MODE|XCB_RENDER_CP_POLY_EDGE,
		values); // make it smooth

	xcb_render_create_picture_checked(c,
		pic_mask, // pid
		temp_px, // drawable from the user
		fmt->id, // format
		XCB_RENDER_CP_POLY_MODE|XCB_RENDER_CP_POLY_EDGE,
		values); // make it smooth

	xcb_render_create_picture_checked(c,
		back_pix, // pid
		temp_px, // drawable from the user
		fmt->id, // format
		XCB_RENDER_CP_POLY_MODE|XCB_RENDER_CP_POLY_EDGE,
		values); // make it smooth

	xcb_render_transform_t  transform;
	double angle = M_PI / 180 * 90; // 30 degrees
	double sina = sin( angle );
	double cosa = cos( angle );

	transform.matrix11 = float_to_fixed(cosa);
	transform.matrix12 = float_to_fixed(sina);
	transform.matrix13 = float_to_fixed(0.0);

	transform.matrix21 = float_to_fixed(-sina);
	transform.matrix22 = float_to_fixed(cosa);
	transform.matrix23 = float_to_fixed(0.0);

	transform.matrix31 = float_to_fixed(0.0);
	transform.matrix32 = float_to_fixed(0.0);
	transform.matrix33 = float_to_fixed(1.0);

	xcb_render_set_picture_transform_checked(c, picture, transform);

	xcb_render_composite_checked (c,
			XCB_RENDER_PICT_OP_OVER,
			picture,
			pic_mask,
			back_pix,
			-300,
			0,
			0,
			0,
			0,
			0,
			300,
			300);


	// TODO: end of tricky part

	// show the window and start the event loop
	xcb_map_window_checked(c, win);
	xcb_flush(c);

	while ((e = xcb_wait_for_event(c))) {
		xcb_generic_error_t *err = (xcb_generic_error_t *)e;

		switch (e->response_type & ~0x80) {
		case XCB_EXPOSE:
			// We draw the pixmap

	xcb_copy_area(c, /* xcb_connection_t */
		temp_px, /* The Drawable we want to paste */
		win,  /* The Drawable on which we copy the previous Drawable */
		gc,   /* A Graphic Context */
		0,    /* Top left x coordinate of the region to copy */
		0,    /* Top left y coordinate of the region to copy */
		0,    /* Top left x coordinate of the region where to copy */
		0,    /* Top left y coordinate of the region where to copy */
		300,  /* Width of the region to copy */
		300); /* Height of the region to copy */
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
		free(e);
	}
endloop:
	puts("end");
        if (e) free(e);

	xcb_free_pixmap(c, pmap);
	xcb_free_gc(c, gc);
	xcb_disconnect(c);
	// XXX: DEBUG

	utf_holder_destroy(text);
	xcbft_face_holder_destroy(faces);
	xcbft_done();
	return 0;
}

