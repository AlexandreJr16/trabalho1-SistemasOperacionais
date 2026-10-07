CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2 -pthread

all: v1

v1: v1_sem_preferencia.o comum.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c comum.h
	$(CC) $(CFLAGS) -c $<

# make run  -> executa com os valores padrão
run: v1
	./v1

clean:
	rm -f *.o v1

.PHONY: all run clean
