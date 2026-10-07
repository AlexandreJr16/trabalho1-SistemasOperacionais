// Log com tempo, atrasos e semaforos usados pelas duas questoes
#ifndef COMUM_H
#define COMUM_H

#include <pthread.h>
#include <semaphore.h>

/*
- Tipos:
    Thread: o que cada thread precisa para aparecer no log e sortear os proprios valores
*/
typedef struct {
  pthread_t id;
  int tipo;
  int numero;
  char nome[16];
  unsigned int semente;
} Thread;

/*
- Log:
    iniciarLog: marca o instante zero; todo tempo do log conta a partir dele
    registrar: imprime uma linha com o tempo, inteira, sem misturar com a de outra thread
    travarLog: segura o log para atualizar um contador e registrar a linha dele de uma vez so
    destravarLog: solta o log
*/
void iniciarLog(void);
void registrar(const char *formato, ...);
void travarLog(void);
void destravarLog(void);

/*
- Atrasos:
    dormirMs: suspende a thread por alguns milissegundos
    atrasoAleatorio: sorteia entre metade e uma vez e meia a base, para variar as velocidades
*/
void dormirMs(long ms);
long atrasoAleatorio(Thread *thread, long base);

/*
- Semaforos:
    P: espera no semaforo
    V: libera o semaforo
    PLog: igual ao P, mas avisa no log quando a thread fica bloqueada e quando e liberada
*/
void P(sem_t *semaforo);
void V(sem_t *semaforo);
void PLog(sem_t *semaforo, Thread *thread, const char *motivo);

#endif
