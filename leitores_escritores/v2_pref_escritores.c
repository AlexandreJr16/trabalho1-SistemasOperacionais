// Questao 1, versao 2: escritores com PREFERENCIA sobre leitores
// Solucao de Courtois, Heymans e Parnas (1971), problema 2.
#include "conta.h"

// Quantos leitores estao lendo agora. Protegido por mutexLeitores.
static int leitoresAtivos;

// Quantos escritores estao esperando ou escrevendo. Protegido por mutexEscritores.
static int escritoresPendentes;

static sem_t mutexLeitores;
static sem_t mutexEscritores;

// So um leitor por vez espera na catraca; os outros esperam aqui. Assim o escritor que chega
// disputa a catraca com no maximo UM leitor.
static sem_t filaLeitores;

// Fechada pelo PRIMEIRO escritor que chega e reaberta pelo ULTIMO que sai: enquanto houver
// escritor esperando ou escrevendo, nenhum leitor novo entra.
static sem_t catracaLeitores;

// Escritor exclusivo na conta; os leitores ocupam como um grupo so.
static sem_t acessoConta;

// O PRIMEIRO leitor tranca a conta para os escritores; quem libera e o ULTIMO a sair.
static void entradaLeitor(Thread *thread) {
  PLog(&filaLeitores, thread, "outro leitor esperando na catraca");
  PLog(&catracaLeitores, thread, "escritor esperando ou escrevendo");
  P(&mutexLeitores);
  leitoresAtivos++;
  if (leitoresAtivos == 1)
    PLog(&acessoConta, thread, "escritor na conta");
  V(&mutexLeitores);
  V(&catracaLeitores);
  V(&filaLeitores);
}

static void saidaLeitor(void) {
  P(&mutexLeitores);
  leitoresAtivos--;
  if (leitoresAtivos == 0)
    V(&acessoConta);
  V(&mutexLeitores);
}

static void entradaEscritor(Thread *thread) {
  PLog(&mutexEscritores, thread, "outro escritor esperando a catraca");
  escritoresPendentes++;
  if (escritoresPendentes == 1)
    PLog(&catracaLeitores, thread, "leitor passando pela catraca");
  V(&mutexEscritores);
  PLog(&acessoConta, thread, "conta em uso");
}

static void saidaEscritor(void) {
  V(&acessoConta);
  P(&mutexEscritores);
  escritoresPendentes--;
  if (escritoresPendentes == 0)
    V(&catracaLeitores);
  V(&mutexEscritores);
}

static void *leitor(void *argumento) {
  Thread *thread = argumento;
  for (int operacao = 1; operacao <= config.operacoes; operacao++) {
    dormirMs(atrasoAleatorio(thread, config.atrasoLeitor));
    registrar("%s quer consultar o saldo", thread->nome);
    entradaLeitor(thread);
    consultarSaldo(thread);
    saidaLeitor();
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
  sem_init(&mutexLeitores, 0, 1);
  sem_init(&mutexEscritores, 0, 1);
  sem_init(&filaLeitores, 0, 1);
  sem_init(&catracaLeitores, 0, 1);
  sem_init(&acessoConta, 0, 1);
  executar("versao 2: escritores com preferencia", leitor, escritor);
  return 0;
}
