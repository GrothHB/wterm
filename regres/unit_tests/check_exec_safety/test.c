#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "body.h"

int
main(void)
{
	char command[MAX_CANON + 1];
	char old_path[PATH_MAX + 1];
	char shell[4096 * 2] = { '\0' };
	char *result;

	if (fgets(command, sizeof(command), stdin) == NULL)
		return (0);
	fgets(old_path, sizeof(old_path), stdin);
	fgets(shell, sizeof(shell), stdin);

	command[strcspn(command, "\n")] = '\0';
	old_path[strcspn(old_path, "\n")] = '\0';
	shell[strcspn(shell, "\n")] = '\0';
	if (strlen(command) == 0) {
		command[0] = 'l';
		command[5] = '\0';
	}
	
	if (strlen(shell) == 0)
		result = check_exec_safety("/bin/zsh", command, old_path);
	else
		result = check_exec_safety(shell, command, old_path);

	if (result != NULL) {
		printf("%s", result);
		free(result);
	}
	return (0);
}
