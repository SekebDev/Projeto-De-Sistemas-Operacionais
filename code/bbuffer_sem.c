/*
 * bbuffer_sem.c - Versao B: dois semaforos contadores + um mutex.
 *
 *   slots: vagas livres     (comeca em cap)
 *   items: itens no buffer  (comeca em 0)
 *
 * Os semaforos ja fazem a contagem e o bloqueio, entao o mutex so
 * precisa proteger os indices head/tail (com varios produtores, ou
 * varios consumidores, dois deles poderiam pegar o mesmo indice).
 * Aqui nao ha while porque nao existe condicao a testar: o proprio
 * sem_wait so retorna quando ha recurso.
 */
#include <stdlib.h>
#include <string.h>
#include "bbuffer.h"
#include "platform_thread.h"

typedef struct {
    item_t    *buf;
    size_t     cap;
    size_t     head;
    size_t     tail;
    pt_sem_t   slots;
    pt_sem_t   items;
    pt_mutex_t m;
} buf_sem_t;

static bbuffer_t *sem_create(size_t capacity)
{
    buf_sem_t *b;
    if (capacity == 0) return NULL;

    b = calloc(1, sizeof *b);
    if (!b) return NULL;
    b->buf = malloc(capacity * sizeof *b->buf);
    if (!b->buf) { free(b); return NULL; }
    b->cap = capacity;

    if (pt_sem_init(&b->slots, (unsigned)capacity, (unsigned)capacity) ||
        pt_sem_init(&b->items, 0, (unsigned)capacity) ||
        pt_mutex_init(&b->m)) {
        free(b->buf);
        free(b);
        return NULL;
    }
    return b;
}

static void sem_destroy_buf(bbuffer_t *p)
{
    buf_sem_t *b = p;
    if (!b) return;
    pt_mutex_destroy(&b->m);
    pt_sem_destroy(&b->items);
    pt_sem_destroy(&b->slots);
    free(b->buf);
    free(b);
}

static void sem_put(bbuffer_t *p, item_t it)
{
    buf_sem_t *b = p;

    pt_sem_wait(&b->slots);

    pt_mutex_lock(&b->m);
    b->buf[b->tail] = it;
    b->tail = (b->tail + 1) % b->cap;
    pt_mutex_unlock(&b->m);

    pt_sem_post(&b->items);
}

static item_t sem_get(bbuffer_t *p)
{
    buf_sem_t *b = p;
    item_t it;

    pt_sem_wait(&b->items);

    pt_mutex_lock(&b->m);
    it = b->buf[b->head];
    b->head = (b->head + 1) % b->cap;
    pt_mutex_unlock(&b->m);

    pt_sem_post(&b->slots);
    return it;
}

const bb_ops_t bb_sem_ops = { "B", sem_create, sem_destroy_buf, sem_put, sem_get };

/* fica aqui so para nao criar um arquivo a mais */
const bb_ops_t *bb_por_nome(const char *nome)
{
    if (strcmp(nome, "A") == 0) return &bb_cond_ops;
    if (strcmp(nome, "B") == 0) return &bb_sem_ops;
    return NULL;
}
