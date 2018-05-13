#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/render.h>
// This is for the wrappers
#include <xcb/xcb_renderutil.h>

#include "../xcbft/xcbft.h"

/* This is the final function we want to be using after loading the faces
 */
xcb_bool32_t // TODO: think of something good to return, like status
xcbft_draw_text(
	xcb_connection_t*, // conn
	xcb_drawable_t, // win or pixmap
	int16_t, int16_t, // x, y
	struct utf_holder, // text
	xcb_render_color_t,
	struct xcbft_face_holder); // faces

xcb_render_picture_t xcbft_create_pen(
	xcb_connection_t*, xcb_render_color_t);

xcb_render_glyphset_t xcbft_load_glyphset(
	xcb_connection_t *c,
	struct xcbft_face_holder faces,
	struct utf_holder text);

void
xcbft_load_glyph(
	xcb_connection_t *c, xcb_render_glyphset_t gs, FT_Face face, int charcode);

xcb_bool32_t
xcbft_draw_text(
	xcb_connection_t *c, // conn
	xcb_drawable_t pmap, // win or pixmap
	int16_t x, int16_t y, // x, y
	struct utf_holder text, // text
	xcb_render_color_t color,
	struct xcbft_face_holder faces)
{
	xcb_void_cookie_t cookie;
	uint32_t values[2];
	xcb_generic_error_t *error;
	xcb_render_picture_t picture;
	xcb_render_pictforminfo_t *fmt;
	const xcb_render_query_pict_formats_reply_t *fmt_rep =
		xcb_render_util_query_formats(c);

	fmt = xcb_render_util_find_standard_format(
		fmt_rep,
		XCB_PICT_STANDARD_RGB_24
	);

	// create the picture with its attribute and format
	picture = xcb_generate_id(c);
	values[0] = XCB_RENDER_POLY_MODE_IMPRECISE;
	values[1] = XCB_RENDER_POLY_EDGE_SMOOTH;
	cookie = xcb_render_create_picture_checked(c,
		picture, // pid
		pmap, // drawable from the user
		fmt->id, // format
		XCB_RENDER_CP_POLY_MODE|XCB_RENDER_CP_POLY_EDGE,
		values); // make it smooth

	error = xcb_request_check(c, cookie);
	if (error) {
		error->error_code;
		fprintf(stderr, "ERROR: %s : %d\n",
			"could not create picture",
			error->error_code);
	}

	puts("creating pen");
	// create a 1x1 pixel pen (on repeat mode) of a certain color
	xcb_render_picture_t fg_pen = xcbft_create_pen(c, color);

	// load all the glyphs in a glyphset
	// TODO: maybe cache the xcb_render_glyphset_t
	puts("loading glyphset");
	xcb_render_glyphset_t font = xcbft_load_glyphset(c, faces, text);

	// we now have a text stream - a bunch of glyphs basically
	xcb_render_util_composite_text_stream_t *ts =
		xcb_render_util_composite_text_stream(font, text.length, 0);

	// draw the text at a certain positions
	puts("rendering inside text stream");
	xcb_render_util_glyphs_32(ts, x, y, text.length, text.str);

	// finally render using the repeated pen color on the picture
	// (which is related to the pixmap)
	puts("composite text");
	xcb_render_util_composite_text(
		c, // connection
		XCB_RENDER_PICT_OP_OVER, //op
		fg_pen, // src
		picture, // dst
		0, // fmt
		0, // src x
		0, // src y
		ts); // txt stream

	xcb_render_free_picture(c, picture);
	xcb_render_free_picture(c, fg_pen);

	return 0;
}

