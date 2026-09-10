#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <stdarg.h>
#include <err.h>
#include <time.h>

#define ESC '\033'
#define DEL '\177'
#define NEWLINE '\n'
#define OPENBSD_MAX 76 /* style(9) 80 column max */
#define HISTORY_MAX_SIZE 100 
#define MAX_OFF_SET ((MAX_CANON / OPENBSD_MAX) * OPENBSD_MAX)
#define PS_SIZE 4
#define PS2_SIZE 4

typedef enum {
	UP,
	DOWN,
	SAVE,
	WHIPE,
	CLEAR
} hflags;

typedef enum {
	M_LEFT,
	M_RIGHT
} cm;

typedef enum {
	SNAP,
	CLEAR_TERM
} cmsnap;

typedef enum {
	ID_RIGHT,
	ID_LEFT
} side;

struct i_command {
	uint8_t idx;
	char *command;
};

struct cpkg {
	struct i_command com;
	struct termios base;
	char **history;
	uint8_t curs;
	hflags flag;
};

void disp_stream(char , struct cpkg *);
void neutral(char c, struct cpkg * pk) { (void)c; (void)pk; return; }
void repaint(struct cpkg *);
void cans(char, struct cpkg *);
void spec(char, struct cpkg *);
void hcntl(struct cpkg *);
void check_break(char, struct cpkg *);
void clear_comm(char, struct cpkg *);
void clear_page(struct cpkg *);
void clear_mem(struct cpkg *);
void parse_esc_com(char c, struct cpkg *);
void parse_message_disp(struct cpkg *);
void send_message(struct cpkg *);
void assigne(char, struct cpkg *);
void id_shift(side, struct cpkg *);
void cursor_cntl(cm, struct cpkg *);
void *push_memory(int *, struct cpkg *);
void push_command(int, struct cpkg *);
void set_cursor(int);

int
main(void)
{
	struct termios ancer, raw;
	struct cpkg header;
	char command[MAX_CANON + 1];
	char c;
	char (*command_history[HISTORY_MAX_SIZE + 1]);
	void (*ascii[128])(char c, struct cpkg *);
	/* ascii array is declared in `keytable.h' */
	const char ps[] = "$> ";
	tcgetattr(STDIN_FILENO, &ancer);
	header.curs = 0;
	header.com.idx = 0;
	header.com.command = command;
	header.history = command_history;
	header.base = ancer;
	header.flag = SAVE;
	command_history[0] = NULL;
	command_history[HISTORY_MAX_SIZE] = NULL;
	raw = ancer; 
	cfmakeraw(&raw);
	/* Setting enter to FL becouse in raw mode
	 * it is defaulter as CR
	 */
	raw.c_iflag |= ICRNL;
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);
	write(STDOUT_FILENO, ps, PS_SIZE);
	#include "keytable.h"

	while (read(STDIN_FILENO, &c, 1)) {
		if (c < 0)
			continue;
		ascii[c % 128](c, &header);
		clear_page(&header);
		repaint(&header);
	}

	clear_mem(&header);
	tcsetattr(STDIN_FILENO, TCSANOW, &ancer);
	return 0;
}

void
hcntl(struct cpkg *pk)
{
	static int rbp = 0;
	static int rsp = 0;
	switch (pk->flag) {
	case UP:
		if (rbp == 0)
			break;
		--rbp;
		push_command(rbp, pk);
		break;
	case DOWN:
		if (rbp == rsp) 
			break;
		++rbp;
		push_command(rbp, pk);
		break;
	case SAVE:
		push_memory(&rsp, pk);
		break;
	case CLEAR:
		rbp = rsp;
		pk->flag = SAVE;
		break;
	default:
		break;
	}
}

