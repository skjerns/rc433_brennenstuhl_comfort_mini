CC = gcc
CFLAGS = -O2 -Wall
LDFLAGS = -lwiringPi -lpthread

all: record playback

record: record.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

playback: playback.c
	$(CC) $(CFLAGS) -o $@ $< -lwiringPi

clean:
	rm -f record playback

.PHONY: all clean
