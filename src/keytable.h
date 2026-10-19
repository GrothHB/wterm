#ifndef KEYTABLE_H
#define KEYTABLE_H

#include "my_term.h"
#include "sterm.h"
#include "rvs_mode.h"
#define FF 0x0C

void
init_ascii(void (*ascii[])(char, struct cpkg *))
{
	int 		i;
	for (i = 0; i < 128; ++i) {
		if (i >= 0x20 && i <= 0x7E)
			ascii[i] = disp_stream;
		else
			ascii[i] = neutral;
	}
	ascii[3] = check_break;
	ascii[HT] = find_fill;
	ascii[LF] = clear_comm;
	ascii[FF] = clear_screen;
	ascii[ESC] = spec;
	ascii[DEL] = cans;
	ascii[BS] = cans;
	ascii[DC2] = set_rvs_mode;
}
#endif
