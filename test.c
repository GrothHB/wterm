#include <sys/stat.h>
#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include "my_term.h"

/*#define _POSIX_C_SOURCE 200809L*/
/*#define _DEFAULT_SOURCE*/
#define _ISOC99_SOURCE
#define SEARCH_FAIL 0x01
#define OPENBSD_MAX 76

int
fcompare(const void* x, const void* y)
{
	return strcmp(*(char**)x, *(char**)y);
}

char *
get_looking_word(struct cpkg *pk)
{
	int lidx, size;
	char *ptr;
	for (lidx = pk->com.idx - 1; pk->com.command[lidx] != ' '; --lidx)
		if (lidx == 0)
			break;
	size = strlen(&pk->com.command[lidx]);
	if ((ptr = malloc(size + 1)) == NULL)
		return NULL;
	strlcpy(ptr, &pk->com.command[lidx], size + 1);
	return ptr;
}

char *
fill_command(char *word)
{
	DIR* ptr;
	struct dirent *dir_list;
	struct stat btyp;
	char **farray, **tmp;
	size_t c_size, c_index, i, bsize, len, strsize;
	int sflag, max_length, num_of_finded;
	char bufer[PATH_MAX + 1], *sciezka, teststr;
	char sbuf[OPENBSD_MAX + 32];
	bsize = sizeof(sbuf);
	c_index = sflag = max_length = num_of_finded = 0;
	c_size = 1;
	farray = malloc(sizeof(char*));
	if ((sciezka = getcwd(bufer, PATH_MAX)) == NULL)
		return NULL;

	if ((ptr = opendir(sciezka)) == NULL)
		return NULL;

	while ((dir_list = readdir(ptr)) != NULL) {
		if (dir_list->d_name[0] == '.')
			continue;
		strsize = strlen(word);
		if (strlen(dir_list->d_name) < strsize)
			continue;
		teststr = dir_list->d_name[strsize];
		dir_list->d_name[strsize] = '\0';
		if (strcmp(dir_list->d_name, word) != 0)
			continue;
		dir_list->d_name[strsize] = teststr;
		if (strsize > max_length)
			max_length = strsize;
		++num_of_finded;
		
		if (c_index == c_size) {
			size_t size = sizeof(char*);
			c_size *= 2;
			if ((tmp = reallocarray(farray, c_size, size)) == NULL) {
				sflag = SEARCH_FAIL;
				break;
			}
			else {
				farray = tmp;
			}
		}
		if (stat(dir_list->d_name, &btyp) == -1)
			continue;
		if (S_ISDIR(btyp.st_mode)) {
			char disp_name[NAME_MAX + 2];
			size_t size = sizeof(disp_name);
			snprintf(disp_name, size, "%s/", dir_list->d_name);
			farray[c_index] = strdup(disp_name);
		}
		else {
			farray[c_index] = strdup(dir_list->d_name);
		}
		c_index++;
	}

	if (sflag == SEARCH_FAIL)
		return NULL;
	qsort(farray, c_index, sizeof(char*), fcompare);
	if (c_index == 1)
		return farray[0];
	write(STDOUT_FILENO, "\n\r", 2);
	len = 0;
	for(i = 0; i < c_index; ++i) {
		size_t size = strlen(farray[i]);
		if (len + size + 1 > bsize)
			break;
		if (len + size + 1 > OPENBSD_MAX) {
			len += snprintf(&sbuf[len], bsize - len, "\n\r");
			write(STDOUT_FILENO, sbuf, len);
			len = 0;
		}
		len += snprintf(&sbuf[len], bsize - len, "%-*s  ", 
				max_length, farray[i]);
		free(farray[i]);
		farray[i] = NULL;
	}
	if (len > 0 && len + 2 < bsize) {
		len += snprintf(&sbuf[len], bsize - len, "\n\r");
		write(STDOUT_FILENO, sbuf, len);
	}
	free(farray);
	farray = NULL;
	free(word);
	word = NULL;
	closedir(ptr);
	return NULL;
}
