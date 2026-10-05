#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <sys/types.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <termios.h>
#include <stdarg.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include "keytable.h"
#include "tab.h"
#include "my_term.h"
#include "sterm.h"

#define ESC '\033'
#define DEL '\177'
#define NEWLINE '\n'
#define OPENBSD_MAX 80 /* style(9) 80 column max */
#define HISTORY_MAX_SIZE 250 
#define PS2_SIZE 1
#define BURN_N_REBUILD_MAX \
(MAX_CANON + ((MAX_CANON) / (OPENBSD_MAX - 65) * 5 + 32))
#ifdef __AFL_COMPILER
#define WAIT_FOR_SPEC 0
#else
#define WAIT_FOR_SPEC 25
#endif

static volatile sig_atomic_t term_sig = 0;
#ifndef TEST
int
main(void)
{
	char 			command[MAX_CANON + 1];
	struct termios 		raw;
	struct cpkg 		header;
	struct pollfd		fds[1];
	struct sigaction	sa;
	char 			*command_history[HISTORY_MAX_SIZE + 1], *ptr;
	void 			(*ascii[128])(char c, struct cpkg *);
	unsigned char 		c;
	char			ps[] = "$ ";
	size_t			ps_size;
	nfds_t fds_num = 1;
	sa.sa_handler = handle_term_inter;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGWINCH, &sa, NULL);
	fds[0].fd = STDIN_FILENO;
	fds[0].events |= POLLIN;
	/* ascii array is declared in `keytable.h' */
	init_ascii(ascii);
#ifndef __AFL_COMPILER
	tcgetattr(STDIN_FILENO, &header.base);
#endif
	header.curs = 0;
	header.com.idx = 0;
	header.com.max_hst_num = 0;
	header.hst_num = 0;
	header.com.command = command;
	header.history = command_history;
	header.rvs_search_hst_max_prt = 10;
	header.flag = SAVE;
	header.fill = ADD_SPACE;
	header.tptr = ascii;
	header.word_search = NULL;
	header.mode = 0;
	command_history[0] = NULL;
#ifndef __AFL_COMPILER
	raw = header.base; 
	cfmakeraw(&raw);
	/* Setting enter to FL becouse in raw mode
	 * it is defaulter as CR
	 */
	raw.c_iflag |= ICRNL;
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);
#endif
	if ((ptr = getenv("PS1")) == NULL) {
		header.ps = ps;
	} else {
		ps_size = strlen(ptr);
		if ((header.ps = malloc(ps_size + 3)) == NULL) {
			header.ps = ps;
		} else {
			strlcpy(header.ps, ptr, ps_size + 1);
			header.ps[ps_size++] = '$';
			header.ps[ps_size++] = ' ';
			header.ps[ps_size] = '\0';
		}
	}
	set_screen_limit(&header);

	write(STDOUT_FILENO, header.ps, strlen(header.ps));

	while (1) {
		if (term_sig == 1) {
			set_screen_limit(&header);
			term_sig = 0;
		}
		if (poll(fds, fds_num, -1) > 0) {
			if (fds[0].revents & POLLIN) {
				int n = read(STDIN_FILENO, &c, 1);
				if (n <= 0)
					break;
				if (c & (1 << 7))
					continue;
				ascii[c](c, &header);
			}
		}
	}
	if (header.ps != ps)
		free(header.ps);
	clear_mem(&header);
#ifndef __AFL_COMPILER
	tcsetattr(STDIN_FILENO, TCSANOW, &header.base);
