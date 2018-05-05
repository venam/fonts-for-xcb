#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "../utf8_utils/utf8.h"
/*
 * Mixing fontconfig and freetype2
 *
 * Goal:
 * Pass the text and a list of fonts in preferred order
 * The font information for both font will be fetched
 * Pass through the text and check which character is supported (store it)
 * Load freetype2 using fontconfig returned options for the fonts
 * Finally return the glyphs positions in that loaded font (for testing)
 */

struct xcbft_patterns_holder {
	FcPattern **patterns;
	uint8_t length;
};

struct xcbft_face_holder {
	FT_Face *faces;
	uint8_t length;
	FT_Library library;
};


/*
 * Do the font queries through fontconfig and return the info
 * Assumes:
 *	Fontconfig is already init & cleaned outside
 *	the FcPattern return needs to be cleaned outside
 */
FcPattern*
xcbft_query_fontsearch(FcChar8 *fontquery)
{
	FcBool status;
	FcPattern *fc_finding_pattern, *pat_output;
	FcResult result;

	fc_finding_pattern = FcNameParse(fontquery);

	// to match we need to fix the pattern (fill unspecified info)
	FcDefaultSubstitute(fc_finding_pattern);
	status = FcConfigSubstitute(NULL, fc_finding_pattern, FcMatchPattern);
	if (status == FcFalse) {
		fprintf(stderr, "could not perform config font substitution");
		return NULL;
	}

	pat_output = FcFontMatch(NULL, fc_finding_pattern, &result);

	FcPatternDestroy(fc_finding_pattern);
	if (result == FcResultMatch) {
		return pat_output;
	} else if (result == FcResultNoMatch) {
		fprintf(stderr, "there wasn't a match");
	} else {
		fprintf(stderr, "the match wasn't as good as it should be");
	}
	return NULL;
}

struct xcbft_patterns_holder
xcbft_query_fontsearch_all(FcStrSet *queries)
{
	FcBool status;
	struct xcbft_patterns_holder font_patterns;
	FcPattern *font_pattern;
	uint8_t current_allocated;

	font_patterns.patterns = NULL;
	font_patterns.length = 0;

	status = FcInit();
	if (status == FcFalse) {
		fprintf(stderr, "Could not initialize fontconfig");
		return font_patterns;
	}

	// start with 5, expand if needed
	current_allocated = 5;
	font_patterns.patterns = malloc(sizeof(FcPattern *)*current_allocated);

	// safely iterate over set
	FcStrList *iterator = FcStrListCreate(queries);
	FcChar8 *fontquery = NULL;
	FcStrListFirst(iterator);
	while ((fontquery = FcStrListNext(iterator)) != NULL) {
		font_pattern = xcbft_query_fontsearch(fontquery);
		if (font_pattern != NULL) {
			if (font_patterns.length + 1 > current_allocated) {
				current_allocated += 5;
				font_patterns.patterns = realloc(
					font_patterns.patterns,
					sizeof(FcPattern*) * current_allocated);
			}
			font_patterns.patterns[font_patterns.length] = font_pattern;
			font_patterns.length++;
		}
	}
	FcStrListDone(iterator);
	// end of safely iterate over set

	return font_patterns;
}

struct xcbft_face_holder
xcbft_load_faces(struct xcbft_patterns_holder patterns)
{
	int i;
	struct xcbft_face_holder faces;
	FcResult result;
	FcValue fc_file, fc_index;
	FT_Error error;
	FT_Library library;

	faces.length = 0;
	error = FT_Init_FreeType(&library);
	if (error != FT_Err_Ok) {
		perror(NULL);
		return faces;
	}

	// allocate the same size as patterns as it should be <= its length
	faces.faces = malloc(sizeof(FT_Face)*patterns.length);
	for (i = 0; i < patterns.length; i++) {
		// get the information needed from the pattern
		result = FcPatternGet(patterns.patterns[i], FC_FILE, 0, &fc_file);
		if (result != FcResultMatch) {
			fprintf(stderr, "font has not file location");
			continue;
		}
		result = FcPatternGet(patterns.patterns[i], FC_INDEX, 0, &fc_index);
		if (result != FcResultMatch) {
			fprintf(stderr, "font has no index, using 0 by default");
			fc_index.type = FcTypeInteger;
			fc_index.u.i = 0;
		}
		// TODO: load more info like
		//	transform matrix
		//	autohint
		//	hinting
		//	size or pixelsize
		//	verticallayout

		// load the face
		error = FT_New_Face(
				library,
				(const char *) fc_file.u.s,
				fc_index.u.i,
				&(faces.faces[faces.length]) );
		if (error == FT_Err_Unknown_File_Format) {
			fprintf(stderr, "wrong file format");
			continue;
		} else if (error == FT_Err_Cannot_Open_Resource) {
			fprintf(stderr, "could not open resource");
			continue;
		} else if (error) {
			fprintf(stderr, "another sort of error");
			continue;
		}
		if (faces.faces[faces.length] == NULL) {
			fprintf(stderr, "face was empty");
			continue;
		}
		faces.length++;
	}

	faces.library = library;

	return faces;
}

FcStrSet*
xcbft_extract_fontsearch_list(char *string)
{
	FcStrSet *fontsearch = NULL;
	FcChar8 *fontquery;
	FcBool result = FcFalse;
	
	fontsearch = FcStrSetCreate();

	while ( (fontquery = (FcChar8*)strsep(&string,",")) != NULL ) {
		result = FcStrSetAdd(fontsearch, fontquery);
		if (result == FcFalse) {
			fprintf(stderr,
				"Couldn't add fontquery to fontsearch set");
		}
	}

	return fontsearch;
}

void
xcbft_patterns_holder_destroy(struct xcbft_patterns_holder patterns)
{
	int i = 0;

	for (; i < patterns.length; i++) {
		FcPatternDestroy(patterns.patterns[i]);
	}
	free(patterns.patterns);
	FcFini();
}

void
xcbft_face_holder_destroy(struct xcbft_face_holder faces)
{
	int i = 0;

	for (; i < faces.length; i++) {
		FT_Done_Face(faces.faces[i]);
	}
	if (faces.faces) {
		free(faces.faces);
	}
	FT_Done_FreeType(faces.library);
}

int
main(int argc, char** argv)
{
	FcStrSet *fontsearch;
	struct xcbft_patterns_holder font_patterns;
	int i = 0;
	struct utf_holder holder;
	struct xcbft_face_holder faces;

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
	xcbft_face_holder_destroy(faces);

	xcbft_patterns_holder_destroy(font_patterns);

	holder = char_to_uint32(argv[2]);
	for (i = 0; i < holder.length; i++) {
		printf("%02x\n", holder.str[i]);
	}
	utf_holder_destroy(holder);

	return 0;
}

