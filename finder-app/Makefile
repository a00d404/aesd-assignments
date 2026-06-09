CC=$(CROSS_COMPILE)gcc

all: writer

writer: writer.c
	$(CC) -Wall -Werror writer.c -o writer

clean:
	rm -f writer *.o
