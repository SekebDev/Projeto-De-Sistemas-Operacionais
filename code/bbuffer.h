/*
 * bbuffer.h - interface do buffer circular limitado.
 *
 * Existem duas implementacoes com a mesma interface:
 *   bbuffer_cond.c -> versao A (mutex + 2 variaveis de condicao)
 *   bbuffer_sem.c  -> versao B (2 semaforos + mutex so nos indices)
 *
 * Para as duas caberem no mesmo executavel, cada arquivo deixa suas
 * funcoes static e exporta so uma tabela (bb_ops_t). O bench escolhe
 * a tabela pela opcao --version.
 */
#ifndef BBUFFER_H
#define BBUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t seq;            /* numero de sequencia global do item */
    uint64_t t_produced_ns;  /* instante em que foi produzido (latencia) */
} item_t;

/* cada versao tem sua propria struct; por fora e so um ponteiro */
typedef void bbuffer_t;

typedef struct {
    const char *nome;                     /* "A" ou "B" */
    bbuffer_t *(*create)(size_t capacity);
    void       (*destroy)(bbuffer_t *b);
    void       (*put)(bbuffer_t *b, item_t it);
    item_t     (*get)(bbuffer_t *b);
} bb_ops_t;

extern const bb_ops_t bb_cond_ops;  /* versao A */
extern const bb_ops_t bb_sem_ops;   /* versao B */

/* devolve a tabela da versao "A" ou "B", ou NULL */
const bb_ops_t *bb_por_nome(const char *nome);

#endif /* BBUFFER_H */
