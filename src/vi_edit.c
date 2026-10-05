#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include "my_term.h"
#include "sterm.h"
#include "test3.h"
#include "vi_edit.h"

static uint8_t mult = 0;
static enum grammar gramma_state = GRAMMAR_NONE;

void
set_vi_edit_mode(void (**ascii)(char, struct cpkg *))
{
	int 		i;
	for (i = 0; i < 128; ++i)
		ascii[i] = neutral;
	ascii['k'] = ve_move_up;
	ascii['j'] = ve_move_down;
	ascii['h'] = ve_move_left;
	ascii['l'] = ve_move_right;
	ascii['e'] = ve_jmp_end_of_word;
	ascii['c'] = ve_gramma;
	ascii['b'] = ve_jmp_beg_of_word;
	ascii['B'] = ve_jmp_beg;
	ascii['E'] = ve_jmp_end;
	ascii['d'] = ve_gramma;
	ascii['y'] = ve_gramma;
	ascii['x'] = ve_delet_char;
	ascii['r'] = ve_change_char;
	ascii['p'] = ve_put;
	ascii['^'] = ve_jmp_beg_line;
	ascii['$'] = ve_jmp_end_line;
	ascii['u'] = ve_undo;
	ascii['U'] = ve_un_undo;
	ascii['i'] = ve_insert_mode;
	ascii[ESC] = ve_clear_comm;
}

void
ve_move_up(char c, struct cpkg *pk)
{
	(void)c;
	if (pk->curs - pk->screen_limit < 0)
		return;
	write(STDOUT_FILENO, "\033[A", 3);
}

void
ve_move_down(char c, struct cpkg *pk)
{
	int i = pk->curs + pk->screen_limit;
	(void)c;
	if (i > pk->com.idx || i > MAX_CANON)
		return;
	write(STDOUT_FILENO, "\033[B", 3);
}

void
ve_move_left(char c, struct cpkg *pk)
{
	(void)c;
	if (pk->curs % pk->screen_limit == 0)
		return;
	write(STDOUT_FILENO, "\033[D", 3);
}

void
ve_move_right(char c, struct cpkg *pk)
{
	(void)c;
	if (pk->curs == pk->com.idx)
		return;
	if (pk->curs > 0 && pk->curs == pk->screen_limit - 1)
		return;
	write(STDOUT_FILENO, "\033[C", 3);
}

void
ve_jmp_end_of_word(char c, struct cpkg *pk)
{
	char buf[15];
	size_t len;
	int i, cr;
	if (pk->curs == pk->com.idx)
		return;
	if (pk->com.command[pk->curs + 1] == ' ') {
		for (i = pk->curs + 1; i < pk->com.idx; ++i) {
			if (pk->com.command[i] != ' ')
				break;
		}
		if (i == pk->com.idx)
			return;
		cr = i;
	} else {
		cr = pk->curs;
	}
	for (i = cr; i < pk->com.idx; ++i) {
		c = pk->com.command[i];
		if (isalpha((unsigned char)c) || c == '_')
			continue;
		break;
	}
	if (pk->curs == i)
		return;
	if (i < 0)
		--i;
	if (i / pk->screen_limit - pk->curs / pk->screen_limit == 0) {
		pk->curs = i - 1;
		write(STDOUT_FILENO, "\033[B", 3);
		set_cursor(pk->curs, pk);
		return;
	}
	len = i - pk->curs - 1;
	len = snprintf(buf, sizeof(buf), "\033[%dC", len);
	write(STDOUT_FILENO, buf, len);
}

void
ve_wrong_exit(char c, struct cpkg *pk)
{
	(void)c;
	set_vi_edit_mode(pk->tptr);
}

void
change_word_action(char c, struct cpkg *pk)
{
	char *dst, *src, cp[MAX_CANON + 1];
	size_t size;
	int i;
	if (pk->curs == pk->com.idx)
		return;
	for (i = pk->curs; i < pk->com.idx; ++i) {
		c = pk->com.command[i];
		if (isalpha((unsigned char)c) || c == '_')
			continue;
		break;
	}
	size = pk->com.idx - i;
	strlcpy(cp, &pk->com.command[pk->curs], i - pk->curs + 1);
	handle_vreg(VREG_SAVE, pk, cp);
	handle_vreg(VREG_SNAP_SCR, pk);
	if (size == 0) {
		pk->com.command[pk->curs] = pk->com.command[i];
		pk->com.idx = pk->curs;
		return;
	}
	dst = &pk->com.command[pk->curs];
	src = &pk->com.command[i];
	memmove(dst, src, size + 1);
	pk->com.idx -= size;
	set_vi_edit_mode(pk->tptr);
}

