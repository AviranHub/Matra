#include "vec.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool vec_init(Vec *vec, size_t element_size) {
    if (vec == NULL || element_size == 0U) {
        return false;
    }
    vec->data = NULL;
    vec->length = 0U;
    vec->capacity = 0U;
    vec->element_size = element_size;
    return true;
}

bool vec_reserve(Vec *vec, size_t capacity) {
    size_t new_capacity;
    size_t allocation_size;
    unsigned char *new_data;

    if (vec == NULL || vec->element_size == 0U) {
        return false;
    }
    if (capacity <= vec->capacity) {
        return true;
    }
    new_capacity = vec->capacity == 0U ? 8U : vec->capacity;
    while (new_capacity < capacity) {
        if (new_capacity > SIZE_MAX / 2U) {
            new_capacity = capacity;
            break;
        }
        new_capacity *= 2U;
    }
    if (new_capacity > SIZE_MAX / vec->element_size) {
        return false;
    }
    allocation_size = new_capacity * vec->element_size;
    new_data = (unsigned char *)realloc(vec->data, allocation_size);
    if (new_data == NULL) {
        return false;
    }
    vec->data = new_data;
    vec->capacity = new_capacity;
    return true;
}

bool vec_push(Vec *vec, const void *element) {
    unsigned char *destination;
    size_t source_offset = 0U;
    bool source_is_internal = false;

    if (vec == NULL || element == NULL || vec->length == SIZE_MAX) {
        return false;
    }
    if (vec->data != NULL) {
        uintptr_t source_address = (uintptr_t)element;
        uintptr_t base_address = (uintptr_t)(void *)vec->data;
        size_t initialized_bytes = vec->length * vec->element_size;
        if (source_address >= base_address &&
            source_address - base_address < initialized_bytes) {
            source_offset = (size_t)(source_address - base_address);
            if (vec->element_size > initialized_bytes - source_offset) {
                return false;
            }
            source_is_internal = true;
        }
    }
    if (!vec_reserve(vec, vec->length + 1U)) {
        return false;
    }
    destination = vec->data + vec->length * vec->element_size;
    memmove(destination, source_is_internal ? vec->data + source_offset : element,
            vec->element_size);
    vec->length += 1U;
    return true;
}

bool vec_resize(Vec *vec, size_t length) {
    size_t old_length;

    if (vec == NULL) {
        return false;
    }
    if (length > vec->length && !vec_reserve(vec, length)) {
        return false;
    }
    old_length = vec->length;
    if (length > old_length) {
        size_t first_new_byte = old_length * vec->element_size;
        size_t added_bytes = (length - old_length) * vec->element_size;
        memset(vec->data + first_new_byte, 0, added_bytes);
    }
    vec->length = length;
    return true;
}

void *vec_at(Vec *vec, size_t index) {
    if (vec == NULL || index >= vec->length) {
        return NULL;
    }
    return vec->data + index * vec->element_size;
}

const void *vec_at_const(const Vec *vec, size_t index) {
    if (vec == NULL || index >= vec->length) {
        return NULL;
    }
    return vec->data + index * vec->element_size;
}

void vec_clear(Vec *vec) {
    if (vec != NULL) {
        vec->length = 0U;
    }
}

void vec_destroy(Vec *vec) {
    if (vec == NULL) {
        return;
    }
    free(vec->data);
    vec->data = NULL;
    vec->length = 0U;
    vec->capacity = 0U;
    vec->element_size = 0U;
}
