#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "parse_env.h"

#define HASH_SIZE (1024 / 24 + 1)
#define MEMORY_CHUNK 4096

static char              *parse(const char *);
static struct shell_vars *lookup(const char *);
static struct shell_vars *save_var(const char *, const char *);
static uint32_t          hash(const char *);

struct shell_vars {
	struct shell_vars 	*next;
	char			*name;
	char			*defun;
};

static struct shell_vars 		*hash_table[HASH_SIZE];

extern char **environ;

void
clear_hash(void)
{
	struct shell_vars *ptr;
	int i;
	for (i = 0; i < HASH_SIZE; ++i) {
		ptr = hash_table[i];
		while (ptr != NULL) {
			void *np;
			free(ptr->name);
			free(ptr->defun);
			np = ptr->next;
			free(ptr);
			ptr = np;
		}
	}
}

static uint32_t
hash(const char *name)
{
	uint32_t hash_val;
	for (hash_val = 0; *name != '\0'; ++name)
		hash_val = *name + 31 * hash_val;
	return (hash_val % HASH_SIZE);
}

static struct shell_vars *
lookup(const char *name)
{
	struct shell_vars *p;
	for (p = hash_table[hash(name)]; p != NULL; p = p->next) {
		if (strcmp(name, p->name) == 0)
			return (p);
	}
	return (NULL);
}

static struct shell_vars *
save_var(const char *name, const char *defun)
{
	struct shell_vars *p;
	if ((p = lookup(name)) == NULL) {
		uint32_t hash_val;
		if ((p = malloc(sizeof(*p))) == NULL)
			return (NULL);
		if ((p->name = strdup(name)) == NULL)
			return (NULL);
		hash_val = hash(name);
		p->next = hash_table[hash_val];
		hash_table[hash_val] = p;
	} else {
		free(p->defun);
	}
	if ((p->defun = strdup(defun)) == NULL)
		return (NULL);
	return (p);
}

static char *
parse(const char *env)
{
	char buf[MEMORY_CHUNK + 1], *name, *ifs, *backup;
	const struct shell_vars *env_var;
	size_t bsize;
	int c, i, k;
	assert(env != NULL);
	i = k = 0;
	env_var = NULL;
	ifs = name = NULL;
	c = env[i++];
	while (c != EOF && c != '\0') {
		if (k == MEMORY_CHUNK - 1) {
			buf[k++] = c;
			break;
		}
		assert(k < MEMORY_CHUNK - 1);
		if (c == '=') {
			buf[k++] = '\0';
			ifs = &buf[k];
			name = &buf[0];
			c = env[i++];
			continue;
		}
		if (c != '\n') {
			buf[k++] = c;
			c = env[i++];
			continue;
		}
		buf[k] = '\0';
		if (ifs != NULL && *ifs != '\n' && *ifs != '\0') {
			if (ifs[0] == '\'') {
				size_t dist;
				if ((dist = strlen(ifs)) > 2) {
					dist -= 2;
				/*
				 * cutting the '\'' from ifs which is the value
				 * of shell variable.
				 */
					memmove(ifs, &ifs[1], dist);
					assert(dist < i - strlen(name) - 3);
				} else {
					dist = 0;
				}
				ifs[dist] = '\0';
			}
			env_var = save_var(name, ifs);
		}
		if (env_var != NULL) {
			setenv(name, ifs, 1);
			env_var = NULL;
		}
		ifs = NULL;
		name = NULL;
		k = 0;
		c = env[i++];
	}
	if (ifs != NULL)
		*ifs = '=';
	if (name == NULL)
		return (name);
	buf[k] = '\0';
	bsize = strlen(name) + 1;
	if (bsize == MEMORY_CHUNK + 1)
		return (NULL);
	if ((backup = malloc(bsize)) == NULL)
		return (NULL);
	strlcpy(backup, name, bsize);
	return (backup);
}

int
handle_exec(const char *shell, const char *command)
{	
	char buffer[MEMORY_CHUNK + 1], *prv_env_var, current_dir[PATH_MAX + 1];
	int fields[2], status, wstatus;
	struct shell_vars *tmp;
	pid_t winf, shell_id;
	size_t wsize;
	ssize_t n;
	char * const args[] = {
		(char *)shell,
		"-c",
		(char *)command,
		NULL
	};
	status = pipe(fields);
	if (status == -1) {
		printf("Pipe has failed\n");
		return (1);
	}
	switch (shell_id = fork()) {
	case -1:
		printf("The fork didn't work\n");
		break;
	case 0:
		close(fields[0]);
		dup2(fields[1], 3);
		close(fields[1]);
		execve(shell, args, environ);
		_exit(127);
		break;
	default:
		close(fields[1]);
		prv_env_var = NULL;

		while (1) {
			if (prv_env_var != NULL) {
				wsize = strlen(prv_env_var);
				strlcpy(buffer, prv_env_var,
					  MEMORY_CHUNK);
				free(prv_env_var);
				prv_env_var = NULL;
			} else {
				wsize = 0;
			}
			assert(wsize <= MEMORY_CHUNK);
			n = read(fields[0], &buffer[wsize],
				  MEMORY_CHUNK - wsize);
			if (n > 0) {
				assert(wsize + n <= MEMORY_CHUNK + 1);
				buffer[wsize + n] = '\0';
			} else {
				break;
			}
			prv_env_var = parse(buffer);
		}
		close(fields[0]);
		if (prv_env_var != NULL)
			free(prv_env_var);

		do
			winf = waitpid(shell_id, &wstatus, WUNTRACED);
		while (winf == -1 && errno == EINTR);
		if (winf >= 0 && WIFEXITED(wstatus)) {
			int ex = WEXITSTATUS(wstatus);
			if (ex == 127) {
				close(fields[0]);
				printf("wt: Something went wrong in fork\n");
				return (1);
			} 
		}

		if (n < 0)
			return (1);
		if ((tmp = lookup("PWD")) == NULL) {
			close(fields[0]);
			break;
		}
		if (getcwd(current_dir, PATH_MAX + 1) != NULL) {
			if (strcmp(current_dir, tmp->defun) != 0)
				chdir(tmp->defun);
		}
		close(fields[0]);

		break;
	}
	return (0);
}
