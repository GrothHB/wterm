#ifndef MY_TERM_H
#define MY_TERM_H

#include <termios.h>
#include <stddef.h>
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
	uint32_t	max_hst_num;
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

struct vi_edit_reg {
	char		*command;
	int		idx;
	int		curs;
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
	uint8_t			screen_limit;
	uint8_t			rvs_search_hst_max_prt;
};

enum lex_word_search {
	INFST,
	INWORD,
	DOT,
	DIRECT,
	FEXEC,
	PDIRECT,
	EXEC
};

struct word_t {
	int 			sidx;
	enum lex_word_search	type;
};

struct ve_snap {
	char		*command;
	int		curs, idx;
};

struct ve_regs {
	char		*last_taken;
	struct ve_snap	*snaps;
};

enum lex_state {
	LEX_IN_QUOTE,
	LEX_NORMAL,
	LEX_IN_SQUOTE,
	LEX_ESCAPE
};

enum lex_paging {
	PAG_HANDLING_SPACE,
	PAG_HANDLING_BACK,
	PAG_COMMANDS,
	PAG_SPEC_COMM,
	PAG_CLEAR
};

enum vreg_state {
	VREG_SAVE,
	VREG_SNAP_SCR,
	VREG_PUT,
	VREG_SET_ALL,
	VREG_UNDO,
	VREG_UN_UNDO,
	VREG_CLEAR_ALL
};
#ifndef _CFMAKERAW_DECLARED
#define _CFMAKERAW_DECLARED
struct termios;
extern void cfmakeraw(struct termios *termios_p);
#endif
#endif /* MY_TERM_H */
