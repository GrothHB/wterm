#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <stdarg.h>

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

struct i_word {
	uint8_t idx;
	char *word;
};

struct i_command {
	uint8_t idx;
	char *command;
};

struct cpkg {
	struct i_command comm;
	struct i_word crw;
	char **history;
	uint8_t *width;
	hflags flag;
};

void clear_word(char , struct cpkg *);
void disp_stream(char , struct cpkg *);
void neutral(char, struct cpkg *) { return; };

int
main(void)
{
	struct termios ancer, raw;
	char command[MAX_CANON];
	char c, word_buffer[MAX_CANON];
	char (*comand_hitory[HISTORY_MAX_SIZE]);
	int x , i;
	const char ps[] = "$> ";
	const char ps2[] = ">  ";
	x = i = 0;
	tcgetattr(STDIN_FILENO, &ancer);
	raw = ancer; 
	cfmakeraw(&raw);
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);
	write(STDOUT_FILENO, ps, PS_SIZE);
	#include "keytable.h"

	while (read(STDIN_FILENO, &c, 1)) {
		switch (c) {
		case DEL:
			if (x) {
				write(STDIN_FILENO, "\b\b\033[2X", 6);
				word_buffer[--i] = '\0';
				--x;
			}
			break;
		case NEWLINE:
			write(STDOUT_FILENO, "\r\n", 2);
			write(STDOUT_FILENO, ps, 4);
			if(i < 50) {
				word_buffer[i] = '\0';
				write(STDOUT_FILENO, word_buffer , 50);
			}
			for(i = 0; i< 50; ++i)
				word_buffer[i] = '\0';
			i = 0;
			x = 0;
			break;
		case ' ':
			write(STDOUT_FILENO, &c, 1);
			for(i = 0; i< 50; ++i)
				word_buffer[i] = '\0';
			i = 0;
			++x;
			break;
		case '@':
			word_buffer[i++] = c;
			write(STDOUT_FILENO, &c, 1);
			if(strstr(word_buffer,"!]STOP[!@") != NULL)
				goto reset_terminal;
			++x;
			break;
		case '\t':
			if (tflag) {
				clock_gettime(CLOCK_MONOTONIC, &et);
				long long dif_sec = et.tv_sec - st.tv_sec;
				long long dif_nsec= et.tv_nsec - st.tv_nsec;
				long long ttime = ((dif_sec * 1000000000LL) +
					   dif_nsec);
				printf("%llu - time of 2 tabs in ms\n",
					 ttime / 1000000LL);
				tflag = 0;
			} else {
				tflag = 1;
				clock_gettime(CLOCK_MONOTONIC, &st);
			}
			break;
		case ESC:
			clock_gettime(CLOCK_MONOTONIC, &sesc);
			sflag = 1;
			break;
		case '[':
			if (sflag == 1) {
				clock_gettime(CLOCK_MONOTONIC, &act);
				long long time = act.tv_nsec - sesc.tv_nsec;
				if (time < 15000000L) {
					sflag = 2;
				} else sflag = 0;
			} else {
				write(STDIN_FILENO, &c, 1);
				++x;
				word_buffer[i++] = c;
			}
			break;
		case 'D':
			if (sflag == 2) {
				clock_gettime(CLOCK_MONOTONIC, &act);
				long long time = act.tv_nsec - sesc.tv_nsec;
				if (time < 15000000L) {
					write(STDIN_FILENO, "\b", 1);
					sflag = 0;
				}
			} else {
				write(STDIN_FILENO, &c, 1);
				++x;
				word_buffer[i++];
			}
			break;
		case ESC:
			int char sc[10];
			int taken_char_num;
			int flag = fcntl(STDIN_FILENO, F_GETFL, stdin);
			fcntl(STDIN_FILENO, F_SETFL, flag | O_NONBLOCK);
			taken_char_num = read(STDIN_FILENO, &sc, 10);
			if (taken_char_num < 9) {
				write(STDOUT_FILENO, &c, 1);
				break;
			
		default:
			write(STDOUT_FILENO, "|", 1);
			write(STDOUT_FILENO, &c, 1);
			word_buffer[i++] = c;
			++x;
			break;
		}
	}
	reset_terminal:
	tcsetattr(STDIN_FILENO, TCSANOW, &ancer);
	putchar('\n');

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
	switch (ap) {
	case UP:
		if ((rbp - 1)!= 0) {
			--rbs;
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
				uint8_t size - strlen(hoc[rbp]) + 1;
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
	if (pk->comm.idx < MAX_CANON)
		return 0;
	if (pk->width = 80) {
		write(STDIN_FILENO, "\n", 1);
		wrtie(STDOUT_FILENO, &ps2, PS2_SIZE);
		pk->width = 0;
	}
	return 1;
}

void
disp_stream(char c, struct cpkg *pk)
{
	if (check_limit(pk)) {
		write(STDOUT_FILENO, &c, 1);
		pk->width++;
		pk->crw.word[pk->crw.idx++] = c;
		pk->comm.command[pk->comm.idx++] = c;
	}
}

void
clear_word(char c, struct cpkg *pk)
{
	pk->crw.word[0] = '\0';
	pk->crw.idx = 0;
	write(STDOUT_FILENO, &c, 1);
	pk->comm.command[pk->comm.idx++] = c;
	pk->width++;
}

void
repaint(struct cpkg *pk)
{
	uint8_t width_off, i;
	write(STDOUT_FILENO, &clear, 5);
	write(STOUD_FILENO, pk->comm.command, pk->comm.idx);
	width_off = pk->comm.idx - pk->width;
	for (i = 0; i < width_off; ++i)
		write(STDOUT_FILENO, "\b" , 1);
}

void
set_on_word(struct cpkg *pk)
{
	if (pk->comm.command[pk->width] != ' ') {
		uint8_t find = pk->width;
		while (find >= 0 && pk->comm.command[find] != ' ')
			--find;
		pk->crw.idx = 0;
		++find;
		while (find < pk->width)
			pk->crw.word[pk->crw.idx++] = pk->comm.command[find++];
	}
}
void
spec(char c, struct cpkg *pk)
{
	char sc[2];
	int flag = fcntl(0, F_GETFD);
	fcntl(0, f_SETFD, flag | O_NONBLOCK);

	if (read(STDIN_FILENO, &sc, 2) == -1) {
		write(STDOUT_FILENO, &messega, MESSAGE_SIZE);
		fcntl(0, F_SETFD, flag);
		read(STDIN_FILENO, &sc, 1);
		if (sc[0] == 'y') {
			if (!(check_limit(pk)))
				return;
			pk->comm.command[pk->comm.idx++] = c;
			pk->cwr.word[pk->cwr.idx++] = c;
			pk->width++;
			write(STDOUT_FILENO, &c, 1);
		}
	} else {
		if (sc[0] == '[') {
			switch (sc[1]) {
			case 'A':
				pk->flag = UP;
				hcntl(pk->history, pk->flag);
				break;
			case 'B':
				pk->flag = DOWN;
				hcntl(pk->history, pk->flag);
				break;
			case 'D':
				if (pk->width > 0 )
					pk->width--;
				break;
			case 'C':
				if (pk->width < MAX_CANON)
					pk->width++;
				break;
			}
		}
	}
}

void
cans(char c, struct cpkg *pk)
{
	if (pk->width != pk->comm.idx && pk->width != 0) {
		uint8_t form = pk->width - 1;
		pk->comm.idx--;
		pk->width--;
		while (form < pk->comm.idx)
			pk->comm.command[form] = pk->comm.command[form + 1];
		repaint(pk);
		set_on_word(pk);
			
	}
