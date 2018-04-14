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

static FcStrSet*
extract_fontsearch_list(char *string)
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

int
main(int argc, char** argv)
{
	FcStrSet *fontsearch;
	if (argc < 3) {
		puts("pass the fonts and the text");
		return 1;
	}

	fontsearch = extract_fontsearch_list(argv[1]);

	// safely iterate over set
	FcStrList *iterator = FcStrListCreate(fontsearch);
	FcChar8 *fontquery = NULL;
	FcStrListFirst(iterator);
	while ((fontquery = FcStrListNext(iterator)) != NULL) {
		printf("fontquery: %s\n", fontquery);
	}
	FcStrListDone(iterator);
	// end of safely iterate over set

	struct utf_holder holder;
	holder = char_to_uint32(argv[2]);
	int i = 0;
	for (; i < holder.length; i++) {
		printf("%02x\n", holder.str[i]);
	}

	FcStrSetDestroy(fontsearch);
	return 0;
}

