CC ?= gcc

all: writer

writer: writer.c
	$(CC) -o writer writer.c

clean:
	rm -f writer

.PHONY: all clean
