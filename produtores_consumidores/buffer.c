#include "buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define TAMANHO_MAXIMO 20
#define MAXIMO_PRODUTORES 9
#define MAXIMO_ITENS 99
#define VAZIO 0

// Dado compartilhado: vetor circular. O produtor grava em posicaoEntrada e o consumidor tira de
// posicaoSaida; as duas so andam para frente e voltam ao inicio no fim do vetor.
typedef struct {
  int vetor[TAMANHO_MAXIMO];
  int posicaoEntrada;
  int posicaoSaida;
} Buffer;

static Buffer buffer;

Config config = {
  .produtores = 3,
  .consumidores = 2,
  .itens = 4,
  .tamanho = 5,
  .atrasoProdutor = 200,
  .atrasoConsumidor = 300,
};

// Instrumentacao para o resumo, protegida pelo log: quantas vezes cada item foi retirado
// (o indice e o valor do item) e quantas retiradas acharam a posicao vazia.
static int vezesRetirado[MAXIMO_PRODUTORES * 100 + MAXIMO_ITENS + 1];
static int retiradasVazias;

static void mostrarUso(void) {
  printf("Uso: [-p produtores (1 a %d)] [-c consumidores] [-n itens por produtor (1 a %d)]\n"
         "     [-t tamanho do buffer (1 a %d)] [-P atraso do produtor em ms]\n"
         "     [-C atraso do consumidor em ms] [-s semente]\n",
         MAXIMO_PRODUTORES, MAXIMO_ITENS, TAMANHO_MAXIMO);
}

int lerConfig(int argc, char **argv) {
  config.semente = (unsigned int)time(NULL);
  int opcao;
  while ((opcao = getopt(argc, argv, "p:c:n:t:P:C:s:")) != -1) {
    switch (opcao) {
    case 'p': config.produtores = atoi(optarg); break;
    case 'c': config.consumidores = atoi(optarg); break;
    case 'n': config.itens = atoi(optarg); break;
    case 't': config.tamanho = atoi(optarg); break;
    case 'P': config.atrasoProdutor = atol(optarg); break;
    case 'C': config.atrasoConsumidor = atol(optarg); break;
    case 's': config.semente = (unsigned int)atol(optarg); break;
    default: mostrarUso(); return 0;
    }
  }

  int valida = config.produtores >= 1 && config.produtores <= MAXIMO_PRODUTORES &&
               config.consumidores >= 1 && config.itens >= 1 && config.itens <= MAXIMO_ITENS &&
               config.tamanho >= 1 && config.tamanho <= TAMANHO_MAXIMO &&
               config.atrasoProdutor >= 0 && config.atrasoConsumidor >= 0;
  if (!valida)
    mostrarUso();
  return valida;
}

// O buffer do momento, com _ nas posicoes vazias: [ 101 203 _ _ _ ].
static void descreverBuffer(char *texto) {
  texto += sprintf(texto, "[");
  for (int posicao = 0; posicao < config.tamanho; posicao++) {
    int valor = buffer.vetor[posicao];
    if (valor == VAZIO)
      texto += sprintf(texto, " _");
    else
      texto += sprintf(texto, " %d", valor);
  }
  sprintf(texto, " ]");
}

int produzir(Thread *thread, int item) {
  int valor = thread->numero * 100 + item;
  registrar("%s produzindo o item %d", thread->nome, valor);
  dormirMs(atrasoAleatorio(thread, config.atrasoProdutor));
  return valor;
}

// Sem controle, dois produtores leem a mesma posicao e o segundo SOBRESCREVE o item do
// primeiro; com o buffer cheio, o produtor sobrescreve um item que ninguem consumiu.
void inserir(Thread *thread, int valor) {
  int posicao = buffer.posicaoEntrada;
  registrar("%s vai inserir %d na posicao %d", thread->nome, valor, posicao);
  dormirMs(atrasoAleatorio(thread, config.atrasoProdutor));
  int anterior = buffer.vetor[posicao];
  buffer.vetor[posicao] = valor;
  buffer.posicaoEntrada = (posicao + 1) % config.tamanho;

  char texto[100];
  descreverBuffer(texto);
  if (anterior == VAZIO) {
    registrar("%s inseriu %d na posicao %d | buffer %s", thread->nome, valor, posicao, texto);
    return;
  }
  registrar("%s inseriu %d na posicao %d | buffer %s  <<< SOBRESCREVEU o item %d, que se perdeu",
            thread->nome, valor, posicao, texto, anterior);
}

