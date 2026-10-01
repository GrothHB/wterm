#ifndef MY_TERM_H
#define MY_TERM_H

#include <stddef.h>
#include <termios.h>
#include <stdint.h>

#define NO_ACTION 0x0F
#define BS '\010' /* backspace */
#define DEL '\177' 
#define HT '\011' 
#define LF '\012' /* new line */
#define CR '\015' 
#define ESC '\033'
#define SPACE '\040'
#define DC2 0x12


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

struct cpkg {
	void			(**tptr)(char, struct cpkg *);
	char 			**history, **word_search, *ps;
	struct termios 		base;
	int 			curs;
	struct i_command 	com;
	enum hflags 		flag;
	enum search_m		fill;
	size_t			hst_num;
	uint8_t			mode;
	uint8_t			rvs_action;
	uint8_t			last_list_st;
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

#endif
