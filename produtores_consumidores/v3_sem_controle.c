// Questao 2, versao 3: produtores e consumidores SEM controle de concorrencia
#include "buffer.h"

// Mesmas threads das outras versoes, sem protocolo de entrada e saida: o produtor grava com o
// buffer cheio, o consumidor retira com ele vazio e dois do mesmo tipo usam a mesma posicao.
static void *produtor(void *argumento) {
  Thread *thread = argumento;
  for (int item = 1; item <= config.itens; item++) {
    int valor = produzir(thread, item);
    inserir(thread, valor);
  }
  registrar("%s terminou", thread->nome);
  return NULL;
}

static void *consumidor(void *argumento) {
  Thread *thread = argumento;
  int cota = cotaConsumidor(thread);
  for (int retirada = 1; retirada <= cota; retirada++) {
    int valor = retirar(thread);
    consumir(thread, valor);
  }
  registrar("%s terminou", thread->nome);
  return NULL;
}

int main(int argc, char **argv) {
  if (!lerConfig(argc, argv))
    return 1;
  executar("versao 3: sem controle de concorrencia", produtor, consumidor);
  return 0;
}
