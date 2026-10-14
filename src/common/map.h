#ifndef MATRA_COMMON_MAP_H
#define MATRA_COMMON_MAP_H

#include <stdbool.h>
#include <stddef.h>

typedef struct MapSlot MapSlot;

typedef struct {
    MapSlot *slots;
    unsigned char *values;
    size_t length;
    size_t capacity;
    size_t tombstones;
    size_t value_size;
    size_t value_stride;
} Map;

bool map_init(Map *map, size_t value_size);
bool map_put(Map *map, const void *key, size_t key_length, const void *value,
             bool *replaced);
void *map_get(Map *map, const void *key, size_t key_length);
const void *map_get_const(const Map *map, const void *key, size_t key_length);
bool map_remove(Map *map, const void *key, size_t key_length);
void map_clear(Map *map);
void map_destroy(Map *map);

#endif
