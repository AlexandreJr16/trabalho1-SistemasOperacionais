/*
 * comum.h - parte compartilhada do TP1 de SO (Questão 1: Leitores x Escritores)
 * Tema: conta bancária (consulta de saldo, depósito e saque).
 *
 * Aqui ficam: a conta (dado compartilhado), configuração, log com tempo,
 * wrappers de semáforo (P/V), as regiões críticas de leitura e escrita e o
 * resumo final. Cada versão (v1, v2, v3) só implementa o seu protocolo de
 * entrada/saída e as suas threads.
 */
#ifndef COMUM_H
#define COMUM_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <semaphore.h>

#define LEITOR   0
#define ESCRITOR 1

/* Dado compartilhado. Invariante: saldo == saldo_inicial + total_dep - total_saq.
 * Se um leitor enxergar isso quebrado, ele leu um dado "sujo" (meio atualizado). */
typedef struct {
    long saldo;
    long total_dep;   /* soma de todos os depósitos   */
    long total_saq;   /* soma de todos os saques      */
    long n_ops;       /* escritas concluídas          */
} Conta;

typedef struct {
    int  n_leitores;
    int  n_escritores;
    int  rodadas;           /* operações por thread                       */
    long atraso_leitor;     /* atraso médio entre operações do leitor, ms */
    long atraso_escritor;   /* idem para o escritor, ms                   */
    long passo_ms;          /* tempo gasto dentro da região crítica, ms   */
    long valor_max;         /* valor máximo de depósito/saque             */
    long saldo_inicial;
    unsigned int semente;
} Config;

typedef struct {
    int  id;
    int  tipo;               /* LEITOR ou ESCRITOR */
    char nome[24];
    unsigned int seed;       /* semente própria (rand_r) */
    long delta_aplicado;     /* soma do que ESTA thread aplicou na conta (dado privado) */
    long ops_ok;             /* escritas concluídas por esta thread (dado privado)     */
} ThreadArgs;

extern Conta  conta;
extern Config cfg;

/* configuração: sem argumentos pergunta no teclado; retorna 0 se ok */
int  config_ler(int argc, char **argv);

/* utilidades */
void dormir_ms(long ms);
long atraso_aleatorio(ThreadArgs *a, long base);   /* 50% a 150% da base */
void registrar(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* semáforos */
void P(sem_t *s);
void V(sem_t *s);
void P_log(sem_t *s, ThreadArgs *a, const char *motivo);  /* avisa se bloqueou */

/* região crítica (chamar DEPOIS de obter acesso pelo protocolo da versão) */
void secao_leitura(ThreadArgs *a);
void secao_escrita(ThreadArgs *a, int deposito, long valor);

/* cria as threads, espera todas e imprime o resumo. Retorna 0 se consistente. */
int  executar(const char *titulo, void *(*leitor)(void *), void *(*escritor)(void *));

#endif
