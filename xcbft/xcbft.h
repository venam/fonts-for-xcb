#ifndef _XCBFT
#define _XCBFT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "../utf8_utils/utf8.h"

struct xcbft_patterns_holder {
	FcPattern **patterns;
	uint8_t length;
};

struct xcbft_face_holder {
	FT_Face *faces;
	uint8_t length;
	FT_Library library;
};

// signatures
FcPattern* xcbft_query_fontsearch(FcChar8 *);
struct xcbft_face_holder xcbft_query_by_char_support(
		FcChar32, const FcPattern *, FT_Library);
struct xcbft_patterns_holder xcbft_query_fontsearch_all(FcStrSet *);
struct xcbft_face_holder xcbft_load_faces(struct xcbft_patterns_holder);
FcStrSet* xcbft_extract_fontsearch_list(char *);
void xcbft_patterns_holder_destroy(struct xcbft_patterns_holder);
void xcbft_face_holder_destroy(struct xcbft_face_holder);


/*
 * Do the font queries through fontconfig and return the info
 *
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

/*
 * Query a font based on character support
 * Optionally pass a pattern that it'll use as the base for the search
 *
 * TODO: fallback of font added to the list or somehow done when drawing,
 *       to do that we need to search by the charset we want to draw
 *       and if they are in one of the font already specified, thus need to
 *       know what the users want to insert as text. This needs more thinking
 *       to be decoupled.

	Assumes the ft2 library is already loaded
	Assumes the face will be cleaned outside
 */
struct xcbft_face_holder
xcbft_query_by_char_support(FcChar32 character,
		const FcPattern *copy_pattern,
		FT_Library library)
{
	FcBool status;
	FcResult result;
	FcCharSet *charset;
	FcPattern *charset_pattern, *pat_output;
	struct xcbft_patterns_holder patterns;
	struct xcbft_face_holder faces;

	faces.length = 0;

	// add characters we need to a charset
	charset = FcCharSetCreate();
	FcCharSetAddChar(charset, character);

	// if we pass a pattern then copy it to get something close
	if (copy_pattern != NULL) {
		charset_pattern = FcPatternDuplicate(copy_pattern);
	} else {
		charset_pattern = FcPatternCreate();
	}

	// use the charset for the pattern search
	FcPatternAddCharSet(charset_pattern, FC_CHARSET, charset);
	// also force it to be scalable
	FcPatternAddBool(charset_pattern, FC_SCALABLE, FcTrue);

	// default & config substitutions, the usual
	FcDefaultSubstitute(charset_pattern);
	status = FcConfigSubstitute(NULL, charset_pattern, FcMatchPattern);
	if (status == FcFalse) {
		fprintf(stderr, "could not perform config font substitution");
		return faces;
	}

	pat_output = FcFontMatch(NULL, charset_pattern, &result);

	FcPatternDestroy(charset_pattern);

	if (result != FcResultMatch) {
		fprintf(stderr, "there wasn't a match");
		return faces;
	}

	patterns.patterns = malloc(sizeof(FcPattern *));
	patterns.length = 1;
	patterns.patterns[0] = pat_output;

	faces = xcbft_load_faces(patterns);

	// cleanup
	xcbft_patterns_holder_destroy(patterns);

	return faces;
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
	FcValue fc_file, fc_index, fc_matrix, fc_pixel_size;
	FT_Matrix ft_matrix;
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
		//	autohint
		//	hinting
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

		result = FcPatternGet(patterns.patterns[i], FC_MATRIX, 0, &fc_matrix);
		if (result == FcResultMatch) {
			ft_matrix.xx = fc_matrix.u.m->xx;
			ft_matrix.xy = fc_matrix.u.m->xy;
			ft_matrix.yx = fc_matrix.u.m->yx;
			ft_matrix.yy = fc_matrix.u.m->yy;
			// apply the matrix
			FT_Set_Transform(
				faces.faces[faces.length],
				&ft_matrix,
				NULL);
		}

		result = FcPatternGet(patterns.patterns[i], FC_PIXEL_SIZE, 0, &fc_pixel_size);
		if (result != FcResultMatch || fc_pixel_size.u.d == 0) {
			fprintf(stderr, "font has no pixel size, using 12 by default");
			fc_pixel_size.type = FcTypeInteger;
			fc_pixel_size.u.i = 12;
		}
		//error = FT_Set_Pixel_Sizes(
		//	faces.faces[faces.length],
		//	0, // width
		//	fc_pixel_size.u.i); // height

		// TODO get screen resolution or pass as parameter or
		// using Xresources (xrm) xcb_xrm_resource_get_string Xft.dpi
		// pixel_size/ (resolution/72.0)
		FT_Set_Char_Size(
			faces.faces[faces.length], 0,
			(fc_pixel_size.u.d/(90.0/72.0))*64,
			90, 90);
		if (error != FT_Err_Ok) {
			perror(NULL);
			fprintf(stderr, "could not char size");
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

#endif // _XCBFT
