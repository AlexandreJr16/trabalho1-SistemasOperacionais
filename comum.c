#include "comum.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

Conta  conta;
Config cfg = {
    .n_leitores = 4, .n_escritores = 2, .rodadas = 3,
    .atraso_leitor = 400, .atraso_escritor = 1000, .passo_ms = 100,
    .valor_max = 200, .saldo_inicial = 1000, .semente = 0
};

/* ------------------------- log e contadores globais -------------------------- */

static pthread_mutex_t log_mtx = PTHREAD_MUTEX_INITIALIZER;  /* protege saída e contadores abaixo */
static struct timespec t0;
static int ativos_l = 0, ativos_e = 0;       /* threads dentro da região crítica */
static int esperando[2] = {0, 0};            /* threads bloqueadas, por tipo     */
static int violacoes = 0;                    /* exclusão mútua violada           */
static int leituras_inconsistentes = 0;      /* leitor viu a invariante quebrada */

static long agora_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long)(t.tv_sec - t0.tv_sec) * 1000L + (t.tv_nsec - t0.tv_nsec) / 1000000L;
}

void dormir_ms(long ms) {
    if (ms <= 0) return;
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) { }
}

long atraso_aleatorio(ThreadArgs *a, long base) {
    return base / 2 + (long)(rand_r(&a->seed) % (unsigned)(base + 1));
}

/* emitir(): imprime com tempo. Chamar SOMENTE com log_mtx travado. */
__attribute__((format(printf, 1, 2)))
static void emitir(const char *fmt, ...) {
    va_list ap;
    printf("[%6ld ms] ", agora_ms());
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
}

void registrar(const char *fmt, ...) {
    char buf[300];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    pthread_mutex_lock(&log_mtx);
    emitir("%s", buf);
    pthread_mutex_unlock(&log_mtx);
}

/* ------------------------------- semáforos ----------------------------------- */

void P(sem_t *s) { while (sem_wait(s) == -1 && errno == EINTR) { } }
void V(sem_t *s) { sem_post(s); }

void P_log(sem_t *s, ThreadArgs *a, const char *motivo) {
    if (sem_trywait(s) == 0) return;             /* não bloqueou */
    pthread_mutex_lock(&log_mtx);
    esperando[a->tipo]++;
    emitir("%s BLOQUEADO: %s", a->nome, motivo);
    pthread_mutex_unlock(&log_mtx);

    P(s);

    pthread_mutex_lock(&log_mtx);
    esperando[a->tipo]--;
    emitir("%s DESBLOQUEADO", a->nome);
    pthread_mutex_unlock(&log_mtx);
}

/* --------------------------- região crítica ---------------------------------- */

static void entrar_cs(ThreadArgs *a) {
    pthread_mutex_lock(&log_mtx);
    int viol;
    if (a->tipo == ESCRITOR) { viol = (ativos_l > 0 || ativos_e > 0); ativos_e++; }
    else                     { viol = (ativos_e > 0);                 ativos_l++; }
    if (viol) violacoes++;
    emitir("%s ENTROU na região crítica   [dentro: %d leitor(es), %d escritor(es) | esperando: %d leitor(es), %d escritor(es)]%s",
           a->nome, ativos_l, ativos_e, esperando[LEITOR], esperando[ESCRITOR],
           viol ? "  <<< VIOLAÇÃO DE EXCLUSÃO MÚTUA" : "");
    pthread_mutex_unlock(&log_mtx);
}

static void sair_cs(ThreadArgs *a) {
    pthread_mutex_lock(&log_mtx);
    if (a->tipo == ESCRITOR) ativos_e--; else ativos_l--;
    emitir("%s SAIU da região crítica", a->nome);
    pthread_mutex_unlock(&log_mtx);
}

