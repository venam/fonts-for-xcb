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
		// DEBUG
		FcPatternPrint(pat_output);
		// DEBUG
		return pat_output;
	} else if (result == FcResultNoMatch) {
		fprintf(stderr, "there wasn't a match");
	} else {
		fprintf(stderr, "the match wasn't as good as it should be");
	}
	return NULL;
}

struct xcbft_patterns_holder {
	FcPattern **patterns;
	uint8_t length;
};

// need to store the size of the array somewhere
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
		printf("fontquery: %s\n", fontquery);
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
xcbft_free_patterns_holder(struct xcbft_patterns_holder patterns)
{
	int i = 0;

	for (; i < patterns.length; i++) {
		FcPatternDestroy(patterns.patterns[i]);
	}
	free(patterns.patterns);
	FcFini();
}

int
main(int argc, char** argv)
{
	FcStrSet *fontsearch;
	struct xcbft_patterns_holder font_patterns;
	int i = 0;
	struct utf_holder holder;

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

	xcbft_free_patterns_holder(font_patterns);

	holder = char_to_uint32(argv[2]);
	for (i = 0; i < holder.length; i++) {
		printf("%02x\n", holder.str[i]);
	}
	utf_holder_destroy(holder);

	return 0;
}