void
disp_stream(char c, struct cpkg *pk)
{
	if (pk->com.idx == MAX_CANON)
		return;
	if (pk->curs != pk->com.idx) {
		char *src, *dst;
		size_t size = pk->com.idx - pk->curs + 2;
		dst = &pk->com.command[pk->curs + 1];
		src = &pk->com.command[pk->curs];
		memmove(dst, src, size);
		pk->com.command[pk->curs] = c;
		id_shift(ID_RIGHT, pk);
		cursor_cntl(M_RIGHT, pk);
	} else {
		assigne(c, pk);
		id_shift(ID_RIGHT, pk);
		cursor_cntl(M_RIGHT, pk);
	}
}

void
repaint(struct cpkg *pk)
{
	int cpos, times, iter;
	const char ps[] = "$> ";
	const char ps2[] = ">  ";
	times = pk->com.idx / OPENBSD_MAX;
	cpos = pk->curs % OPENBSD_MAX;
	write(STDOUT_FILENO, &ps, 3);
	if (times)
		for (iter = 0; iter < times; ++iter) {
			write(STDOUT_FILENO, &ps2, 3);
			write(STDOUT_FILENO, pk->com.command, OPENBSD_MAX);
		}
	else {
		write(STDOUT_FILENO, pk->com.command, pk->com.idx);
		set_cursor(cpos);
		return;
	}
	switch (pk->curs) {
	case OPENBSD_MAX:
		set_cursor(pk->curs);
		break;
	case (2 * OPENBSD_MAX):
		set_cursor(pk->curs);
		break;
	case (3 * OPENBSD_MAX):
		set_cursor(pk->curs);
		break;
	default:
		set_cursor(cpos);
		break;
	}
}

void
spec(char c, struct cpkg *pk)
{
	(void)c;
	struct timespec timeout, inttime;
	char sc[2];
	int flag = fcntl(0, F_GETFL);
	fcntl(0, F_SETFL, flag | O_NONBLOCK);
	timeout.tv_sec = 0;
	timeout.tv_nsec = 25000000;
	nanosleep(&timeout, &inttime);

	if (read(STDIN_FILENO, &sc, 2) == -1) {
		fcntl(0, F_SETFL, flag);
		send_message(pk);
	} else {
		fcntl(0, F_SETFL, flag);
		if (sc[0] == '[')
			parse_esc_com(sc[1], pk);
	}
}

void
cans(char c, struct cpkg *pk)
{
	(void)c;
	if (pk->curs != 0) {
		char *dst, *src;
		size_t size = pk->com.idx - pk->curs + 1;
		cursor_cntl(M_LEFT, pk);
		dst = &pk->com.command[pk->curs];
		src = &pk->com.command[pk->curs + 1];
		memmove(dst, src, size);
		id_shift(ID_LEFT, pk);
		repaint(pk);
	}
}

void
check_break(char c, struct cpkg *pk)
{
	const char dummy[] = "!]STOP[!@";
	if (pk->com.idx == MAX_CANON)
		return;
	assigne(c, pk);
	id_shift(ID_RIGHT, pk);
	++pk->curs;
	if (pk->com.idx >= 9)
		if (strstr(pk->com.command, dummy) != NULL) {
			write(STDOUT_FILENO, "\r\n", 2);
			tcsetattr(STDIN_FILENO, TCSANOW, &pk->base);
			exit(2);
		}
}

void
clear_mem(struct cpkg *pk)
{
	int i = 0;
	while (pk->history[i] != NULL && i < HISTORY_MAX_SIZE) {
		free(pk->history[i]);
		pk->history[i] = NULL;
		++i;
	}
}