#endif
	return (0);
}
#endif
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
		pk->com.max_hst_num = rsp;
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
	char 			sc[2];
	struct pollfd		fds;
	nfds_t nfds = 1;
	fds.fd = STDIN_FILENO;
	fds.events = POLLIN;
	/*struct timespec timeout, inttime;
	int flag = fcntl(0, F_GETFL);
	fcntl(0, F_SETFL, flag | O_NONBLOCK);
	timeout.tv_sec = 0;
	timeout.tv_nsec = 25000000;
	nanosleep(&timeout, &inttime); */
	
	(void)c;

	if (poll(&fds, nfds, WAIT_FOR_SPEC) <= 0) {
		/*fcntl(0, F_SETFL, flag);*/
		if (pk->tptr['x'] == disp_stream)
			set_vil_mode(pk->tptr);
	} else {
		/*fcntl(0, F_SETFL, flag);*/
		read(STDIN_FILENO, &sc, 2);
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
			if (pk->curs % pk->screen_limit == 0 &&
			    pk->curs > 0) {
				write(STDOUT_FILENO, "\033[A", 3);
				set_cursor(--pk->curs, pk);
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
		if (pk->curs % pk->screen_limit == 0 && pk->curs > 0) {
			write(STDOUT_FILENO, "\033[B", 3);
			set_cursor(pk->curs, pk);
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
	if (pk->com.idx > 0 && (pk->com.idx + 1) % pk->screen_limit == 0)
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
set_cursor(int position, struct cpkg *pk)
{
	char 		max_mov[16];
	int 		len;
	position %= pk->screen_limit;
	position += strlen(pk->ps);
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
	linep = (pk->com.idx > 0) ? ((pk->com.idx - 1) / pk->screen_limit): 0;
	slide = linep - (pk->curs / pk->screen_limit);
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
	times = pk->com.idx / pk->screen_limit;
	for (iter = 0; iter < times; ++iter)
		write(STDOUT_FILENO, erease_one_up, len_er_one_up);
	write(STDOUT_FILENO, clean, 3);
}

void
break_line(struct cpkg *pk)
{
	char 		buf[20];
	size_t		len;
	len = snprintf(buf, sizeof(buf), "\n\r>%-*s", (int)strlen(pk->ps) - 1, "");
	write(STDOUT_FILENO, buf, len);
}

void
burn_n_rebuild(struct cpkg *pk, enum rebuild state, int old_curs)
{
	char 		buffer[BURN_N_REBUILD_MAX];
	size_t 		len, bsize, pssize;
	int 		y, iter, cl;
	cl = y = len = 0;
	bsize = sizeof(buffer);
	if (state == INLINE_CANS) {
		int 	linep;
		y = pk->curs / pk->screen_limit;
		linep = (pk->com.idx - 1) / pk->screen_limit;
		linep = (pk->com.idx > 0) ? linep: 0;
		cl = linep - y;
		if ((pk->curs + 1) % pk->screen_limit == 0 && pk->curs > 0)
			++y;
		cl = (cl > 0) ? cl: 0;
	} else if (state == INLINE_ADD || state == TERM_RB) {
		int 	linep;
		y = pk->curs / pk->screen_limit;
		linep = (pk->com.idx - 1) / pk->screen_limit;
		linep = (pk->com.idx > 0) ? linep: 0;
		cl = linep - y;
		cl = (cl > 0) ? cl: 0;
	} else if (state == HISTORY) {
		y = old_curs / pk->screen_limit;
		cl = 0;
	}
	if (y && state != TERM_RB)
		len = snprintf(buffer, bsize, "\033[%dA\r\033[J", y);
	else if (state != TERM_RB)
		len = snprintf(buffer, bsize, "\r\033[J");
	pssize = strlen(pk->ps);
	strlcpy(&buffer[len], pk->ps, pssize + 1);
	len += pssize;
	for (iter = 0; iter < pk->com.idx; ++iter) {
		if (iter % pk->screen_limit == 0 && iter > 0) {
			int 		it;
			buffer[len++] = '\n';
			buffer[len++] = '\r';
			buffer[len++] = '>';
			for (it = 0; it < (int)strlen(pk->ps) - 1; ++it)
				buffer[len++] = ' ';
		}
		buffer[len++] = pk->com.command[iter];
	}
	if (cl > 0)
		len += snprintf(&buffer[len], bsize - len, "\033[%dA", cl);
	if (pk->com.idx > 0 && pk->curs % pk->screen_limit == 0 &&
	    pk->curs == pk->com.idx)
		len += snprintf(&buffer[len], bsize - len, "\n\r>%-*s",
				(int)strlen(pk->ps) - 1, "");
	write(STDOUT_FILENO, buffer, len);
	set_cursor(pk->curs, pk);
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
	char 		path[PATH_MAX], word[MAX_CANON + 1]; 
	char 		*dummy, **xdirs, *dir, *tm;
	int 		fidx, sofw, xf, i, flag;
	struct word_t	sword;
	size_t		wsize;
	(void)c;
	xf = i = flag = 0;
	dummy = NULL;
	sword = parse(pk);
	sofw = pk->curs - sword.sidx;
	if (sofw > MAX_CANON)
		sofw = MAX_CANON;
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
	if (dummy == NULL || strcmp(dummy, "NOTF") == 0)
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

void
set_screen_limit(struct cpkg *pk)
{
	struct winsize			term_size;
	static uint8_t last_screen_limit = 0;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &term_size) == -1) {
		pk->screen_limit = OPENBSD_MAX - strlen(pk->ps);
		if (!last_screen_limit)
			last_screen_limit = pk->screen_limit;
		return;
	}
	if (pk->com.idx == 0) {
		if (term_size.ws_col >= OPENBSD_MAX)
			pk->screen_limit = OPENBSD_MAX - strlen(pk->ps);
		else
			pk->screen_limit = term_size.ws_col - strlen(pk->ps);
		last_screen_limit = pk->screen_limit;
		return;
	}
	if (term_size.ws_col >= OPENBSD_MAX) {
		if (last_screen_limit == 0) {
			pk->screen_limit = OPENBSD_MAX - strlen(pk->ps);
			last_screen_limit = pk->screen_limit;
			return;
		}
		if (last_screen_limit < OPENBSD_MAX) {
			char			buf[20];
			size_t			bsize;
			uint8_t 		nol;
			pk->screen_limit = OPENBSD_MAX - strlen(pk->ps);
			nol = (pk->curs / last_screen_limit);
			if (nol <= 0) {
				last_screen_limit = pk->screen_limit;
				burn_n_rebuild(pk, INLINE_ADD, 0);
				return;
			}
			bsize = snprintf(buf, sizeof(buf), "\033[%dA\r\033[J",
					 (int)nol);
			write(STDOUT_FILENO, buf, bsize);
			burn_n_rebuild(pk, TERM_RB, 0);
			last_screen_limit = pk->screen_limit;
			return;
		}
	}
	if (term_size.ws_col < OPENBSD_MAX) {
		pk->screen_limit = term_size.ws_col - strlen(pk->ps);
		if (last_screen_limit > pk->screen_limit) {
			char			buf[20];
			size_t			bsize, i, nol;
			nol = 0;
			for (i = 0; (int)i <= pk->curs; ++i) {
				size_t		cond1, cond2;
				cond1 = i % last_screen_limit;
				cond2 = i % pk->screen_limit;
				if (i > 0 && cond1 == 0)
					nol++;
				if (i > 0 && cond2 == 0 && (int)i < pk->curs)
					nol++;
				if (cond1 == 0 && cond2 == 0 && i > 0)
					nol--;
			}
			if (nol <= 0) {
				last_screen_limit = pk->screen_limit;
				burn_n_rebuild(pk, INLINE_ADD, 0);
				return;
			}
			bsize = snprintf(buf, sizeof(buf), "\033[%dA\r\033[J",
					 (int)nol);
			write(STDOUT_FILENO, buf, bsize);
			burn_n_rebuild(pk, TERM_RB, 0);
			last_screen_limit = pk->screen_limit;
			return;
		} else if (last_screen_limit < pk->screen_limit) {
			char			buf[20];
			size_t			bsize;
			uint8_t 		nol;
			nol = (pk->curs / last_screen_limit);
			if (nol <= 0) {
				last_screen_limit = pk->screen_limit;
				burn_n_rebuild(pk, INLINE_ADD, 0);
				return;
			}
			bsize = snprintf(buf, sizeof(buf), "\033[%dA\r\033[J",
					 (int)nol);
			write(STDOUT_FILENO, buf, bsize);
			burn_n_rebuild(pk, TERM_RB, 0);
			last_screen_limit = pk->screen_limit;
			return;
		}
	}
	last_screen_limit = pk->screen_limit;
}

void
handle_term_inter(int c)
{
	(void)c;
	term_sig = 1;
}
