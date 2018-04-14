#ifndef _UTF8_UTILS_
#define _UTF8_UTILS_

#include <fontconfig/fontconfig.h>

struct utf_holder {
	FcChar32 *str;
	unsigned int length;
};
struct utf_holder char_to_uint32(char *str);
void utf_holder_destroy(struct utf_holder holder);

struct utf_holder
char_to_uint32(char *str)
{
	struct utf_holder holder;
	FcChar32 *output = NULL;
	int length = 0, shift = 0;

	// there should be less than or same as the strlen of str
	output = (FcChar32 *)malloc(sizeof(FcChar32)*strlen(str));
	if (!output) {
		puts("couldn't allocate mem for char_to_uint32");
	}

	while (*(str+shift)) {
		shift += FcUtf8ToUcs4(
			(FcChar8*)(str+shift),
			output+length,
			strlen(str)-shift
		);
		length++;
	}

	holder.length = length;
	holder.str = output;

	return holder;
}

void
utf_holder_destroy(struct utf_holder holder)
{

	free(holder.str);
}

#endif
