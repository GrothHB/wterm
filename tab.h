#ifndef TAB_H
#define TAB_H
#include "my_term.h"
#include "config.h"

/*extern char *get_looking_word(struct cpkg *);*/
extern char 		*fill_command(char *, char*);
extern word_t 		parse(struct cpkg *);
extern char		**exec_paths(int *);
#ifndef HAVE_STRLCPY
extern size_t strlcpy(char *, char *, size_t);
#endif /* HAVE_STRLCPY */

#ifndef HAVE_REALLOCARRAY
extern void *reallocarray(void *, size_t, size_t);
#endif /* HAVE_REALLOCARRAY */

#endif /* TAB_H */
