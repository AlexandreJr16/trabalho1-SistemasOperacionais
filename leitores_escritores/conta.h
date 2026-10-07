// Conta bancaria compartilhada da questao 1: configuracao, operacoes na conta e resumo
#ifndef CONTA_H
#define CONTA_H

#include "comum.h"

/*
- Tipos de thread:
    LEITOR: consulta o saldo
    ESCRITOR: deposita ou saca
*/
#define LEITOR 0
#define ESCRITOR 1

/*
- Configuracao:
    Config: quantas threads de cada tipo, quantas operacoes cada uma faz e os atrasos em ms
    config: a configuracao desta execucao
    lerConfig: le a configuracao dos argumentos; retorna 1 se ela for valida
*/
typedef struct {
  int leitores;
  int escritores;
  int operacoes;
  long atrasoLeitor;
  long atrasoEscritor;
  unsigned int semente;
} Config;
extern Config config;
int lerConfig(int argc, char **argv);

/*
- Funcoes:
    consultarSaldo: leitura em etapas (le o saldo, dorme, le o movimentado) que confere a conta
    sortearMovimento: sorteia um deposito (positivo) ou um saque (negativo) e anuncia no log
    movimentar: escrita em etapas (le o saldo, dorme, grava o saldo, dorme, grava o movimentado)
    executar: imprime a configuracao, cria as threads, espera todas e imprime o resumo
*/
void consultarSaldo(Thread *thread);
long sortearMovimento(Thread *thread);
void movimentar(Thread *thread, long valor);
void executar(const char *titulo, void *(*leitor)(void *), void *(*escritor)(void *));

#endif
