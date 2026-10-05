CFLAGS = -Wall -Wextra -pedantic -g -DNDEBUG
COMP = clang
SRCS = ./src/sterm.c ./src/tab_compl.c ./src/sparse.c ./src/compatb.c ./src/rvs_mode.c ./src/parse_env.c
OBJS = ${SRCS:.c=.o}

CFLAG_OP = 
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
optsize: wt
debug:
	@:
opt: wt
stest: wt
wt: $(OBJS)
	$(COMP) $(CFLAGS) $(OPT) $(DEB) $(CFLAG_OP) $(STEST) $(OSIZE) -o $@ $(OBJS)
.c.o:
	$(COMP) $(CFLAGS) $(OPT) $(DEB) $(CFLAG_OP) $(STEST) $(OSIZE) -c -o $@ $<
check:
	cppcheck --enable=all --inconclusive --std=c89 --suppress=missingIncludeSystem --error-exitcode=1 \
	--suppress=funcArgNamesDifferentUnnamed  --check-level=exhaustive -i src/vi_edit.c src/
clean:
	rm ./src/*.o
