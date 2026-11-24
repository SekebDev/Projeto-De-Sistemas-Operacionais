/*
 * test_bbuffer.c - corretude do buffer limitado (versoes A e B).
 *
 * Para cada configuracao, os consumidores marcam o seq de cada item
 * recebido num vetor de contadores. No fim, todo seq precisa ter
 * aparecido exatamente uma vez (nada perdido, nada duplicado).
 * Inclui o caso 4x4 com buffer 1, que e o mais propenso a deadlock.
 */
#include <stdio.h>
#include <stdlib.h>
#include "platform_thread.h"
#include "bbuffer.h"

#define M 40000   /* divisivel por 1, 2 e 4 */

typedef struct {
    const bb_ops_t *ops;
    bbuffer_t      *buf;
    uint64_t        inicio, qtd;
    unsigned char  *vistos;   /* so consumidores usam */
    pt_mutex_t     *mv;       /* protege vistos */
} arg_t;

static void *prod(void *p)
{
    arg_t *a = p;
    uint64_t i;
    for (i = 0; i < a->qtd; i++) {
        item_t it;
        it.seq = a->inicio + i;
        it.t_produced_ns = 0;
        a->ops->put(a->buf, it);
    }
    return NULL;
}

static void *cons(void *p)
{
    arg_t *a = p;
    uint64_t i;
    for (i = 0; i < a->qtd; i++) {
        item_t it = a->ops->get(a->buf);
        pt_mutex_lock(a->mv);
        if (it.seq < M && a->vistos[it.seq] < 255)
            a->vistos[it.seq]++;
        pt_mutex_unlock(a->mv);
    }
    return NULL;
}

static int roda(const bb_ops_t *ops, int P, int C, size_t N)
{
    pt_thread_t th[8];
    arg_t args[8];
    unsigned char *vistos = calloc(M, 1);
    pt_mutex_t mv;
    bbuffer_t *buf = ops->create(N);
    int i, n = 0, ok = 1;
    uint64_t s;

    if (!vistos || !buf) { printf("sem memoria\n"); return 0; }
    pt_mutex_init(&mv);

    for (i = 0; i < P; i++, n++) {
        args[n].ops = ops; args[n].buf = buf; args[n].vistos = vistos; args[n].mv = &mv;
        args[n].qtd = M / P; args[n].inicio = (uint64_t)i * (M / P);
        pt_thread_create(&th[n], prod, &args[n]);
    }
    for (i = 0; i < C; i++, n++) {
        args[n].ops = ops; args[n].buf = buf; args[n].vistos = vistos; args[n].mv = &mv;
        args[n].qtd = M / C; args[n].inicio = 0;
        pt_thread_create(&th[n], cons, &args[n]);
    }
    for (i = 0; i < n; i++)
        pt_thread_join(&th[i], NULL);

    for (s = 0; s < M; s++) {
        if (vistos[s] != 1) {
            printf("    seq %llu apareceu %u vez(es)\n", (unsigned long long)s, vistos[s]);
            ok = 0;
            break;
        }
    }

    pt_mutex_destroy(&mv);
    ops->destroy(buf);
    free(vistos);
    return ok;
}

int main(void)
{
    const bb_ops_t *versoes[2] = { &bb_cond_ops, &bb_sem_ops };
    int pc[2][2] = { {1, 1}, {4, 4} };
    size_t tams[3] = { 1, 16, 256 };
    int v, k, t, falhas = 0;

    for (v = 0; v < 2; v++)
        for (k = 0; k < 2; k++)
            for (t = 0; t < 3; t++) {
                int ok = roda(versoes[v], pc[k][0], pc[k][1], tams[t]);
                printf("  %-5s versao %s  %dx%d  buffer %3u\n", ok ? "ok" : "FALHA",
                       versoes[v]->nome, pc[k][0], pc[k][1], (unsigned)tams[t]);
                if (!ok) falhas++;
            }

    printf("\n%s (%d falha(s))\n", falhas ? "FALHOU" : "PASSOU", falhas);
    return falhas ? 1 : 0;
}
