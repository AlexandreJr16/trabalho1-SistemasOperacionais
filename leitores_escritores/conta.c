#include "conta.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define SALDO_INICIAL 1000

// Dado compartilhado. Invariante: saldo == SALDO_INICIAL + movimentado. Quem enxergar
// isso quebrado pegou a conta no meio de uma escrita: LEITURA SUJA.
typedef struct {
  long saldo;
  long movimentado;
} Conta;

static Conta conta = {SALDO_INICIAL, 0};

Config config = {
  .leitores = 3,
  .escritores = 2,
  .operacoes = 3,
  .atrasoLeitor = 100,
  .atrasoEscritor = 300,
};

// Instrumentacao para o resumo, protegida pelo log. dentro[tipo] e quantos de cada tipo estao
// na conta agora; totalAplicado e a soma do que os escritores aplicaram (o saldo esperado).
static int dentro[2];
static int escritoresJuntos;
static int leitorComEscritor;
static int leiturasSujas;
static long totalAplicado;

static void mostrarUso(void) {
  printf("Uso: [-l leitores] [-e escritores] [-n operacoes por thread]\n"
         "     [-L atraso do leitor em ms] [-E atraso do escritor em ms] [-s semente]\n");
}

int lerConfig(int argc, char **argv) {
  config.semente = (unsigned int)time(NULL);
  int opcao;
  while ((opcao = getopt(argc, argv, "l:e:n:L:E:s:")) != -1) {
    switch (opcao) {
    case 'l': config.leitores = atoi(optarg); break;
    case 'e': config.escritores = atoi(optarg); break;
    case 'n': config.operacoes = atoi(optarg); break;
    case 'L': config.atrasoLeitor = atol(optarg); break;
    case 'E': config.atrasoEscritor = atol(optarg); break;
    case 's': config.semente = (unsigned int)atol(optarg); break;
    default: mostrarUso(); return 0;
    }
  }

  int valida = config.leitores >= 0 && config.escritores >= 0 && config.operacoes >= 1 &&
               config.atrasoLeitor >= 0 && config.atrasoEscritor >= 0;
  if (!valida)
    mostrarUso();
  return valida;
}

// Marca na propria linha de entrada quem entrou junto com quem nao devia. Esses encontros
// sao proibidos na v2; na v1 so o de dois escritores e proibido.
static void registrarEntrada(Thread *thread) {
  travarLog();
  int outroTipo = thread->tipo == LEITOR ? ESCRITOR : LEITOR;
  int doisEscritores = thread->tipo == ESCRITOR && dentro[ESCRITOR] > 0;
  int leitorEEscritor = dentro[outroTipo] > 0;
  escritoresJuntos += doisEscritores;
  leitorComEscritor += leitorEEscritor;
  dentro[thread->tipo]++;
  registrar("%s ENTROU na conta (dentro: %d leitor(es), %d escritor(es))%s%s", thread->nome,
            dentro[LEITOR], dentro[ESCRITOR], doisEscritores ? "  <<< DOIS ESCRITORES" : "",
            leitorEEscritor ? "  <<< LEITOR COM ESCRITOR" : "");
  destravarLog();
}

static void registrarSaida(Thread *thread) {
  travarLog();
  dentro[thread->tipo]--;
  registrar("%s SAIU da conta", thread->nome);
  destravarLog();
}

// Le o saldo e o movimentado em momentos diferentes. Se um escritor mexer na conta no
// meio, os dois nao batem.
void consultarSaldo(Thread *thread) {
  registrarEntrada(thread);
  long saldo = conta.saldo;
  dormirMs(atrasoAleatorio(thread, config.atrasoLeitor));
  long movimentado = conta.movimentado;
  int suja = saldo != SALDO_INICIAL + movimentado;

  travarLog();
  leiturasSujas += suja;
  registrar("%s LEU saldo R$ %ld, movimentado R$ %+ld%s", thread->nome, saldo, movimentado,
            suja ? "  <<< LEITURA SUJA: saldo nao bate com o movimentado" : "");
  destravarLog();
  registrarSaida(thread);
}

