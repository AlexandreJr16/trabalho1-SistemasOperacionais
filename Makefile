CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread -D_POSIX_C_SOURCE=200809L -Icomum
COMUM = comum/comum.c comum/comum.h
CONTA = $(COMUM) leitores_escritores/conta.c leitores_escritores/conta.h
BUFFER = $(COMUM) produtores_consumidores/buffer.c produtores_consumidores/buffer.h
BINARIOS = bin/le_v1 bin/le_v2 bin/le_v3 bin/pc_v1 bin/pc_v2 bin/pc_v3
DEMO_LE = -l 3 -e 3 -n 4 -L 200 -E 300 -s 4
DEMO_PC = -p 3 -n 4 -t 3 -P 150 -C 200 -s 5
EXECUCOES = docs/execucoes

all: $(BINARIOS)

bin/le_v1: leitores_escritores/v1_sem_preferencia.c $(CONTA)
bin/le_v2: leitores_escritores/v2_pref_escritores.c $(CONTA)
bin/le_v3: leitores_escritores/v3_sem_controle.c $(CONTA)
bin/pc_v1: produtores_consumidores/v1_um_consumidor.c $(BUFFER)
bin/pc_v2: produtores_consumidores/v2_varios_consumidores.c $(BUFFER)
bin/pc_v3: produtores_consumidores/v3_sem_controle.c $(BUFFER)

$(BINARIOS):
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(filter %.c,$^)

# Recompila com o ThreadSanitizer, conta os avisos de corrida de cada programa e recompila normal.
tsan:
	@$(MAKE) -s -B CFLAGS="$(CFLAGS) -fsanitize=thread -g"
	@for programa in $(BINARIOS); do \
	  avisos=$$($$programa -s 1 2>&1 >/dev/null | grep -c 'WARNING: ThreadSanitizer'); \
	  echo "$$programa: $$avisos avisos"; \
	done
	@$(MAKE) -s -B

# Mesma entrada para as tres versoes de cada questao; os logs vao para o relatorio.
demo: all
	@mkdir -p $(EXECUCOES)
	bin/le_v1 $(DEMO_LE) > $(EXECUCOES)/le_v1.txt
	bin/le_v2 $(DEMO_LE) > $(EXECUCOES)/le_v2.txt
	bin/le_v3 $(DEMO_LE) > $(EXECUCOES)/le_v3.txt
	bin/pc_v1 $(DEMO_PC) -c 1 > $(EXECUCOES)/pc_v1.txt
	bin/pc_v2 $(DEMO_PC) -c 3 > $(EXECUCOES)/pc_v2.txt
	bin/pc_v3 $(DEMO_PC) -c 3 > $(EXECUCOES)/pc_v3.txt

clean:
	rm -rf bin

.PHONY: all tsan demo clean
