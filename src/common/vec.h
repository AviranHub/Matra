#ifndef MATRA_COMMON_VEC_H
#define MATRA_COMMON_VEC_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    unsigned char *data;
    size_t length;
    size_t capacity;
    size_t element_size;
} Vec;

bool vec_init(Vec *vec, size_t element_size);
bool vec_reserve(Vec *vec, size_t capacity);
bool vec_push(Vec *vec, const void *element);
bool vec_resize(Vec *vec, size_t length);
void *vec_at(Vec *vec, size_t index);
const void *vec_at_const(const Vec *vec, size_t index);
void vec_clear(Vec *vec);
void vec_destroy(Vec *vec);

#endif
