#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <sys/types.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <stdarg.h>
#include <time.h>
#include "keytable.h"
#include "tab.h"
#include "my_term.h"
#include "sterm.h"

#define ESC '\033'
#define DEL '\177'
#define NEWLINE '\n'
#define OPENBSD_MAX 77 /* style(9) 80 column max */
#define HISTORY_MAX_SIZE 250 
#define MAX_Y ((MAX_CANON / OPENBSD_MAX) * OPENBSD_MAX)
#define PS2_SIZE 4
#define BURN_N_REBUILD_MAX (MAX_CANON + ((MAX_CANON) / OPENBSD_MAX) * 5 + 32)
#define DONT_BREAK 0x1
#define BREAK 0x2
#define CANS 0xFF

int
main(void)
{
	struct termios 		raw;
	struct cpkg 		header;
	char 			command[MAX_CANON + 1];
	char			ps[] = "$ ";
	unsigned char 		c;
	char 			*command_history[HISTORY_MAX_SIZE + 1];
	void 			(*ascii[128])(char c, struct cpkg *);
	/* ascii array is declared in `keytable.h' */
	init_ascii(ascii);
	tcgetattr(STDIN_FILENO, &header.base);
	header.curs = 0;
	header.com.idx = 0;
	header.hst_num = 0;
	header.com.command = command;
	header.history = command_history;
	header.flag = SAVE;
	header.fill = ADD_SPACE;
	header.tptr = ascii;
	header.word_search = NULL;
	header.last_list_st = 0;
	header.mode = 0;
	header.ps = ps;
	command_history[0] = NULL;
	raw = header.base; 
	cfmakeraw(&raw);
	/* Setting enter to FL becouse in raw mode
	 * it is defaulter as CR
	 */
	raw.c_iflag |= ICRNL;
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);
	write(STDOUT_FILENO, ps, 2);

	while (read(STDIN_FILENO, &c, 1)) {
		if (c & (1 << 7))
			continue;
		ascii[c](c, &header);
	}

	clear_mem(&header);
	tcsetattr(STDIN_FILENO, TCSANOW, &header.base);
	return (0);
}

void
hcntl(struct cpkg *pk)
{
	int 		prev_curs;
	static int rbp = 0;
	static int rsp = 0;
	switch (pk->flag) {
	case UP:
		if (rbp == 0)
			break;
		--rbp;
		prev_curs = pk->com.idx;
		push_command(rbp, pk);
		burn_n_rebuild(pk, HISTORY, prev_curs);
		break;
	case DOWN:
		if (rbp == rsp) 
			break;
		++rbp;
		prev_curs = pk->com.idx;
		push_command(rbp, pk);
		burn_n_rebuild(pk, HISTORY, prev_curs);
		break;
	case SAVE:
		if (pk->com.command[0] == '\0')
			break;
		push_memory(&rsp, pk);
		break;
	case CLEAR:
		rbp = rsp;
		pk->hst_num = rsp;
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
		char 		*src, *dst;
		size_t size = pk->com.idx - pk->curs + 1;
		dst = &pk->com.command[pk->curs + 1];
		src = &pk->com.command[pk->curs];
		memmove(dst, src, size);
		pk->com.command[pk->curs] = c;
		id_shift(ID_RIGHT, pk);
		burn_n_rebuild(pk, INLINE_ADD, 0);
		cursor_cntl(QR, pk);
	} else {
		assigne(c, pk);
		id_shift(ID_RIGHT, pk);
		cursor_cntl(M_RIGHT, pk);
	}
}
void
spec(char c, struct cpkg *pk)
{
	struct timespec timeout, inttime;
	char 		sc[2];
	int flag = fcntl(0, F_GETFL);
	(void)c;
	fcntl(0, F_SETFL, flag | O_NONBLOCK);
	timeout.tv_sec = 0;
	timeout.tv_nsec = 25000000;
	nanosleep(&timeout, &inttime);

	if (read(STDIN_FILENO, &sc, 2) == -1) {
		fcntl(0, F_SETFL, flag);
		if (pk->tptr['x'] == disp_stream)
			set_vil_mode(pk->tptr);
	} else {
		fcntl(0, F_SETFL, flag);
		if (sc[0] == '[')
			parse_esc_com(sc[1], pk);
	}
}

