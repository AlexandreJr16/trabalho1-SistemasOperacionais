// Questao 1, versao 1: leitores e escritores SEM preferencia de ordem de acesso
#include "conta.h"

// So os escritores se excluem entre si. O leitor NAO espera ninguem: pode ler no meio de uma
// escrita, e e isso que permite a LEITURA SUJA.
static sem_t acessoEscrita;

static void entradaEscritor(Thread *thread) {
  PLog(&acessoEscrita, thread, "outro escritor na conta");
}

static void saidaEscritor(void) { V(&acessoEscrita); }

// O leitor nao tem protocolo: entra direto na conta.
static void *leitor(void *argumento) {
  Thread *thread = argumento;
  for (int operacao = 1; operacao <= config.operacoes; operacao++) {
    dormirMs(atrasoAleatorio(thread, config.atrasoLeitor));
    registrar("%s quer consultar o saldo", thread->nome);
    consultarSaldo(thread);
  }
  registrar("%s terminou", thread->nome);
  return NULL;
}

static void *escritor(void *argumento) {
  Thread *thread = argumento;
  for (int operacao = 1; operacao <= config.operacoes; operacao++) {
    dormirMs(atrasoAleatorio(thread, config.atrasoEscritor));
    long valor = sortearMovimento(thread);
    entradaEscritor(thread);
    movimentar(thread, valor);
    saidaEscritor();
  }
  registrar("%s terminou", thread->nome);
  return NULL;
}

int main(int argc, char **argv) {
  if (!lerConfig(argc, argv))
    return 1;
  sem_init(&acessoEscrita, 0, 1);
  executar("versao 1: sem preferencia de ordem de acesso", leitor, escritor);
  return 0;
}
