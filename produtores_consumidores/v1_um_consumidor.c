// Questao 2, versao 1: varios produtores e UM consumidor
#include "buffer.h"

#include <stdio.h>

// Quantas posicoes livres o buffer tem. Comeca no tamanho do buffer; o produtor dorme no zero.
static sem_t vagas;

// Quantos itens o buffer tem. Comeca em 0; o consumidor dorme no zero.
static sem_t itens;

// Uma thread por vez mexe no vetor e nas posicoes. Mesmo com um consumidor so, ele disputa o
// buffer com os produtores.
static sem_t mutexBuffer;

// Primeiro garante a vaga e SO DEPOIS pega o mutex: quem dorme esperando vaga nao pode
// segurar o buffer, senao o consumidor nunca entra para abrir a vaga.
static void entradaProdutor(Thread *thread) {
  PLog(&vagas, thread, "sem vaga, dormindo");
  PLog(&mutexBuffer, thread, "buffer em uso");
}

static void saidaProdutor(void) {
  V(&mutexBuffer);
  V(&itens);
}

static void entradaConsumidor(Thread *thread) {
  PLog(&itens, thread, "sem item, dormindo");
  PLog(&mutexBuffer, thread, "buffer em uso");
}

static void saidaConsumidor(void) {
  V(&mutexBuffer);
  V(&vagas);
}

static void *produtor(void *argumento) {
  Thread *thread = argumento;
  for (int item = 1; item <= config.itens; item++) {
    int valor = produzir(thread, item);
    entradaProdutor(thread);
    inserir(thread, valor);
    saidaProdutor();
  }
  registrar("%s terminou", thread->nome);
  return NULL;
}

static void *consumidor(void *argumento) {
  Thread *thread = argumento;
  int cota = cotaConsumidor(thread);
  for (int retirada = 1; retirada <= cota; retirada++) {
    entradaConsumidor(thread);
    int valor = retirar(thread);
    saidaConsumidor();
    consumir(thread, valor);
  }
  registrar("%s terminou", thread->nome);
  return NULL;
}

int main(int argc, char **argv) {
  config.consumidores = 1;
  if (!lerConfig(argc, argv))
    return 1;
  if (config.consumidores != 1) {
    printf("A versao 1 tem um consumidor so.\n");
    return 1;
  }
  sem_init(&vagas, 0, config.tamanho);
  sem_init(&itens, 0, 0);
  sem_init(&mutexBuffer, 0, 1);
  executar("versao 1: varios produtores e um consumidor", produtor, consumidor);
  return 0;
}
