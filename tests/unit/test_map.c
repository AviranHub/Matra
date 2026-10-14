#include "test.h"

#include "../../src/common/map.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

void test_map(void) {
    Map map;
    bool replaced;
    uint64_t value;
    uint64_t *stored;
    size_t index;
    char key[32];
    const unsigned char binary_key[] = { 'a', 0U, 'b' };

    assert(map_init(&map, sizeof(value)));
    value = UINT64_C(11);
    assert(map_put(&map, NULL, 0U, &value, &replaced));
    assert(!replaced);
    stored = (uint64_t *)map_get(&map, NULL, 0U);
    assert(stored != NULL && *stored == UINT64_C(11));

    value = UINT64_C(22);
    assert(map_put(&map, binary_key, sizeof(binary_key), &value, &replaced));
    assert(!replaced);
    stored = (uint64_t *)map_get(&map, binary_key, sizeof(binary_key));
    assert(stored != NULL && *stored == UINT64_C(22));

    value = UINT64_C(33);
    assert(map_put(&map, "", 0U, &value, &replaced));
    assert(replaced);
    stored = (uint64_t *)map_get(&map, NULL, 0U);
    assert(stored != NULL && *stored == UINT64_C(33));

    for (index = 0U; index < 300U; ++index) {
        int written = snprintf(key, sizeof(key), "key-%zu", index);
        assert(written > 0);
        value = (uint64_t)index + UINT64_C(1000);
        assert(map_put(&map, key, (size_t)written, &value, NULL));
    }
    for (index = 0U; index < 300U; ++index) {
        int written = snprintf(key, sizeof(key), "key-%zu", index);
        assert(written > 0);
        stored = (uint64_t *)map_get(&map, key, (size_t)written);
        assert(stored != NULL && *stored == (uint64_t)index + UINT64_C(1000));
    }
    stored = (uint64_t *)map_get(&map, "key-4", 5U);
    assert(stored != NULL);
    assert(map_put(&map, "key-4", 5U, stored, &replaced));
    assert(replaced);
    stored = (uint64_t *)map_get(&map, "key-4", 5U);
    assert(stored != NULL && *stored == UINT64_C(1004));

    assert(map_remove(&map, "key-4", 5U));
    assert(map_get(&map, "key-4", 5U) == NULL);
    assert(!map_remove(&map, "missing", 7U));
    for (index = 300U; index < 600U; ++index) {
        int written = snprintf(key, sizeof(key), "key-%zu", index);
        assert(written > 0);
        value = (uint64_t)index;
        assert(map_put(&map, key, (size_t)written, &value, NULL));
    }
    assert(map_get_const(&map, binary_key, sizeof(binary_key)) != NULL);
    map_clear(&map);
    assert(map.length == 0U);
    assert(map.capacity == 0U);
    map_destroy(&map);

    assert(!map_init(&map, 0U));
    assert(!map_put(&map, "x", 1U, &value, NULL));
}
