#include "config.h"

#include <sys/types.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MUL_NO_OVERFLOW ((size_t)1 << (sizeof(size_t) * 4))
 
#ifndef HAVE_STRLCPY
size_t
strlcpy(char *dst, const char *src, size_t dsize)
{
	const char *osrc = src;
	size_t nleft = dsize;

	if (nleft != 0) {
		while (--nleft != 0) {
			if ((*dst++ = *src++) == '\0')
				break;
		}
	}

	if (nleft == 0) {
		if (dsize != 0)
			*dst = '\0';
		while (*src++)
			;
	}

	return (src - osrc - 1);
}
#endif /* HAVE_STRLCPY */

#ifndef HAVE_REALLOCARRAY
void *
reallocarray(void *optr, size_t nmemb, size_t size)
{
	if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) &&
	    nmemb > 0 && SIZE_MAX /nmemb < size) {
		errno = ENOMEM;
		return (NULL);
	}

	return (realloc(optr, size * nmemb));
}
#endif /* HAVE_REALLOCARRAY */
