#include <sys/types.h>

#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define CHUNK_MEM 4096

static uint32_t mem_chaos = 0;
/* as malloc */
void *
rand_memory_assign(size_t n)
{
	++mem_chaos;
	if (mem_chaos % 31 == 0)
		return (NULL);
	return (malloc(n));
}

/* as strdup */
char *
rand_duplicate_of_string(const char *s)
{
	++mem_chaos;
	if (mem_chaos % 111 == 0)
		return (NULL);
	return (strdup(s));
}

/* as getcwd */
char *
rand_answer_for_path(char *buf, size_t size)
{
	++mem_chaos;
	if (mem_chaos % 13 == 0)
		return (NULL);
	return (getcwd(buf, size));
}

ssize_t
afl_mock_write(int fd, const void *buf, size_t count)
{
	(void)fd;
	if (count > 0) {
		volatile char		check;
		const volatile char *p = (const volatile char *)buf;
		check = p[0];
		check = p[count - 1];
		(void)check;
	}
	return count;
}

char *
rand_env_assign(const char *name)
{
	++mem_chaos;
	if (mem_chaos % 27 == 0)
		return (NULL);
	if (strcmp(name, "PATH") == 0) {
		switch (mem_chaos % 4) {
		case 0:
			return (":/bin::/usr/bin:");
		case 1:
			return ("/bin:/mbin:2:3:4:5:inna_jakas:/weird:4:/6"
				"2:/ansi_c/logic:2:5:/another:1:5:12:512:"
				"/::///:12:53:");
		case 2:
			return ("::::::::::::");
		case 3:
			return ("/usr/local/bin");
		}
	}
	if (strcmp(name, "PS1") == 0) {
		switch (mem_chaos % 3) {
		case 0:
			return ("[category_theory~file/usr~git_repository: "
				"new_branch$]");
		case 1:
			return ("12345678901234567we2");
		case 2:
			return ("\033[31m\033[Aok");
		}
	}
	return (getenv(name));
}

static int
afl_valid_data(const char *buf, ssize_t length)
{
	ssize_t i;
	if (length < 5)
		return (1);
	
	for (i = 0; i < length && buf[i] != '='; ++i) {
		if (!isalnum(buf[i]) && buf[i] != '_')
			return (1);
	}
	if (i >= length - 2)
		return (1);
	if (buf[i + 1] != '\'')
		return (1);
	if (buf[length - 2] != '\'' || buf[length - 1] != '\n')
		return (1);
	return (0);
}

void
my_execve(const char *path, char * const *args, char * const *envp)
{
	char buffer[CHUNK_MEM + 1];
	ssize_t n;
	(void)path;
	(void)args;
	(void)envp;
	while ((n = read(STDIN_FILENO, buffer, CHUNK_MEM)) > 0) {
		buffer[n] = '\0';
		if (afl_valid_data(buffer, n))
			_exit(0);
		
		write(3, buffer, n);
	}
	_exit(0);
}

void
my_setenv(const char *name, const char *val, int overwrite)
{
	(void)name;
	(void)val;
	(void)overwrite;
}
