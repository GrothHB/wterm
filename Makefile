CC = clang
CFLAGS_BASE = -Wall -Wextra -pedantic -g
SRCS = src/sterm.c src/tab_compl.c src/sparse.c src/compatb.c src/rvs_mode.c src/parse_env.c
OBJS = $(SRCS:.c=.o)
TARGET = wt

# Domyślny cel, gdy wpiszesz samo "make"
all: opt

# Profile kompilacji (rekursywne wywołanie gwarantuje działanie w GNU Make i BSD Make)
opt:
	$(MAKE) $(TARGET) CFLAGS="$(CFLAGS_BASE) -O3 -DNDEBUG"

optsize:
	$(MAKE) $(TARGET) CFLAGS="$(CFLAGS_BASE) -Os -DNDEBUG"

debug:
	$(MAKE) $(TARGET) CFLAGS="$(CFLAGS_BASE) -O0"

stest:
	$(MAKE) $(TARGET) \
	CFLAGS="$(CFLAGS_BASE) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fstrict-aliasing -ftrivial-auto-var-init=pattern"

# Główne reguły budowania
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

check:
	cppcheck --enable=all --inconclusive --std=c89 --suppress=missingIncludeSystem \
	--error-exitcode=1 --check-level=exhaustive -i src/vi_edit.c src/

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all opt optsize debug stest check clean
