CC = gcc
CFLAGS = -Wall -Wextra

all: rejs harbourCaptain shipCaptain passenger

rejs: rejs.c utils.c
	$(CC) $(CFLAGS) -o rejs rejs.c utils.c

harbourCaptain: harbourCaptain.c utils.c
	$(CC) $(CFLAGS) -o harbourCaptain harbourCaptain.c utils.c

shipCaptain: shipCaptain.c utils.c
	$(CC) $(CFLAGS) -o shipCaptain shipCaptain.c utils.c

passenger: passenger.c utils.c
	$(CC) $(CFLAGS) -o passenger passenger.c utils.c

clean:
	rm -f rejs harbourCaptain shipCaptain passenger
