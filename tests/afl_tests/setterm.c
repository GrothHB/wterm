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
#define OPENBSD_MAX 77 /* style(9) 80 column max */
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
	uint8_t off_set;
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
int check_limit(struct cpkg *);
void check_break(char, struct cpkg *);
void clear_comm(char, struct cpkg *);
void disp_hist(char *);
void clear_page(struct cpkg *);
void clear_mem(struct cpkg *);
void parse_esc_com(char c, struct cpkg *);
void parse_message_disp(struct cpkg *);
void send_message(struct cpkg *);
void assigne(char, struct cpkg *);
int id_shift(side, struct cpkg *);
void cursor_cntl(cm, struct cpkg *);
void command_snap(cmsnap, ...);
void *push_memory(int *, struct cpkg *);

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
	header.com.off_set = 0;
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
		command_snap(CLEAR_TERM);
		--rbp;
		command_snap(SNAP, pk->history[rbp]);
		disp_hist(pk->history[rbp]);
		break;
	case DOWN:
		if (rbp == rsp) 
			break;
		command_snap(CLEAR_TERM);
		++rbp;
		command_snap(SNAP,pk->history[rbp]);
		disp_hist(pk->history[rbp]);
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

int
check_limit(struct cpkg *pk)
{
	char ps2[] = ">  ";
	if (pk->com.idx + pk->com.off_set == MAX_CANON)
		return 0;
	if (pk->com.idx == OPENBSD_MAX) {
		write(STDIN_FILENO, "\r\n", 2);
		write(STDOUT_FILENO, &ps2, PS2_SIZE);
		pk->curs = 0;
		pk->com.idx = 0;
		pk->com.off_set += OPENBSD_MAX;
	}
	return 1;
}

void
disp_stream(char c, struct cpkg *pk)
{
	if (check_limit(pk)) {
		if (pk->curs != pk->com.idx) {
			char *src, *dst;
			size_t size = pk->com.idx - pk->curs + 2;
			dst = &pk->com.command[pk->curs + pk->com.off_set + 1];
			src = &pk->com.command[pk->curs + pk->com.off_set];
			memmove(dst, src, size);
			pk->com.command[pk->curs + pk->com.off_set] = c;
			id_shift(ID_RIGHT, pk);
			cursor_cntl(M_RIGHT, pk);
			repaint(pk);
		} else {
			assigne(c, pk);
			id_shift(ID_RIGHT, pk);
			++pk->curs;
		}
	}
}

void
repaint(struct cpkg *pk)
{
	uint8_t curs_off, i;
	const char clear[] = "\r\033[K";
	const char ps[] = "$> ";
	const char ps2[] = ">  ";
	write(STDOUT_FILENO, &clear, 4);
	if (pk->com.off_set) {
		uint8_t v = pk->com.idx;
		write(STDOUT_FILENO, &ps2, 3);
		write(STDOUT_FILENO, &pk->com.command[pk->com.off_set], v);
	} else {
		write(STDOUT_FILENO, &ps, 3);
	write(STDOUT_FILENO, pk->com.command, pk->com.idx);
	}
	curs_off = pk->com.idx - pk->curs;
	for (i = 0; i < curs_off; ++i)
		write(STDOUT_FILENO, "\b" , 1);
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
		dst = &pk->com.command[pk->curs + pk->com.off_set];
		src = &pk->com.command[pk->curs + pk->com.off_set + 1];
		memmove(dst, src, size);
		/* while (form < pk->com.idx) {
			pk->com.command[form + v] = pk->com.command[form + v + 1];
			++form;
		} */
		id_shift(ID_LEFT, pk);
		repaint(pk);
	}
}

void
check_break(char c, struct cpkg *pk)
{
	const char dummy[] = "!]STOP[!@";
	if (check_limit(pk)) {
		assigne(c, pk);
		id_shift(ID_RIGHT, pk);
		++pk->curs;
	} else return;
	if (pk->com.idx >= 9)
		if (strstr(pk->com.command, dummy) != NULL) {
			write(STDOUT_FILENO, "\r\n", 2);
			tcsetattr(STDIN_FILENO, TCSANOW, &pk->base);
			exit(3);
		}
}

