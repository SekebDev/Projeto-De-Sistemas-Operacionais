/*
 * platform_thread.c
 *
 * Implementacao da camada de threads. Cada funcao tem duas versoes,
 * escolhidas em tempo de compilacao por _WIN32.
 */
#include "platform_thread.h"

#ifdef _WIN32
/* ====================================================================
 * Windows (Win32 nativo, sem winpthreads)
 * ==================================================================== */

/* CreateThread espera DWORD WINAPI f(LPVOID); a nossa API usa
 * void *f(void *). O trampolim faz a ponte e guarda o retorno. */
static DWORD WINAPI trampolim(LPVOID p)
{
    pt_thread_t *t = (pt_thread_t *)p;
    t->ret = t->fn(t->arg);
    return 0;
}

int pt_thread_create(pt_thread_t *t, void *(*fn)(void *), void *arg)
{
    t->fn = fn;
    t->arg = arg;
    t->ret = NULL;
    t->h = CreateThread(NULL, 0, trampolim, t, 0, NULL);
    return t->h == NULL ? -1 : 0;
}

int pt_thread_join(pt_thread_t *t, void **ret)
{
    if (WaitForSingleObject(t->h, INFINITE) != WAIT_OBJECT_0)
        return -1;
    CloseHandle(t->h);
    t->h = NULL;
    if (ret) *ret = t->ret;
    return 0;
}

int pt_mutex_init(pt_mutex_t *m)    { InitializeCriticalSection(&m->cs); return 0; }
int pt_mutex_destroy(pt_mutex_t *m) { DeleteCriticalSection(&m->cs); return 0; }
int pt_mutex_lock(pt_mutex_t *m)    { EnterCriticalSection(&m->cs); return 0; }
int pt_mutex_unlock(pt_mutex_t *m)  { LeaveCriticalSection(&m->cs); return 0; }

int pt_cond_init(pt_cond_t *c)      { InitializeConditionVariable(&c->cv); return 0; }
int pt_cond_destroy(pt_cond_t *c)   { (void)c; return 0; } /* nao existe Delete no Win32 */
int pt_cond_signal(pt_cond_t *c)    { WakeConditionVariable(&c->cv); return 0; }
int pt_cond_broadcast(pt_cond_t *c) { WakeAllConditionVariable(&c->cv); return 0; }

int pt_cond_wait(pt_cond_t *c, pt_mutex_t *m)
{
    return SleepConditionVariableCS(&c->cv, &m->cs, INFINITE) ? 0 : -1;
}

int pt_sem_init(pt_sem_t *s, unsigned initial, unsigned max)
{
    s->h = CreateSemaphoreA(NULL, (LONG)initial, (LONG)max, NULL);
    return s->h == NULL ? -1 : 0;
}

int pt_sem_destroy(pt_sem_t *s)
{
    return CloseHandle(s->h) ? 0 : -1;
}

int pt_sem_wait(pt_sem_t *s)
{
    return WaitForSingleObject(s->h, INFINITE) == WAIT_OBJECT_0 ? 0 : -1;
}

int pt_sem_post(pt_sem_t *s)
{
    return ReleaseSemaphore(s->h, 1, NULL) ? 0 : -1;
}

uint64_t pt_time_ns(void)
{
    static LARGE_INTEGER freq; /* lida uma vez so */
    LARGE_INTEGER c;
    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c);
    /* separa parte inteira e resto para nao estourar 64 bits */
    return (uint64_t)(c.QuadPart / freq.QuadPart) * 1000000000ULL +
           (uint64_t)(c.QuadPart % freq.QuadPart) * 1000000000ULL / (uint64_t)freq.QuadPart;
}

#else
/* ====================================================================
 * POSIX (Linux / FreeBSD)
 * ==================================================================== */
#include <errno.h>
#include <time.h>

int pt_thread_create(pt_thread_t *t, void *(*fn)(void *), void *arg)
{
    return pthread_create(&t->t, NULL, fn, arg);
}

int pt_thread_join(pt_thread_t *t, void **ret)
{
    return pthread_join(t->t, ret);
}

int pt_mutex_init(pt_mutex_t *m)    { return pthread_mutex_init(&m->m, NULL); }
int pt_mutex_destroy(pt_mutex_t *m) { return pthread_mutex_destroy(&m->m); }
int pt_mutex_lock(pt_mutex_t *m)    { return pthread_mutex_lock(&m->m); }
int pt_mutex_unlock(pt_mutex_t *m)  { return pthread_mutex_unlock(&m->m); }

int pt_cond_init(pt_cond_t *c)      { return pthread_cond_init(&c->c, NULL); }
int pt_cond_destroy(pt_cond_t *c)   { return pthread_cond_destroy(&c->c); }
int pt_cond_wait(pt_cond_t *c, pt_mutex_t *m) { return pthread_cond_wait(&c->c, &m->m); }
int pt_cond_signal(pt_cond_t *c)    { return pthread_cond_signal(&c->c); }
int pt_cond_broadcast(pt_cond_t *c) { return pthread_cond_broadcast(&c->c); }

int pt_sem_init(pt_sem_t *s, unsigned initial, unsigned max)
{
    (void)max; /* sem_t nao tem limite configuravel */
    return sem_init(&s->s, 0, initial);
}

int pt_sem_destroy(pt_sem_t *s) { return sem_destroy(&s->s); }

int pt_sem_wait(pt_sem_t *s)
{
    /* sem_wait pode voltar antes da hora se chegar um sinal */
    while (sem_wait(&s->s) != 0) {
        if (errno != EINTR)
            return -1;
    }
    return 0;
}

int pt_sem_post(pt_sem_t *s) { return sem_post(&s->s); }

uint64_t pt_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#endif
