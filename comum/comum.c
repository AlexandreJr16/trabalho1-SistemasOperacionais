#include "comum.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Protege a saida e os contadores de instrumentacao. E RECURSIVO para que quem
// travou o log com travarLog ainda possa chamar registrar.
static pthread_mutex_t mutexLog;

static struct timespec inicio;

void iniciarLog(void) {
  pthread_mutexattr_t atributos;
  pthread_mutexattr_init(&atributos);
  pthread_mutexattr_settype(&atributos, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&mutexLog, &atributos);
  clock_gettime(CLOCK_MONOTONIC, &inicio);
}

static long msDesdeInicio(void) {
  struct timespec agora;
  clock_gettime(CLOCK_MONOTONIC, &agora);
  long segundos = (long)(agora.tv_sec - inicio.tv_sec);
  long nanossegundos = agora.tv_nsec - inicio.tv_nsec;
  return (segundos * 1000000000 + nanossegundos) / 1000000;
}

// O tempo e lido com o log ja travado, assim as linhas saem sempre em ordem de tempo.
void registrar(const char *formato, ...) {
  travarLog();
  printf("[%6ld ms] ", msDesdeInicio());
  va_list argumentos;
  va_start(argumentos, formato);
  vprintf(formato, argumentos);
  va_end(argumentos);
  printf("\n");
  fflush(stdout);
  destravarLog();
}

void travarLog(void) { pthread_mutex_lock(&mutexLog); }

void destravarLog(void) { pthread_mutex_unlock(&mutexLog); }

void dormirMs(long ms) {
  struct timespec duracao = {ms / 1000, ms % 1000 * 1000000};
  nanosleep(&duracao, NULL);
}

long atrasoAleatorio(Thread *thread, long base) {
  return base / 2 + rand_r(&thread->semente) % (base + 1);
}

void P(sem_t *semaforo) { sem_wait(semaforo); }

void V(sem_t *semaforo) { sem_post(semaforo); }

// Tenta sem esperar; so registra BLOQUEADO quando a thread vai mesmo dormir no semaforo.
void PLog(sem_t *semaforo, Thread *thread, const char *motivo) {
  if (sem_trywait(semaforo) == 0)
    return;
  registrar("%s BLOQUEADO: %s", thread->nome, motivo);
  P(semaforo);
  registrar("%s DESBLOQUEADO", thread->nome);
}
