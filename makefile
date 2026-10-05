CFLAGS = -Wall -Wextra -pedantic -g
COMP = gcc
SRCS = sterm.c test.c sparse.c compatb.c test2.c test3.c 
OBJS = ${SRCS:.c=.o}

CFLAG_OP = 
.if !empty(.TARGETS:Mansi)
CFLAG_OP = -ansi
.endif

.if !empty(.TARGETS:Mdebug)
DEB = -O0
.elif !empty(.TARGETS:Mopt)
OPT = -O3
.elif !empty(.TARGETS:Mstest)
STEST = -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
	-fstrict-aliasing
.elif !empty(.TARGETS:Moptsize)
OSIZE = -Os
.endif

.SUFFIXES:
.SUFFIXES: .c .o

.c:
	$(COMP) $(CFLAGS) $(CFLAG_OP) $(DEB) $(OPT) $(STEST) $(OSIZE) -o $@ $<
ansi: wt
optsize: wt
debug:
	@:
opt: wt
stest: wt
wt: $(OBJS)
	$(COMP) $(CFLAGS) $(OPT) $(DEB) $(CFLAG_OP) $(STEST) $(OSIZE) -o $@ $(OBJS)
.c.o:
	$(COMP) $(CFLAGS) $(OPT) $(DEB) $(CFLAG_OP) $(STEST) $(OSIZE) -c -o $@ $<

clean:
	rm *.o
