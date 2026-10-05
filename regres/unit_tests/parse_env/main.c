#define _POSIX_C_SOURCE
#define _DEFAULT_SOURCE

#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <poll.h>
#include <stdio.h>
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
#define execve my_execve
#define setenv my_setenv

#include "parse_env.c"

int
main(void)
{
	while (__AFL_LOOP(10000))
		handle_exec("/bin/sh", "dummy_command");

	clear_hash();
	return (0);
}