void
send_message(struct cpkg *pk)
{
	struct cpkg state;
	char mem[2];
	char answer;
	char message[] = "## Are you sure you want esc [Y/n]: ";
	state.com.command = mem;
	state.com.idx = 0;
	state.curs = 0;
	state.history = NULL;
	write(STDOUT_FILENO, "\r\033[K", 4);
	write(STDOUT_FILENO, &message, strlen(message));
	while (read(STDIN_FILENO, &answer, 1)) {
		if (answer == '\n')
			break;
		if (answer == 'n' || answer == 'Y') {
			write(STDOUT_FILENO, &answer, 1);
			state.com.idx = 1;
			state.com.command[0] = answer;
			state.com.command[1] = '\0';
			state.curs = state.com.idx;
		}
		if (answer == DEL) {
			parse_message_disp(&state);
		}
	}
	if (answer == 'Y') {
		if (pk->com.idx == MAX_CANON)
			return;
		assigne(ESC, pk);
		id_shift(ID_RIGHT, pk);
		repaint(pk);
	}
}

void
parse_esc_com(char c, struct cpkg *pk)
{
	if (c < 0)
		return;
	switch (c) {
	case 'A':
		if (pk->com.idx == pk->curs) {
			pk->flag = UP;
			hcntl(pk);
		}
		break;
	case 'B':
		if (pk->com.idx == pk->curs) {
			pk->flag = DOWN;
			hcntl(pk);
		}
		break;
	case 'D':
		cursor_cntl(M_LEFT, pk);
		break;
	case 'C':
		cursor_cntl(M_RIGHT, pk);
		break;
	default:
		break;
	}
}

void
parse_message_disp(struct cpkg *pk)
{
	char mess[] = "## Are you sure you want esc [Y/n]: ";
	const char clear[] = "\r\033[K";
	pk->com.command[0] = '\0';
	pk->com.idx--;
	pk->curs--;
	write(STDOUT_FILENO, &clear, 4);
	write(STDOUT_FILENO, &mess, strlen(mess));
	write(STDOUT_FILENO, pk->com.command, pk->com.idx);
	return;
}

void
cursor_cntl(cm action, struct cpkg *pk)
{
	switch (action) {
	case M_LEFT:
		if (pk->curs > 0)
			--pk->curs;
		break;
	case M_RIGHT:
		if (pk->curs == pk->com.idx)
			break;
		pk->curs++;
		break;
	}
}

void
id_shift(side s, struct cpkg *pk)
{
	switch (s) {
	case ID_RIGHT:
		if (pk->com.idx == MAX_CANON)
			break;
		++pk->com.idx;
		break;
	case ID_LEFT:
		if (pk->com.idx == 0)
			break;
		pk->com.idx--;
		break;
	}
}

void
assigne(char c, struct cpkg *pk)
{
	pk->com.command[pk->com.idx]  = c;
	pk->com.command[pk->com.idx + 1] = '\0';
}

void
*push_memory(int *rsp, struct cpkg *pk)
{
	if (*rsp == HISTORY_MAX_SIZE) {
		int len;
		char *ptr;
		size_t size = (HISTORY_MAX_SIZE - 1) * sizeof(char *);
		free(pk->history[0]);
		memmove(&pk->history[0], &pk->history[1], size);
		len = strlen(pk->com.command);
		if ((ptr = malloc(len + 1)) == NULL)
			return NULL;
		strlcpy(ptr, pk->com.command, len + 1);
		pk->history[HISTORY_MAX_SIZE - 1] = ptr;
		return pk->com.command;
	} else {
		char *ptr;
		int len = strlen(pk->com.command);
		if ((ptr = malloc(len + 1)) == NULL)
			return NULL;
		strlcpy(ptr, pk->com.command, len + 1);
		pk->history[*rsp] = ptr;
		++(*rsp);
		pk->history[*rsp] = NULL;
		return pk->com.command;
	}
}

void
push_command(int rbp, struct cpkg *pk)
{
	size_t size = strlen(pk->history[rbp]);
	if (size == 0)
		return;
	strlcpy(pk->com.command, pk->history[rbp], size);
	pk->com.command[size] = '\0';
}

void
set_cursor(int position)
{
	char max_mov[7];
	int len;
	position += 3;
	len = snprintf(max_mov, sizeof(max_mov), "\r\033[%dC", position);
	write(STDOUT_FILENO, max_mov, len - 1);
}
