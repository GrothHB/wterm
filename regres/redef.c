#define _POSIX_C_SOURCE
#define _DEFAULT_SOURCE

#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <poll.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include "create_chaos.h"

#define malloc rand_memory_assign
#define strdup rand_duplicate_of_string
#define getcwd rand_answer_for_path
#define write afl_mock_write
#define getenv rand_env_assign

static int 
afl_tcgetattr(int fd, struct termios *p) 
{
	(void)fd;
	(void)*p;
	return (0);
}

static int
afl_tcsetattr(int fd, int a, const struct termios *p)
{
	(void)fd;
	(void)a;
	(void)*p;
	return (0);
}

static void
afl_cfmakeraw(struct termios *p)
{
	(void)*p;
}

static int
afl_sigemptyset(sigset_t *set)
{
	(void)*set;
	return (0);
}

static int
afl_sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
	(void)sig;
	(void)*act;
	(void)*oact;
	return (0);
}

static int
afl_ioctl(int fd, int op, ...)
{
	va_list ap;
	static uint8_t ct = 0;
	++ct;
	va_start(ap, op);
	if (op == TIOCGWINSZ) {
		if (ct < 20)
			(va_arg(ap, struct winsize *))->ws_col = 20;
		else if (ct > 80)
			(va_arg(ap, struct winsize *))->ws_col = 80;
		else
			(va_arg(ap, struct winsize *))->ws_col = ct;
	}
	va_end(ap);
	return (0);
}

int
afl_poll(struct pollfd *fds, nfds_t fd, int timeout)
{
	if (timeout == -1)
		timeout = 0;
	return (syscall(SYS_poll, fds, fd, timeout));
}

#define tcgetattr afl_tcgetattr
#define tcsetattr afl_tcsetattr
#define cfmakeraw afl_cfmakeraw
#define sigemptyset afl_sigemptyset
#define sigaction(sig, act, oact) afl_sigaction(sig, act, oact)
#define poll afl_poll
#define ioctl afl_ioctl

#include "../src/sterm.c"