void secao_leitura(ThreadArgs *a) {
    entrar_cs(a);

    long s = conta.saldo;
    dormir_ms(cfg.passo_ms);                     /* leitura "demorada" */
    long d = conta.total_dep, q = conta.total_saq, n = conta.n_ops;
    int consistente = (s == cfg.saldo_inicial + d - q);

    pthread_mutex_lock(&log_mtx);
    if (!consistente) leituras_inconsistentes++;
    emitir("%s LEU: saldo=%ld | depósitos=%ld saques=%ld (ops=%ld)%s",
           a->nome, s, d, q, n,
           consistente ? "" : "  <<< DADO INCONSISTENTE (saldo != inicial + dep - saq)");
    pthread_mutex_unlock(&log_mtx);

    sair_cs(a);
}

void secao_escrita(ThreadArgs *a, int deposito, long valor) {
    long delta = deposito ? valor : -valor;

    entrar_cs(a);

    long visto = conta.saldo;
    registrar("%s vai fazer %s de R$ %ld (saldo visto: %ld)",
              a->nome, deposito ? "DEPÓSITO" : "SAQUE", valor, visto);

    if (!deposito && visto < valor) {
        registrar("%s SAQUE RECUSADO: saldo insuficiente", a->nome);
        sair_cs(a);
        return;
    }

    /* Atualização em etapas (ler, modificar, escrever, atualizar totais). */
    long tmp = conta.saldo;
    dormir_ms(cfg.passo_ms);
    conta.saldo = tmp + delta;
    dormir_ms(cfg.passo_ms);
    if (deposito) conta.total_dep += valor; else conta.total_saq += valor;
    conta.n_ops++;

    a->delta_aplicado += delta;
    a->ops_ok++;

    registrar("%s ATUALIZOU: %s de R$ %ld -> saldo agora = %ld",
              a->nome, deposito ? "depósito" : "saque", valor, conta.saldo);

    sair_cs(a);
}

/* ------------------------------ configuração --------------------------------- */

static long perguntar(const char *msg, long padrao) {
    char buf[64];
    printf("%s [%ld]: ", msg, padrao);
    fflush(stdout);
    if (!fgets(buf, sizeof buf, stdin)) return padrao;
    char *fim;
    long v = strtol(buf, &fim, 10);
    return (fim == buf) ? padrao : v;
}

static void uso(void) {
    fprintf(stderr,
        "Uso: ./programa [-l leitores] [-e escritores] [-r operações]\n"
        "                [-L atraso_leitor_ms] [-E atraso_escritor_ms] [-p passo_ms]\n"
        "                [-m valor_max] [-i saldo_inicial] [-s semente]\n"
        "Sem argumentos: pergunta os valores no teclado.\n");
}

int config_ler(int argc, char **argv) {
    cfg.semente = (unsigned)time(NULL);

    if (argc == 1) {
        cfg.n_leitores     = (int)perguntar("Quantidade de leitores", cfg.n_leitores);
        cfg.n_escritores   = (int)perguntar("Quantidade de escritores", cfg.n_escritores);
        cfg.rodadas        = (int)perguntar("Operações por thread", cfg.rodadas);
        cfg.atraso_leitor  = perguntar("Atraso médio do leitor (ms)", cfg.atraso_leitor);
        cfg.atraso_escritor = perguntar("Atraso médio do escritor (ms)", cfg.atraso_escritor);
        cfg.valor_max      = perguntar("Valor máximo de depósito/saque", cfg.valor_max);
        cfg.saldo_inicial  = perguntar("Saldo inicial", cfg.saldo_inicial);
    } else {
        int op;
        while ((op = getopt(argc, argv, "l:e:r:L:E:p:m:i:s:h")) != -1) {
            switch (op) {
            case 'l': cfg.n_leitores      = atoi(optarg); break;
            case 'e': cfg.n_escritores    = atoi(optarg); break;
            case 'r': cfg.rodadas         = atoi(optarg); break;
            case 'L': cfg.atraso_leitor   = atol(optarg); break;
            case 'E': cfg.atraso_escritor = atol(optarg); break;
            case 'p': cfg.passo_ms        = atol(optarg); break;
            case 'm': cfg.valor_max       = atol(optarg); break;
            case 'i': cfg.saldo_inicial   = atol(optarg); break;
            case 's': cfg.semente         = (unsigned)atol(optarg); break;
            default:  uso(); return 1;
            }
        }
    }

    if (cfg.n_leitores < 0 || cfg.n_escritores < 0 ||
        cfg.n_leitores + cfg.n_escritores == 0 ||
        cfg.rodadas < 1 || cfg.valor_max < 10) {
        uso();
        return 1;
    }
    return 0;
}