void
cans(char c, struct cpkg *pk)
{
	int 		old_curs;
	(void)c;
	if (pk->curs != 0) {
		char 		*dst, *src;
		size_t size = pk->com.idx - pk->curs + 1;
		old_curs = pk->curs;
		cursor_cntl(M_LEFT, pk);
		dst = &pk->com.command[pk->curs];
		src = &pk->com.command[pk->curs + 1];
		memmove(dst, src, size);
		id_shift(ID_LEFT, pk);
		if (pk->curs == pk->com.idx)
			burn_n_rebuild(pk, HISTORY, old_curs);
		else
			burn_n_rebuild(pk, INLINE_CANS, 0);
	}
}

void
check_break(char c, struct cpkg *pk)
{
	(void)c;
	write(STDOUT_FILENO, "\r\n", 2);
	tcsetattr(STDIN_FILENO, TCSANOW, &pk->base);
	clear_mem(pk);
	exit(2);
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
parse_esc_com(char c, struct cpkg *pk)
{
	if (c < 0)
		return;
	if (pk->tptr['k'] != disp_stream) 
		return;
	switch (c) {
	case 'A':
		if (pk->tptr[LF] == rvs_search_mode)
			break;
		if (pk->com.idx == pk->curs) {
			pk->flag = UP;
			hcntl(pk);
		}
		break;
	case 'B':
		if (pk->tptr[LF] == rvs_search_mode)
			break;
		if (pk->com.idx == pk->curs) {
			pk->flag = DOWN;
			hcntl(pk);
		}
		break;
	case 'D':
		cursor_cntl(QL, pk);
		break;
	case 'C':
		cursor_cntl(QR, pk);
		break;
	default:
		break;
	}
}

void
cursor_cntl(enum cursmov action, struct cpkg *pk)
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
	case QL:
		if (pk->curs > 0) {
			if (pk->curs % OPENBSD_MAX == 0 && pk->curs > 0) {
				write(STDOUT_FILENO, "\033[A", 3);
				set_cursor(--pk->curs, pk->ps);
				break;
			}
			--pk->curs;
			write(STDOUT_FILENO, "\033[D", 3);
		}
		break;
	case QR:
		if (pk->curs == pk->com.idx)
			break;
		++pk->curs;
		if (pk->curs % OPENBSD_MAX == 0 && pk->curs > 0) {
			write(STDOUT_FILENO, "\033[B", 3);
			set_cursor(pk->curs, pk->ps);
			break;
		}
		write(STDOUT_FILENO, "\033[C", 3);
		break;
	}
}

void
id_shift(enum sid s, struct cpkg *pk)
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
	write(STDOUT_FILENO, &c, 1);
	if (pk->com.idx > 0 && (pk->com.idx + 1) % OPENBSD_MAX == 0)
		break_line(pk);
}

void
push_memory(int *rsp, struct cpkg *pk)
{
	int 		len;
	char 		*ptr;
	len = strlen(pk->com.command);
	if ((ptr = malloc(len + 1)) == NULL)
		return;
	
	strlcpy(ptr, pk->com.command, len + 1);
	if (*rsp == HISTORY_MAX_SIZE) {
		size_t size = (HISTORY_MAX_SIZE - 1) * sizeof(char *);
		free(pk->history[0]);
		memmove(&pk->history[0], &pk->history[1], size);
		pk->history[HISTORY_MAX_SIZE - 1] = ptr;
	} else {
		pk->history[*rsp + 1] = pk->history[*rsp];
		pk->history[*rsp] = ptr;
		++(*rsp);
	}
}

