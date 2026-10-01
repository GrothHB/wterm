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
	int			old_curs;
	(void)c;
	pk->mode ^= 0x1;
	if (pk->rvs_action == NO_ACTION) {
		pk->rvs_action = 0;
		return;
	}
	if (pk->mode) {
		pk->tptr[LF] = rvs_search_mode;
		pk->ps[strlen(pk->ps) - 2] = '?';
		old_curs = pk->com.idx;
		burn_n_rebuild(pk, HISTORY, old_curs);
	} else {
		pk->tptr[LF] = clear_comm;
		pk->ps[strlen(pk->ps) - 2] = '$';
		old_curs = pk->com.idx;
		burn_n_rebuild(pk, HISTORY, old_curs);
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
	if (pk->com.idx == 0) {
		list_commands(pk->hst_num - rest_comm, pk, &rest_comm);
		if (rest_comm == 0)
			goto clean;
		return;
	}
	for (i = 0; i < pk->com.idx; ++i) {
		if (pk->com.command[i] != ' ')
			break;
	}
	if (i == pk->com.idx) {
		return;
	}
	if (pk->word_search != NULL && pk->word_search != pk->history) {
		free(pk->word_search);
		pk->word_search = NULL;
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
			list_commands(0, pk, &rest_comm);
			if (rest_comm == 0)
				break;
			return;
		}
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
		pk->word_search = pk->history;
		list_commands(i, pk, &rest_comm);
		if (rest_comm > 0)
			return;
		break;
	}
	clean:
	pk->com.command[0] = '\0';
	pk->com.idx = 0;
	pk->curs = 0;
	if (pk->word_search != NULL && pk->word_search != pk->history)
		pk->word_search = NULL;
	burn_n_rebuild(pk, HISTORY, old_curs);
}

void
list_commands(int base, struct cpkg *pk, int *cmdlf)
{
	char			buf[MAX_SIZE_FOR_PRINT];
	size_t			bsize;
	int 			lines_printed, num, i, rvs_disp_max;
	size_t 			len;
	static int inter = 1;
	bsize = sizeof(buf);
	lines_printed = i = len = 0;
	rvs_disp_max = 79 - strlen(pk->ps);
	if (base) {
		if (base > (int)pk->hst_num - 2)
			return;
		inter = base;
	}
	while ((i = (int)(pk->hst_num - inter)) >= 0) {
		int 		idx;
		int hidx = 0;
		if (lines_printed == 15)
			break;
		size_t wsize = strlen(pk->word_search[i]);
		len += snprintf(&buf[len], bsize - len, "\n\r%*d",
				3, inter);
		buf[len++] = ':';
		buf[len++] = ' ';
		for (idx = 0; idx < (int)wsize; ++idx) {
			if (idx > 0 && idx % rvs_disp_max == 0) {
				buf[len++] = '\n';
				buf[len++] = '\r';
				len += snprintf(&buf[len], bsize- len,
						"%-*s  ", 3, "");
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
		burn_n_rebuild(pk, HISTORY, 0);
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
	char 			word[MAX_CANON + 50], **tmp;
	int			i;
	if (pk->word_search != NULL)
		return;
	pk->word_search = pk->history;
	i = 0;
	while (base <= pk->com.idx) {
		char c = pk->com.command[base];
		if (c == ' ' || c == '\0') {
			if (i > 0) {
				word[i] = '\0';
				tmp = search_word(pk->word_search, word,
						  &pk->hst_num);
				if (pk->word_search != pk->history)
					free(pk->word_search);
				pk->word_search = tmp;
				if (pk->word_search == NULL) {
					pk->hst_num = 0;
					return;
				}
				++base;
				i = 0;
				continue;
			}
		} else {
			word[i++] = pk->com.command[base++];
		}
	}
}