// Sem controle, dois consumidores leem a mesma posicao e o item sai DUPLICADO; com o buffer
// vazio, o consumidor pega uma posicao VAZIA.
int retirar(Thread *thread) {
  int posicao = buffer.posicaoSaida;
  registrar("%s vai retirar da posicao %d", thread->nome, posicao);
  dormirMs(atrasoAleatorio(thread, config.atrasoConsumidor));
  int valor = buffer.vetor[posicao];
  buffer.vetor[posicao] = VAZIO;
  buffer.posicaoSaida = (posicao + 1) % config.tamanho;

  char texto[100];
  descreverBuffer(texto);
  travarLog();
  if (valor == VAZIO) {
    retiradasVazias++;
    registrar("%s retirou da posicao %d | buffer %s  <<< a posicao estava VAZIA", thread->nome,
              posicao, texto);
  } else {
    vezesRetirado[valor]++;
    registrar("%s retirou %d da posicao %d | buffer %s%s", thread->nome, valor, posicao, texto,
              vezesRetirado[valor] > 1 ? "  <<< DUPLICADO: o item ja tinha sido retirado" : "");
  }
  destravarLog();
  return valor;
}

void consumir(Thread *thread, int valor) {
  if (valor == VAZIO)
    registrar("%s consumindo nada: a posicao estava vazia", thread->nome);
  else
    registrar("%s consumindo o item %d", thread->nome, valor);
  dormirMs(atrasoAleatorio(thread, config.atrasoConsumidor));
}

// Os consumidores dividem o total produzido; os primeiros levam a sobra da divisao.
int cotaConsumidor(Thread *thread) {
  int total = config.produtores * config.itens;
  int sobra = total % config.consumidores;
  return total / config.consumidores + (thread->numero <= sobra ? 1 : 0);
}

static void imprimirCabecalho(const char *titulo) {
  printf("=== Produtores x Consumidores, %s ===\n", titulo);
  printf("Configuracao: %d produtor(es) com %d item(ns) cada, %d consumidor(es), "
         "buffer de %d posicoes\n",
         config.produtores, config.itens, config.consumidores, config.tamanho);
  printf("Atrasos: produtor ~%ld ms, consumidor ~%ld ms | Semente: %u\n\n", config.atrasoProdutor,
         config.atrasoConsumidor, config.semente);
}

static void imprimirResumo(void) {
  int perdidos = 0;
  int duplicados = 0;
  for (int produtor = 1; produtor <= config.produtores; produtor++) {
    for (int item = 1; item <= config.itens; item++) {
      int vezes = vezesRetirado[produtor * 100 + item];
      if (vezes == 0)
        perdidos++;
      if (vezes > 1)
        duplicados++;
    }
  }

  int produzidos = config.produtores * config.itens;
  char texto[100];
  descreverBuffer(texto);
  printf("\n=== Resumo ===\n");
  printf("Itens produzidos: %d | retirados uma vez so: %d (esperado: todos)\n", produzidos,
         produzidos - perdidos - duplicados);
  printf("Itens perdidos (nunca retirados): %d\n", perdidos);
  printf("Itens duplicados (retirados mais de uma vez): %d\n", duplicados);
  printf("Retiradas de posicao vazia: %d\n", retiradasVazias);
  printf("Buffer no fim: %s\n", texto);
}

// A semente de cada thread e a da execucao mais algo unico dela, para os sorteios variarem.
static void criarThread(Thread *thread, int tipo, int numero, void *(*funcao)(void *)) {
  thread->tipo = tipo;
  thread->numero = numero;
  thread->semente = config.semente + (unsigned int)(tipo * 1000 + numero);
  snprintf(thread->nome, sizeof(thread->nome), "%s %d",
           tipo == PRODUTOR ? "Produtor" : "Consumidor", numero);
  registrar("%s criado", thread->nome);
  pthread_create(&thread->id, NULL, funcao, thread);
}

void executar(const char *titulo, void *(*produtor)(void *), void *(*consumidor)(void *)) {
  imprimirCabecalho(titulo);
  iniciarLog();
  int total = config.produtores + config.consumidores;
  Thread *threads = malloc(total * sizeof(Thread));
  for (int numero = 1; numero <= config.produtores; numero++)
    criarThread(&threads[numero - 1], PRODUTOR, numero, produtor);
  for (int numero = 1; numero <= config.consumidores; numero++)
    criarThread(&threads[config.produtores + numero - 1], CONSUMIDOR, numero, consumidor);

  for (int indice = 0; indice < total; indice++)
    pthread_join(threads[indice].id, NULL);
  free(threads);
  imprimirResumo();
}
