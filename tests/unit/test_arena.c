#include "test.h"

#include "../../src/common/arena.h"

#include <assert.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>

void test_arena(void) {
    Arena arena;
    void *first;
    void *second;
    unsigned char *zeroed;
    size_t index;

    arena_init(&arena, 32U);
    first = arena_alloc(&arena, sizeof(uint64_t), alignof(uint64_t));
    second = arena_alloc(&arena, 96U, alignof(max_align_t));
    assert(first != NULL);
    assert(second != NULL);
    assert((uintptr_t)first % alignof(uint64_t) == 0U);
    assert((uintptr_t)second % alignof(max_align_t) == 0U);
    zeroed = (unsigned char *)arena_calloc(&arena, 12U, sizeof(*zeroed), 1U);
    assert(zeroed != NULL);
    for (index = 0U; index < 12U; ++index) {
        assert(zeroed[index] == 0U);
    }
    assert(arena_alloc(&arena, 1U, 3U) == NULL);
    assert(arena_calloc(&arena, SIZE_MAX, 2U, 1U) == NULL);
    arena_reset(&arena);
    assert(arena_alloc(&arena, 8U, alignof(uint64_t)) != NULL);
    arena_destroy(&arena);
    assert(arena.first == NULL);
    assert(arena.current == NULL);
}
