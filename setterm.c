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

#define ESC '\033'
#define DEL '\177'
#define NEWLINE '\n'
#define OPENBSD_STYLE_CR 80 /* style(9) 80 column max */
#define HISTORY_MAX_SIZE 100 
#define PS_SIZE 4
#define PS2_SIZE 4

typedef enum {
	UP,
	DOWN,
	SAVE,
	CLEAR
} hflags;

struct i_command {
	uint8_t idx;
	char *command;
};

struct cpkg {
	struct i_command comm;
	char **history;
	uint8_t width;
	hflags flag;
};

void disp_stream(char , struct cpkg *);
void neutral(char, struct cpkg *) { return; }
void repaint(struct cpkg *);
void cans(char, struct cpkg *);
void spec(char, struct cpkg *);
void hcntl(char **, hflags, ...);
int check_limit(struct cpkg *);
void check_break(char, struct cpkg *);
void clear_comm(char, struct cpkg *);

int
main(void)
{
	struct termios ancer, raw;
	struct cpkg header;
	char command[MAX_CANON];
	char c;
	char (*command_history[HISTORY_MAX_SIZE]);
	const char ps[] = "$> ";
	tcgetattr(STDIN_FILENO, &ancer);
	header.width = 0;
	header.comm.idx = 0;
	header.comm.command = command;
	header.history = command_history;
	raw = ancer; 
	cfmakeraw(&raw);
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);
	write(STDOUT_FILENO, ps, PS_SIZE);
	#include "keytable.h"

	while (read(STDIN_FILENO, &c, 1)) {
		if (c < 0)
			c *= -1;
		ascii[c % 128](c, &header);
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &ancer);
	return 0;
}

void
hcntl(char *hoc[], hflags flag, ...)
{
	va_list ap;
	static uint8_t rbp = 0;
	static uint8_t rsp = 0;
	const char clear[] = "\r\033[K";
	va_start(ap, flag);
	switch (flag) {
	case UP:
		if ((rbp - 1)!= 0) {
			--rbp;
			uint8_t size = strlen(hoc[rbp]) + 1;
			write(STDOUT_FILENO, &clear, 5);
			write(STDOUT_FILENO, &hoc[rbp], size);
			break;
		}
		break;
	case DOWN:
		if (rbp != rsp) {
			++rbp;
			if (rbp == rsp)
				write(STDOUT_FILENO, &clear, 5);
			else {
				uint8_t size = strlen(hoc[rbp]) + 1;
				write(STDOUT_FILENO, &clear, 5);
				write(STDOUT_FILENO, &hoc[rbp], size);
			}
		}
		break;
	case SAVE:
		if (rsp == HISTORY_MAX_SIZE) {
			int i;
			const char *arg = va_arg(ap, const char *);
			free(hoc[0]);
			for (i = 0; i < 100; ++i)
				hoc[i] = hoc[i + 1];
			hoc[100] = malloc(strlen(arg) + 1);
			strncpy(hoc[100], arg, strlen(arg) + 1);
			break;
		} else {
			const char *arg = va_arg(ap, const char *);
			hoc[rsp] = malloc(strlen(arg) + 1);
			strncpy(hoc[rsp], arg, strlen(arg) + 1);
			++rsp;
			break;
		}
	case CLEAR:
		rbp = rsp;
		break;
	}
}

int
check_limit(struct cpkg *pk)
{
	char ps2[] = ">  ";
	if (pk->comm.idx == MAX_CANON)
		return 0;
	if (pk->comm.idx == 80) {
		write(STDIN_FILENO, "\n", 1);
		write(STDOUT_FILENO, &ps2, PS2_SIZE);
		pk->width = 0;
	}
	return 1;
}

void
disp_stream(char c, struct cpkg *pk)
{
	if (check_limit(pk)) {
		if (pk->width != pk->comm.idx && pk->comm.idx + 1 != MAX_CANON) {
			uint8_t form = pk->comm.idx;
			for (; form > pk->width; --form)
				pk->comm.command[form + 1] = pk->comm.command[form];
			pk->comm.command[pk->width] = c;
			repaint(pk);
		} else {
			write(STDOUT_FILENO, &c, 1);
			pk->width++;
			pk->comm.command[pk->comm.idx++] = c;
			pk->comm.command[pk->comm.idx] = '\0';
		}
	}
}

void
repaint(struct cpkg *pk)
{
	uint8_t width_off, i;
	const char clear[] = "\r\033[K";
	const char ps[] = "$> ";
	write(STDOUT_FILENO, &clear, 5);
	write(STDOUT_FILENO, &ps, 4);
	write(STDOUT_FILENO, pk->comm.command, pk->comm.idx);
	width_off = pk->comm.idx - pk->width;
	for (i = 0; i < width_off; ++i)
		write(STDOUT_FILENO, "\b" , 1);
}

void
spec(char c, struct cpkg *pk)
{
	char sc[2];
	int flag = fcntl(0, F_GETFD);
	fcntl(0, F_SETFD, flag | O_NONBLOCK);

	if (read(STDIN_FILENO, &sc, 2) == -1) {
		char message[] = "\nAre you sure you want esc [Y/n]: ";
		write(STDOUT_FILENO, &message, sizeof(message));
		fcntl(0, F_SETFD, flag);
		read(STDIN_FILENO, &sc, 1);
		if (sc[0] == 'y') {
			if (!(check_limit(pk)))
				return;
			pk->comm.command[pk->comm.idx++] = c;
			pk->width++;
			write(STDOUT_FILENO, &c, 1);
		}
	} else {
		if (sc[0] == '[') {
			switch (sc[1]) {
			case 'A':
				if (pk->comm.idx == 0) {
					pk->flag = UP;
					hcntl(pk->history, pk->flag);
				}
				break;
			case 'B':
				pk->flag = DOWN;
				hcntl(pk->history, pk->flag);
				break;
			case 'D':
				if (pk->width > 0) {
					pk->width--;
					write(STDOUT_FILENO, "\033[D", 4);
				}
				break;
			case 'C':
				if (pk->width < MAX_CANON) {
					write(STDOUT_FILENO, "\033[C", 4);
					pk->width++;
				}
				break;
			}
		}
	}
}

void
cans(char c, struct cpkg *pk)
{
	if (pk->width != 0) {
		uint8_t form = pk->width - 1;
		pk->comm.idx--;
		pk->width--;
		while (form < pk->comm.idx)
			pk->comm.command[form] = pk->comm.command[form + 1];
		repaint(pk);
	}
}

void
check_break(char c, struct cpkg *pk)
{
	char dummy[10];
	write(STDOUT_FILENO, &c, 1);
	pk->comm.command[pk->comm.idx] = c;
	if (pk->comm.idx - 10 >= 0) {
		strncpy(dummy, &pk->comm.command[pk->comm.idx - 10], 9);
		dummy[9] = '\0';
		if (strcmp(dummy, "!]STOP[!@") == 0)
			err(EXIT_FAILURE, "Something went wrong");
	}
	pk->comm.idx++;
}

void
clear_comm(char c, struct cpkg *pk)
{
	pk->flag = SAVE;
	hcntl(pk->history, pk->flag, pk->comm.command);
	write(STDIN_FILENO, &c, 1);
	write(STDOUT_FILENO, "$> ", 4);
	pk->width = 0;
	pk->comm.idx = 0;
	pk->flag = CLEAR;
	hcntl(pk->history, pk->flag);
}
