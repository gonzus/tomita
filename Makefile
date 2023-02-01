.DEFAULT_GOAL := all

NAME = tomita

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
CFLAGS += -I. -I/usr/local/include

LDFLAGS += $(AFLAGS)
LDFLAGS += -L. -L/usr/local/lib

LIBRARY = lib$(NAME).a

all: tom

C_SRC_LIB = \
	buffer.c \
	console.c \
	forest.c \
	grammar.c \
	log.c \
	memory.c \
	parser.c \
	slice.c \
	stb.c \
	symbol.c \
	symtab.c \
	timer.c \
	util.c \

LILBS =

C_OBJ_LIB = $(C_SRC_LIB:.c=.o)

$(LIBRARY): $(C_OBJ_LIB)
	ar -crs $@ $^

C_SRC_TEST = $(wildcard t/*.c)
C_OBJ_TEST = $(patsubst %.c, %.o, $(C_SRC_TEST))
C_EXE_TEST = $(patsubst %.c, %, $(C_SRC_TEST))

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $^

tom: main.c $(LIBRARY)  ## build main executable
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

$(C_EXE_TEST): %: %.o $(LIBRARY)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -ltap

tests: $(C_EXE_TEST) ## build all tests

test: tests ## run all tests
	@for t in $(C_EXE_TEST); do ./$$t; done

clean:  ## clean everything
	rm -f *.o
	rm -f tom
	rm -f $(C_OBJ_TEST) $(C_EXE_TEST)

help: ## display this help
	@grep -E '^[ a-zA-Z_-]+:.*?## .*$$' /dev/null $(MAKEFILE_LIST) | sort | awk -F: '{ sub(/.*##/, "", $$3); printf("\033[36;1m%-30s\033[0m %s\n", $$2, $$3); }'

.PHONY: all clean help
