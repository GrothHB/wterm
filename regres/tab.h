#ifndef TAB_H
#define TAB_H
#include <stdint.h>
#include "my_term.h"
#include "openbsd.h"

/*extern char *get_looking_word(struct cpkg *);*/
char 		*fill_command(char *, char*, enum search_m *);
struct word_t 	parse(struct cpkg *);
char		**exec_paths(int *);

#endif /* TAB_H */