xcb_render_picture_t
xcbft_create_pen(xcb_connection_t *c, xcb_render_color_t color)
{
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

xcb_render_glyphset_t
xcbft_load_glyphset(
	xcb_connection_t *c,
	struct xcbft_face_holder faces,
	struct utf_holder text)
{
	int i, j;
	int glyph_index;
	xcb_render_glyphset_t gs;
	xcb_render_pictforminfo_t *fmt_a8;
	struct xcbft_face_holder faces_for_unsupported;
	const xcb_render_query_pict_formats_reply_t *fmt_rep =
		xcb_render_util_query_formats(c);

	// create a glyphset with a specific format
	fmt_a8 = xcb_render_util_find_standard_format(
		fmt_rep,
		XCB_PICT_STANDARD_A_8
	);
	gs = xcb_generate_id (c);
	xcb_render_create_glyph_set(c, gs, fmt_a8->id);

	for (i = 0; i < text.length; i++) {
		for (j = 0; j < faces.length; j++) {
			glyph_index = FT_Get_Char_Index(
				faces.faces[j],
				text.str[i]);
			if (glyph_index != 0) {
				break;
			}
		}
		// here use face at index j
		if (glyph_index != 0) {
			xcbft_load_glyph(c, gs, faces.faces[j], text.str[i]);
		} else {
		// fallback
			// TODO pass at least some of the query (font
			// size, italic, etc..)
			faces_for_unsupported = xcbft_query_by_char_support(
					text.str[i],
					NULL,
					faces.library);
			if (faces_for_unsupported.length == 0) {
				fprintf(stderr,
					"No faces found supporting character: %02x\n",
					text.str[i]);
				// draw a block using whatever font
				xcbft_load_glyph(c, gs, faces.faces[0], text.str[i]);
			} else {
				xcbft_load_glyph(c, gs,
					faces_for_unsupported.faces[0],
					text.str[i]);
				xcbft_face_holder_destroy(faces_for_unsupported);
			}
		}
	}

	return gs;
}

void
xcbft_load_glyph(
	xcb_connection_t *c, xcb_render_glyphset_t gs, FT_Face face, int charcode)
{
	uint32_t gid;
	xcb_render_glyphinfo_t ginfo;

	FT_Select_Charmap(face , ft_encoding_unicode);
	int glyph_index = FT_Get_Char_Index(face, charcode);
	FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT);
	
	FT_Bitmap *bitmap = &face->glyph->bitmap;
	ginfo.x = -face->glyph->bitmap_left;
	ginfo.y = face->glyph->bitmap_top;
	ginfo.width = bitmap->width;
	ginfo.height = bitmap->rows;
	ginfo.x_off = face->glyph->advance.x/64;
	ginfo.y_off = face->glyph->advance.y/64;

	gid=charcode;
	
	int stride=(ginfo.width+3)&~3;
	uint8_t tmpbitmap[stride*ginfo.height];
	int y;
	for(y=0; y<ginfo.height; y++)
		memcpy(tmpbitmap+y*stride, bitmap->buffer+y*ginfo.width, ginfo.width);
	printf("loading glyph %02x\n", charcode);
	
	xcb_render_add_glyphs_checked(c,
		gs, 1, &gid, &ginfo, stride*ginfo.height, tmpbitmap);

	xcb_flush(c);
}


// END OF FUNCTIONS START OF TESTING

int
main(int argc, char** argv)
{
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

	fontsearch = xcbft_extract_fontsearch_list(
		"times:pixelsize=30");
	// test fallback support also
	text = char_to_uint32("Héllo World!A탄ཀ");
	font_patterns = xcbft_query_fontsearch_all(fontsearch);
	FcStrSetDestroy(fontsearch);
	faces = xcbft_load_faces(font_patterns);
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

	xcb_create_window (c,                          /* connection    */
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

	xcbft_draw_text(
		c,
		pmap, // win or pixmap
		50, 60, // x, y
		text, // text
		text_color,
		faces); // faces
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
		pmap, /* The Drawable we want to paste */
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
		free (e);
	}
	endloop:
	puts("end");

	xcb_free_pixmap(c, pmap);
	xcb_free_gc(c, gc);
	xcb_disconnect (c);
	// XXX: DEBUG

	utf_holder_destroy(text);
	xcbft_face_holder_destroy(faces);
	return 0;
}

