#ifndef MY_TERM_H
#define MY_TERM_H

#include <termios.h>
#include <stdint.h>

struct i_command {
	int idx;
	char *command;
};

enum hflags {
	UP,
	DOWN,
	SAVE,
	CLEAR
};

struct cpkg {
	struct i_command com;
	struct termios base;
	char **history;
	int curs;
	enum hflags flag;
};

typedef struct word_t {
	uint8_t type;
	int sidx;
} word_t;

enum {
	INFST,
	INWORD,
	RFILE,
	DIRECT,
	QUOAT,
	SQUOAT,
	ESCQUOAT,
	FEXEC,
	PDIRECT,
	EXEC
};
#endif
