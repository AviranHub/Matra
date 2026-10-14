#include "arena.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

struct ArenaBlock {
    ArenaBlock *next;
    size_t capacity;
    size_t used;
    max_align_t alignment;
    unsigned char data[];
};

static bool arena_is_power_of_two(size_t value) {
    return value != 0U && (value & (value - 1U)) == 0U;
}

static ArenaBlock *arena_block_create(size_t capacity) {
    ArenaBlock *block;
    size_t allocation_size;

    if (capacity > SIZE_MAX - sizeof(*block)) {
        return NULL;
    }
    allocation_size = sizeof(*block) + capacity;
    block = (ArenaBlock *)malloc(allocation_size);
    if (block == NULL) {
        return NULL;
    }
    block->next = NULL;
    block->capacity = capacity;
    block->used = 0U;
    return block;
}

void arena_init(Arena *arena, size_t block_size) {
    if (arena == NULL) {
        return;
    }
    arena->first = NULL;
    arena->current = NULL;
    arena->block_size = block_size == 0U ? 4096U : block_size;
}

void *arena_alloc(Arena *arena, size_t size, size_t alignment) {
    ArenaBlock *block;
    size_t actual_size;

    if (arena == NULL) {
        return NULL;
    }
    if (alignment == 0U) {
        alignment = alignof(max_align_t);
    }
    if (!arena_is_power_of_two(alignment) || alignment > alignof(max_align_t)) {
        return NULL;
    }
    actual_size = size == 0U ? 1U : size;
    block = arena->current;
    for (;;) {
        size_t remainder;
        size_t padding;

        if (block != NULL) {
            remainder = block->used & (alignment - 1U);
            padding = remainder == 0U ? 0U : alignment - remainder;
            if (padding <= block->capacity - block->used &&
                actual_size <= block->capacity - block->used - padding) {
                void *result = block->data + block->used + padding;
                block->used += padding + actual_size;
                return result;
            }
        }

        if (block != NULL && block->next != NULL) {
            block = block->next;
            block->used = 0U;
            arena->current = block;
            continue;
        }

        {
            size_t minimum_capacity;
            size_t capacity;
            ArenaBlock *new_block;

            if (actual_size > SIZE_MAX - (alignment - 1U)) {
                return NULL;
            }
            minimum_capacity = actual_size + alignment - 1U;
            capacity = arena->block_size > minimum_capacity
                           ? arena->block_size
                           : minimum_capacity;
            new_block = arena_block_create(capacity);
            if (new_block == NULL) {
                return NULL;
            }
            if (arena->first == NULL) {
                arena->first = new_block;
            } else {
                block->next = new_block;
            }
            arena->current = new_block;
            block = new_block;
        }
    }
}

void *arena_calloc(Arena *arena, size_t count, size_t size, size_t alignment) {
    size_t total_size;
    void *memory;

    if (size != 0U && count > SIZE_MAX / size) {
        return NULL;
    }
    total_size = count * size;
    memory = arena_alloc(arena, total_size, alignment);
    if (memory != NULL) {
        memset(memory, 0, total_size == 0U ? 1U : total_size);
    }
    return memory;
}

void arena_reset(Arena *arena) {
    ArenaBlock *block;

    if (arena == NULL || arena->first == NULL) {
        return;
    }
    block = arena->first->next;
    arena->first->next = NULL;
    arena->first->used = 0U;
    arena->current = arena->first;
    while (block != NULL) {
        ArenaBlock *next = block->next;
        free(block);
        block = next;
    }
}

void arena_destroy(Arena *arena) {
    ArenaBlock *block;

    if (arena == NULL) {
        return;
    }
    block = arena->first;
    while (block != NULL) {
        ArenaBlock *next = block->next;
        free(block);
        block = next;
    }
    arena->first = NULL;
    arena->current = NULL;
    arena->block_size = 0U;
}
