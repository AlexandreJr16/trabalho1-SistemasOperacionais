CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread -D_POSIX_C_SOURCE=200809L -Icomum
COMUM = comum/comum.c comum/comum.h
CONTA = $(COMUM) leitores_escritores/conta.c leitores_escritores/conta.h
BINARIOS = bin/le_v1 bin/le_v3

all: $(BINARIOS)

bin/le_v1: leitores_escritores/v1_sem_preferencia.c $(CONTA)
bin/le_v3: leitores_escritores/v3_sem_controle.c $(CONTA)

$(BINARIOS):
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

# Recompila com o ThreadSanitizer, conta os avisos de corrida de cada programa e recompila normal.
tsan:
	@$(MAKE) -s -B CFLAGS="$(CFLAGS) -fsanitize=thread -g"
	@for programa in $(BINARIOS); do \
	  echo "$$programa: $$($$programa -s 1 2>&1 >/dev/null | grep -c 'WARNING: ThreadSanitizer') avisos"; \
	done
	@$(MAKE) -s -B

clean:
	rm -rf bin

.PHONY: all tsan clean
