/*
 * TP1 SO - Questão 1, versão 1: Leitores e escritores SEM preferência de ordem.
 *
 * Solução clássica de Courtois, Heymans e Parnas (CACM, 1971), "problema 1":
 *   - v1_mutex protege o contador de leitores (v1_readcount);
 *   - v1_db dá acesso exclusivo aos dados: o PRIMEIRO leitor tranca (P) e o
 *     ÚLTIMO leitor libera (V); cada escritor tranca/libera por conta própria.
 * Vários leitores podem ler juntos; escritor é sempre exclusivo. Leitores que
 * chegam com outros leitores já lendo entram na frente de escritores esperando.
 */
#include "comum.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static sem_t v1_mutex, v1_db;
static int   v1_readcount = 0;

static void entrada_leitor(ThreadArgs *a) {
    P_log(&v1_mutex, a, "outro leitor está esperando o escritor ativo terminar");
    v1_readcount++;
    if (v1_readcount == 1)                       /* primeiro leitor tranca os escritores */
        P_log(&v1_db, a, "há escritor ativo na conta");
    V(&v1_mutex);
}

static void saida_leitor(void) {
    P(&v1_mutex);
    v1_readcount--;
    if (v1_readcount == 0) V(&v1_db);            /* último leitor libera os escritores */
    V(&v1_mutex);
}

static void entrada_escritor(ThreadArgs *a) {
    P_log(&v1_db, a, "há leitor(es) ou outro escritor ativo");
}

static void saida_escritor(void) {
    V(&v1_db);
}

static void *thread_leitor(void *arg) {
    ThreadArgs *a = arg;
    registrar("%s INICIOU", a->nome);
    for (int i = 1; i <= cfg.rodadas; i++) {
        dormir_ms(atraso_aleatorio(a, cfg.atraso_leitor));
        registrar("%s quer consultar o saldo (%d/%d)", a->nome, i, cfg.rodadas);
        entrada_leitor(a);
        secao_leitura(a);
        saida_leitor();
    }
    registrar("%s FINALIZADO", a->nome);
    return NULL;
}

static void *thread_escritor(void *arg) {
    ThreadArgs *a = arg;
    registrar("%s INICIOU", a->nome);
    for (int i = 1; i <= cfg.rodadas; i++) {
        dormir_ms(atraso_aleatorio(a, cfg.atraso_escritor));
        int  deposito = rand_r(&a->seed) % 2;
        long valor = 10 + (long)(rand_r(&a->seed) % (unsigned)(cfg.valor_max - 9));
        registrar("%s quer fazer %s de R$ %ld (%d/%d)", a->nome,
                  deposito ? "um depósito" : "um saque", valor, i, cfg.rodadas);
        entrada_escritor(a);
        secao_escrita(a, deposito, valor);
        saida_escritor();
    }
    registrar("%s FINALIZADO", a->nome);
    return NULL;
}

int main(int argc, char **argv) {
    if (config_ler(argc, argv) != 0) return 1;

    sem_init(&v1_mutex, 0, 1);
    sem_init(&v1_db, 0, 1);

    int r = executar("Versão 1 - leitores e escritores SEM preferência", thread_leitor, thread_escritor);

    sem_destroy(&v1_mutex);
    sem_destroy(&v1_db);
    return r;
}
