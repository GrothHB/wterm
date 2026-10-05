#define TEST 1
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../my_term.h"
#include "../test3.h"
#include "../sterm.h"
#include "../sterm.c"
#include "../tab.h"

void
test_rvs_search_mode(char c, struct cpkg *pk)
{
	int 			i, old_curs;
	char			*endptr;
	(void)c;
	i = 0;
	for (; i < pk->com.idx; ++i) {
		if (pk->com.command[i] != ' ')
			break;
	}
	switch (pk->com.command[i]) {
	case 's':
		if (pk->com.command[i + 1] == 's' &&
		    pk->com.command[i + 2] == ' ') {
			char		**tmp, *pt;
			size_t		*hn;
			hn = &pk->hst_num;
			pt = &pk->com.command[i + 3];
			if ((tmp = search_word(pk->history, pt, hn)) == NULL)
				break;
			pk->word_search = tmp;
			paging(pk, LEX_COMMANDS);
			return;
		}
		if (pk->com.command[i + 1] != ' ')
			break;
		rvs_search(i + 2, pk);
		if (pk->hst_num == 0)
			break;
		paging(pk, LEX_COMMANDS);
		return;
	case 't':
		if (pk->com.command[i + 1] != ' ')
			break;
		take_command(i + 2, pk);
		return;
	case 'l':
		if (pk->com.command[i + 1] != ' ')
			break;
		i = strtol(&pk->com.command[i + 2], &endptr, 0);
		if (*endptr != '\0' || i < 0)
			break;
		i--;
		if ((size_t)i > pk->hst_num)
			break;
		paging(pk, LEX_SPEC_COMM, &i);
		break;
	}
	pk->com.command[0] = '\0';
	pk->com.idx = 0;
	pk->curs = 0;
	if (pk->word_search != NULL && pk->word_search != pk->history) {
		free(pk->word_search);
		pk->word_search = NULL;
	}
	burn_n_rebuild(pk, HISTORY, old_curs);
}

int
main(void)
{
	char		**input;
	struct cpkg 	t;
	int		i;
	if ((input = malloc(3 * sizeof(char *))) == NULL)
		return (1);
	input[1] = "s ";
	input[2] = "s  us]/|isabelle_client";
	input[0] = "s  /usr/bin/";
	for (i = 0; i < 3; ++i) {
		t.com.command = input[i];
		t.com.idx = strlen(t.com.command);
		t.curs = strlen(t.com.command);
		test_rvs_search_mode('A', &t);
	}

	return (0);
}