void
push_command(int rbp, struct cpkg *pk)
{
	size_t 		size;
	if (pk->history[rbp] == NULL) {
		pk->com.command[0] = '\0';
		pk->com.idx = 0;
		pk->curs = 0;
		return;
	}
	size = strlen(pk->history[rbp]);
	strlcpy(pk->com.command, pk->history[rbp], size + 1);
	pk->com.command[size] = '\0';
	pk->com.idx = size;
	pk->curs = pk->com.idx;
}

void
set_cursor(int position, char *PS1)
{
	char 		max_mov[7];
	int 		len;
	position %= OPENBSD_MAX;
	position += strlen(PS1);
	len = snprintf(max_mov, sizeof(max_mov), "\r\033[%dC", position);
	write(STDOUT_FILENO, max_mov, len);
}

void
clear_comm(char c, struct cpkg *pk)
{
	int 		len, slide, linep;
	char 		buf[32];
	size_t		pssize;
	(void)c;
	pk->flag = SAVE;
	hcntl(pk);
	pk->flag = CLEAR;
	hcntl(pk);
	linep = (pk->com.idx > 0) ? ((pk->com.idx - 1) / OPENBSD_MAX): 0; 
	slide = linep - (pk->curs / OPENBSD_MAX);
	if (slide) {
		len = snprintf(buf, sizeof(buf), "\033[%dB\n\r", slide);
		pssize = strlen(pk->ps);
		strlcpy(&buf[len], pk->ps, pssize + 1);
		write(STDOUT_FILENO, buf, len + pssize);
	} else {
		len = snprintf(buf, sizeof(buf), "\n\r%s", pk->ps);
		write(STDOUT_FILENO, buf, strlen(buf));
	}
	pk->curs = 0;
	pk->com.idx = 0;
	pk->com.command[0] = '\0';
}

void
clear_page(struct cpkg *pk)
{
	const char 	erease_one_up[] = "\r\033[K\033[A";
	const char 	clean[] = "\033[K";
	int 		iter, times;
	int len_er_one_up = strlen(erease_one_up);
	times = pk->com.idx / OPENBSD_MAX;
	for (iter = 0; iter < times; ++iter)
		write(STDOUT_FILENO, erease_one_up, len_er_one_up);
	write(STDOUT_FILENO, clean, 3);
}

void
break_line(struct cpkg *pk)
{
	char 		buf[20];
	size_t		len;
	len = snprintf(buf, sizeof(p2), "\n\r>%-*s", strlen(pk->ps) - 1, "");
	write(STDOUT_FILENO, buf, len);
}

void
burn_n_rebuild(struct cpkg *pk, enum rebuild state, int old_curs)
{
	char 		buffer[BURN_N_REBUILD_MAX];
	size_t 		len, bsize, pssize;
	int 		y, iter, cl;
	cl = y = 0;
	bsize = sizeof(buffer);
	if (state == INLINE_CANS) {
		int 	linep;
		y = pk->curs / OPENBSD_MAX;
		linep = (pk->com.idx - 1) / OPENBSD_MAX;
		linep = (pk->com.idx > 0) ? linep: 0;
		cl = linep - y;
		if ((pk->curs + 1) % OPENBSD_MAX == 0 && pk->curs > 0)
			++y;
		cl = (cl > 0) ? cl: 0;
	} else if (state == INLINE_ADD) {
		int 	linep;
		y = pk->curs / OPENBSD_MAX;
		linep = (pk->com.idx - 1) / OPENBSD_MAX;
		linep = (pk->com.idx > 0) ? linep: 0;
		cl = linep - y;
		cl = (cl > 0) ? cl: 0;
	} else if (state == HISTORY) {
		y = old_curs / OPENBSD_MAX;
		cl = 0;
	}
	if (y)
		len = snprintf(buffer, bsize, "\033[%dA\r\033[J", y);
	else
		len = snprintf(buffer, bsize, "\r\033[J");
	pssize = strlen(pk->ps);
	strlcpy(&buffer[len], pk->ps, pssize + 1);
	len += pssize;
	for (iter = 0; iter < pk->com.idx; ++iter) {
		if (iter % OPENBSD_MAX == 0 && iter > 0) {
			int 		it;
			buffer[len++] = '\n';
			buffer[len++] = '\r';
			buffer[len++] = '>';
			for (it = 0; it < (int)strlen(pk->ps); ++it)
				buffer[len++] = ' ';
		}
		buffer[len++] = pk->com.command[iter];
	}
	if (cl > 0)
		len += snprintf(&buffer[len], bsize - len, "\033[%dA", cl);
	if (pk->com.idx > 0 && pk->curs % OPENBSD_MAX == 0 &&
	    pk->curs == pk->com.idx)
		len += snprintf(&buffer[len], bsize - len, "\n\r>  ");
	write(STDOUT_FILENO, buffer, len);
	set_cursor(pk->curs, pk->ps);
}

