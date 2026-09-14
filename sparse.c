#include "my_term.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

word_t
parse(struct cpkg *pk)
{
	int i;
	word_t lw;
	char c;
	lw.type = EXEC;
	lw.sidx = 0;
	for (i = 0; i < pk->curs; ++i) {
		c = pk->com.command[i];
		switch (c) {
		case '>':
			lw.type = RFILE;
			break;
		case '&':
		case '|':
		case '<':
		case '(':
		case ';':
			lw.type = EXEC;
			break;
		case ' ':
			if (lw.type == INFST || lw.type == INWORD ||
			    lw.type == DIRECT) {
				lw.type = FEXEC;
				lw.sidx = i;
				break;
			}
			if (lw.type == UNKNOWN)
				lw.type = FEXEC;
			if (lw.type == QUOAT || lw.type == SQUOAT)
				break;
			lw.sidx = i;
			break;
		case '/': 
			if (lw.type == FEXEC)
				break;
			if (i == 0 || pk->com.command[i-1] == ' ')
				lw.sidx = i;
			lw.type = DIRECT;
			break;
		case '.':
			lw.type = DOT;
			lw.sidx = i;
			break;
		case '\"':
			if (lw.type == QUOAT)
				lw.type = RFILE;
			else if (lw.type == ESCQUOAT)
				lw.type = QUOAT;
			else
				lw.type = QUOAT;
			break;
		case '\'':
			if (lw.type == QUOAT)
				break;
			else if (lw.type == ESCQUOAT)
				lw.type = SQUOAT;
			else if (lw.type == SQUOAT)
				lw.type = RFILE;
			else
				lw.type = SQUOAT;
			break;
		case '~':
			lw.type = PDIRECT;
			lw.sidx = i;
			break;
		case '\\':
			lw.type = ESCQUOAT;
			break;
		case '\n':
		case '\0':
			break;
		default:
			if (lw.type == EXEC) {
				lw.sidx = i;
				lw.type = INFST;
				break;
			}
			if (lw.type == FEXEC) {
				lw.sidx = i;
				lw.type = INWORD;
				break;
			}
			if (lw.type == QUOAT || lw.type == SQUOAT)
				break;
			if (lw.type == PDIRECT)
				lw.type = UNKNOWN;
			if (lw.type == DOT)
				lw.type = INWORD;
			break;
		}
	}
	return (lw);
}
