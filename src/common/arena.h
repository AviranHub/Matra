#ifndef MATRA_COMMON_ARENA_H
#define MATRA_COMMON_ARENA_H

#include <stddef.h>

typedef struct ArenaBlock ArenaBlock;

typedef struct {
    ArenaBlock *first;
    ArenaBlock *current;
    size_t block_size;
} Arena;

void arena_init(Arena *arena, size_t block_size);
void *arena_alloc(Arena *arena, size_t size, size_t alignment);
void *arena_calloc(Arena *arena, size_t count, size_t size, size_t alignment);
void arena_reset(Arena *arena);
void arena_destroy(Arena *arena);

#endif
