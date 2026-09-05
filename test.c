#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _ISOC99_SOURCE

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <err.h>
#include <string.h>
#include <unistd.h>

int
fcompare(const void* x, const void* y)
{
	return strcmp(*(char**)x, *(char**)y);
}

int
main(void)
{
	DIR* ptr;
	struct dirent *dir_list;
	char **farray, **tmp;
	size_t current_size, current_index, i;
	char bufer[PATH_MAX], *sciezka;
	struct stat btyp;
	char cur_dir[] = "/home/strucarch/ansi_c";
	current_index = 0;
	current_size = 1;
	farray = malloc(sizeof(char*));

	ptr = opendir(cur_dir);

	if(ptr == NULL)
		err(EXIT_FAILURE, NULL);

	while((dir_list = readdir(ptr)) != NULL) {
		if(dir_list->d_name[0] == '.')
			continue;
		char full_path[PATH_MAX];
		
		if(current_index == current_size) {
			current_size *= 2;
			if((tmp = reallocarray(farray, current_size, sizeof(char*))) == NULL) {
				err(EXIT_FAILURE, "reallocarray");
			} else farray = tmp;
		}
		snprintf(full_path, sizeof(full_path), "%s/%s", cur_dir, dir_list->d_name);
		 if(stat(full_path, &btyp) == -1) {
			warn("stat had failed: %s\n", full_path);
			continue;
		}
		if(S_ISDIR(btyp.st_mode)) {
			char extended_name[NAME_MAX + 2];
			snprintf(extended_name, sizeof(extended_name), "%s/", dir_list->d_name);
			farray[current_index] = strdup(extended_name);
		}
		else
			farray[current_index] = strdup(dir_list->d_name);
		current_index++;
	}

	qsort(farray, current_index, sizeof(char*), fcompare);
	for(i = 0; i < current_index; ++i) {
		printf("%s\n", farray[i]);
		free(farray[i]);
		farray[i] = NULL;
	}
	free(farray);
	farray = NULL;

	sciezka = getcwd(bufer, PATH_MAX);
	printf("My current location is: %s\n", sciezka);
	closedir(ptr);
	return 0;
}
