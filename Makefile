CC = cc

CFLAGS += -std=c11
CFLAGS += -Wall -Wextra -Wshadow -Wpedantic
CFLAGS += -g

all:	tom

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

tom:	main.o lex.o sym.o mem.o util.o
	$(CC) -o $@ $^

clean:
	rm -f *.o
	rm -f tom

.PHONY: all clean
