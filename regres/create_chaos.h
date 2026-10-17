#ifndef CREATE_CHAOS_H
#define CREATE_CHAOS_H

void 		*rand_memory_assign(size_t);
char 		*rand_duplicate_of_string(const char *);
char 		*rand_answer_for_path(char *, size_t);
ssize_t		afl_mock_write(int, const void *, size_t);

#endif /* CREATE_CAOS_H */
