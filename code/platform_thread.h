/*
 * platform_thread.h
 *
 * Camada fina sobre as threads nativas de cada SO.
 *   Linux/FreeBSD -> pthreads + semaforo POSIX (sem_t)
 *   Windows       -> Win32 (CreateThread, CRITICAL_SECTION,
 *                    CONDITION_VARIABLE, Semaphore)
 *
 * Os tipos ficam visiveis aqui (e nao opacos) para que possam ser
 * declarados na pilha ou dentro de outras structs sem malloc.
 *
 * Todas as funcoes que retornam int devolvem 0 em caso de sucesso.
 */
#ifndef PLATFORM_THREAD_H
#define PLATFORM_THREAD_H

#include <stdint.h>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>

  typedef struct {
      HANDLE h;
      void *(*fn)(void *);
      void *arg;
      void *ret;
  } pt_thread_t;

  typedef struct { CRITICAL_SECTION cs; } pt_mutex_t;
  typedef struct { CONDITION_VARIABLE cv; } pt_cond_t;
  typedef struct { HANDLE h; } pt_sem_t;
#else
  #include <pthread.h>
  #include <semaphore.h>

  typedef struct { pthread_t t; } pt_thread_t;
  typedef struct { pthread_mutex_t m; } pt_mutex_t;
  typedef struct { pthread_cond_t c; } pt_cond_t;
  typedef struct { sem_t s; } pt_sem_t;
#endif

/* threads */
int pt_thread_create(pt_thread_t *t, void *(*fn)(void *), void *arg);
int pt_thread_join(pt_thread_t *t, void **ret);

/* mutex */
int pt_mutex_init(pt_mutex_t *m);
int pt_mutex_destroy(pt_mutex_t *m);
int pt_mutex_lock(pt_mutex_t *m);
int pt_mutex_unlock(pt_mutex_t *m);

/* variavel de condicao */
int pt_cond_init(pt_cond_t *c);
int pt_cond_destroy(pt_cond_t *c);
int pt_cond_wait(pt_cond_t *c, pt_mutex_t *m);
int pt_cond_signal(pt_cond_t *c);
int pt_cond_broadcast(pt_cond_t *c);

/* semaforo contador (max so e usado no Windows) */
int pt_sem_init(pt_sem_t *s, unsigned initial, unsigned max);
int pt_sem_destroy(pt_sem_t *s);
int pt_sem_wait(pt_sem_t *s);
int pt_sem_post(pt_sem_t *s);

/* relogio monotonico em nanossegundos */
uint64_t pt_time_ns(void);

#endif /* PLATFORM_THREAD_H */
