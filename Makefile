CC = /Users/gonzo/homebrew/Cellar/llvm/15.0.7_1/bin/clang
LD = /Users/gonzo/homebrew/Cellar/llvm/15.0.7_1/bin/clang
# CC = cc
# LD = cc

AFLAGS += -std=c11
AFLAGS += -g
AFLAGS += -fsanitize=undefined
AFLAGS += -fsanitize=address   # cannot be used with thread
# AFLAGS += -fsanitize=thread  # cannot be used with address
# AFLAGS += -fsanitize=memory  # not supported on M1

CFLAGS += $(AFLAGS)
CFLAGS += -c
CFLAGS += -Wall -Wextra -Wshadow -Wpedantic
CFLAGS += -D_GNU_SOURCE -D_XOPEN_SOURCE -D_POSIX_C_SOURCE=200809L

LDFLAGS += $(AFLAGS)

all:	tom

C_SRC = \
	forest.c \
	grammar.c \
	log.c \
	main.c \
	parser.c \
	slice.c \
	symbol.c \
	symtab.c \
	util.c \

C_OBJ = $(C_SRC:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $^

tom:	$(C_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o
	rm -f tom

.PHONY: all clean
