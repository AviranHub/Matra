#ifndef MATRA_COMMON_STR_H
#define MATRA_COMMON_STR_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const char *data;
    size_t length;
} StrView;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StrBuf;

void str_init(StrBuf *str);
bool str_append(StrBuf *str, const char *bytes, size_t length);
bool str_append_cstr(StrBuf *str, const char *text);
void str_clear(StrBuf *str);
StrView str_view(const StrBuf *str);
const char *str_cstr(const StrBuf *str);
bool str_equal(StrView left, StrView right);
void str_destroy(StrBuf *str);

#endif
