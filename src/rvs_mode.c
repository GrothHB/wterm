#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include "my_term.h"
#include "openbsd.h"
#include "sterm.h"
#include "tab_compl.h"
#include "rvs_mode.h"

#define OPENBSD_MAX 80 /* style(9) 80 column max */
#define PRESSED_ENTER 0x1
#define MAX_SIZE_FOR_PRINT (MAX_CANON + (MAX_CANON / OPENBSD_MAX) * \
			    2 + 24)
#define WORD_SEARCH 0xF
#define WRONG_EXIT_PAGING INT_MAX
#define ABORD_ACTION INT_MIN

static char              **search_word(char **, const char *, size_t *);
static void              list_commands(int *, struct cpkg *);
static void              paging(struct cpkg *, enum lex_paging, ...);
static void              rvs_search(int, struct cpkg *);
static void              rvs_search_back(char, struct cpkg *);
static void              take_command(int, struct cpkg *);

static char **
search_word(char **hist, const char *lw, size_t *nf)
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
		if (c_idx == c_size - 1) {
			assert(c_size < (SIZE_MAX / 2));
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
	if (pk->mode) {
		pk->tptr[LF] = rvs_search_mode;
		pk->tptr['b'] = rvs_search_back;
		pk->tptr[HT] = neutral;
		pk->ps[strlen(pk->ps) - 2] = '?';
		old_curs = pk->com.idx;
		burn_n_rebuild(pk, HISTORY, old_curs);
	} else {
		pk->tptr[LF] = clear_comm;
		pk->tptr['b'] = disp_stream;
		pk->tptr[HT] = find_fill;
		pk->ps[strlen(pk->ps) - 2] = '$';
		old_curs = pk->com.idx;
		paging(pk, PAG_CLEAR);
		burn_n_rebuild(pk, HISTORY, old_curs);
	}
}

void
set_vil_mode(void (**ascii)(char, struct cpkg *))
{
	(void)**ascii;
}

static void
take_command(int indx, struct cpkg *pk)
{
	int		i;
	char		*endptr;
	size_t		wsize;
	i = strtol(&pk->com.command[indx], &endptr, 0);
	if (*endptr != '\0' || i < 0)
		return;
	i--;
	if ((size_t)i >= pk->hst_num)
		return;
	assert(pk->word_search!= NULL);
	wsize = strlen(pk->word_search[i]);
	assert(wsize <= MAX_CANON);
	memcpy(pk->com.command, pk->word_search[i], wsize + 1);
	pk->curs = wsize;
	set_cursor(pk->curs, pk);
	pk->com.idx = pk->curs;
	set_rvs_mode((char)i, pk);
	
}
void
rvs_search_mode(char c, struct cpkg *pk)
{
	int 			i, old_curs;
	char			*endptr;
	(void)c;
	i = 0;
	old_curs = pk->curs;
	if (pk->com.idx == 0) {
		if (pk->word_search == NULL) {
			pk->word_search = pk->history;
			pk->hst_num = pk->com.max_hst_num;
		}
		paging(pk, PAG_HANDLING_SPACE);
		return;
	}
	if (pk->word_search == NULL) {
		pk->word_search = pk->history;
		pk->hst_num = pk->com.max_hst_num;
	}
	for (; i < pk->com.idx; ++i) {
		if (pk->com.command[i] != ' ')
			break;
	}
	switch (pk->com.command[i]) {
	case 's':
		if (pk->word_search != NULL &&
		    pk->word_search != pk->history) {
			free(pk->word_search);
			pk->word_search = NULL;
		}
		if (pk->com.command[i + 1] == 's' &&
		    pk->com.command[i + 2] == ' ') {
			char **tmp;
			const char *pt;
			size_t *hn;
			hn = &pk->hst_num;
			pt = &pk->com.command[i + 3];
			if ((tmp = search_word(pk->history, pt, hn)) == NULL)
				break;
			pk->word_search = tmp;
			paging(pk, PAG_COMMANDS);
			return;
		}
		if (pk->com.command[i + 1] != ' ')
			break;
		rvs_search(i + 2, pk);
		if (pk->hst_num == 0)
			break;
		paging(pk, PAG_COMMANDS);
		return;
	case 't':
		if (pk->com.command[i + 1] != ' ')
			break;
		take_command(i + 2, pk);
		return;
	case 'l':
		if (pk->word_search != NULL &&
		    pk->word_search != pk->history) {
			free(pk->word_search);
			pk->word_search = NULL;
		}
		if (pk->com.command[i + 1] != ' ')
			break;
		i = strtol(&pk->com.command[i + 2], &endptr, 0);
		if (*endptr != '\0' || i < 0)
			break;
		i--;
		if ((size_t)i > pk->hst_num)
			break;
		paging(pk, PAG_SPEC_COMM, &i);
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

static void
list_commands(int *base, struct cpkg *pk)
{
	char			buf[MAX_SIZE_FOR_PRINT];
	size_t			bsize;
	int 			lines_printed, rvs_disp_max;
	size_t 			len;
	bsize = sizeof(buf);
	lines_printed = len = 0;
	rvs_disp_max = 79 - strlen(pk->ps);
	assert(rvs_disp_max > 0);
	while (*base < (int)pk->hst_num) {
		int 		idx;
		int hidx = 0;
		if (lines_printed >= pk->rvs_search_hst_max_prt)
			break;
		size_t wsize = strlen(pk->word_search[*base]);
		if (len + wsize + 35 > bsize) {
			write(STDOUT_FILENO, buf, len);
			len = 0;
		}
		len += snprintf(&buf[len], bsize - len, "\n\r%*d",
				3, *base + 1);
		buf[len++] = ':';
		buf[len++] = ' ';
		for (idx = 0; idx < (int)wsize; ++idx) {
			if (len + 9 > bsize) {
				write(STDOUT_FILENO, buf, len);
				len = 0;
			}
			if (idx > 0 && idx % rvs_disp_max == 0) {
				buf[len++] = '\n';
				buf[len++] = '\r';
				len += snprintf(&buf[len], bsize- len,
						"%-*s  ", 3, "");
			}
			buf[len++] = pk->word_search[*base][hidx++];
		}
		buf[len] = '\0';
		(*base)++;
		++lines_printed;
	}
	len += snprintf(&buf[len], bsize - len, "\n\r");
	write(STDOUT_FILENO, buf, len);
	if (*base < (int)pk->hst_num) {
		char msg[50];
		size_t cmdlf;
		cmdlf = pk->hst_num - (size_t)*base;
		len = snprintf(msg, 50, "there are %d commands left\n\r",
			       (int)cmdlf);
		write(STDOUT_FILENO, msg, len);
		burn_n_rebuild(pk, HISTORY, 0);
		return;
	}
	pk->com.command[0] = '\0';
	pk->com.idx = 0;
	pk->curs = 0;
	burn_n_rebuild(pk, HISTORY, 0);
}

static void
rvs_search(int base, struct cpkg *pk)
{
	char 			word[MAX_CANON + 50], **tmp;
	int			i;
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
			}
			++base;
		} else {
			word[i++] = pk->com.command[base++];
		}
	}
}

