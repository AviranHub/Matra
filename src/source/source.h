#ifndef MATRA_SOURCE_SOURCE_H
#define MATRA_SOURCE_SOURCE_H

#include "../common/error.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *name;
    unsigned char *bytes;
    size_t length;
} SourceBuffer;

bool source_buffer_from_bytes(SourceBuffer *source, const char *name,
                              const unsigned char *bytes, size_t length,
                              Diagnostics *diagnostics);
bool source_buffer_load(SourceBuffer *source, const char *path,
                        Diagnostics *diagnostics);
void source_buffer_destroy(SourceBuffer *source);

#endif
