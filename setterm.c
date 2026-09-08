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
	uint8_t off_set;
	char *command;
};

struct cpkg {
	struct i_command comm;
	struct termios base;
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
	unsigned char c;
	char (*command_history[HISTORY_MAX_SIZE]);
	const char ps[] = "$> ";
	tcgetattr(STDIN_FILENO, &ancer);
	header.width = 0;
	header.comm.idx = 0;
	header.comm.command = command;
	header.history = command_history;
	header.base = ancer;
	header.comm.off_set = 0;
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
	const char clear[] = "\r\033[K$> ";
	va_start(ap, flag);
	switch (flag) {
	case UP:
		if (rbp == 0)
			break;
		--rbp;
		write(STDOUT_FILENO, &clear, 7);
		write(STDOUT_FILENO, &hoc[rbp], strlen(hoc[rbp]));
		break;
	case DOWN:
		if (rbp == rsp) 
			break;
		++rbp;
		write(STDOUT_FILENO, &clear, 8);
		if (rbp != rsp)
			write(STDOUT_FILENO, &hoc[rbp], strlen(hoc[rbp]));
		break;
	case SAVE:
		if (rsp == HISTORY_MAX_SIZE) {
			int i;
			const char *arg = va_arg(ap, const char *);
			free(hoc[0]);
			for (i = 0; i < 99; ++i)
				hoc[i] = hoc[i + 1];
			hoc[99] = malloc(strlen(arg));
		strncpy(hoc[99], arg, strlen(arg));
			break;
		} else {
			const char *arg = va_arg(ap, const char *);
			hoc[rsp] = malloc(strlen(arg));
			strncpy(hoc[rsp], arg, strlen(arg));
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
	if (pk->comm.idx + pk->comm.off_set == MAX_CANON)
		return 0;
	if (pk->comm.idx == 81) {
		write(STDIN_FILENO, "\r\n", 2);
		write(STDOUT_FILENO, &ps2, PS2_SIZE);
		pk->width = 0;
		pk->comm.idx = 0;
		pk->comm.off_set += 81;
	}
	return 1;
}

void
disp_stream(char c, struct cpkg *pk)
{
	if (check_limit(pk)) {
		uint8_t dummy = pk->comm.idx + pk->comm.off_set;
		if (pk->width != pk->comm.idx && dummy + 1 != MAX_CANON) {
			uint8_t form = pk->comm.idx + pk->comm.off_set;
			uint8_t lim = pk->width + pk->comm.off_set;
			++pk->comm.idx;
			for (; form > lim - 1; --form)
				pk->comm.command[form + 1] = pk->comm.command[form];
			pk->comm.command[pk->width + pk->comm.off_set] = c;
			pk->width++;
			repaint(pk);
		} else {
			write(STDOUT_FILENO, &c, 1);
			pk->width++;
			pk->comm.command[pk->comm.idx + pk->comm.off_set] = c;
			pk->comm.idx++;
			pk->comm.command[pk->comm.idx + pk->comm.off_set] = '\0';
		}
	}
}

void
repaint(struct cpkg *pk)
{
	uint8_t width_off, i;
	const char clear[] = "\r\033[K";
	const char ps[] = "$> ";
	const char ps2[] = ">  ";
	write(STDOUT_FILENO, &clear, 4);
	if (pk->comm.off_set) {
		uint8_t v = pk->comm.idx;
		write(STDOUT_FILENO, &ps2, 3);
		write(STDOUT_FILENO, &pk->comm.command[pk->comm.off_set], v);
	} else {
		write(STDOUT_FILENO, &ps, 3);
		write(STDOUT_FILENO, pk->comm.command, pk->comm.idx);
	}
	width_off = pk->comm.idx - pk->width;
	for (i = 0; i < width_off; ++i)
		write(STDOUT_FILENO, "\b" , 1);
}

void
spec(char c, struct cpkg *pk)
{
	struct timespec timeout, inttime;
	char sc[2];
	uint8_t tflag = 0;
	int flag = fcntl(0, F_GETFL);
	fcntl(0, F_SETFL, flag | O_NONBLOCK);
	timeout.tv_sec = 0;
	timeout.tv_nsec = 25000000;

	if (read(STDIN_FILENO, &sc, 2) == -1) {
		nanosleep(&timeout, &inttime);
		if (tflag) {
			fcntl(0, F_SETFL, flag);
			char message[] = "\nAre you sure you want esc [Y/n]: ";
			write(STDOUT_FILENO, &message, sizeof(message));
			read(STDIN_FILENO, &sc, 1);
			if (sc[0] == 'y') {
				if (!(check_limit(pk)))
					return;
				pk->comm.command[pk->comm.idx + pk->comm.off_set] = c;
				pk->comm.idx++;
				pk->width++;
				write(STDOUT_FILENO, &c, 1);
			}
		}
		tflag++;
	} else {
		if (sc[0] == '[') {
			fcntl(0, F_SETFL, flag);
			switch (sc[1]) {
			case 'A':
				if (pk->comm.idx == 0) {
					pk->flag = UP;
					hcntl(pk->history, pk->flag);
				}
				break;
			case 'B':
				if (pk->comm.idx == 0) {
					pk->flag = DOWN;
					hcntl(pk->history, pk->flag);
				}
				break;
			case 'D':
				if (pk->width > 0) {
					pk->width--;
					write(STDOUT_FILENO, "\033[D", 4);
				}
				break;
			case 'C':
				if (pk->width < pk->comm.idx) {
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
	if (pk->width != 0) {
		uint8_t form = pk->width - 1;
		uint8_t v = pk->comm.off_set;
		pk->width--;
		while (form < pk->comm.idx) {
			pk->comm.command[form + v] = pk->comm.command[form + v + 1];
			++form;
		}
		pk->comm.idx--;
		repaint(pk);
	}
}

void
check_break(char c, struct cpkg *pk)
{
	char dummy[] = "!]STOP[!@";
	if (check_limit(pk)) {
		write(STDOUT_FILENO, &c, 1);
		pk->comm.command[pk->comm.idx + pk->comm.off_set] = c;
		pk->comm.idx++;
	} else return;
	if (pk->comm.idx >= 9)
		if (strstr(pk->comm.command, dummy) != NULL) {
			write(STDOUT_FILENO, "\r\n", 2);
			tcsetattr(STDIN_FILENO, TCSANOW, &pk->base);
			exit(3);
		}
}

void
clear_comm(char c, struct cpkg *pk)
{
	pk->flag = SAVE;
	hcntl(pk->history, pk->flag, pk->comm.command);
	write(STDIN_FILENO, &c, 1);
	write(STDOUT_FILENO, "\r$> ", 5);
	pk->width = 0;
	pk->comm.idx = 0;
	pk->flag = CLEAR;
	hcntl(pk->history, pk->flag);
}
