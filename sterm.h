#ifndef STERM_H
#define STERM_H

enum rebuild {
	INLINE_ADD,
	INLINE_CANS,
	HISTORY
}; 

enum cursmov{
	M_LEFT,
	M_RIGHT,
	QR,
	QL
};

enum sid{
	ID_RIGHT,
	ID_LEFT
};

void 		disp_stream(char, struct cpkg *);
void 		neutral(char, struct cpkg *);
void 		cans(char, struct cpkg *);
void 		spec(char, struct cpkg *);
void 		hcntl(struct cpkg *);
void 		check_break(char, struct cpkg *);
void 		clear_comm(char, struct cpkg *);
void 		clear_page(struct cpkg *);
void 		clear_mem(struct cpkg *);
void 		parse_esc_com(char c, struct cpkg *);
void 		assigne(char, struct cpkg *);
void 		id_shift(enum sid, struct cpkg *);
void 		cursor_cntl(enum cursmov, struct cpkg *);
void 		push_memory(int *, struct cpkg *);
void 		push_command(int, struct cpkg *);
void 		set_cursor(int);
void 		break_line(void);
void 		burn_n_rebuild(struct cpkg *, enum rebuild, int);
void 		find_fill(char, struct cpkg *);
void		shift_n_fill(struct cpkg *, int , char *, struct word_t);
char		*parse_dir(char *);
void		init_ascii(void (**)(char, struct cpkg *));

#endif /* STERM_H */