void
handle_vreg(enum vi_state act, struct cpkg *pk, ...)
{
	static struct ve_regs st;
	va_list	ap;
	static uint8_t c_idx = 0;
	static uint8_t s_idx = 0;
	va_start(ap, pk);
	if (act == VREG_SET_ALL) {
		if ((st.snaps = malloc(sizeof(struct ve_snap) * 10)) == NULL)
			return;
		st.snaps[0].command = NULL;
		st.last_taken = NULL;
	} else if (act == VREG_CLEAR_ALL) {
		int i;
		if (st.snaps == NULL)
			return;
		for (i = 0; i < 10; ++i) {
			if (st.snap[i].command != NULL)
				free(st.snap[i].command);
		}
		free(st.snaps);
		if (st.last_taken != NULL)
			free(st.last_taken);
		c_idx = 0;
	} else if (act == VREG_SNAP_SCR) {
		if (s_idx == 10) {
			size_t wsize, size;
			char *tmp;
			wsize = strlen(pk->com.command);
			if ((tmp = malloc(wsize + 1)) == NULL)
				return;
			free(st.snap[0].command);
			size = sizeof(char *) * 9;
			memmove(&st.snap[0], &st.snap[1], size);
			strlcpy(tmp, pk->com.command, wsize + 1);
			st.snap[9] = tmp;
			st.snap[9].idx = pk->com.idx;
			st.snap[9].curs = pk->curs;
			c_idx = 9;
		} else {
			size_t wsize;
			char *tmp;
			wsize = strlen(pk->com.command);
			if ((tmp = malloc(wsize + 1)) == NULL)
				return;
			strlcpy(tmp, pk->com.command, wsize + 1);
			st.snap[s_idx++] = tmp;
			st.idx = pk->com.idx;
			st.curs = pk->curs;
			c_idx = s_idx - 1;
		}
	} else if (act == VREG_SAVE) {
		char *tmp;
		if ((tmp = strdup(va_arg(ap, char *))) == NULL)
			return;
		st.last_taken = tmp;
	} else if (act == VREG_PUT) {
		size_t wsize, csize;
		char *dst, *src;
		int old_curs;
		if (st.last_taken == NULL)
			return;
		wsize = strlen(st.last_taken);
		csize = strlen(pk->com.command);
		if (wsize + csize > MAX_CANON)
			return;
		src = &pk->com.command[pk->curs + 1];
		dst = &pk->com.command[pk->curs + wsize + 1];
		memmove(dst, src, pk->com.idx - pk->curs);
		memmove(src, st.last_taken, wsize);
		old_curs = pk->curs;
		pk->curs += wsize;
		pk->com.idx += wsize;
		burn_n_rebuild(pk, HISTORY, old_curs);
	} else if (act == VREG_UNDO) {
		int old_curs;
		char *ptr;
		if (s_idx == 0)
			return;
		old_curs = pk->curs;
		ptr = pk->com.command;
		strlcpy(ptr, st.snap[c_idx].command, MAX_CANON + 1);
		pk->curs = st.snap[c_idx].curs;
		pk->com.idx = st.snap[c_idx].idx;
		burn_n_rebuild(pk, HISTORY, old_curs);
		if (c_idx > 0)
			c_idx--;
	} else if (act == VREG_UN_UNDO) {
		int old_curs;
	 	char *ptr;
		if (c_idx + 1 == s_idx)
			return;
		++c_idx;
		old_curs = pk->curs;
		ptr = pk->com.command;
		strlcpy(ptr, st.snap[c_idx].command, MAX_CANON + 1);
		pk->curs = st.snap[c_idx].curs;
		pk->com.idx = st.snap[c_idx].idx;
		burn_n_rebuild(pk, HISTORY, old_curs);
	}
	va_end(ap);
}

void
jmp_beg_of_word(char c, struct cpkg *pk)
{
	char		buf[15];
	int		i, cr;
	size_t		len;
	uint8_t		y1, y2;
	(void)c;
	if (pk->curs == 0)
		return;
	if (pk->com.command[pk->curs - 1] == ' ') {
		for (i = pk->curs - 1; i >= 0; --i) {
			if (pk->com.command[i] != ' ')
				break;
		}
		if (i <= 0)
			return;
		cr = i;
	} else {
		cr = pk->curs;
	}
	for (i = cr; i >= 0; --i) {
		if (!isalpha((unsigned char)c) && c != '_')
			break;
	}
	i = i <= 0? 0: i + 1;
	y1 = pk->curs / pk->screen_limit;
	y2 = i / pk->screen_limit;
	pk->curs = i;
	if (y1 == y2) {
		set_cursor(pk->curs, pk);
	} else {
		len = snprintf(buf, sizeof(buf), "\033[%dA", y1 - y2);
		write(STDOUT_FILENO, buf, len);
		set_cursor(pk->curs, pk);
	}
}

