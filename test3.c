#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "my_term.h"
#include "openbsd.h"
#include "sterm.h"
#include "revs.h"

char **
search_word(char **hist, char *lw, size_t *nf, size_t *mxl)
{
	char		**sh, **tmp;
	size_t		c_idx, c_size, max_len, wsize, lw_size;
	int		i;

	c_idx = max_len = 0;
	c_size = 16;
	lw_size = strlen(lw);
	if ((sh = malloc(c_size * sizeof(char *))) == NULL)
		return (NULL);
	for (i = 0; hist[i] != NULL; ++i) {
		if (strstr(hist[i], lw) == NULL && lw_size > 0) {
			continue;
		}
		wsize = strlen(hist[i]);
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
		max_len = (wsize > max_len) ? wsize: max_len;
	}
	*nf = c_idx;
	*mxl = max_len;
	return (sh);
}

void
set_rvs_mode(char c, struct cpkg *pk)
{
	(void)c;
	pk->mode ^= 0x1;
	if (pk->revs_action == NO_ACTION) {
		pk->revs_action = 0;
		return;
	}
	if (pk->mode) {
		uint8_t		i;
		for (i = 0; i < 128; ++i) {
			if (pk->tptr[i] == disp_stream)
				pk->tptr[i] = rvs_fst_disp_pars;
		}
		pk->tptr[LF] = list_hist;
		pk->tptr[HT] = neutral;
		pk->tptr[REVS_LEX_SEARCH] = set_search;
		pk->tptr[REVS_LEX_TAKE] = take_command;
		pk->tptr[REVS_LEX_DISP] = list_hist;
	} else {
		init_ascii(pk->tptr);
	}
}

void
rvs_fst_disp_pars(char c, struct cpkg *pk)
{
	disp_stream(c, pk);
	pk->rvs_action = NO_ACTION;
	init_ascii(pk->tptr);
	pk->tptr[LF] = serv_wrong_exit;
	pk->tptr[DEL] = rvs_wex_delete;
}

void
rvs_wex_delete(char c, struct cpkg *pk)
{
	int		i;
	cans(c, pk);
	if (pk->com.idx == 0) {
		set_rvs_mode(c, pk);
		return;
	}
	for (i = pk->curs; i >= 0; --i) {
		if (pk->com.command[i] != ' ')
			break;
	}
	if (i != 0)
		return;
	set_rvs_mode(c, pk);
}

void
set_search(char c, struct cpkg *pk)
{
	disp_stream(c, pk);
	init_search(pk->tptr);
	pk->mode = 1;
}

void
init_search(void (**ascii)(char, struct cpkg *))
{
	uint8_t		i;
	for (i = 0; i < (uint8_t)128; ++i) {
		if (ascii[i] == rvs_fst_disp_pars)
			ascii[i] = rvs_fst_search_mode;
		else if (i == (uint8_t)LF)
			ascii[i] = init_single_search;
		else if (i == (uint8_t)DEL)
			ascii[i] = watch_input_search_bgn;
		else if (i == (unit8_t)ESC)
			ascii[i] = movm_search;
	}
}

void
watch_input_search_bgn(char c, struct cpkg *pk)
{
	int		i;
	cans(c, pk);
	if (pk->curs > 0 && pk->com.command[pk->curs - 1] == 's') {
		init_search(pk->tptr);
		return;
	}
	pk->mode = 0;
	set_rvs_mode(c, pk);
}

void
movm_search(char c, struct cpkg *pk)
{
	int old_curs = pk->curs;
	spec(c, pk);
	if (old_curs < pk->curs) {
		if (pk->com.command[pk->curs] == ' ') {
			if (searched_words == 0) {
				pk->tptr[DEL] = watch_btw_wrds;
				pk->tptr[ESC] = movm_btw_search;
			}
		}
		return;
	} else {
		uint8_t 	i;
		if (pk->com.command[pk->curs] == 's')
			return;
		for (i = 0; i < (uint8_t)128; ++i) {
			if (ascii[i] == rvs_fst_search_mode)
				ascii[i] = rvs_bf_search;
			else if (ascii[i] == movm_search)
				ascii[i] = movm_bf_search;
			else if (ascii[i] == watch_input_search_bgn)
				ascii[i] = cans;
		}
	}
}


/*
void
change_clear_comm(char c, struct cpkg *pk)
{
	unsigned int		i, num, wfs_idx, wfs_size;
	uint8_t			wfs_flag;
	enum revs_lex		lexs;
	enum revs_state		state;
	char			c, **wfs;
	lexs = NORMALL;
	wfs_size = 4;
	wfs_idx = 0;
	if ((wfs = malloc(sizeof(char *) * wfs_size)) == NULL) {
		wfs_flag = 0;
	} else {
		wfs_flag = 1;
		wfs[wfs_idx] = NULL;
	}
	for (i = 0; i < pk->com.idx; ++i) {
		c = pk->com.command[i];
		switch (lexs) {
		case NORMALL:
			switch (c) {
			case 's':
				lexs = SEARCH;
				break;
			case 'g':
				lexs = GET;
				break;
			case 'l':
				lexs = DISPL;
				break;
			case ' ':
				break;
			default:
				goto clean;
			}
			break;
		case DISPL:
			if (c != ' ')
				goto clean;
			state = LISTC;
			lexs = WORK;
			break;
		case GET:
			if (c != ' ')
				goto clean;
			state = PUTC;
			lexs = WORK;
			break;
		case SEARCH:
			if (c == 's')
				state = SSTRING;
			else if (c == ' ')
				state = SWORD;
			else
				goto clean;
			lexs = WORK;
			break;
		case WORK:
			switch (state) {
			case SWORD:
				if (!wfs_flag)
					return;
				switch (c) {
				case ' ':
					break;
	clean:
	if (wfs_flag) {
		if (wfs_idx > 0) {
			char **tmp = wfs;
			while (*tmp != NULL)
				free(*tmp++);
		}
		free(wfs);
	}
	return;
}
*/
