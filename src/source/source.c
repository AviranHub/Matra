#include "source.h"

#include "../common/str.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *source_copy_string(const char *text) {
    size_t length = strlen(text);
    char *copy;

    if (length == SIZE_MAX) {
        return NULL;
    }
    copy = (char *)malloc(length + 1U);
    if (copy != NULL) {
        memcpy(copy, text, length + 1U);
    }
    return copy;
}

static void source_report(Diagnostics *diagnostics, DiagCode code,
                          const char *path, const char *message) {
    SourceSpan span = { 0U, 0U, 1U, 1U };
    if (diagnostics != NULL) {
        (void)diagnostics_add(diagnostics, DIAG_ERROR, code, path, span,
                              message, NULL);
    }
}

bool source_buffer_from_bytes(SourceBuffer *source, const char *name,
                              const unsigned char *bytes, size_t length,
                              Diagnostics *diagnostics) {
    const char *source_name = name == NULL ? "<memory>" : name;
    char *name_copy;
    unsigned char *byte_copy;

    if (source == NULL || (length > 0U && bytes == NULL) ||
        length == SIZE_MAX) {
        source_report(diagnostics, DIAG_CODE_INVALID_ARGUMENT, source_name,
                      "invalid source buffer arguments");
        return false;
    }
    name_copy = source_copy_string(source_name);
    byte_copy = (unsigned char *)malloc(length + 1U);
    if (name_copy == NULL || byte_copy == NULL) {
        free(name_copy);
        free(byte_copy);
        source_report(diagnostics, DIAG_CODE_OUT_OF_MEMORY, source_name,
                      "unable to allocate source buffer");
        return false;
    }
    if (length > 0U) {
        memcpy(byte_copy, bytes, length);
    }
    byte_copy[length] = 0U;
    source->name = name_copy;
    source->bytes = byte_copy;
    source->length = length;
    return true;
}

bool source_buffer_load(SourceBuffer *source, const char *path,
                        Diagnostics *diagnostics) {
    FILE *file;
    StrBuf contents;
    unsigned char chunk[4096];
    bool success = true;

    if (source == NULL || path == NULL) {
        source_report(diagnostics, DIAG_CODE_INVALID_ARGUMENT, path,
                      "source path is required");
        return false;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        source_report(diagnostics, DIAG_CODE_IO, path, "unable to open source file");
        return false;
    }
    str_init(&contents);
    for (;;) {
        size_t bytes_read = fread(chunk, 1U, sizeof(chunk), file);
        if (bytes_read > 0U &&
            !str_append(&contents, (const char *)chunk, bytes_read)) {
            source_report(diagnostics, DIAG_CODE_OUT_OF_MEMORY, path,
                          "unable to read source file into memory");
            success = false;
            break;
        }
        if (bytes_read < sizeof(chunk)) {
            if (ferror(file) != 0) {
                source_report(diagnostics, DIAG_CODE_IO, path,
                              "error while reading source file");
                success = false;
            }
            break;
        }
    }
    if (fclose(file) != 0 && success) {
        source_report(diagnostics, DIAG_CODE_IO, path,
                      "error while closing source file");
        success = false;
    }
    if (success) {
        success = source_buffer_from_bytes(
            source, path, (const unsigned char *)str_cstr(&contents),
            contents.length, diagnostics);
    }
    str_destroy(&contents);
    return success;
}

void source_buffer_destroy(SourceBuffer *source) {
    if (source == NULL) {
        return;
    }
    free(source->name);
    free(source->bytes);
    source->name = NULL;
    source->bytes = NULL;
    source->length = 0U;
}
