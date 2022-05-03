CC = gcc
CFLAGS = -g -Wshadow -std=gnu99

all: oss user_proc

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

oss: oss.o
	$(CC) $(CFLAGS) -o $@ $^

user_proc: user_proc.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	/bin/rm -f *.o