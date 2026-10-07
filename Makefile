CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread -D_POSIX_C_SOURCE=200809L -Icomum

all:

clean:
	rm -rf bin

.PHONY: all clean
