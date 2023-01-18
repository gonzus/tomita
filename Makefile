CC = cc
LD = cc

AFLAGS += -std=c11
AFLAGS += -g
AFLAGS += -fsanitize=address
# AFLAGS += -fsanitize=thread

CFLAGS += $(AFLAGS)
CFLAGS += -c
CFLAGS += -Wall -Wextra -Wshadow -Wpedantic

LDFLAGS += $(AFLAGS)

all:	tom

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $^

tom:	main.o tomita.o lex.o sym.o mem.o util.o
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o
	rm -f tom

.PHONY: all clean
