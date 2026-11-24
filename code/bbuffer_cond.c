/*
 * bbuffer_cond.c - Versao A: um mutex e duas variaveis de condicao.
 *
 * O mutex protege tudo (vetor, indices e contador). Quem nao pode
 * prosseguir dorme na condicao correspondente e e acordado pelo lado
 * oposto. O teste e sempre feito com while: o wait pode retornar sem
 * que a condicao seja verdadeira (spurious wakeup, ou outra thread
 * chegou antes e consumiu a vaga/item).
 */
#include <stdlib.h>
#include "bbuffer.h"
#include "platform_thread.h"

typedef struct {
    item_t    *buf;
    size_t     cap;
    size_t     head;   /* proxima posicao a ler */
    size_t     tail;   /* proxima posicao a escrever */
    size_t     count;
    pt_mutex_t m;
    pt_cond_t  not_full;
    pt_cond_t  not_empty;
} buf_cond_t;

static bbuffer_t *cond_create(size_t capacity)
{
    buf_cond_t *b;
    if (capacity == 0) return NULL;

    b = calloc(1, sizeof *b);
    if (!b) return NULL;
    b->buf = malloc(capacity * sizeof *b->buf);
    if (!b->buf) { free(b); return NULL; }
    b->cap = capacity;

    if (pt_mutex_init(&b->m) || pt_cond_init(&b->not_full) || pt_cond_init(&b->not_empty)) {
        free(b->buf);
        free(b);
        return NULL;
    }
    return b;
}

static void cond_destroy(bbuffer_t *p)
{
    buf_cond_t *b = p;
    if (!b) return;
    pt_cond_destroy(&b->not_empty);
    pt_cond_destroy(&b->not_full);
    pt_mutex_destroy(&b->m);
    free(b->buf);
    free(b);
}

static void cond_put(bbuffer_t *p, item_t it)
{
    buf_cond_t *b = p;

    pt_mutex_lock(&b->m);
    while (b->count == b->cap)
        pt_cond_wait(&b->not_full, &b->m);

    b->buf[b->tail] = it;
    b->tail = (b->tail + 1) % b->cap;
    b->count++;

    pt_cond_signal(&b->not_empty);
    pt_mutex_unlock(&b->m);
}

static item_t cond_get(bbuffer_t *p)
{
    buf_cond_t *b = p;
    item_t it;

    pt_mutex_lock(&b->m);
    while (b->count == 0)
        pt_cond_wait(&b->not_empty, &b->m);

    it = b->buf[b->head];
    b->head = (b->head + 1) % b->cap;
    b->count--;

    pt_cond_signal(&b->not_full);
    pt_mutex_unlock(&b->m);
    return it;
}

const bb_ops_t bb_cond_ops = { "A", cond_create, cond_destroy, cond_put, cond_get };
