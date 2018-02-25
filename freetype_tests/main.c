#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include <ft2build.h>
#include FT_FREETYPE_H


int
main(int argc, char** argv)
{
	int error;
	FT_Library library;
	FT_Face face;
	FT_UInt glyph_index;

	error = FT_Init_FreeType(&library);
	if (error != FT_Err_Ok) {
		perror(NULL);
		return 1;
	} else {
		puts("Freetype2 initialization successful");
	}

	// lib, path, index, &face
	error = FT_New_Face(library, "/usr/share/fonts/TTF/times.ttf", 0, &face);
	if (error == FT_Err_Unknown_File_Format) {
		puts("wrong file format");
		return 1;
	} else if (error == FT_Err_Cannot_Open_Resource) {
		puts("could not open resource");
		return 1;
	} else if (error) {
		puts("another sort of error");
		return 1;
	}
	assert(face != NULL);

	// we can use the face data

	// pixel_size = point_size * resolution / 72
	// 1 point equals 1/72th of an inch in digital typography
	//
	// "The character widths and heights are specified in 1/64th of points.
	// A point is a physical distance, equaling 1/72th of an inch. Normally,
	// it is not equivalent to a pixel." (multiple of 64 to fit int)
	//
	// The horizontal and vertical device resolutions are expressed in
	// dots-per-inch, or dpi. Standard values are 72 or 96 dpi for
	// display devices like the screen. The resolution is used to compute
	// the character pixel size from the character point size.

	//error = FT_Set_Char_Size(
	//		face,    /* handle to face object           */
	//		0,       /* char_width in 1/64th of points  */
	//		16*64,   /* char_height in 1/64th of points */
	//		300,     /* horizontal device resolution    */
	//		300 );   /* vertical device resolution      */

	// example with pixels
	// 16 pixels/dpi => 0.166 inch
	// 0.166 inch (0.42cm) = 12.01 points which is (0.166)/(1/72)
	// the reverse of the formula:
	// pixel_size = 12.01 (point_size)/72 * 96 (resolution) = 16px
	error = FT_Set_Pixel_Sizes(
			face,   /* handle to face object */
			0,      /* pixel_width           */
			16 );   /* pixel_height          */

	if (error != FT_Err_Ok) {
		perror(NULL);
		return 1;
	}

	// make sure it's unicode
	FT_Select_Charmap(face, FT_ENCODING_UNICODE);

	glyph_index = FT_Get_Char_Index(face, (FT_ULong) 0x41);

	printf("glyph index is: %d \n", glyph_index);

	FT_Int32 load_flags = FT_LOAD_DEFAULT;
	// fill the glyph slot, aka face->glyph
	// face−>glyph−>format describes the format used for storing the glyph
	error = FT_Load_Glyph(face, glyph_index, load_flags);
	if (error != FT_Err_Ok) {
		perror(NULL);
		return 1;
	}

	FT_Int32 render_mode = FT_RENDER_MODE_NORMAL;
	// access it directly through glyph->bitmap 
	// position it through glyph->bitmap_left and glyph->bitmap_top
	error = FT_Render_Glyph( face->glyph,   /* glyph slot  */
			render_mode ); /* render mode */
	if (error != FT_Err_Ok) {
		perror(NULL);
		return 1;
	}

	// apply the matrix (fontconfig can help generate it)
	//error = FT_Set_Transform(
	//		face,       /* target face object    */
	//		&matrix,    /* pointer to 2x2 matrix */
	//		&delta );   /* pointer to 2d vector  */

	FT_Done_Face(face);
	FT_Done_FreeType(library);

	return 0;
}

