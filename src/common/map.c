#include "map.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

struct MapSlot {
    unsigned char *key;
    size_t key_length;
    uint64_t hash;
    bool occupied;
    bool tombstone;
};

static bool map_add_overflow(size_t left, size_t right, size_t *result) {
    if (left > SIZE_MAX - right) {
        return true;
    }
    *result = left + right;
    return false;
}

static bool map_multiply_overflow(size_t left, size_t right, size_t *result) {
    if (right != 0U && left > SIZE_MAX / right) {
        return true;
    }
    *result = left * right;
    return false;
}

static bool map_align_size(size_t value, size_t alignment, size_t *result) {
    size_t remainder = value % alignment;
    size_t padding = remainder == 0U ? 0U : alignment - remainder;
    return !map_add_overflow(value, padding, result);
}

static uint64_t map_hash(const void *key, size_t key_length) {
    const unsigned char *bytes = (const unsigned char *)key;
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t index;

    for (index = 0U; index < key_length; ++index) {
        hash ^= (uint64_t)bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool map_key_matches(const MapSlot *slot, const void *key,
                            size_t key_length, uint64_t hash) {
    if (!slot->occupied || slot->hash != hash ||
        slot->key_length != key_length) {
        return false;
    }
    return key_length == 0U || memcmp(slot->key, key, key_length) == 0;
}

static size_t map_find_slot(const Map *map, const void *key, size_t key_length,
                            uint64_t hash, bool *found) {
    size_t index = (size_t)(hash & (uint64_t)(map->capacity - 1U));
    size_t first_tombstone = SIZE_MAX;
    size_t probes;

    for (probes = 0U; probes < map->capacity; ++probes) {
        const MapSlot *slot = &map->slots[index];
        if (slot->occupied) {
            if (map_key_matches(slot, key, key_length, hash)) {
                *found = true;
                return index;
            }
        } else if (slot->tombstone) {
            if (first_tombstone == SIZE_MAX) {
                first_tombstone = index;
            }
        } else {
            *found = false;
            return first_tombstone == SIZE_MAX ? index : first_tombstone;
        }
        index = (index + 1U) & (map->capacity - 1U);
    }
    *found = false;
    return first_tombstone;
}

static bool map_rehash(Map *map, size_t new_capacity) {
    MapSlot *new_slots;
    unsigned char *new_values;
    size_t slots_size;
    size_t values_size;
    size_t index;

    if (map_multiply_overflow(new_capacity, sizeof(*new_slots), &slots_size) ||
        map_multiply_overflow(new_capacity, map->value_stride, &values_size)) {
        return false;
    }
    new_slots = (MapSlot *)calloc(1U, slots_size);
    new_values = (unsigned char *)malloc(values_size);
    if (new_slots == NULL || new_values == NULL) {
        free(new_slots);
        free(new_values);
        return false;
    }
    for (index = 0U; index < map->capacity; ++index) {
        MapSlot *old_slot = &map->slots[index];
        if (old_slot->occupied) {
            size_t new_index =
                (size_t)(old_slot->hash & (uint64_t)(new_capacity - 1U));
            size_t probes;
            for (probes = 0U; new_slots[new_index].occupied; ++probes) {
                new_index = (new_index + 1U) & (new_capacity - 1U);
            }
            new_slots[new_index] = *old_slot;
            memcpy(new_values + new_index * map->value_stride,
                   map->values + index * map->value_stride, map->value_size);
        }
    }
    free(map->slots);
    free(map->values);
    map->slots = new_slots;
    map->values = new_values;
    map->capacity = new_capacity;
    map->tombstones = 0U;
    return true;
}

bool map_init(Map *map, size_t value_size) {
    size_t value_stride;

    if (map == NULL || value_size == 0U ||
        !map_align_size(value_size, alignof(max_align_t), &value_stride)) {
        return false;
    }
    map->slots = NULL;
    map->values = NULL;
    map->length = 0U;
    map->capacity = 0U;
    map->tombstones = 0U;
    map->value_size = value_size;
    map->value_stride = value_stride;
    return true;
}

bool map_put(Map *map, const void *key, size_t key_length, const void *value,
             bool *replaced) {
    uint64_t hash;
    bool found;
    size_t index;
    size_t occupied_count;
    size_t load_limit;
    unsigned char *key_copy = NULL;
    unsigned char *value_copy = NULL;
    const void *stable_value = value;

    if (replaced != NULL) {
        *replaced = false;
    }
    if (map == NULL || map->value_size == 0U || value == NULL ||
        (key_length > 0U && key == NULL)) {
        return false;
    }
    hash = map_hash(key, key_length);
    if (map->capacity > 0U) {
        index = map_find_slot(map, key, key_length, hash, &found);
        if (found) {
            memcpy(map->values + index * map->value_stride, value,
                   map->value_size);
            if (replaced != NULL) {
                *replaced = true;
            }
            return true;
        }
    }
    if (key_length > 0U) {
        key_copy = (unsigned char *)malloc(key_length);
        if (key_copy == NULL) {
            return false;
        }
        memcpy(key_copy, key, key_length);
    }
    if (map->values != NULL) {
        uintptr_t value_address = (uintptr_t)value;
        uintptr_t values_address = (uintptr_t)(void *)map->values;
        size_t values_size;
        if (!map_multiply_overflow(map->capacity, map->value_stride,
                                   &values_size) &&
            value_address >= values_address &&
            value_address - values_address < values_size) {
            value_copy = (unsigned char *)malloc(map->value_size);
            if (value_copy == NULL) {
                free(key_copy);
                return false;
            }
            memcpy(value_copy, value, map->value_size);
            stable_value = value_copy;
        }
    }
    if (map->capacity == 0U) {
        if (!map_rehash(map, 16U)) {
            free(key_copy);
            free(value_copy);
            return false;
        }
    } else {
        occupied_count = map->length + map->tombstones;
        load_limit = map->capacity - map->capacity / 3U;
        if (occupied_count + 1U >= load_limit ||
            map->tombstones > map->length) {
            if (map->capacity > SIZE_MAX / 2U ||
                !map_rehash(map, map->tombstones > map->length
                                      ? map->capacity
                                      : map->capacity * 2U)) {
                free(key_copy);
                free(value_copy);
                return false;
            }
        }
    }
    index = map_find_slot(map, key, key_length, hash, &found);
    if (index == SIZE_MAX || found) {
        free(key_copy);
        free(value_copy);
        return false;
    }
    map->slots[index].key = key_copy;
    map->slots[index].key_length = key_length;
    map->slots[index].hash = hash;
    map->slots[index].occupied = true;
    map->slots[index].tombstone = false;
    memcpy(map->values + index * map->value_stride, stable_value,
           map->value_size);
    free(value_copy);
    map->length += 1U;
    if (map->tombstones > 0U) {
        bool was_tombstone = false;
        size_t check_index = (size_t)(hash & (uint64_t)(map->capacity - 1U));
        size_t probes;
        for (probes = 0U; probes < map->capacity; ++probes) {
            if (check_index == index) {
                break;
            }
            if (!map->slots[check_index].occupied &&
                map->slots[check_index].tombstone) {
                was_tombstone = true;
            } else if (!map->slots[check_index].occupied) {
                break;
            }
            check_index = (check_index + 1U) & (map->capacity - 1U);
        }
        if (was_tombstone) {
            map->tombstones -= 1U;
        }
    }
    return true;
}

void *map_get(Map *map, const void *key, size_t key_length) {
    bool found;
    size_t index;
    uint64_t hash;

    if (map == NULL || map->capacity == 0U ||
        (key_length > 0U && key == NULL)) {
        return NULL;
    }
    hash = map_hash(key, key_length);
    index = map_find_slot(map, key, key_length, hash, &found);
    return found ? map->values + index * map->value_stride : NULL;
}

const void *map_get_const(const Map *map, const void *key, size_t key_length) {
    bool found;
    size_t index;
    uint64_t hash;

    if (map == NULL || map->capacity == 0U ||
        (key_length > 0U && key == NULL)) {
        return NULL;
    }
    hash = map_hash(key, key_length);
    index = map_find_slot(map, key, key_length, hash, &found);
    return found ? map->values + index * map->value_stride : NULL;
}

bool map_remove(Map *map, const void *key, size_t key_length) {
    bool found;
    size_t index;
    uint64_t hash;

    if (map == NULL || map->capacity == 0U ||
        (key_length > 0U && key == NULL)) {
        return false;
    }
    hash = map_hash(key, key_length);
    index = map_find_slot(map, key, key_length, hash, &found);
    if (!found) {
        return false;
    }
    free(map->slots[index].key);
    map->slots[index].key = NULL;
    map->slots[index].key_length = 0U;
    map->slots[index].occupied = false;
    map->slots[index].tombstone = true;
    map->length -= 1U;
    map->tombstones += 1U;
    return true;
}

void map_clear(Map *map) {
    size_t index;

    if (map == NULL) {
        return;
    }
    for (index = 0U; index < map->capacity; ++index) {
        if (map->slots[index].occupied) {
            free(map->slots[index].key);
        }
    }
    free(map->slots);
    free(map->values);
    map->slots = NULL;
    map->values = NULL;
    map->length = 0U;
    map->capacity = 0U;
    map->tombstones = 0U;
}

void map_destroy(Map *map) {
    if (map == NULL) {
        return;
    }
    map_clear(map);
    map->value_size = 0U;
    map->value_stride = 0U;
}
