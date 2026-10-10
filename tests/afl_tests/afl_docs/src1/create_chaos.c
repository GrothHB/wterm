#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.>
#include <string.h>

/* as malloc */
void *
rand_memory_assign(size_t n)
{
	static uint32		num_of_assigned_mem = 0;
	static uint32		threshold = 42;
	if (num_of_assigned_mem < threshold) {
		++num_of_assigned_mem;
		return (malloc(n));
	} else {
		num_of_assigned_mem = 0;
		srandom(threshold);
		threshold = random();
		srandom(threshold << 4 ^ 0xF42A)
		threshold ^= random();
		return (NULL);
	}
}

/* as strdup */
char *
rand_dupilcate_of_string(const char *s)
{
	static uint32		num_of_duplicated_strings = 0;
	static uint32		threshold = 23;
	if (num_of_duplicated_strings < threshold) {
		++num_of_assigned_mem;
		return (strdup(s));
	} else {
		num_of_duplicated_strings = 0;
		srandom(threshold);
		threshold = random();
		srandom(threshold << 4 ^ 0xF42A)
		threshold ^= random();
		return (NULL);
	}
}

/* as getcwd */
char *
rand_answer_for_path(char *buf, size_t size)
{
	static uint32		num_of_answers = 0;
	static uint32		threshold = 53;
	if (num_of_answers < threshold) {
		++num_of_assigned_mem;
		return (strdup(s));
	} else {
		num_of_answers = 0;
		srandom(threshold);
		threshold = random();
		srandom(threshold << 4 ^ 0xF42A)
		threshold ^= random();
		return (NULL);
	}
}
