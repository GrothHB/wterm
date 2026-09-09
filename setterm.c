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
#define PS_SIZE 4
#define PS2_SIZE 4

typedef enum {
	UP,
	DOWN,
	SAVE,
	QUESTION,
	CLEAR
} hflags;

struct i_command {
	uint8_t idx;
	uint8_t off_set;
	char *command;
};

struct cpkg {
	struct i_command com;
	struct termios base;
	char **history;
	uint8_t width;
	hflags flag;
};

void disp_stream(char , struct cpkg *);
void neutral(char c, struct cpkg * pk) { (void)c; (void)pk; return; }
void repaint(struct cpkg *);
void cans(char, struct cpkg *);
void spec(char, struct cpkg *);
void hcntl(char **, hflags, struct cpkg *, ...);
int check_limit(struct cpkg *);
void check_break(char, struct cpkg *);
void clear_comm(char, struct cpkg *);
void disp_hist(char *);
void clear_page(struct cpkg *);

int
main(void)
{
	struct termios ancer, raw;
	struct cpkg header;
	char command[MAX_CANON + 1];
	char c;
	char (*command_history[HISTORY_MAX_SIZE]);
	void (*ascii[128])(char c, struct cpkg *);
	/* ascii array is declared in `keytable.h' */
	const char ps[] = "$> ";
	tcgetattr(STDIN_FILENO, &ancer);
	header.width = 0;
	header.com.idx = 0;
	header.com.command = command;
	header.history = command_history;
	header.base = ancer;
	header.com.off_set = 0;
	command_history[0] = NULL;
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

	tcsetattr(STDIN_FILENO, TCSANOW, &ancer);
	return 0;
}

void
hcntl(char *hoc[], hflags flag, struct cpkg *pk, ...)
{
	va_list ap;
	short int val;
	static uint8_t rbp = 0;
	static uint8_t rsp = 0;
	va_start(ap, pk);
	switch (flag) {
	case UP:
		if (rbp == 0)
			break;
		if (rbp != rsp)
			clear_page(pk);
		--rbp;
		val = strlen(hoc[rbp]);
		strlcpy(pk->com.command, hoc[rbp], val + 1);
		pk->com.off_set = (val / OPENBSD_MAX) * OPENBSD_MAX;
		pk->com.idx = val % OPENBSD_MAX;
		pk->width = pk->com.idx;
		disp_hist(hoc[rbp]);
		break;
	case DOWN:
		if (rbp == rsp) 
			break;
		++rbp;
		if (pk->com.off_set > 0)
			clear_page(pk);
		if (rbp != rsp) {
			val = strlen(hoc[rbp]);
			disp_hist(hoc[rbp]);
			strlcpy(pk->com.command, hoc[rbp], val + 1);
			pk->com.off_set = (val / OPENBSD_MAX) * OPENBSD_MAX;
			pk->com.idx = val % OPENBSD_MAX;
			pk->width = pk->com.idx;
		} else write(STDOUT_FILENO, "\033[K$> ", 6);
		break;
	case SAVE:
		if (rsp == HISTORY_MAX_SIZE) {
			int i;
			const char *arg = va_arg(ap, const char *);
			free(hoc[0]);
			for (i = 0; i < HISTORY_MAX_SIZE - 1 ; ++i)
				hoc[i] = hoc[i + 1];
			i = strlen(arg);
			hoc[HISTORY_MAX_SIZE - 1] = malloc(i + 1);
			strlcpy(hoc[HISTORY_MAX_SIZE - 1], arg, i);
			hoc[HISTORY_MAX_SIZE - 1][i] = '\0';
			break;
		} else {
			const char *arg = va_arg(ap, const char *);
			int size = strlen(arg);
			hoc[rsp] = malloc(size + 1);
			strlcpy(hoc[rsp], arg, size);
			hoc[rsp][size] = '\0';
			++rsp;
			break;
		}
	case CLEAR:
		rbp = rsp;
		break;
	default:
		break;
	}
	va_end(ap);
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
		pk->width = 0;
		pk->com.idx = 0;
		pk->com.off_set += OPENBSD_MAX;
	}
	return 1;
}

void
disp_stream(char c, struct cpkg *pk)
{
	if (check_limit(pk)) {
		if (pk->width != pk->com.idx) {
			short int form = pk->com.idx + pk->com.off_set;
			short int lim = pk->width + pk->com.off_set;
			++pk->com.idx;
			for (; form > lim - 1; --form)
				pk->com.command[form + 1] = pk->com.command[form];
			pk->com.command[pk->width + pk->com.off_set] = c;
			pk->width++;
			repaint(pk);
		} else {
			write(STDOUT_FILENO, &c, 1);
			pk->width++;
			pk->com.command[pk->com.idx + pk->com.off_set] = c;
			pk->com.idx++;
			pk->com.command[pk->com.idx + pk->com.off_set] = '\0';
		}
	}
}

