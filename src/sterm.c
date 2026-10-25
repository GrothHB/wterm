#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "keytable.h"
#include "tab_compl.h"
#include "my_term.h"
#include "sparse.h"
#include "rvs_mode.h"
#include "parse_env.h"
#include "sterm.h"

#define ESC '\033'
#define DEL '\177'
#define NEWLINE '\n'
#define OPENBSD_MAX 80 /* style(9) 80 column max */
#ifndef HISTORY_MAX_SIZE
#define HISTORY_MAX_SIZE 250 
#endif /* HISTORY_MAX_SIZE */
#ifndef _POSIX_ARG_MAX
#define _POSIX_ARG_MAX 4096
#endif /* _POSIX_ARG_MAX */
#define PS2_SIZE 1
#define BURN_N_REBUILD_MAX (MAX_CANON * 2 + 32)
#define COMMAND_IDX_REACH(x) ((x) >= 0 && (x) <= MAX_CANON + 1)
#define SCREEN_COND ( pk->screen_limit > 0 && l_col > 0 && l_col <= \
		      OPENBSD_MAX && pk->screen_limit <= \
		      OPENBSD_MAX - strlen(pk->ps) )

static char              *check_exec_safety(const char *, const char *, const char *);
static void              check_arg_max(void);
static char              *parse_dir(char *);
static void              assigne(char, struct cpkg *);
static void              break_line(const struct cpkg *);
static void              clear_mem(struct cpkg *);
static void              cursor_cntl(enum cursmov, struct cpkg *);
static void              handle_term_inter(int);
static void              hcntl(struct cpkg *);
static void              id_shift(enum sid, struct cpkg *);
static void              init_sig(void);
static void              init_term(struct cpkg *);
static void              parse_esc_com(char, struct cpkg *);
static void              push_command(int, struct cpkg *);
static void              push_memory(int *, struct cpkg *);
static void              set_screen_limit(struct cpkg *);
static void              shift_n_fill(struct cpkg *, int, char *, struct word_t);
static void		 init_term(struct cpkg *);
static void 		 init_sig(void);

extern char **environ;
static volatile sig_atomic_t term_sig = 0;
static long int arg_max = 0;

