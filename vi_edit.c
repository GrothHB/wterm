#include <unistd.h>
#include <stdint.h>
#include "my_term.h"
#include "sterm.h"
#include "test3.h"

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