void
repaint(struct cpkg *pk)
{
	uint8_t width_off, i;
	char mess[] = "## Are you sure you want esc [Y/n]: ";
	const char clear[] = "\r\033[K";
	const char ps[] = "$> ";
	const char ps2[] = ">  ";
	if (pk->flag == QUESTION) {
		write(STDOUT_FILENO, &clear, 4);
		write(STDOUT_FILENO, &mess, strlen(mess));
		write(STDOUT_FILENO, pk->com.command, pk->com.idx);
		return;
	}
	write(STDOUT_FILENO, &clear, 4);
	if (pk->com.off_set) {
		uint8_t v = pk->com.idx;
		write(STDOUT_FILENO, &ps2, 3);
		write(STDOUT_FILENO, &pk->com.command[pk->com.off_set], v);
	} else {
		write(STDOUT_FILENO, &ps, 3);
	write(STDOUT_FILENO, pk->com.command, pk->com.idx);
	}
	width_off = pk->com.idx - pk->width;
	for (i = 0; i < width_off; ++i)
		write(STDOUT_FILENO, "\b" , 1);
}

void
spec(char c, struct cpkg *pk)
{
	struct timespec timeout, inttime;
	struct cpkg state;
	char sc[2];
	char mem[2];
	char answer;
	int flag = fcntl(0, F_GETFL);
	state.com.command = mem;
	state.com.idx = 0;
	state.width = 0;
	state.com.off_set = 0;
	state.history = NULL;
	fcntl(0, F_SETFL, flag | O_NONBLOCK);
	timeout.tv_sec = 0;
	timeout.tv_nsec = 25000000;
	nanosleep(&timeout, &inttime);

	if (read(STDIN_FILENO, &sc, 2) == -1) {
		char message[] = "## Are you sure you want esc [Y/n]: ";
		fcntl(0, F_SETFL, flag);
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
				state.width = state.com.idx;
			}
			if (answer == DEL) {
				state.flag = QUESTION;
				cans(answer, &state);
			}
		}
		if (answer == 'Y') {
			if (!(check_limit(pk)))
				return;
			pk->com.command[pk->com.idx + pk->com.off_set] = c;
			pk->com.idx++;
			pk->width++;
			write(STDIN_FILENO, "\n", 1);
			repaint(pk);
		} else repaint(pk);
	} else {
		fcntl(0, F_SETFL, flag);
		if (sc[0] == '[') {
			switch (sc[1]) {
			case 'A':
				if (pk->com.idx == pk->width) {
					pk->flag = UP;
					hcntl(pk->history, pk->flag, pk);
				}
				break;
			case 'B':
				if (pk->com.idx == pk->width) {
					pk->flag = DOWN;
					hcntl(pk->history, pk->flag, pk);
				}
				break;
			case 'D':
				if (pk->width > 0) {
					pk->width--;
					write(STDOUT_FILENO, "\033[D", 4);
				}
				break;
			case 'C':
				if (pk->width < pk->com.idx) {
					write(STDOUT_FILENO, "\033[C", 4);
					pk->width++;
				}
				break;
			}
		} else fcntl(0, F_SETFL, flag);
	}
}

void
cans(char c, struct cpkg *pk)
{
	(void)c;
	if (pk->width != 0) {
		uint8_t form = pk->width - 1;
		uint8_t v = pk->com.off_set;
		pk->width--;
		while (form < pk->com.idx) {
			pk->com.command[form + v] = pk->com.command[form + v + 1];
			++form;
		}
		pk->com.idx--;
		repaint(pk);
	}
}

void
check_break(char c, struct cpkg *pk)
{
	const char dummy[] = "!]STOP[!@";
	if (check_limit(pk)) {
		write(STDOUT_FILENO, &c, 1);
		pk->com.command[pk->com.idx + pk->com.off_set] = c;
		pk->com.idx++;
		pk->width++;
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
	pk->flag = SAVE;
	hcntl(pk->history, pk->flag, pk, pk->com.command);
	write(STDIN_FILENO, &c, 1);
	write(STDOUT_FILENO, "\r$> ", 4);
	pk->width = 0;
	pk->com.idx = 0;
	pk->com.off_set = 0;
	pk->flag = CLEAR;
	hcntl(pk->history, pk->flag, pk);
}

void
disp_hist(char arg[])
{
	char ps[] = "$> ";
	char ps2[] = ">  ";
	char clear[] = "\r\033[K";
	int off = 0;
	int len = strlen(arg);
	if (len > OPENBSD_MAX) {
		write(STDOUT_FILENO, &clear, 4);
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
		write(STDOUT_FILENO, &clear, 4);
		write(STDOUT_FILENO, &ps, 3);
		write(STDOUT_FILENO, arg, len);
	}
}

void
clear_page(struct cpkg *pk)
{
	const char erease_one_up[] = "\r\033[K\033[A";
	short int iter;
	short int times;
	int len_er_one_up = strlen(erease_one_up);
	if (pk->com.off_set == 0)
		return;
	else {
		times = pk->com.off_set / OPENBSD_MAX;
		for (iter = 0; iter < times; ++iter)
			write(STDOUT_FILENO, &erease_one_up, len_er_one_up);
	}
}
