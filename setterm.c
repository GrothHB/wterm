#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

int
main(void)
{
	struct termios ancer, raw;
	char c, word_buffer[50];
	int x , i;
	const char ps[] = "$> ";
	x = i = 0;
	tcgetattr(STDIN_FILENO, &ancer);
	raw = ancer; 
	cfmakeraw(&raw);
	raw.c_iflag |= ICRNL;
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);
	write(STDOUT_FILENO, ps, 4);

	while(read(STDIN_FILENO, &c, 1)) {
		if(c == '\177') {
			if(i > 0) {
				write(STDIN_FILENO, "\b\033[X", 4);
				word_buffer[--i] = '\0';
			}
		}
		else
		if(c == '\n') {
			write(STDOUT_FILENO, "\r\n", 2);
			write(STDOUT_FILENO, ps, 4);
			if(i < 50) {
				word_buffer[i] = '\0';
				write(STDOUT_FILENO, word_buffer , 50);
			}
			for(i = 0; i< 50; ++i)
				word_buffer[i] = '\0';
			i = 0;
		}
		else
		if(c == ' ') {
			write(STDOUT_FILENO, &c, 1);
			for(i = 0; i< 50; ++i)
				word_buffer[i] = '\0';
			i = 0;
		}
		else
		if(c == '@') {
			word_buffer[i++] = c;
			write(STDOUT_FILENO, &c, 1);
			if(strstr(word_buffer,"!]STOP[!@") != NULL)
				break;
		}
		else
		if(c == '\t') {
			write(STDOUT_FILENO, &c, 1);
		}
		else {
			write(STDOUT_FILENO, &c, 1);
			word_buffer[i++] = c;
		}
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &ancer);
	putchar('\n');

	return 0;
}
