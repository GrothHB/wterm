#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include "my_term.h"
#include "sterm.h"
#include "test3.h"
#include "vi_edit.h"

void
set_vi_edit_mode(void (**ascii)(char, struct cpkg *))
{
	int 		i;
	for (i = 0; i < 128; ++i) {
		switch (i) {
		case 'k':
			ascii[i] = ve_move_up;
			break;
		case 'j':
			ascii[i] = ve_move_down;
			break;
		case 'h':
			ascii[i] = ve_move_left;
			break;
		case 'l':
			ascii[i] = ve_move_right;
			break;
		case 'e':
			ascii[i] = ve_jmp_end_of_word;
			break;
		case 'c':
			ascii[i] = ve_change_word;
			break;
		case 'b':
			ascii[i] = ve_jmp_beg_of_word;
			break;
		case 'B':
			ascii[i] = ve_jmp_beg;
			break;
		case 'E':
			ascii[i] = ve_jmp_end;
			break;
		case 'd':
			ascii[i] = ve_delet;
			break;
		case 'y':
			ascii[i] = ve_yank;
			break;
		case 'x':
			ascii[i] = ve_delet_char;
			break;
		case 'p':
			ascii[i] = ve_put;
			break;
		case '^':
			ascii[i] = ve_jmp_beg_line;
			break;
		case '$':
			ascii[i] = ve_jmp_end_line;
			break;
		case 'u':
			ascii[i] = ve_undo;
			break;
		case 'i':
			ascii[i] = ve_insert_mode;
			break;
		case ESC:
			ascii[i] = ve_clear_comm;
			break;
		default:
			ascii[i] = neutral;
			break;
		}
	}
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
	char		buf[15];
	int		i, state;
	size_t		len;
	state = 0;
	for (i = pk->curs; i < pk->com.idx; ++i) {
		c = pk->com.command[i];
		if (c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' || c == '_')
			continue;
		break;
	}
	if (i == 0 || pk->curs == i)
		return;
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
change_word(char c, struct cpkg *pk)
{
	int		i;
	(void)c;
	for (i = 0; i < 128; ++i) {
		if (i == 'w')
			pk->tptr[i] = change_word_action;
		pk->tptr[i] = ve_wrong_exit;
	}
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
	int		i;
	char		*dst, *src;
	size_t		size;
	if (pk->curs == pk->com.idx)
		return;
	for (i = pk->curs; i < pk->com.idx; ++i) {
		c = pk->com.command[i];
		if (c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' || c == '_')
			continue;
		break;
	}
	size = pk->com.idx - i;
	handle_vreg(VREG_SAVE, pk);
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


