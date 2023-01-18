CC = cc
LD = cc

AFLAGS += -std=c11
AFLAGS += -g
AFLAGS += -fsanitize=undefined
AFLAGS += -fsanitize=address   # cannot be used with thread
# AFLAGS += -fsanitize=thread  # cannot be used with address
# AFLAGS += -fsanitize=memory  # not supported on M1

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