void
ve_jmp_beg(char c, struct cpkg *pk)
{
	char		buf[20];
	int		i, y1, y2;
	size_t		len;
	(void)c;
	if (pk->curs == 0)
		return;
	if (pk->com.command[pk->curs - 1] == ' ') {
		for (i = pk->curs - 2; i >= 0; --i) {
			if (pk->com.command[i] != ' ')
				break;
		}
	}
	if (i == 0)
		return;
	for (; i >= 0; --i) {
		if (pk->com.command[i] == ' ')
			break;
	}
	if (i > 0)
		++i;
	y1 = i / pk->screen_limit;
	y2 = pk->curs / pk->screen_limit;
	if (y1 == y2) {
		pk->curs = i;
	}  else {
		len = snprintf(buf, sizeof(buf), "\033[%dA", y2 - y1);
		write(STDOUT_FILENO, buf, len);
	}
	set_cursor(pk->curs, pk);
}

void
ve_jmp_end(char c, struct cpkg *pk)
{
	char		buf[20];
	int		i, y1, y2;
	size_t		len;
	(void)c;
	if (pk->curs == pk->com.idx)
		return;
	if (pk->com.command[pk->curs + 1] == ' ') {
		for (i = pk->curs + 2; i < pk->com.idx; ++i) {
			if (pk->com.command[i] != ' ')
				break;
		}
	}
	if (i == pk->com.idx)
		return;
	for (; i < pk->com.idx; ++i) {
		if (pk->com.command[i] == ' ')
			break;
	}
	if (i < 0)
		++i;
	y1 = i / pk->screen_limit;
	y2 = pk->curs / pk->screen_limit;
	if (y1 == y2) {
		pk->curs = i;
	}  else {
		len = snprintf(buf, sizeof(buf), "\033[%dB",y1 - y2);
		write(STDOUT_FILENO, buf, len);
	}
	set_cursor(pk->curs, pk);
}

void
ve_get_mult(char c, struct cpkg *pk)
{
	char			*endptr, ptr[2];
	int			i;
	ptr[0] = c;
	ptr[1] = '\0';
	mult = strtol(ptr, &endptr, 0);
	if (*endptr != '\0' || endptr == ptr)
		mult = 0;
	for (i = '0'; i < '9' + 1; ++i)
		ascii[i] = ve_wrong_exit;
}

void
ve_gramma_act_word(char c, struct cpkg *pk)
{
	char *dst, *src;
	int del_ch;
	size_t size, fidx;
	(void)c;
	if (mult == 0 || grammar_state == GRAM_NONE) {
		grammar_state = GRAM_NONE;
		return;
	}
	fidx = find_last_word(pk);
	state = DS_LOOKING_WORD;
	if (mult > 0) {
		c_words = mult;
		mult = 0;
	}
	if (pk->curs == pk->com.idx) {
		set_vi_mode(pk->tptr);
		return;
	}
	if (grammar_state == GRAM_DELETE || grammar_state == GRAM_CHANGE_WORD)
		handle_vreg(VREG_SNAP_SCR, pk);
	dst = &pk->com.command[pk->curs];
	src = &pk->com.command[fidx];
	size = pk->com.idx - fidx + 1;
	if (grammar_state == GRAM_DELETE || grammar_state == GRAM_CHANGE_WORD) {
		memmove(dst, src, size);
		del_ch = fidx - pk->curs;
		pk->com.idx -= del_ch;
		if (pk->curs > 0 && pk->curs == pk->com.idx) {
			pk->curs--;
			burn_n_rebuild(pk, INLINE_CANS, 0);
		} else {
			burn_n_rebuild(pk, INLINE_ADD, 0);
		}
	} else if (grammar_state == GRAM_YANK) {
		char st[MAX_CANON + 1];
		strlcpy(st, src, size + 1);
		handle_vreg(VREG_SAVE, pk, st);
	}
	if (grammar_state == GRAM_CHANGE_WORD) {
		init_ascii(pk->tptr);
		grammar_state = GRAM_NONE;
	}
}

