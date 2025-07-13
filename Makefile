CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS = -lncurses
SRC = src/game_of_life.c
OUT = life

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(OUT)
