#include <sys/types.h>

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifndef ARG_MAX
#define ARG_MAX 131072 /* Zastępstwo POSIX ARG_MAX limitem jądra MAX_ARG_STRLEN */
#endif

char *
check_exec_safety(const char *shell, char *command, const char *old_path)
{
	size_t bsize, len, csize, shell_size, ops;
	size_t ssize, old_path_size, fcsize;
	char path_comm[] = "export OLDPWD=";
	char get_env_comm[] = ";set >&3";
	char *buf;
	assert(command != NULL && strlen(command) > 0);
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
	if (fcsize + ops + shell_size + 3 > ARG_MAX)
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
