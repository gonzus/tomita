CC = cc

all:	tom

%.o: %.c
	$(CC) -c -o $@ $<

tom:	tom.o
	$(CC) -o $@ $<

clean:
	rm -f *.o
	rm -f tom

.PHONY: all clean
