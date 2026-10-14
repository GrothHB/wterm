#ifndef OPENBSD_H
#define OPENBSD_H

#include "config.h"

#ifndef HAVE_STRLCPY
extern size_t strlcpy(char *, char *, size_t);
#endif /* HAVE_STRLCPY */

#ifndef HAVE_REALLOCARRAY
extern void *reallocarray(void *, size_t, size_t);
#endif /* HAVE_REALLOCARRAY */

#endif /* OPENBSD_H */
