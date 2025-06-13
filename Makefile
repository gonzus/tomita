.DEFAULT_GOAL := all

OS := $(shell uname -s)

NAME = tomita

CC = cc
LD = cc

AFLAGS += -std=c11
AFLAGS += -g

ifeq ($(OS),Darwin)
# Linux also has sanitizers, but valgrind is more valuable there.
AFLAGS += -fsanitize=undefined
AFLAGS += -fsanitize=address   # cannot be used with thread
# AFLAGS += -fsanitize=thread  # cannot be used with address
# AFLAGS += -fsanitize=memory  # not supported on M1
endif

TAP_DIR = ../libtap

CFLAGS += $(AFLAGS)
CFLAGS += -c
CFLAGS += -Wall -Wextra -Wshadow -Wpedantic
CFLAGS += -I.
CFLAGS += -I/usr/local/include
CFLAGS += -I$(TAP_DIR)
ifeq ($(OS),Linux)
CFLAGS += -D_GNU_SOURCE
CFLAGS += -D_XOPEN_SOURCE
endif

LDFLAGS += $(AFLAGS)
LDFLAGS += -L.
LDFLAGS += -L/usr/local/lib
LDFLAGS += -L$(TAP_DIR)

LIBRARY = lib$(NAME).a

C_SRC_LIB = \
	buffer.c \
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
	tomita.c \
	util.c \

C_OBJ_LIB = $(C_SRC_LIB:.c=.o)

$(LIBRARY): $(C_OBJ_LIB)
	ar -crs $@ $^

C_SRC_TEST = $(wildcard t/*.c)
C_OBJ_TEST = $(patsubst %.c, %.o, $(C_SRC_TEST))
C_EXE_TEST = $(patsubst %.c, %, $(C_SRC_TEST))

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $^

tom: main.o $(LIBRARY)  ## build main executable
	$(LD) $(LDFLAGS) -o $@ $^

$(C_EXE_TEST): %: %.o $(LIBRARY)
	$(CC) $(LDFLAGS) -o $@ $^ -ltap

tests: $(C_EXE_TEST) ## build all tests

test: tests ## run all tests
	@for t in $(C_EXE_TEST); do ./$$t; done

ifeq ($(OS),Linux)
# Linux has valgrind!
valgrind: tests ## run all tests under valgrind
	@for t in $(C_EXE_TEST); do valgrind --leak-check=full ./$$t; done
endif

all: tom  ## (re)build everything

clean:  ## clean everything
	rm -f *.o
	rm -fr tom tom.dSYM
	rm -f $(C_OBJ_TEST) $(C_EXE_TEST)

help: ## display this help
	@grep -E '^[ a-zA-Z_-]+:.*?## .*$$' /dev/null $(MAKEFILE_LIST) | sort | awk -F: '{ sub(/.*##/, "", $$3); printf("\033[36;1m%-30s\033[0m %s\n", $$2, $$3); }'

.PHONY: all clean help test tests