/* --------------------------- execução e resumo ------------------------------- */

int executar(const char *titulo, void *(*leitor)(void *), void *(*escritor)(void *)) {
    conta = (Conta){ cfg.saldo_inicial, 0, 0, 0 };
    clock_gettime(CLOCK_MONOTONIC, &t0);

    printf("=== Conta bancária compartilhada ===\n");
    printf("%s\n", titulo);
    printf("Leitores: %d | Escritores: %d | Operações por thread: %d\n",
           cfg.n_leitores, cfg.n_escritores, cfg.rodadas);
    printf("Atraso leitor: ~%ld ms | Atraso escritor: ~%ld ms | Passo na região crítica: %ld ms\n",
           cfg.atraso_leitor, cfg.atraso_escritor, cfg.passo_ms);
    printf("Saldo inicial: R$ %ld | Semente: %u\n\n", cfg.saldo_inicial, cfg.semente);

    int total = cfg.n_leitores + cfg.n_escritores;
    ThreadArgs *args = calloc((size_t)total, sizeof *args);
    pthread_t  *tids = calloc((size_t)total, sizeof *tids);
    if (!args || !tids) { perror("calloc"); return 1; }

    /* cria intercalando leitores e escritores */
    int k = 0;
    int maior = cfg.n_leitores > cfg.n_escritores ? cfg.n_leitores : cfg.n_escritores;
    for (int i = 0; i < maior; i++) {
        for (int t = 0; t < 2; t++) {
            int qtd = (t == LEITOR) ? cfg.n_leitores : cfg.n_escritores;
            if (i >= qtd) continue;
            ThreadArgs *a = &args[k];
            a->id   = i + 1;
            a->tipo = t;
            snprintf(a->nome, sizeof a->nome, "%s %d", t == LEITOR ? "Leitor" : "Escritor", a->id);
            a->seed = cfg.semente * 7919u + (unsigned)k * 104729u + 1u;
            registrar("Criando %s", a->nome);
            pthread_create(&tids[k], NULL, t == LEITOR ? leitor : escritor, a);
            k++;
        }
    }

    for (int i = 0; i < total; i++) pthread_join(tids[i], NULL);

    long esperado = cfg.saldo_inicial, ops_esperadas = 0;
    for (int i = 0; i < total; i++)
        if (args[i].tipo == ESCRITOR) {
            esperado      += args[i].delta_aplicado;
            ops_esperadas += args[i].ops_ok;
        }

    int ok = (conta.saldo == esperado && conta.n_ops == ops_esperadas &&
              violacoes == 0 && leituras_inconsistentes == 0);

    printf("\n=== RESUMO ===\n");
    printf("Saldo inicial ............................ R$ %ld\n", cfg.saldo_inicial);
    printf("Saldo final obtido ....................... R$ %ld\n", conta.saldo);
    printf("Saldo esperado (inicial + operações) ..... R$ %ld\n", esperado);
    printf("Escritas concluídas (contador / real) .... %ld / %ld\n", conta.n_ops, ops_esperadas);
    printf("Leituras com dado inconsistente .......... %d\n", leituras_inconsistentes);
    printf("Violações de exclusão mútua .............. %d\n", violacoes);
    printf("Resultado: %s\n", ok ? "CONSISTENTE" : "PROBLEMAS DE CONCORRÊNCIA DETECTADOS");

    free(args);
    free(tids);
    return ok ? 0 : 1;
}
