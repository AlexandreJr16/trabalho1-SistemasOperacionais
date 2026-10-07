// Questao 1, versao 3: leitores e escritores SEM controle de concorrencia
#include "conta.h"

// Mesmas threads das outras versoes, sem protocolo de entrada e saida: nada impede dois
// escritores na conta ao mesmo tempo, nem um leitor no meio de uma escrita.
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
    movimentar(thread, valor);
  }
  registrar("%s terminou", thread->nome);
  return NULL;
}

int main(int argc, char **argv) {
  if (!lerConfig(argc, argv))
    return 1;
  executar("versao 3: sem controle de concorrencia", leitor, escritor);
  return 0;
}
