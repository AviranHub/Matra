#include "test.h"

#include "../../src/common/vec.h"

#include <assert.h>
#include <stdint.h>

void test_vec(void) {
    Vec vec;
    uint64_t value;
    const uint64_t *stored;
    size_t index;
    size_t old_length;

    assert(vec_init(&vec, sizeof(value)));
    for (index = 0U; index < 40U; ++index) {
        value = (uint64_t)index * UINT64_C(3);
        assert(vec_push(&vec, &value));
    }
    assert(vec.length == 40U);
    stored = (const uint64_t *)vec_at_const(&vec, 17U);
    assert(stored != NULL && *stored == UINT64_C(51));
    assert(vec_at(&vec, vec.length) == NULL);
    assert(vec_resize(&vec, 45U));
    for (index = 40U; index < 45U; ++index) {
        stored = (const uint64_t *)vec_at_const(&vec, index);
        assert(stored != NULL && *stored == 0U);
    }
    while (vec.length < vec.capacity) {
        value = (uint64_t)vec.length;
        assert(vec_push(&vec, &value));
    }
    assert(vec_push(&vec, vec_at(&vec, 0U)));
    stored = (const uint64_t *)vec_at_const(&vec, vec.length - 1U);
    assert(stored != NULL && *stored == 0U);
    old_length = vec.length;
    assert(!vec_reserve(&vec, SIZE_MAX));
    assert(vec.length == old_length);
    assert(vec_resize(&vec, 4U));
    vec_clear(&vec);
    assert(vec.length == 0U);
    vec_destroy(&vec);
    assert(vec.data == NULL);
    assert(!vec_init(&vec, 0U));
}
