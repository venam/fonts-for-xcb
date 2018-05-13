#include "../xcbft/xcbft.h"

int
main(int argc, char** argv)
{
	FcStrSet *fontsearch;
	struct xcbft_patterns_holder font_patterns;
	int i = 0, j = 0;
	int glyph_index;
	struct utf_holder holder;
	struct xcbft_face_holder faces;
	struct xcbft_face_holder faces_for_unsupported;

	if (argc < 3) {
		puts("pass the fonts and the text");
		return 1;
	}

	fontsearch = xcbft_extract_fontsearch_list(argv[1]);
	font_patterns = xcbft_query_fontsearch_all(fontsearch);
	FcStrSetDestroy(fontsearch);

	for (i = 0; i < font_patterns.length; i++) {
		FcPatternPrint(font_patterns.patterns[i]);
	}


	faces = xcbft_load_faces(font_patterns);

	xcbft_patterns_holder_destroy(font_patterns);

	holder = char_to_uint32(argv[2]);
	for (i = 0; i < holder.length; i++) {
		printf("Checking support for character with hexcode: %02x\n", holder.str[i]);
		// here do a mini test to check for face support of char
		for (j = 0; j < faces.length; j++) {
			//FT_Select_Charmap(faces.faces[j], ft_encoding_unicode);
			glyph_index = FT_Get_Char_Index(faces.faces[j], holder.str[i]);
			if (glyph_index != 0) {
				break;
			}
		}
		if (glyph_index != 0) {
			printf("Supported, found glyph_index: %d\n", glyph_index);
			printf("In family_name: %s\n", faces.faces[j]->family_name);
			continue;
		}

		puts("Not supported, fallback, searching for font with character");
		faces_for_unsupported = xcbft_query_by_char_support(
				holder.str[i],
				NULL,
				faces.library);
		if (faces_for_unsupported.length == 0) {
			puts("No faces found supporting this character");
		} else {
			printf("Face found for character, family_name: %s\n",
					faces_for_unsupported.faces[0]->family_name);
			xcbft_face_holder_destroy(faces_for_unsupported);
		}
	}
	utf_holder_destroy(holder);
	xcbft_face_holder_destroy(faces);

	return 0;
}

