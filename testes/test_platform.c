/*
 * test_platform.c - testa cada operacao da camada platform_thread.
 *
 * Sai com codigo 0 se tudo passou, 1 caso contrario.
 */
#include <stdio.h>
#include "platform_thread.h"

static int falhas = 0;

#define CHECA(cond, msg) do {                                   \
    if (cond) printf("  ok    %s\n", msg);                     \
    else    { printf("  FALHA %s\n", msg); falhas++; }        \
} while (0)

/* ---------- 1. create/join com valor de retorno ---------- */
static void *devolve_dobro(void *arg)
{
    long *x = arg;
    *x *= 2;
    return arg;
}

static void teste_thread(void)
{
    pt_thread_t t;
    long v = 21;
    void *ret = NULL;

    printf("thread create/join\n");
    CHECA(pt_thread_create(&t, devolve_dobro, &v) == 0, "create");
    CHECA(pt_thread_join(&t, &ret) == 0, "join");
    CHECA(v == 42, "thread rodou");
    CHECA(ret == &v, "valor de retorno chega no join");
}

/* ---------- 2. contador protegido por mutex ---------- */
#define N_THREADS 8
#define N_INCR    100000

static pt_mutex_t mtx;
static long contador;

static void *incrementa(void *arg)
{
    int i;
    (void)arg;
    for (i = 0; i < N_INCR; i++) {
        pt_mutex_lock(&mtx);
        contador++;
        pt_mutex_unlock(&mtx);
    }
    return NULL;
}

static void teste_mutex(void)
{
    pt_thread_t t[N_THREADS];
    int i;

    printf("mutex\n");
    contador = 0;
    CHECA(pt_mutex_init(&mtx) == 0, "init");
    for (i = 0; i < N_THREADS; i++) pt_thread_create(&t[i], incrementa, NULL);
    for (i = 0; i < N_THREADS; i++) pt_thread_join(&t[i], NULL);
    CHECA(contador == (long)N_THREADS * N_INCR, "contador exato com 8 threads");
    CHECA(pt_mutex_destroy(&mtx) == 0, "destroy");
}

/* ---------- 3. variavel de condicao ---------- */
static pt_mutex_t cm;
static pt_cond_t  cv;
static int pronto, acordados;

static void *espera_pronto(void *arg)
{
    (void)arg;
    pt_mutex_lock(&cm);
    while (!pronto)
        pt_cond_wait(&cv, &cm);
    acordados++;
    pt_mutex_unlock(&cm);
    return NULL;
}

static void teste_cond(void)
{
    pt_thread_t t[4];
    int i;

    printf("variavel de condicao\n");
    pt_mutex_init(&cm);
    CHECA(pt_cond_init(&cv) == 0, "init");

    /* signal: uma thread esperando */
    pronto = 0; acordados = 0;
    pt_thread_create(&t[0], espera_pronto, NULL);
    pt_mutex_lock(&cm);
    pronto = 1;
    pt_cond_signal(&cv);
    pt_mutex_unlock(&cm);
    pt_thread_join(&t[0], NULL);
    CHECA(acordados == 1, "signal acorda a thread");

    /* broadcast: varias threads esperando */
    pronto = 0; acordados = 0;
    for (i = 0; i < 4; i++) pt_thread_create(&t[i], espera_pronto, NULL);
    pt_mutex_lock(&cm);
    pronto = 1;
    pt_cond_broadcast(&cv);
    pt_mutex_unlock(&cm);
    for (i = 0; i < 4; i++) pt_thread_join(&t[i], NULL);
    CHECA(acordados == 4, "broadcast acorda as 4 threads");

    CHECA(pt_cond_destroy(&cv) == 0, "destroy");
    pt_mutex_destroy(&cm);
}

/* ---------- 4. semaforo ---------- */
static pt_sem_t sem;
static volatile int passou;

static void *espera_sem(void *arg)
{
    (void)arg;
    pt_sem_wait(&sem);
    passou = 1;
    return NULL;
}

static void teste_sem(void)
{
    pt_thread_t t;

    printf("semaforo\n");

    /* valor inicial > 0: wait nao bloqueia */
    CHECA(pt_sem_init(&sem, 2, 10) == 0, "init com 2");
    CHECA(pt_sem_wait(&sem) == 0, "1o wait passa direto");
    CHECA(pt_sem_wait(&sem) == 0, "2o wait passa direto");
    CHECA(pt_sem_destroy(&sem) == 0, "destroy");

    /* valor inicial 0: a thread so passa depois do post */
    passou = 0;
    pt_sem_init(&sem, 0, 10);
    pt_thread_create(&t, espera_sem, NULL);
    CHECA(passou == 0, "thread bloqueada antes do post");
    CHECA(pt_sem_post(&sem) == 0, "post");
    pt_thread_join(&t, NULL);
    CHECA(passou == 1, "thread liberada pelo post");
    pt_sem_destroy(&sem);
}

/* ---------- 5. relogio ---------- */
static void teste_tempo(void)
{
    uint64_t a, b;
    printf("relogio\n");
    a = pt_time_ns();
    b = pt_time_ns();
    CHECA(a > 0, "pt_time_ns devolve valor");
    CHECA(b >= a, "monotonico");
}

int main(void)
{
    teste_thread();
    teste_mutex();
    teste_cond();
    teste_sem();
    teste_tempo();

    printf("\n%s (%d falha(s))\n", falhas ? "FALHOU" : "PASSOU", falhas);
    return falhas ? 1 : 0;
}
