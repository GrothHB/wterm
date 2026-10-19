#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "my_term.h"
#include "sparse.h"

#define COMMAND_IDX_REACH(x) ((x) >= 0 && (x) <= MAX_CANON + 1)

struct word_t
parse(struct cpkg *pk)
{
	int 		i, wflag;
	enum lex_state	state, prev_state;
	struct word_t 	lw;
	lw.type = EXEC;
	lw.sidx = wflag = 0;
	state = LEX_NORMAL;
	assert(COMMAND_IDX_REACH(pk->curs));
	for (i = 0; i < pk->curs; ++i) {
		char c;
		c = pk->com.command[i];
		switch (state) {
		case LEX_NORMAL:
			switch (c) {
			case '>':
				lw.type = FEXEC;
				wflag = 0;
				break;
			case '&':
			case '|':
			case '<':
			case '(':
			case ';':
				wflag = 0;
				lw.type = EXEC;
				break;
			case '/': 
				lw.type = DIRECT;
				if (!wflag) {
					lw.sidx = i;
					wflag = 1;
				}
				break;
			case ' ':
				wflag = 0;
				if (lw.type != EXEC)
					lw.type = FEXEC;
				break;
			case '.':
				if (!wflag) {
					lw.type = DOT;
					wflag = 1;
					lw.sidx = i;
				}
				break;
			case '\"':
				state = LEX_IN_QUOTE;
				if (!wflag) {
					wflag = 1;
					lw.sidx = i;
				}
				break;
			case '\'':
				state = LEX_IN_SQUOTE;
				if (!wflag) {
					wflag = 1;
					lw.sidx = i;
				}
				break;
			case '~':
				if (!wflag) {
					lw.type = PDIRECT;
					lw.sidx = i;
					wflag = 1;
				}
				break;
			case '\\':
				if (!wflag) {
					prev_state = state;
					state = LEX_ESCAPE;
					lw.sidx = i;
					wflag = 1;
				}
				break;
			case '\n':
			case '\0':
				break;
			default:
				if (!wflag) {
					lw.sidx = i;
					wflag = 1;
					if (lw.type == FEXEC)
						lw.type = INWORD;
					else if (lw.type == EXEC)
						lw.type = INFST;
				}
				break;
			}
			break;
		case LEX_IN_QUOTE:
			if (c == '\\') {
				prev_state = state;
				state = LEX_ESCAPE;
			} else if (c == '\"') {
				state = LEX_NORMAL;
			}
			break;
		case LEX_IN_SQUOTE:
			if (c == '\'')
				state = LEX_NORMAL;
			break;
		case LEX_ESCAPE:
			state = prev_state;
			break;
		default:
			assert(0);
			break;
		}
	}
	if (wflag == 0)
		lw.sidx = i;
	return (lw);
}
