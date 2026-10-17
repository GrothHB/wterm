#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "my_term.h"
#include "openbsd.h"
#include "tab.h"
#ifdef __AFL_COMPILER
#include "create_chaos.h"
#define malloc rand_memory_assign
#define strdup rand_duplicate_of_string
#define getcwd rand_answer_for_path
#define write(fd, buf, count) afl_mock_write(fd, buf, count)
#endif

char **
exec_paths(int *npaths)
{
	char 		**stable, **fsd, *path, *pol;
	size_t 		npath, i, lidx, c_size, lpath;
	assert(npaths != NULL);
	size_t size = sizeof(char *);
	lidx = npath = 0;
	c_size = 16;
	if ((fsd = malloc(c_size * size)) == NULL)
		return (NULL);
	stable = fsd;
	if ((pol= getenv("PATH")) == NULL)
		goto precleanup;
	if ((path = strdup(pol)) == NULL)
		goto precleanup;
	lpath = strlen(path) + 1;
	for (i = 0; i < lpath; ++i) {
		if (npath == c_size - 1) {
			char **p = stable;
			assert(c_size < (SIZE_MAX / 2));
			c_size *= 2;
			if ((fsd = reallocarray(p, c_size, size)) == NULL)
				goto clean;
			stable = fsd;
		}
		if (path[i] == ':' || path[i] == '\0') {
			assert(i >= lidx);
			size_t size = i - lidx;
			assert(npath < c_size - 1);
			if ((stable[npath] = malloc(size + 1)) == NULL)
				return (NULL);
			strlcpy(stable[npath++], &path[lidx], size + 1);
			stable[npath] = NULL;
			lidx = i + 1;
		}
	}
	*npaths = npath;
	free(path);
	return (stable);
clean:
	for (i = 0; i < npath; ++i)
		free(stable[i]);
	free(path);
	free(stable);
	return (NULL);
precleanup:
	free(stable);
	return (NULL);
}
