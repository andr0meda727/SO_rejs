CC = gcc
CFLAGS = -Wall -Wextra -pthread

all: rejs harbourCaptain shipCaptain

rejs: rejs.c utils.c passenger.c
	$(CC) $(CFLAGS) -o rejs rejs.c utils.c passenger.c

harbourCaptain: harbourCaptain.c utils.c
	$(CC) $(CFLAGS) -o harbourCaptain harbourCaptain.c utils.c

shipCaptain: shipCaptain.c utils.c
	$(CC) $(CFLAGS) -o shipCaptain shipCaptain.c utils.c

clean:
	rm -f rejs harbourCaptain shipCaptain
