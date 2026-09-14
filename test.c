#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _ISOC99_SOURCE

#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include "my_term.h"

#define OPENBSD_MAX 76

int
message(int nf)
{
	char buffer[50], c;
	int len = snprintf(buffer, sizeof(buffer), "\r\033[K"
			   "Finded %d posibilites. Display [Y/n]", nf);
	write(STDOUT_FILENO, buffer, len);
	while (read(STDIN_FILENO, &c, 1)) {
		if (c == 'Y')
			return (0);
		if (c == 'n')
			return (1);
	}
	return (1);
}

int
fcompare(const void* x, const void* y)
{
	return (strcmp(*(char**)x, *(char**)y));
}

char *
fill_command(char *word, char *path)
{
	DIR* 			ptr;
	struct dirent 		*dir_list;
	struct stat 		btyp;
	char 			**farray, **tmp, *t;
	static char		ret[] = "NOTFOUND";
	static char		retmulti[] = "MFOUND";
	size_t 			c_size, c_index, i, bsize, len, strsize;
	size_t 			max_length;
	int			fst;
	char 			sbuf[OPENBSD_MAX + 32];
	bsize = sizeof(sbuf);
	c_index = max_length = 0;
	c_size = 1;
	farray = malloc(sizeof(char*));

	if ((ptr = opendir(path)) == NULL) {
		free(farray);
		return (NULL);
	}

	while ((dir_list = readdir(ptr)) != NULL) {
		if (word[0] != '.')
			if (dir_list->d_name[0] == '.')
				continue;
		strsize = strlen(word);
		if (strlen(dir_list->d_name) < strsize && strsize > 0)
			continue;
		if (strsize > 0)
			if (strncmp(dir_list->d_name, word, strsize) != 0)
				continue;
		if (strlen(dir_list->d_name) > max_length)
			max_length = strlen(dir_list->d_name);
		
		if (c_index == c_size) {
			size_t size = sizeof(char*);
			c_size *= 2;
			if ((tmp = reallocarray(farray, c_size, size)) == NULL)
				goto clean;
			else
				farray = tmp;
		}
		fst = fstatat(dirfd(ptr), dir_list->d_name, &btyp, AT_SYMLINK_NOFOLLOW);
		if (fst == -1)
			continue;
		if (S_ISDIR(btyp.st_mode)) {
			char disp_name[NAME_MAX + 2];
			size_t size = sizeof(disp_name);
			snprintf(disp_name, size, "%s/", dir_list->d_name);
			if ((t = strdup(disp_name)) == NULL)
				goto clean;
			else
				farray[c_index] = t;
		}
		else {
			if ((t = strdup(dir_list->d_name)) == NULL)
				goto clean;
			else 
				farray[c_index] = t;
		}
		c_index++;
	}

	qsort(farray, c_index, sizeof(char*), fcompare);
	if (c_index == 1) {
		char *fw = farray[0];
		closedir(ptr);
		free(farray);
		return (fw);
	}
	if (c_index >= 50)
		if (message(c_index))
			goto clean;
	if (c_index == 0) {
		free(farray);
		closedir(ptr);
		return (ret);
	}
	write(STDOUT_FILENO, "\n\r", 2);
	len = 0;
	for (i = 0; i < c_index; ++i) {
		if (max_length + 1 + len > bsize) {
			write(STDOUT_FILENO, sbuf, len);
			len = 0;
		}
		if (max_length + 1 + len > OPENBSD_MAX) {
			len += snprintf(&sbuf[len], bsize - len, "\n\r");
			write(STDOUT_FILENO, sbuf, len);
			len = 0;
		}
		len += snprintf(&sbuf[len], bsize - len, "%-*s  ", 
				(int)max_length, farray[i]);
		free(farray[i]);
		farray[i] = NULL;
	}
	if (len > 0 && len + 2 < bsize) {
		len += snprintf(&sbuf[len], bsize - len, "\n\r");
		write(STDOUT_FILENO, sbuf, len);
	}
	free(farray);
	farray = NULL;
	closedir(ptr);
	return (retmulti);
clean:
	for (i = 0; i < c_index; ++i)
		free(farray[i]);
	closedir(ptr);
	free(farray);
	return (NULL);
}