void
clear_comm(char c, struct cpkg *pk)
{
	(void)c;
	if (pk->flag == WHIPE)
		pk->flag = SAVE;
	else {
		hcntl(pk);
		write(STDIN_FILENO, &c, 1);
		write(STDOUT_FILENO, "\r$> ", 4);
		pk->flag = CLEAR;
		hcntl(pk);
	}
	pk->curs = 0;
	pk->com.idx = 0;
	pk->com.off_set = 0;
}

void
disp_hist(char arg[])
{
	char ps[] = "$> ";
	char ps2[] = ">  ";
	int off = 0;
	if (arg == NULL)
		return;
	int len = strlen(arg);
	if (len > OPENBSD_MAX) {
		write(STDOUT_FILENO, &ps, 3);
		write(STDOUT_FILENO, arg, OPENBSD_MAX);
		write(STDOUT_FILENO, "\033[B\r", 4);
		len -= OPENBSD_MAX;
		off += OPENBSD_MAX;
		while (len > OPENBSD_MAX) {
			write(STDOUT_FILENO, &ps2, 3);
			write(STDOUT_FILENO, &arg[off], OPENBSD_MAX);
			write(STDOUT_FILENO, "\033[B\r", 4);
			off += OPENBSD_MAX;
			len -= OPENBSD_MAX;
		}
		write(STDOUT_FILENO, &ps2, 3);
		write(STDOUT_FILENO, &arg[off], len);
	} else {
		write(STDOUT_FILENO, &ps, 3);
		write(STDOUT_FILENO, arg, len);
	}
}

void
clear_page(struct cpkg *pk)
{
	const char erease_one_up[] = "\r\033[K\033[A";
	const char clean[] = "\r\033[K";
	short int iter;
	short int times;
	int len_er_one_up = strlen(erease_one_up);
	if (pk->com.off_set == 0)
		write(STDOUT_FILENO, &clean, 4);
	else {
		times = pk->com.off_set / OPENBSD_MAX;
		for (iter = 0; iter < times; ++iter)
			write(STDOUT_FILENO, &erease_one_up, len_er_one_up);
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
	state.com.off_set = 0;
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
		if (check_limit(pk)) {
			assigne(ESC, pk);
			id_shift(ID_RIGHT, pk);
			repaint(pk);
		}
	} else repaint(pk);
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
		if (pk->curs > 0) {
			--pk->curs;
			write(STDOUT_FILENO, "\033[D", 3);
		}
		break;
	case M_RIGHT:
		if (pk->curs < pk->com.idx) {
			pk->curs = (pk->curs + 1) % OPENBSD_MAX;
			if (pk->curs == 0)
				write(STDOUT_FILENO, "\r\033[B\033[3C", 8);
			else write(STDOUT_FILENO, "\033[C", 3);
		}
		break;
	}
}

void
command_snap(cmsnap state, ...)
{
	va_list ap;
	static char command[MAX_CANON + 1];
	static struct cpkg snap;
	int len;
	va_start(ap, state);
	snap.com.command = command;
	snap.history = NULL;
	snap.com.idx = snap.com.off_set = snap.curs = 0;
	switch (state) {
	case SNAP:
		char *arg = va_arg(ap, char *);
		if (arg == NULL) {
			snap.flag = WHIPE;
			clear_comm('\0', &snap);
			break;
		}
		len = strlen(arg);
		strlcpy(snap.com.command, arg, len + 1);
		snap.com.idx = (len) % OPENBSD_MAX;
		snap.com.off_set = ((len) / OPENBSD_MAX) * OPENBSD_MAX;
		snap.curs = snap.com.idx;
		break;
	case CLEAR_TERM:
		clear_page(&snap);
		break;
	}
	va_end(ap);
}

int
id_shift(side s, struct cpkg *pk)
{
	switch (s) {
	case ID_RIGHT:
		pk->com.idx = (pk->com.idx + 1) % OPENBSD_MAX;
		if (pk->com.idx == 0)
			pk->com.off_set += OPENBSD_MAX;
		return 1;
	case ID_LEFT:
		if (pk->com.idx == 0)
			return 0;
		pk->com.idx--;
		return 1;
	}
	return 0;
}

void
assigne(char c, struct cpkg *pk)
{
	pk->com.command[pk->com.idx + pk->com.off_set] = c;
	pk->com.command[pk->com.idx + pk->com.off_set + 1] = '\0';
	write(STDOUT_FILENO, &c, 1);
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