// Deposito ou saque de R$ 10 a R$ 200. O saque nunca e recusado: o saldo pode ficar negativo.
long sortearMovimento(Thread *thread) {
  long valor = 10 + rand_r(&thread->semente) % 191;
  int deposito = rand_r(&thread->semente) % 2;
  registrar("%s quer %s R$ %ld", thread->nome, deposito ? "depositar" : "sacar", valor);
  return deposito ? valor : -valor;
}

// Sem exclusao, dois escritores leem o mesmo saldo e um movimento se PERDE. Entre gravar o
// saldo e gravar o movimentado, a conta fica inconsistente para quem estiver lendo.
void movimentar(Thread *thread, long valor) {
  registrarEntrada(thread);
  long saldoLido = conta.saldo;
  dormirMs(atrasoAleatorio(thread, config.atrasoEscritor));
  conta.saldo = saldoLido + valor;
  dormirMs(atrasoAleatorio(thread, config.atrasoEscritor));
  conta.movimentado += valor;

  travarLog();
  totalAplicado += valor;
  registrar("%s %s R$ %ld: saldo lido R$ %ld, gravado R$ %ld", thread->nome,
            valor > 0 ? "DEPOSITOU" : "SACOU", labs(valor), saldoLido, saldoLido + valor);
  destravarLog();
  registrarSaida(thread);
}

static void imprimirCabecalho(const char *titulo) {
  printf("=== Leitores x Escritores, %s ===\n", titulo);
  printf("Configuracao: %d leitor(es), %d escritor(es), %d operacao(oes) cada\n", config.leitores,
         config.escritores, config.operacoes);
  printf("Atrasos: leitor ~%ld ms, escritor ~%ld ms | Semente: %u\n", config.atrasoLeitor,
         config.atrasoEscritor, config.semente);
  printf("Saldo inicial: R$ %d\n\n", SALDO_INICIAL);
}

static void imprimirResumo(void) {
  long saldoEsperado = SALDO_INICIAL + totalAplicado;
  printf("\n=== Resumo ===\n");
  printf("Saldo final: R$ %ld | esperado: R$ %ld (inicial + o que os escritores aplicaram)%s\n",
         conta.saldo, saldoEsperado, conta.saldo != saldoEsperado ? "  <<< SALDO ERRADO" : "");
  printf("Escritor entrou com outro escritor na conta: %d vez(es)\n", escritoresJuntos);
  printf("Leitor e escritor na conta ao mesmo tempo: %d vez(es)\n", leitorComEscritor);
  printf("Leituras sujas: %d\n", leiturasSujas);
}

// A semente de cada thread e a da execucao mais algo unico dela, para os sorteios variarem.
static void criarThread(Thread *thread, int tipo, int numero, void *(*funcao)(void *)) {
  thread->tipo = tipo;
  thread->numero = numero;
  thread->semente = config.semente + (unsigned int)(tipo * 1000 + numero);
  snprintf(thread->nome, sizeof(thread->nome), "%s %d", tipo == LEITOR ? "Leitor" : "Escritor",
           numero);
  registrar("%s criado", thread->nome);
  pthread_create(&thread->id, NULL, funcao, thread);
}

void executar(const char *titulo, void *(*leitor)(void *), void *(*escritor)(void *)) {
  imprimirCabecalho(titulo);
  iniciarLog();
  int total = config.leitores + config.escritores;
  Thread *threads = malloc(total * sizeof(Thread));
  for (int numero = 1; numero <= config.leitores; numero++)
    criarThread(&threads[numero - 1], LEITOR, numero, leitor);
  for (int numero = 1; numero <= config.escritores; numero++)
    criarThread(&threads[config.leitores + numero - 1], ESCRITOR, numero, escritor);

  for (int indice = 0; indice < total; indice++)
    pthread_join(threads[indice].id, NULL);
  free(threads);
  imprimirResumo();
}
