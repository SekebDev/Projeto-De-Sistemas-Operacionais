/*
 * bench.c - mede vazao (e opcionalmente latencia) do buffer limitado.
 *
 * Uso:
 *   bench --version A|B --producers P --consumers C --buffer N
 *         --items M --rep R --os linux|windows [--latency arq.csv]
 *
 * Imprime UMA linha CSV em stdout:
 *   so,versao,produtores,consumidores,buffer,itens,rep,tempo_s,vazao_itens_s
 *
 * Termino: M precisa ser divisivel por P e por C. Cada produtor gera
 * M/P itens e cada consumidor retira M/C. Como o total produzido e
 * igual ao consumido, ninguem fica preso no final e todos os join
 * retornam.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform_thread.h"
#include "bbuffer.h"

#define MAX_THREADS 64

typedef struct {
    const bb_ops_t *ops;
    bbuffer_t      *buf;
    uint64_t        primeiro_seq;  /* produtor: de onde comeca */
    uint64_t        quantos;       /* itens que esta thread produz/consome */
    uint64_t       *lat;           /* consumidor: vetor de latencias ou NULL */
} tarefa_t;

static void *produtor(void *arg)
{
    tarefa_t *t = arg;
    uint64_t i;
    for (i = 0; i < t->quantos; i++) {
        item_t it;
        it.seq = t->primeiro_seq + i;
        it.t_produced_ns = t->lat ? pt_time_ns() : 0;
        t->ops->put(t->buf, it);
    }
    return NULL;
}

static void *consumidor(void *arg)
{
    tarefa_t *t = arg;
    uint64_t i;
    for (i = 0; i < t->quantos; i++) {
        item_t it = t->ops->get(t->buf);
        if (t->lat)
            t->lat[it.seq] = pt_time_ns() - it.t_produced_ns;
    }
    return NULL;
}

static void uso(const char *prog)
{
    fprintf(stderr,
        "uso: %s --version A|B --producers P --consumers C --buffer N\n"
        "          --items M --rep R --os linux|windows [--latency arq.csv]\n",
        prog);
    exit(2);
}

int main(int argc, char **argv)
{
    const char *versao = NULL, *so = NULL, *arq_lat = NULL;
    long P = 0, C = 0, N = 0, R = 0;
    unsigned long long M = 0;
    const bb_ops_t *ops;
    bbuffer_t *buf;
    pt_thread_t th[2 * MAX_THREADS];
    tarefa_t tar[2 * MAX_THREADS];
    uint64_t *lat = NULL, t0, t1;
    double seg;
    int i, nth = 0;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (i + 1 >= argc) uso(argv[0]);
        if      (!strcmp(a, "--version"))   versao  = argv[++i];
        else if (!strcmp(a, "--producers")) P       = atol(argv[++i]);
        else if (!strcmp(a, "--consumers")) C       = atol(argv[++i]);
        else if (!strcmp(a, "--buffer"))    N       = atol(argv[++i]);
        else if (!strcmp(a, "--items"))     M       = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(a, "--rep"))       R       = atol(argv[++i]);
        else if (!strcmp(a, "--os"))        so      = argv[++i];
        else if (!strcmp(a, "--latency"))   arq_lat = argv[++i];
        else uso(argv[0]);
    }

    if (!versao || !so || P <= 0 || C <= 0 || N <= 0 || M == 0)
        uso(argv[0]);
    if (P > MAX_THREADS || C > MAX_THREADS) {
        fprintf(stderr, "no maximo %d produtores/consumidores\n", MAX_THREADS);
        return 2;
    }
    if (M % (unsigned long long)P != 0 || M % (unsigned long long)C != 0) {
        fprintf(stderr, "itens (%llu) precisa ser divisivel por P e por C\n", M);
        return 2;
    }
    ops = bb_por_nome(versao);
    if (!ops) {
        fprintf(stderr, "versao invalida: %s (use A ou B)\n", versao);
        return 2;
    }
    if (arq_lat && (P != 1 || C != 1 || N != 1)) {
        fprintf(stderr, "--latency so faz sentido com P=1, C=1, N=1\n");
        return 2;
    }

    buf = ops->create((size_t)N);
    if (!buf) { fprintf(stderr, "falha ao criar o buffer\n"); return 1; }

    /* alocado antes da medicao para nao interferir no tempo */
    if (arq_lat) {
        lat = calloc((size_t)M, sizeof *lat);
        if (!lat) { fprintf(stderr, "sem memoria para latencias\n"); return 1; }
    }

    for (i = 0; i < P; i++) {
        tar[nth].ops = ops; tar[nth].buf = buf; tar[nth].lat = lat;
        tar[nth].quantos = M / (unsigned long long)P;
        tar[nth].primeiro_seq = (uint64_t)i * tar[nth].quantos;
        nth++;
    }
    for (i = 0; i < C; i++) {
        tar[nth].ops = ops; tar[nth].buf = buf; tar[nth].lat = lat;
        tar[nth].quantos = M / (unsigned long long)C;
        tar[nth].primeiro_seq = 0;
        nth++;
    }

    t0 = pt_time_ns();
    for (i = 0; i < nth; i++) {
        void *(*fn)(void *) = (i < P) ? produtor : consumidor;
        if (pt_thread_create(&th[i], fn, &tar[i]) != 0) {
            fprintf(stderr, "falha ao criar thread %d\n", i);
            return 1;
        }
    }
    for (i = 0; i < nth; i++)
        pt_thread_join(&th[i], NULL);
    t1 = pt_time_ns();

    seg = (double)(t1 - t0) / 1e9;
    printf("%s,%s,%ld,%ld,%ld,%llu,%ld,%.6f,%.1f\n",
           so, ops->nome, P, C, N, M, R, seg, (double)M / seg);

    /* gravacao das latencias fica fora da medicao */
    if (arq_lat) {
        FILE *f = fopen(arq_lat, "a");
        unsigned long long s;
        if (!f) { perror(arq_lat); return 1; }
        for (s = 0; s < M; s++)
            fprintf(f, "%s,%s,%ld,%llu,%llu\n", so, ops->nome, R, s,
                    (unsigned long long)lat[s]);
        fclose(f);
        free(lat);
    }

    ops->destroy(buf);
    return 0;
}
