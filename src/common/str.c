#include "str.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool str_reserve(StrBuf *str, size_t required_capacity) {
    size_t new_capacity;
    char *new_data;

    if (required_capacity <= str->capacity) {
        return true;
    }
    new_capacity = str->capacity == 0U ? 16U : str->capacity;
    while (new_capacity < required_capacity) {
        if (new_capacity > SIZE_MAX / 2U) {
            new_capacity = required_capacity;
            break;
        }
        new_capacity *= 2U;
    }
    new_data = (char *)realloc(str->data, new_capacity);
    if (new_data == NULL) {
        return false;
    }
    str->data = new_data;
    str->capacity = new_capacity;
    if (str->length == 0U) {
        str->data[0] = '\0';
    }
    return true;
}

void str_init(StrBuf *str) {
    if (str == NULL) {
        return;
    }
    str->data = NULL;
    str->length = 0U;
    str->capacity = 0U;
}

bool str_append(StrBuf *str, const char *bytes, size_t length) {
    size_t required_capacity;
    size_t source_offset = 0U;
    bool source_is_internal = false;
    char *source;

    if (str == NULL || (length > 0U && bytes == NULL) ||
        str->length == SIZE_MAX || length > SIZE_MAX - str->length - 1U) {
        return false;
    }
    if (length == 0U) {
        if (str->data != NULL) {
            str->data[str->length] = '\0';
        }
        return true;
    }
    if (str->data != NULL) {
        uintptr_t source_address = (uintptr_t)(const void *)bytes;
        uintptr_t base_address = (uintptr_t)(void *)str->data;
        if (source_address >= base_address &&
            source_address - base_address < str->capacity) {
            source_offset = (size_t)(source_address - base_address);
            if (length > str->capacity - source_offset) {
                return false;
            }
            source_is_internal = true;
        }
    }
    required_capacity = str->length + length + 1U;
    if (!str_reserve(str, required_capacity)) {
        return false;
    }
    source = source_is_internal ? str->data + source_offset : (char *)(void *)bytes;
    memmove(str->data + str->length, source, length);
    str->length += length;
    str->data[str->length] = '\0';
    return true;
}

bool str_append_cstr(StrBuf *str, const char *text) {
    if (text == NULL) {
        return false;
    }
    return str_append(str, text, strlen(text));
}

void str_clear(StrBuf *str) {
    if (str == NULL) {
        return;
    }
    str->length = 0U;
    if (str->data != NULL) {
        str->data[0] = '\0';
    }
}

StrView str_view(const StrBuf *str) {
    static const char empty[] = "";
    StrView view;

    if (str == NULL) {
        view.data = empty;
        view.length = 0U;
        return view;
    }
    view.data = str->data == NULL ? empty : str->data;
    view.length = str->length;
    return view;
}

const char *str_cstr(const StrBuf *str) {
    static const char empty[] = "";
    return str == NULL || str->data == NULL ? empty : str->data;
}

bool str_equal(StrView left, StrView right) {
    if (left.length != right.length) {
        return false;
    }
    if (left.length == 0U) {
        return true;
    }
    return left.data != NULL && right.data != NULL &&
           memcmp(left.data, right.data, left.length) == 0;
}

void str_destroy(StrBuf *str) {
    if (str == NULL) {
        return;
    }
    free(str->data);
    str->data = NULL;
    str->length = 0U;
    str->capacity = 0U;
}
