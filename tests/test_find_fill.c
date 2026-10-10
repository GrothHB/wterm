#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../my_term.h"
#include "../tab.h"

void
find_fill(char c, struct cpkg *pk)
{
        char            path[PATH_MAX], word[MAX_CANON + 1];
        char            *dummy, **xdirs, *dir, *tm;
        int             fidx, sofw, xf, i, flag;
        struct word_t   sword;
        size_t          wsize;
        (void)c;
        xf = i = flag = 0;
        dummy = NULL;
        sword = parse(pk);
        sofw = pk->curs - sword.sidx;
        if (sofw > MAX_CANON)
                sofw = MAX_CANON;
        strlcpy(word, &pk->com.command[sword.sidx], sofw + 1);
        wsize = strlen(word);
	printf("%d tyle wynosi wlasnie dlugosc a taki jest ostatni char: %c", wsize, word[wsize]);

}

int
main(void)
{
	struct cpkg  	t;
	int		i;
	t.com.command = malloc(256);
	if (t.com.command == NULL)
		return (1);
	for (i = 0; i < 255; ++i)
		t.com.command[i] = 'A';
	t.com.command[i] = '\0';
	t.com.idx = 255;
	t.curs = 255;
	find_fill('A', &t);

	return (0);
}