void
neutral(char c, struct cpkg *pk)
{
	(void)c;
	(void)pk;
}

void
find_fill(char c, struct cpkg *pk)
{
	char 		path[PATH_MAX], word[MAX_CANON]; 
	char 		*dummy, **xdirs, *dir, *tm;
	int 		fidx, sofw, xf, i, flag;
	struct word_t	sword;
	size_t		wsize;
	(void)c;
	xf = i = flag = 0;
	dummy = NULL;
	sword = parse(pk);
	sofw = pk->curs - sword.sidx;
	strlcpy(word, &pk->com.command[sword.sidx], sofw + 1);
	wsize = strlen(word);
	if (wsize > 0) {
		char		*src, *new;
		src = new = word;
		while (*src != '\0') {
			if (*src == '\"') {
				++src;
			} else if (*src == '\\') {
				++src;
				if (*src != '\0')
					*new++ = *src++;
			} else {
				*new++ = *src++;
			}
		}
		*new = '\0';
	}
	switch (sword.type) {
	case INWORD:
	case FEXEC:
		if (getcwd(path, PATH_MAX) == NULL)
			return;
		if (wsize == 1 && word[0] == ' ')
			dummy = fill_command("", path, &pk->fill);
		else
			dummy = fill_command(word, path, &pk->fill);
		if (dummy == NULL) 
			break;
		if (strcmp(dummy, "NOTF") == 0)
			break;
		if (strcmp(dummy, "MF") == 0)
			break;
		if (strcmp(dummy, "TM") == 0)
			break;
		shift_n_fill(pk, sofw, dummy, sword);
		burn_n_rebuild(pk, INLINE_ADD, 0);
		return;
	case DIRECT:
		if (pk->curs == 1) {
			dummy = fill_command("", word, &pk->fill);
			if (dummy == NULL)
				break;
			if (strcmp(dummy, "NOTF") == 0)
				break;
			if (strcmp(dummy, "MF") == 0)
				break;
			if (strcmp(dummy, "TM") == 0)
				break;
			shift_n_fill(pk, sofw, dummy, sword);
			burn_n_rebuild(pk, INLINE_ADD, 0);
			return;
		}
		for (fidx = wsize - 1; fidx > 0; --fidx) {
			if (word[fidx] == '/')
				break;
		}
		strlcpy(path, word, fidx + 2);
		if ((dir = parse_dir(path)) == NULL)
			break;
		if (strcmp(dir, "SP") != 0) {
			dummy = fill_command(&word[fidx + 1], dir, &pk->fill);
			if (dummy == NULL) {
				free(dir);
				break;
			}
			free(dir);
		} else {
			dummy = fill_command(&word[fidx + 1], path, &pk->fill);
			if (dummy == NULL)
				break;
		}
		if (strcmp(dummy, "NOTF") == 0)
			break;
		if (strcmp(dummy, "MF") == 0)
			break;
		if (strcmp(dummy, "TM") == 0)
			break;
		if ((tm = malloc(strlen(path) + strlen(dummy) + 1)) == NULL) {
			free(dummy);
			break;
		}
		snprintf(tm, strlen(path) + strlen(dummy) + 1, "%s%s",
			 path, dummy);
		free(dummy);
		shift_n_fill(pk, sofw, tm, sword);
		burn_n_rebuild(pk, INLINE_ADD, 0);
		return;
	case EXEC:
	case INFST:
		if ((xdirs = exec_paths(&xf)) == NULL)
			return;
		if (wsize == 0 || (wsize == 1 && word[0] == ' '))
			dummy = fill_command("", xdirs[0], &pk->fill);
		if (dummy == NULL || (strcmp(dummy, "NOTF") == 0)) {
			for (i = 0; i < xf; ++i) {
				dummy = fill_command(word, xdirs[i], &pk->fill);
				if (dummy == NULL) {
					free(xdirs[i]);
					continue;
				}
				if (strcmp(dummy, "NOTF") == 0) {
					free(xdirs[i]);
					continue;
				}
				break;
			}
		}
		if (dummy != NULL && strcmp(dummy, "NOTF") != 0 &&
	 	    strcmp(dummy, "TM") != 0 && strcmp(dummy, "MF") != 0) {
			shift_n_fill(pk, sofw, dummy, sword);
			burn_n_rebuild(pk, INLINE_ADD, 0);
			flag = 1;
		}
		for (; i < xf; ++i)
			free(xdirs[i]);
		free(xdirs);
		if (flag)
			return;
		break;
	case DOT:
		return;
	}
	burn_n_rebuild(pk, HISTORY, 0);
}

