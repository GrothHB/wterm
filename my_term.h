#ifndef MY_TERM_H
#define MY_TERM_H

#include <termios.h>
#include <stdint.h>

#define NO_ACTION 0x0F

struct i_command {
	char 		*command;
	int 		idx;
};

enum hflags {
	UP,
	DOWN,
	SAVE,
	CLEAR
};

enum search_m {
	CLOSE_FILL,
	ADD_SPACE
};

enum vi_like_state {
	VIL_INSERT,
	VIL_EDIT,
};

struct cpkg {
	void			(**tptr)(char, struct cpkg *);
	char 			**history, **search_hist;
	struct termios 		base;
	int 			curs;
	struct i_command 	com;
	enum hflags 		flag;
	enum search_m		fill;
	enum vi_like_state	vil_mode;
	uint8_t			mode;
	uint8_t			rvs_action;
};

struct word_t {
	int 		sidx;
	uint8_t 	type;
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
	REVS_LEX_NORMAL = ' ',
	REVS_LEX_SEARCH = 's',
	REVS_LEX_TAKE = 't',
	REVS_LEX_DISPL = 'l'
};
#endif
