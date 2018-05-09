
// TODO: decouple de functions from the ../fc_w_ft/main.c and move them
// in a header to be reused here


/* This is the final function we want to be using after loading the faces
 */
xcb_bool32_t // TODO: think of something good to return, like status
xcbft_draw_text(
	xcb_drawable_t, // win or pixmap
	int16_t, int16_t, // x, y
	struct utf_holder, // text
	struct xcbft_face_holder); // faces

int
main(int argc, char** argv)
{
	return 0;
}

