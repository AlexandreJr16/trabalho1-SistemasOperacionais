// Buffer circular compartilhado da questao 2: configuracao, operacoes no buffer e resumo
#ifndef BUFFER_H
#define BUFFER_H

#include "comum.h"

/*
- Tipos de thread:
    PRODUTOR: produz itens e poe no buffer
    CONSUMIDOR: tira itens do buffer e consome
*/
#define PRODUTOR 0
#define CONSUMIDOR 1

/*
- Configuracao:
    Config: quantas threads de cada tipo, itens por produtor, tamanho do buffer e atrasos em ms
    config: a configuracao desta execucao
    lerConfig: le a configuracao dos argumentos; retorna 1 se ela for valida
*/
typedef struct {
  int produtores;
  int consumidores;
  int itens;
  int tamanho;
  long atrasoProdutor;
  long atrasoConsumidor;
  unsigned int semente;
} Config;
extern Config config;
int lerConfig(int argc, char **argv);

/*
- Funcoes:
    produzir: anuncia e gasta o tempo de producao; o valor do item e produtor x 100 + item
    inserir: poe o item na posicao de entrada em etapas (le a posicao, dorme, grava, avanca)
    retirar: tira o item da posicao de saida em etapas (le o item, dorme, esvazia, avanca)
    consumir: anuncia e gasta o tempo de consumo
    cotaConsumidor: quantos itens o consumidor retira, para o total bater com o produzido
    executar: imprime a configuracao, cria as threads, espera todas e imprime o resumo
*/
int produzir(Thread *thread, int item);
void inserir(Thread *thread, int valor);
int retirar(Thread *thread);
void consumir(Thread *thread, int valor);
int cotaConsumidor(Thread *thread);
void executar(const char *titulo, void *(*produtor)(void *), void *(*consumidor)(void *));

#endif