static void
rvs_search_back(char c, struct cpkg *pk)
{
	if (pk->com.idx == 0)
		paging(pk, PAG_HANDLING_BACK);
	else
		disp_stream(c, pk);
}

static void
paging(struct cpkg *pk, enum lex_paging state, ...)
{
	va_list		ap;
	static int base = 0;
	va_start(ap, state);
	if (state == PAG_HANDLING_SPACE) {
		if (base >= (int)pk->hst_num) {
			base = 0;
			if (pk->word_search != NULL &&      
			    pk->word_search != pk->history) {
				free(pk->word_search);
				pk->word_search = pk->history;
				pk->hst_num = pk->com.max_hst_num;
			}
		}
		list_commands(&base, pk);
	} else if (state == PAG_HANDLING_BACK) {
		uint8_t 	vl;
		if (base == 0 || pk->word_search == NULL) {
			va_end(ap);
			return;
		}
		assert(pk->rvs_search_hst_max_prt > 0);
		vl = base % pk->rvs_search_hst_max_prt;
		if (vl) {
			base = pk->hst_num - vl;
			base -= pk->rvs_search_hst_max_prt;
			base = base < 0? 0: base;
			list_commands(&base, pk);
			va_end(ap);
			return;
		}
		base += -2 * pk->rvs_search_hst_max_prt;
		if (base < 0) {
			base = 0;
			va_end(ap);
			return;
		}
		list_commands(&base, pk);
	} else if (state == PAG_COMMANDS) { 
		base = 0;
		list_commands(&base, pk);
		if (base < (int)pk->hst_num) {
			int old_curs = pk->curs;
			pk->com.command[0] = '\0';
			pk->com.idx = 0;
			pk->curs = 0;
			burn_n_rebuild(pk, HISTORY, old_curs);
		}
	} else if (state == PAG_SPEC_COMM) {
		int *val = va_arg(ap, int *);
		list_commands(val, pk);
		base = *val;
	} else if (state == PAG_CLEAR) {
		if (pk->word_search != NULL &&
		    pk->word_search != pk->history) {
			free(pk->word_search);
			pk->word_search = NULL;
		}
		base = 0;
	}
	va_end(ap);
}