void
ve_gramma_act_to_end(char c, struct cpkg *pk)
{
	int old_curs;
	(void)c;
	if (pk->curs == pk->com.idx)
		return;
	if (grammar_state == GRAMMAR_DELETE) {
		handle_vreg(VREG_SNAP_SCR, pk);
		pk->com.idx = pk->curs;
		pk->com.command[pk->com.idx] = '\0';
		old_curs = pk->curs;
		if (pk->curs > 0)
			pk->curs--;
		burn_n_rebuild(pk, HISTORY, old_curs);
	} else if (grammar_state == GRAMMAR_YANK) {
		char st[MAX_CANON + 1];
		strlcpy(st, &pk->com.command[pk->curs], pk->com.idx - pk->curs);
		handle_vreg(VREG_SAVE, pk, st);
	}
	grammar_state = GRAMMAR_NONE;
}

void
ve_gramma_act_line(char c, struct cpkg *pk)
{
	char *dst, *src;
	size_t size, src_idx, fst_idx_line;
	int line, old_curs;
	line = pk->curs / pk->screen_limit;
	old_curs = pk->curs;
	fst_idx_line = line * pk->screen_limit;
	(void)c;
	if ((src_idx = (line + 1) * pk->screen_limit) > pk->com.idx) {
		pk->com.idx = fst_idx_line;
		pk->curs = pk->com.idx;
		pk->com.command[pk->com.idx] = '\0';
	} else {
		dst = &pk->com.command[fst_idx_line];
		src = &pk->com.command[src_idx];
		size = pk->com.idx - src_idx + 1;
		memmove(dst, src, size);
		pk->com.idx -= pk->screen_limit;
		pk->curs = fst_idx_line;
	}
	burn_n_rebuild(pk, HISTORY, old_curs);
	put_cursor(pk, pk->com.idx);
}

void
ve_gramma_act_to_beg(char c, struct cpkg *pk)
{
	char *dst, *src;
	size_t size;
	int old_curs;
	(void)c;
	if (pk->curs == 0) {
		if (grammar_state == GRAMMAR_CHANGE_WORD)
			init_ascii(pk->tptr);
		return;
	}
	if (grammar_state == GRAMMAR_DELETE || grammar_state ==
			GRAMMAR_CHANGE_WORD) {
		handle_vreg(VREG_SNAP_SCR, pk);
		old_curs = pk->curs;
		dst = pk->com.command;
		src = &pk->com.command[pk->curs];
		size = pk->com.idx - pk->curs + 1;
		memmove(dst, src, size);
		pk->curs = 0;
		pk->com.idx -= old_curs;
		burn_n_rebuild(pk, HISTORY, old_curs);
		put_cursor(pk, pk->com.idx);
	} else if (grammar_state == GRAMMAR_YANK) {
		char st[MAX_CANON + 1];
		strlcpy(st, &pk->com.command[pk->curs], pk->curs + 1);
		handle_vreg(VREG_SAVE, pk, st);
	}
	grammar_state = GRAMMAR_NONE;
}

void
ve_gramma(char c, struct cpkg *pk)
{
	int i;
	for (i = 0; i < 128; ++i)
		pk->tptr[i] = ve_wrong_exit;
	for (i = 0; i < 10; ++i)
		pk->tptr[i + '0'] = ve_get_mul;
	switch (c) {
	case 'y':
		gramma_state = GRAMMAR_YANK;
		pk->tptr['y'] = ve_gramma_act_line;
		break;
	case 'c':
		gramma_state = GRAMMAR_CHANGE_WORD;
		break;
	case 'd':
		gramma_state = GRAMMAR_DELETE;
		ascii['d'] = ve_gramma_act_line;
		break;
	default:
		break;
	}
	ascii['w'] = ve_gramma_act_word;
	ascii['$'] = ve_gramma_act_to_end;
	ascii['^'] = ve_gramma_act_to_beg;
}

size_t
find_last_word(struct cpkg *pk)
{
	enum looking_word state;
	uint8_t c_words;
	int i;
	if (mult > 0) {
		c_words = mult;
		mult = 0;
	}
	if (pk->com.command[pk->curs] == ' ') {
		state = LK_BEG_SPACE;
		i = pk->curs + 1;
	} else {
		i = pk->curs;
	}
	for (; i < pk->com.idx; ++i) {
		if (isspace(pk->com.command[i]) && state == DS_IN_WORD) {
			state = LK_LOOKING_WORD;
			continue;
		}
		if  (isspace(pk->com.command[i]) && state == DS_LOOKING_WORD)
			continue;
		if (!isspace(pk->com.command[i]) && state == DS_LOOKING_WORD) {
			if (!c_words)
				break;
			--c_words;
			state = LK_IN_WORD;
		}
		if (!isspace(pk->com.command[i]) && state == DS_BEG_SPACE) {
			if (!c_words)
				break;
			--c_words;
			state = LS_IN_WORD;
		}
	}
	return (i);
}
