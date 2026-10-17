#include <sys/types.h>

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

#define tcgetattr afl_tcgetattr
#define tcsetattr afl_tcsetattr
#define cfmakeraw afl_cfmakeraw
#define sigemptyset afl_sigemptyset
#define sigaction(sig, act, oact) afl_sigaction(sig, act, oact)

#include "../src/sterm.c"