int
main(void)
{
	char command[MAX_CANON + 1];
	char *command_history[HISTORY_MAX_SIZE + 1], *ptr;
	void (*ascii[128])(char c, struct cpkg *);
	struct cpkg header;
	struct pollfd fds[1];
	char ps[] = "$ ";
	char ps1[24];
	int timeout;
	unsigned char c;
	nfds_t fds_num = 1;
	timeout = -1;
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	/* ascii array is declared in `keytable.h' */
	init_ascii(ascii);
	header.com.command = command;
	command_history[0] = NULL;
	header.history = command_history;
	header.tptr = ascii;
	init_term(&header);
	init_sig();
	if ((ptr = getenv("PS1")) == NULL) {
		header.ps = ps;
	} else {
		size_t ps_size;
		ps_size = strlen(ptr);
		if (ps_size > 21) {
			header.ps = ps;
		} else {
			strlcpy(ps1, ptr, sizeof(ps1));
			header.ps = ps1;
			header.ps[ps_size++] = '$';
			header.ps[ps_size++] = ' ';
			header.ps[ps_size] = '\0';
		}
	}
	assert(strlen(header.ps) <= 23);
	set_screen_limit(&header);

	write(STDOUT_FILENO, header.ps, strlen(header.ps));

	while (1) {
		if (term_sig == 1) {
			term_sig = 0;
			set_screen_limit(&header);
		}
		if (poll(fds, fds_num, timeout) > 0) {
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
	clear_mem(&header);
	clear_hash();
	tcsetattr(STDIN_FILENO, TCSANOW, &header.base);
	return (0);
}

static void
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
		assert(COMMAND_IDX_REACH(pk->com.idx));
		prev_curs = pk->com.idx;
		push_command(rbp, pk);
		burn_n_rebuild(pk, HISTORY, prev_curs);
		break;
	case DOWN:
		if (rbp == rsp) 
			break;
		++rbp;
		assert(COMMAND_IDX_REACH(pk->com.idx));
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
	if (pk->com.idx >= MAX_CANON)
		return;
	if (pk->curs != pk->com.idx) {
		char 		*src, *dst;
		size_t size = pk->com.idx - pk->curs + 1;
		assert(COMMAND_IDX_REACH(pk->curs));
		dst = &pk->com.command[pk->curs + 1];
		src = &pk->com.command[pk->curs];
		assert(size + 1 <= MAX_CANON && size > 0);
		memmove(dst, src, size);
		pk->com.command[pk->curs] = c;
		assert(COMMAND_IDX_REACH(pk->com.idx));
		id_shift(ID_RIGHT, pk);
		burn_n_rebuild(pk, INLINE_ADD, 0);
		cursor_cntl(QR, pk);
	} else {
		assigne(c, pk);
		assert(COMMAND_IDX_REACH(pk->curs));
		assert(COMMAND_IDX_REACH(pk->com.idx));
		id_shift(ID_RIGHT, pk);
		cursor_cntl(M_RIGHT, pk);
	}
}

void
spec(char c, struct cpkg *pk)
{
	static int time_period = -1;
	struct pollfd fds;
	nfds_t nfds = 1;
	fds.fd = STDIN_FILENO;
	fds.events = POLLIN;
	(void)c;
	if (time_period == -1) {
		const char *t;
		if ((t = getenv("MY_KEYTIMEP")) != NULL) {
			char *endptr;
			int val = strtol(t, &endptr, 0);
			if (*endptr != '\0' || endptr == t || val < 0)
				time_period = 50;
			else
				time_period = val;
		} else if (getenv("SSH_TTY") != NULL) {
			time_period = 200;
		} else {
			time_period = 50;
		}
	}
	if (poll(&fds, nfds, time_period) <= 0) {
		if (pk->tptr['x'] == disp_stream)
			set_vil_mode(pk->tptr);
	} else {
		if (fds.revents & POLLIN) {
			char sc[16];
			ssize_t nc = read(STDIN_FILENO, sc, sizeof(sc));
			if (2 == nc && sc[0] == '[')
				parse_esc_com(sc[1], pk);
		}
	}
}

void
cans(char c, struct cpkg *pk)
{
	(void)c;
	if (pk->curs != 0) {
		char *dst, *src;
		int old_curs;
		assert(COMMAND_IDX_REACH(pk->curs));
		size_t size = pk->com.idx - pk->curs + 1;
		old_curs = pk->curs;
		cursor_cntl(M_LEFT, pk);
		dst = &pk->com.command[pk->curs];
		src = &pk->com.command[pk->curs + 1];
		assert(size + 1 <= MAX_CANON && size > 0);
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

static void
clear_mem(struct cpkg *pk)
{
	int i = 0;
	while (pk->history[i] != NULL && i < HISTORY_MAX_SIZE) {
		free(pk->history[i]);
		pk->history[i] = NULL;
		++i;
	}
}

static void
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
		assert(COMMAND_IDX_REACH(pk->curs));
		if (pk->com.idx == pk->curs) {
			pk->flag = UP;
			hcntl(pk);
		}
		break;
	case 'B':
		if (pk->tptr[LF] == rvs_search_mode)
			break;
		assert(COMMAND_IDX_REACH(pk->curs));
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

static void
cursor_cntl(enum cursmov action, struct cpkg *pk)
{
	switch (action) {
	case M_LEFT:
		assert(COMMAND_IDX_REACH(pk->curs));
		if (pk->curs > 0)
			--pk->curs;
		break;
	case M_RIGHT:
		assert(COMMAND_IDX_REACH(pk->curs));
		if (pk->curs == pk->com.idx)
			break;
		pk->curs++;
		break;
	case QL:
		assert(COMMAND_IDX_REACH(pk->curs));
		if (pk->curs > 0) {
			if (pk->curs % pk->screen_limit == 0) {
				write(STDOUT_FILENO, "\033[A", 3);
				set_cursor(--pk->curs, pk);
				break;
			}
			--pk->curs;
			write(STDOUT_FILENO, "\033[D", 3);
		}
		break;
	case QR:
		assert(COMMAND_IDX_REACH(pk->curs));
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

static void
id_shift(enum sid s, struct cpkg *pk)
{
	switch (s) {
	case ID_RIGHT:
		assert(COMMAND_IDX_REACH(pk->com.idx));
		if (pk->com.idx >= MAX_CANON)
			break;
		++pk->com.idx;
		break;
	case ID_LEFT:
		assert(COMMAND_IDX_REACH(pk->com.idx));
		if (pk->com.idx == 0)
			break;
		pk->com.idx--;
		break;
	}
}

static void
assigne(char c, struct cpkg *pk)
{
	assert(COMMAND_IDX_REACH(pk->com.idx));
	pk->com.command[pk->com.idx]  = c;
	pk->com.command[pk->com.idx + 1] = '\0';
	write(STDOUT_FILENO, &c, 1);
	if (pk->com.idx > 0 && (pk->com.idx + 1) % pk->screen_limit == 0)
		break_line(pk);
}

static void
push_memory(int *rsp, struct cpkg *pk)
{
	int 		len;
	char 		*ptr;
	assert(COMMAND_IDX_REACH(pk->com.idx));
	len = strlen(pk->com.command);
	assert(len <= MAX_CANON);
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

static void
push_command(int rbp, struct cpkg *pk)
{
	size_t 		copied;
	assert(rbp >= 0 && rbp <= HISTORY_MAX_SIZE);
	if (pk->history[rbp] == NULL) {
		pk->com.command[0] = '\0';
		pk->com.idx = 0;
		pk->curs = 0;
		return;
	}
	copied = strlcpy(pk->com.command, pk->history[rbp], MAX_CANON + 1);
	if (copied > MAX_CANON)
		copied = MAX_CANON;
	assert(COMMAND_IDX_REACH(pk->com.idx));
	pk->com.idx = copied;
	pk->curs = pk->com.idx;
}

void
set_cursor(int position, const struct cpkg *pk)
{
	char 		max_mov[16];
	int 		len;
	position %= pk->screen_limit;
	position += strlen(pk->ps);
	assert(COMMAND_IDX_REACH(position) && position != 0);
	len = snprintf(max_mov, sizeof(max_mov), "\r\033[%dC", position);
	write(STDOUT_FILENO, max_mov, len);
}

void
clear_comm(char c, struct cpkg *pk)
{
	const char *shell, *old_path; 
	struct termios raw;
	int slide, linep;
	char *command;
	size_t len;
	(void)c;
	pk->flag = SAVE;
	hcntl(pk);
	pk->flag = CLEAR;
	hcntl(pk);
	assert(COMMAND_IDX_REACH(pk->com.idx));
	linep = (pk->com.idx > 0) ? ((pk->com.idx - 1) / pk->screen_limit): 0;
	assert(COMMAND_IDX_REACH(pk->curs));
	slide = linep - (pk->curs / pk->screen_limit);
	if (slide) {
		char buf[32];
		len = snprintf(buf, sizeof(buf), "\033[%dB\n\r", slide);
		assert(sizeof(buf) >= len);
		write(STDOUT_FILENO, buf, len);
	} else {
		write(STDOUT_FILENO, "\n\r", 2);
	}
	if ((shell = getenv("SHELL")) == NULL)
		shell = "/bin/sh";
	old_path = getenv("OLDPWD");
	tcgetattr(STDIN_FILENO, &raw);
	tcsetattr(STDIN_FILENO, TCSANOW, &pk->base);
	command = check_exec_safety(shell, pk->com.command, old_path);
	if (command != NULL) {
		if (handle_exec(shell, command) == 0 || term_sig) {
			char msg[200];
			len = snprintf(msg, sizeof(msg), "\r%s", pk->ps);
			write(STDOUT_FILENO, msg, len);
			pk->curs = 0;
			pk->com.idx = 0;
			pk->com.command[0] = '\0';
		} else {
			if (!term_sig)
				burn_n_rebuild(pk, TERM_RB, 0);
		}
		free(command);
	}
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static void
break_line(const struct cpkg *pk)
{
	char 		buf[50];
	size_t		len;
	len = snprintf(buf, sizeof(buf), "\n\r>%-*s", (int)strlen(pk->ps) - 1, "");
	write(STDOUT_FILENO, buf, len);
}

void
burn_n_rebuild(struct cpkg *pk, enum rebuild state, int old_curs)
{
	char 		buffer[BURN_N_REBUILD_MAX];
	size_t 		len, bsize, copied;
	int 		y, iter, cl;
	cl = y = len = 0;
	bsize = sizeof(buffer);
	assert(COMMAND_IDX_REACH(pk->curs));
	assert(COMMAND_IDX_REACH(pk->com.idx));
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
	copied = strlcpy(&buffer[len], pk->ps, bsize - len);
	assert(copied <= 23);
	assert(copied + len <= bsize);
	len += copied;
	for (iter = 0; iter < pk->com.idx; ++iter) {
		if (len + copied + 4 > bsize) {
			write(STDOUT_FILENO, buffer, len);
			len = 0;
		}
		assert(len < bsize);
		if (iter % pk->screen_limit == 0 && iter > 0) {
			assert(len + copied + 4 <= bsize);
			int it;
			buffer[len++] = '\n';
			buffer[len++] = '\r';
			buffer[len++] = '>';
			for (it = 0; it < (int)(copied - 1); ++it)
				buffer[len++] = ' ';
		}
		buffer[len++] = pk->com.command[iter];
	}
	if (len + copied + 8 > bsize) {
		write(STDOUT_FILENO, buffer, len);
		len = 0;
	}
	if (cl > 0)
		len += snprintf(&buffer[len], bsize - len, "\033[%dA", cl);
	if (pk->com.idx > 0 && pk->curs % pk->screen_limit == 0 &&
	    pk->curs == pk->com.idx)
		len += snprintf(&buffer[len], bsize - len, "\n\r>%-*s",
				(int)(copied - 1), "");
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
	assert(COMMAND_IDX_REACH(pk->curs));
	sofw = pk->curs - sword.sidx;
	if (sofw > MAX_CANON)
		sofw = MAX_CANON;
	strlcpy(word, &pk->com.command[sword.sidx], sofw + 1);
	wsize = strlen(word);
	if (wsize > 0) {
		const char *src;
		char *new;
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
	case PDIRECT:
	case DOT:
		return;
	}
	burn_n_rebuild(pk, HISTORY, 0);
}

static void
shift_n_fill(struct cpkg *pk, int sofw, char *dummy, struct word_t sword)
{
	int		ntm, fwh;
	char		*dst, *src;
	size_t		size, len;
	fwh = 1;
	if (dummy == NULL || strcmp(dummy, "NOTF") == 0)
		return;
	assert(COMMAND_IDX_REACH(pk->curs));
	assert(COMMAND_IDX_REACH(pk->com.idx));
	if (pk->curs != pk->com.idx || pk->fill == CLOSE_FILL ||
		dummy[strlen(dummy) - 1] == '/') {
		fwh = 0;
		pk->fill = ADD_SPACE;
		assert(COMMAND_IDX_REACH(pk->com.idx));
	}
	if (pk->com.idx + (strlen(dummy) - sofw) + fwh > MAX_CANON) {
		free(dummy);
		dummy = NULL;
		return;
	}
	len = strlen(dummy);
	ntm = len - sofw + 1 *fwh;
	src = &pk->com.command[pk->curs];
	dst = &pk->com.command[pk->curs + ntm];
	size = pk->com.idx - pk->curs;
	assert(size + 1 <= MAX_CANON);
	memmove(dst, src, size + 1);
	memcpy(&pk->com.command[sword.sidx], dummy, len);
	if (fwh)
		pk->com.command[sword.sidx + len] = ' ';
	pk->curs = sword.sidx + len + 1 * fwh;
	pk->com.idx += ntm;
	free(dummy);
	dummy = NULL;
}

static char *
parse_dir(char *form)
{
	char		*ptr, *dir, path[PATH_MAX];
	static char	message[] = "SP"; /* abbriev special path */
	size_t		len, size;
	assert(form != NULL);
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

static void
set_screen_limit(struct cpkg *pk)
{
	struct winsize term_size;
	size_t ps_size, bsize, len;
	char buf[30];
	int nol;
	static uint8_t l_col = 0;
	static uint8_t l_screen_limit = 0;
	nol = 0;
	ps_size = strlen(pk->ps);
	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &term_size) == -1) {
		if (!l_col)
			l_col = OPENBSD_MAX;
		if (!l_screen_limit) {
			size_t r = ps_size % OPENBSD_MAX;
			pk->screen_limit = OPENBSD_MAX - r;
			l_screen_limit = pk->screen_limit;
		}
		term_size.ws_col = OPENBSD_MAX;
	} else {
		if (!l_col) {
			if (term_size.ws_col < OPENBSD_MAX)
				l_col = term_size.ws_col;
			else
				l_col = OPENBSD_MAX;
		}
		if (!l_screen_limit) {
			if (term_size.ws_col < OPENBSD_MAX) {
				size_t r = ps_size % term_size.ws_col;
				pk->screen_limit = term_size.ws_col - r;
				l_screen_limit = pk->screen_limit;
			} else {
				size_t r = ps_size % OPENBSD_MAX;
				pk->screen_limit = OPENBSD_MAX - r;
				l_screen_limit = pk->screen_limit;
			}
		}
	}
	assert(COMMAND_IDX_REACH(pk->com.idx));
	if (pk->com.idx == 0) {
		if (term_size.ws_col >= OPENBSD_MAX) {
			size_t r = ps_size % OPENBSD_MAX;
			pk->screen_limit = OPENBSD_MAX - r;
			l_col = OPENBSD_MAX;
		} else {
			size_t r = ps_size % term_size.ws_col;
			pk->screen_limit = term_size.ws_col - r;
			l_col = term_size.ws_col;
		}
		assert(SCREEN_COND);
		return;
	}
	if (term_size.ws_col >= OPENBSD_MAX && l_col >= OPENBSD_MAX) {
		pk->screen_limit = OPENBSD_MAX - (ps_size % OPENBSD_MAX);
		if (!l_screen_limit)
			l_screen_limit = pk->screen_limit;
		assert(SCREEN_COND);
		return;
	}
	if (term_size.ws_col >= OPENBSD_MAX) {
		pk->screen_limit = OPENBSD_MAX - (ps_size % OPENBSD_MAX);
	} else {
		size_t r = ps_size % term_size.ws_col;
		pk->screen_limit = term_size.ws_col - r;
	}
	if (term_size.ws_col < l_col) {
		int ri, i;
		uint8_t	mode1;
		assert(COMMAND_IDX_REACH(pk->curs));
		mode1 = ri = 0;
		for (i = 0; i <= pk->curs; ++i) {
			size_t 		cond1, cond2;
			cond1 = i % l_screen_limit;
			cond2 = ri % pk->screen_limit;
			if (ri > 0 && cond2 == 0 && !mode1) {
				++nol;
				mode1 = 1;
				ri = 0;
			}
			if (ri > 0 && cond1 == 0 && mode1) {
				++nol;
				mode1 = 0;
				ri = 0;
			}
			if (mode1 && ri > 0) {
				nol += ri % term_size.ws_col == 0? 1: 0;
			}
			ri++;
		}
		nol += (ps_size / l_col);
	} else {
		assert(COMMAND_IDX_REACH(pk->curs));
		nol = pk->curs / l_screen_limit;
		nol += ps_size / l_col;
	}
	bsize = sizeof(buf);
	if (nol > 0)
		len = snprintf(buf, bsize, "\033[%dA\r\033[J", nol);
	else
		len = snprintf(buf, bsize, "\r\033[J");
	write(STDOUT_FILENO, buf, len);
	l_screen_limit = pk->screen_limit;
	if (term_size.ws_col >= OPENBSD_MAX)
		l_col = OPENBSD_MAX;
	else
		l_col = term_size.ws_col;
	burn_n_rebuild(pk, TERM_RB, 0);
	assert(SCREEN_COND);
}

static void
handle_term_inter(int c)
{
	(void)c;
	term_sig = 1;
}

void
clear_screen(char c, struct cpkg *pk)
{
	(void)c;
	write(STDOUT_FILENO, "\033[2J\033[H", 7);
	burn_n_rebuild(pk, TERM_RB, 0);
}

static void
init_term(struct cpkg *pk)
{
	struct termios raw;
	tcgetattr(STDIN_FILENO, &pk->base);
	pk->curs = 0;
	pk->com.idx = 0;
	pk->com.max_hst_num = 0;
	pk->hst_num = 0;
	pk->com.command[0] = '\0';
	pk->rvs_search_hst_max_prt = 10;
	pk->flag = SAVE;
	pk->fill = ADD_SPACE;
	pk->word_search = NULL;
	pk->mode = 0;
	raw = pk->base; 
	cfmakeraw(&raw);
	/* Setting enter to FL becouse in raw mode
	 * it is defaulter as CR
	 */
	raw.c_iflag |= ICRNL;
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static void
init_sig(void)
{
	struct sigaction sa;
	sa.sa_handler = handle_term_inter;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGWINCH, &sa, NULL);
}

static char *
check_exec_safety(const char *shell, const char *command, const char *old_path)
{
	size_t bsize, len, csize, shell_size, ops;
	size_t ssize, old_path_size, fcsize;
	const char path_comm[] = "export OLDPWD=";
	const char get_env_comm[] = ";set >&3";
	char *buf;
	assert(command != NULL && strlen(command) <= MAX_CANON);
	csize = strlen(command);
	fcsize = csize + strlen(get_env_comm);

	if (old_path != NULL && (old_path_size = strlen(old_path)) > 0) {
		ssize = strlen(path_comm);
		ops = old_path_size + ssize;
	} else {
		ops = 0;
	}
	shell_size = strlen(shell);
	assert(csize <= MAX_CANON);
	/* Checking for size_t over-flow and limits of execve arguments */
	if (SIZE_MAX - fcsize - ops - 3 < shell_size)
		return (NULL);
	check_arg_max();
	assert(arg_max > 0);
	if (fcsize + ops + shell_size + 3 > (size_t)arg_max)
		return (NULL);
	bsize = fcsize + 2 + ops;
	if ((buf = malloc(bsize)) == NULL)
		return (NULL);
	if (ops > 0) {
		memcpy(buf, path_comm, ssize);
		memcpy(&buf[ssize], old_path, old_path_size);
		buf[ops++] = ';';
	}
	memcpy(&buf[ops], command, csize);
	len = ops + csize;
	strlcpy(&buf[len], get_env_comm, bsize - len);
	assert(strlen(buf) == bsize - 1 || strlen(buf) == fcsize);
	return (buf);
}

static void
check_arg_max(void)
{
	if ((arg_max = sysconf(_SC_ARG_MAX)) == -1)
		arg_max = _POSIX_ARG_MAX;
}
