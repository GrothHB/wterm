#include <sys/types.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>


static uint32_t mem_chaos = 0;
/* as malloc */
void *
rand_memory_assign(size_t n)
{
	++mem_chaos;
	if (mem_chaos % 67 == 0)
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
	if (mem_chaos % 71 == 0)
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