void
shift_n_fill(struct cpkg *pk, int sofw, char *dummy, struct word_t sword)
{
	int		ntm, fwh;
	char		*dst, *src;
	size_t		size, len;
	fwh = 1;
	if (strcmp(dummy, "NOTF") == 0 || dummy == NULL)
		return;
	if (pk->curs != pk->com.idx || sword.type == DIRECT ||
		pk->fill == CLOSE_FILL) {
		fwh = 0;
		pk->fill = ADD_SPACE;
		if (pk->com.idx + (strlen(dummy) - sofw) > MAX_CANON) {
			free(dummy);
			dummy = NULL;
			return;
		}
	} else if (pk->com.idx + (strlen(dummy) - sofw) + 1 > MAX_CANON) {
		free(dummy);
		dummy = NULL;
		return;
	}
	len = strlen(dummy);
	ntm = len - sofw + 1 *fwh;
	src = &pk->com.command[pk->curs];
	dst = &pk->com.command[pk->curs + ntm];
	size = pk->com.idx - pk->curs;
	memmove(dst, src, size + 1);
	memcpy(&pk->com.command[sword.sidx], dummy, len);
	if (fwh)
		pk->com.command[sword.sidx + len] = ' ';
	pk->curs = sword.sidx + len + 1 * fwh;
	pk->com.idx += ntm;
	free(dummy);
	dummy = NULL;
}

char *
parse_dir(char *form)
{
	char		*ptr, *dir, path[PATH_MAX];
	static char	message[] = "SP"; /* abbriev special path */
	size_t		len, size;
	len = strlen(form);
	if (len < 1)
		return (NULL);
	switch (form[0]) {
	case '~':
		if (form[1] == '/') {
			if ((ptr = getenv("HOME")) == NULL)
				return (NULL);
			if ((dir = strdup(ptr)) == NULL)
				return (NULL);
			if (len == 2)
				return (dir);
			size = strlen(dir);
			if (size + len > PATH_MAX) {
				free(dir);
				return (NULL);
			}
			ptr = reallocarray(dir, size + len, 1);
			if (ptr == NULL) {
				free(dir);
				return (NULL);
			}
			dir = ptr;
			strlcpy(&dir[size], &form[1], len);
			return (dir);
		}
		break;
	case '.':
		if (form[1] == '/') {
			if ((ptr = getcwd(path, PATH_MAX)) == NULL)
				return (NULL);
			size = strlen(path);
			if (size + len > PATH_MAX)
				return (NULL);
			strlcpy(&path[size], &form[1], len);
			if ((ptr = strdup(path)) == NULL)
				return (NULL);
			return (ptr);
		}
		break;
	}
	return (message);
}
