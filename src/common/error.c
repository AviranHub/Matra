#include "error.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char *error_copy_string(const char *text) {
    size_t length;
    char *copy;

    if (text == NULL) {
        return NULL;
    }
    length = strlen(text);
    if (length == SIZE_MAX) {
        return NULL;
    }
    copy = (char *)malloc(length + 1U);
    if (copy != NULL) {
        memcpy(copy, text, length + 1U);
    }
    return copy;
}

static const char *error_severity_name(DiagSeverity severity) {
    switch (severity) {
        case DIAG_NOTE:
            return "note";
        case DIAG_WARNING:
            return "warning";
        case DIAG_ERROR:
            return "error";
    }
    return "unknown";
}

static const char *error_code_name(DiagCode code) {
    switch (code) {
        case DIAG_CODE_NONE:
            return "E0000";
        case DIAG_CODE_OUT_OF_MEMORY:
            return "E0001";
        case DIAG_CODE_INVALID_ARGUMENT:
            return "E0002";
        case DIAG_CODE_OVERFLOW:
            return "E0003";
        case DIAG_CODE_IO:
            return "E0004";
        case DIAG_CODE_LEXICAL:
            return "E0100";
        case DIAG_CODE_PARSE:
            return "E0200";
        case DIAG_CODE_NAME:
            return "E0300";
        case DIAG_CODE_TYPE:
            return "E0400";
        case DIAG_CODE_RUNTIME:
            return "E0500";
        case DIAG_CODE_INTERNAL:
            return "E9000";
    }
    return "E9999";
}

static bool diagnostics_reserve(Diagnostics *diagnostics, size_t capacity) {
    size_t new_capacity;
    size_t allocation_size;
    Diagnostic *new_items;

    if (capacity <= diagnostics->capacity) {
        return true;
    }
    new_capacity = diagnostics->capacity == 0U ? 8U : diagnostics->capacity;
    while (new_capacity < capacity) {
        if (new_capacity > SIZE_MAX / 2U) {
            new_capacity = capacity;
            break;
        }
        new_capacity *= 2U;
    }
    if (new_capacity > SIZE_MAX / sizeof(*new_items)) {
        return false;
    }
    allocation_size = new_capacity * sizeof(*new_items);
    new_items = (Diagnostic *)realloc(diagnostics->items, allocation_size);
    if (new_items == NULL) {
        return false;
    }
    diagnostics->items = new_items;
    diagnostics->capacity = new_capacity;
    return true;
}

void diagnostics_init(Diagnostics *diagnostics) {
    if (diagnostics == NULL) {
        return;
    }
    diagnostics->items = NULL;
    diagnostics->length = 0U;
    diagnostics->capacity = 0U;
}

bool diagnostics_add(Diagnostics *diagnostics, DiagSeverity severity,
                     DiagCode code, const char *file, SourceSpan span,
                     const char *message, const char *hint) {
    Diagnostic item;
    const char *effective_file = file == NULL ? "<unknown>" : file;

    if (diagnostics == NULL || message == NULL ||
        severity < DIAG_NOTE || severity > DIAG_ERROR ||
        span.start_byte > span.end_byte || diagnostics->length == SIZE_MAX) {
        return false;
    }
    item.severity = severity;
    item.code = code;
    item.span = span;
    item.file = error_copy_string(effective_file);
    item.message = error_copy_string(message);
    item.hint = hint == NULL ? NULL : error_copy_string(hint);
    if (item.file == NULL || item.message == NULL ||
        (hint != NULL && item.hint == NULL)) {
        free(item.file);
        free(item.message);
        free(item.hint);
        return false;
    }
    if (!diagnostics_reserve(diagnostics, diagnostics->length + 1U)) {
        free(item.file);
        free(item.message);
        free(item.hint);
        return false;
    }
    diagnostics->items[diagnostics->length] = item;
    diagnostics->length += 1U;
    return true;
}

bool diagnostics_print(const Diagnostics *diagnostics, FILE *stream) {
    size_t index;

    if (diagnostics == NULL || stream == NULL) {
        return false;
    }
    for (index = 0U; index < diagnostics->length; ++index) {
        const Diagnostic *item = &diagnostics->items[index];
        if (fprintf(stream, "%s:%zu:%zu: %s [%s]: %s\n", item->file,
                    item->span.line, item->span.column,
                    error_severity_name(item->severity),
                    error_code_name(item->code), item->message) < 0) {
            return false;
        }
        if (item->hint != NULL &&
            fprintf(stream, "  hint: %s\n", item->hint) < 0) {
            return false;
        }
    }
    return ferror(stream) == 0;
}

void diagnostics_clear(Diagnostics *diagnostics) {
    size_t index;

    if (diagnostics == NULL) {
        return;
    }
    for (index = 0U; index < diagnostics->length; ++index) {
        free(diagnostics->items[index].file);
        free(diagnostics->items[index].message);
        free(diagnostics->items[index].hint);
    }
    diagnostics->length = 0U;
}

void diagnostics_destroy(Diagnostics *diagnostics) {
    if (diagnostics == NULL) {
        return;
    }
    diagnostics_clear(diagnostics);
    free(diagnostics->items);
    diagnostics->items = NULL;
    diagnostics->capacity = 0U;
}
