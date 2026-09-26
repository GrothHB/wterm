#ifndef KEYTABLE_H
#define KEYTABLE_H

#include "my_term.h"
#include "sterm.h"

#define BS '\010' /* backspace */
#define DEL '\177' 
#define HT '\011' 
#define LF '\012' /* new line */
#define CR '\015' 
#define ESC '\033'
#define SPACE '\040'

void 		find_fill(char, struct cpkg *);
void 		disp_stream(char, struct cpkg *);
void 		neutral(char, struct cpkg *);
void 		spec(char, struct cpkg *);
void 		check_break(char, struct cpkg *);
void 		clear_comm(char, struct cpkg *);
void 		cans(char, struct cpkg *);

void
init_ascii(void (*ascii[])(char, struct cpkg *))
{
	int 		i;
	for (i = 0; i < 3; ++i)
		ascii[i] = neutral;
	ascii[i++] = check_break;
	for (; i < BS; ++i)
		ascii[i] = neutral;
	ascii[i++] = disp_stream;
	ascii[i++] = find_fill;
	ascii[i++] = clear_comm;
	for (; i < CR; ++i)
		ascii[i] = neutral;
	ascii[i++] = disp_stream;
	for (; i < ESC; ++i)
		ascii[i] = neutral;
	ascii[i++] = spec;
	for (; i < SPACE; ++i)
		ascii[i] = neutral;
	for (; i < DEL; ++i)
		ascii[i] = disp_stream;
	ascii[i] = cans;
	
}
#endif
