#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include "my_term.h"
#include "openbsd.h"
#include "sterm.h"
#include "test3.h"

#define OPENBSD_MAX 77 /* style(9) 80 column max */
#define MAX_SIZE_FOR_PRINT (MAX_CANON + (MAX_CANON / OPENBSD_MAX) * \
			    2 + 24)
#define WORD_SEARCH 0xF

char **
search_word(char **hist, char *lw, size_t *nf)
{
	char		**sh, **tmp;
	size_t		c_idx, c_size;
	int		i;

	c_idx = 0;
	c_size = 16;
	if ((sh = malloc((c_size + 1)* sizeof(char *))) == NULL)
		return (NULL);
	for (i = 0; hist[i] != NULL; ++i) {
		if (strstr(hist[i], lw) == NULL) {
			continue;
		}
		if (c_idx == c_size) {
			c_size *= 2;
			tmp = reallocarray(sh, c_size, sizeof(char *));
			if (tmp == NULL) {
				free(sh);
				return (NULL);
			}
			sh = tmp;
		}
		sh[c_idx++] = hist[i];
		sh[c_idx] = NULL;
	}
	*nf = c_idx;
	if (c_idx == 0) {
		free(sh);
		sh = NULL;
	}
	return (sh);
}

void
set_rvs_mode(char c, struct cpkg *pk)
{
	static char 		*ptr_ps;
	static const char	rvs_ps[] = "@ ";
	(void)c;
	pk->mode ^= 0x1;
	if (pk->rvs_action == NO_ACTION) {
		pk->rvs_action = 0;
		return;
	}
	if (pk->mode) {
		pk->tptr[LF] = rvs_search_mode;
		ptr_ps = pk->ps;
		pk->ps = rvs_ps;
	} else {
		pk->tptr[LF] = clear_comm;
		pk->ps = ptr_ps;
	}
}

void
set_vil_mode(void (**ascii)(char, struct cpkg *))
{
	(void)**ascii;
}

void
take_command(int c, struct cpkg *pk)
{
	(void)c;
	(void)*pk;
}
void
rvs_search_mode(char c, struct cpkg *pk)
{
	int 			i, old_curs;
	char			*endptr;
	static int rest_comm = 0;
	(void)c;
	old_curs = pk->curs;
	if (pk->com.idx == 0 && rest_comm == 0) {
		if (pk->word_search == NULL)
			pk->word_search = pk->history;
		list_commands(0, pk, &rest_comm);
		if (pk->last_list_st == WORD_SEARCH && rest_comm == 0) {
			free(pk->word_search);
			pk->word_search = NULL;
		}
		return;
	}
	for (i = 0; i < pk->com.idx; ++i) {
		if (pk->com.command[i] != ' ')
			break;
	}
	if (i == pk->com.idx) {
		return;
	}
	switch (pk->com.command[i]) {
	case 's':
		if (pk->com.command[i + 1] != ' ')
			break;
		rvs_search(i + 2, pk);
		if (pk->hst_num  == 0)
			break;
		list_commands(0, pk, &rest_comm);
		if (rest_comm > 0) {
			pk->last_list_st = WORD_SEARCH;
			return;
		}
		free(pk->word_search);
		pk->word_search = NULL;
		break;
	case 't':
		take_command(i, pk);
		break;
	case 'l':
		if (pk->com.command[i + 1] != ' ')
			break;
		i = strtol(&pk->com.command[i + 2], &endptr, 0);
		if (*endptr != '\0' || i < 0)
			break;
		if (*endptr != pk->com.command[i + 2])
			break;
		pk->word_search = pk->history;
		list_commands(i, pk, &rest_comm);
		if (rest_comm > 0)
			return;
		break;
	}
	pk->com.command[0] = '\0';
	pk->com.idx = 0;
	pk->curs = 0;
	if (pk->word_search != NULL)
		pk->word_search = NULL;
	burn_n_rebuild(pk, HISTORY, old_curs);
}

void
list_commands(int base, struct cpkg *pk, int *cmdlf)
{
	char			buf[MAX_SIZE_FOR_PRINT];
	size_t			bsize;
	int 			lines_printed, num, i;
	size_t 			len;
	static int inter = 1;
	bsize = sizeof(buf);
	lines_printed = i = len = 0;
	if (base) {
		if (base > (int)pk->hst_num - 2)
			return;
		inter = base;
	}
	while ((i = (int)(pk->hst_num - inter)) >= 0) {
		int 		idx;
		int hidx = 0;
		if (lines_printed == 2)
			break;
		size_t wsize = strlen(pk->word_search[i]);
		len += snprintf(&buf[len], bsize - len, "\n\r%d", inter);
		buf[len++] = ':';
		for (idx = 0; idx < (int)wsize; ++idx) {
			if (idx > 0 && idx % OPENBSD_MAX == 0) {
				buf[len++] = '\n';
				buf[len++] = '\r';
			}
			buf[len++] = pk->word_search[i][hidx++];
		}
		buf[len] = '\0';
		inter++;
		++lines_printed;
	}
	len += snprintf(&buf[len], bsize - len, "\n\r");
	write(STDOUT_FILENO, buf, len + 1);
	num = pk->hst_num - inter;
	if (num > 0) {
		char msg[50];
		*cmdlf = num;
		len = snprintf(msg, 50, "there are %d commands left\n\r", num);
		write(STDOUT_FILENO, msg, len);
		return;
	}
	inter = 1;
	*cmdlf = 0;
	pk->com.command[0] = '\0';
	pk->com.idx = 0;
	pk->curs = 0;
	burn_n_rebuild(pk, HISTORY, 0);
}

void
rvs_search(int base, struct cpkg *pk)
{
	char			word[MAX_CANON];
	char			**tmp;
	int i = 0;
	uint8_t state = 0;
	uint8_t hflag = 0;
	word[0] = '\0';
	while (base < pk->com.idx) {
		if (pk->com.command[base] == ' ' && state) {
			if (pk->word_search == NULL) {
				pk->word_search = pk->history;
				hflag = 1;
			}
			tmp = search_word(pk->word_search, word,
					  &pk->hst_num);
			if (!hflag) {
				free(pk->word_search);
				if (pk->hst_num == 0) {
					pk->word_search = NULL;
					return;
				}
			}
			pk->word_search = tmp;
			state = 0;
			i = 0;
		} else {
			word[i++] = pk->com.command[base++];
			word[i] = '\0';
			state = 1;
		}
	}
	if (base > 0 && pk->com.command[base - 1] != ' ') {
		if (pk->word_search == NULL) {
			pk->word_search = pk->history;
			hflag = 1;
		}
		tmp = search_word(pk->word_search, word, &pk->hst_num);
		if (pk->hst_num == 0 && !hflag) {
			free(pk->word_search);
			pk->word_search = NULL;
			return;
		}
		if (!hflag)
			free(pk->word_search);
		pk->word_search= tmp;
	}
}
