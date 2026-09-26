#ifndef MY_TERM_H
#define MY_TERM_H

#include <termios.h>
#include <stdint.h>

struct i_command {
	int 		idx;
	char 		*command;
};

enum hflags {
	UP,
	DOWN,
	SAVE,
	CLEAR
};

struct cpkg {
	void			(**tptr)(char, struct cpkg *);
	struct i_command 	com;
	struct termios 		base;
	char 			**history;
	int 			curs;
	enum hflags 		flag;
};

struct word_t {
	uint8_t 	type;
	int 		sidx;
};

enum {
	INFST,
	INWORD,
	DOT,
	DIRECT,
	FEXEC,
	PDIRECT,
	EXEC
};

enum lex_state {
	LEX_IN_QUOTE,
	LEX_NORMAL,
	LEX_IN_SQUOTE,
	LEX_ESCAPE
};

enum revs_state {
	REVS_SWORD,
	REVS_SSTRING,
	REVS_PUTC,
	REVS_LISTC,
	REVS_WORK
};

enum revs_lex {
	REVS_LEX_NORMAL,
	REVS_LEX_SEARCH,
	REVS_LEX_GET,
	REVS_LEX_DISPL,
	REVS_LEX_WRONG
};
#endif
