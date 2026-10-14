#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _ISOC99_SOURCE

#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "my_term.h"
#include "tab.h"

#define OPENBSD_MAX 80
#define PIECE_OF_MEM 2048

int
message(int nf)
{
	char buffer[50], c;
	int len = snprintf(buffer, sizeof(buffer), "\r\033[K"
			   "Found %d posibilites. Display [Y/n]", nf);
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
fcompare(const void *x, const void *y)
{
	return (strcmp(*(char **)x, *(char **)y));
}

char *
fill_command(char *word, char *path, enum search_m *fflag)
{
	assert(word != NULL && path != NULL && fflag != NULL);
	char 			sbuf[PIECE_OF_MEM];
	DIR* 			ptr;
	struct dirent 		*dir_list;
	struct stat 		btyp;
	struct winsize		ws;
	char 			**farray, **tmp, *t, *base, *fw;
	static char		ret[] = "NOTF";
	static char		retmulti[] = "MF";
	static char		not_disp[] = "TM";
	size_t 			c_size, c_index, i, bsize, len, strsize, chinl;
	size_t 			max_length, mflag, inter_idx, inter_size, last;
	int			fst;
	uint8_t			scrn_limit;
	bsize = sizeof(sbuf);
	c_index = max_length = mflag = inter_idx = 0;
	c_size = 1;
	scrn_limit = OPENBSD_MAX;
	strsize = strlen(word);
	farray = malloc(sizeof(char*));
	if (farray == NULL)
		return (NULL);
	if ((ptr = opendir(path)) == NULL) {
		free(farray);
		return (NULL);
	}
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col > 0 &&
	    ws.ws_col < OPENBSD_MAX)
		scrn_limit = ws.ws_col;
	while ((dir_list = readdir(ptr)) != NULL) {
		assert(dir_list->d_name != NULL);
		if (word[0] != '.')
			if (dir_list->d_name[0] == '.')
				continue;
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
		} else {
			if ((t = strdup(dir_list->d_name)) == NULL)
				goto clean;
			else 
				farray[c_index] = t;
		}
		c_index++;
	}

	qsort(farray, c_index, sizeof(char*), fcompare);
	if (c_index == 0) {
		free(farray);
		closedir(ptr);
		return (ret);
	}
	if (c_index == 1) {
		fw = farray[0];
		closedir(ptr);
		free(farray);
		return (fw);
	}
	base = &farray[0][strsize];
	last = c_index - 1;
	inter_size = strlen(farray[0]);
	for (inter_idx = 0; inter_idx < (max_length - strsize); ++inter_idx) {
		size_t check_size = strsize + inter_idx;
		assert(check_size <= strlen(farray[last]));
		if (farray[last][check_size] ^ *base)
			break;
		if (check_size <= inter_size)
			base++; 
	}
	if (inter_idx) {
		fw = strdup(farray[0]);
		if (fw == NULL) {
			inter_idx = 0;
			goto clean;
		}
		if (strlen(fw) + 1 > strsize + inter_idx)
			fw[strsize + inter_idx] = '\0';
		*fflag = CLOSE_FILL;
		goto clean;
	}
	if (c_index >= 50) {
		if (message(c_index)) {
			mflag = 1;
			goto clean;
		}
	}
	write(STDOUT_FILENO, "\n\r", 2);
	len = chinl = 0;
	if (max_length > scrn_limit)
		max_length = scrn_limit;
	for (i = 0; i < c_index; ++i) {
		size_t		condp, nr;
		size_t fwl = strlen(farray[i]);
		condp = (fwl > max_length)? fwl: max_length;
		if (condp + 3 + len > bsize) {
			write(STDOUT_FILENO, sbuf, len);
			len = 0;
			chinl = 0;
		}
		if (fwl > scrn_limit) {
			size_t chg = 0;
			if (chinl > 0) {
				len += snprintf(&sbuf[len], bsize - len,
						"\n\r");
				chinl = 0;
			}
			while (chg < fwl) {
				int chunk = ((fwl - chg) > scrn_limit)?
						scrn_limit: fwl - chg;
				if (len + chunk + 3 > bsize) {
					write(STDOUT_FILENO, sbuf, len);
					len = 0;
				}
				assert(len + chunk + 3 <= bsize);
				nr = snprintf(&sbuf[len], bsize - len, 
					      "%.*s\n\r", chunk,
					      &farray[i][chg]);
				chg += chunk;
				len += nr;
					
			}
			write(STDOUT_FILENO, sbuf, len);
			len = 0;
			chinl = 0;
			free(farray[i]);
			farray[i] = NULL;
			continue;
		}
		if (chinl + 2 + fwl > scrn_limit) {
			len += snprintf(&sbuf[len], bsize - len, "\n\r");
			write(STDOUT_FILENO, sbuf, len);
			len = 0;
			chinl = 0;
		}
		assert(len + max_length + 2 < bsize);
		nr = snprintf(&sbuf[len], bsize - len, "%-*s  ", 
				(int)max_length, farray[i]);
		chinl += nr;
		len += nr;
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
	if (inter_idx)
		return (fw);
	if (mflag)
		return (not_disp);
	return (NULL);
}
