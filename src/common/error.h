#ifndef MATRA_COMMON_ERROR_H
#define MATRA_COMMON_ERROR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    size_t start_byte;
    size_t end_byte;
    size_t line;
    size_t column;
} SourceSpan;

typedef enum {
    DIAG_NOTE,
    DIAG_WARNING,
    DIAG_ERROR
} DiagSeverity;

typedef enum {
    DIAG_CODE_NONE,
    DIAG_CODE_OUT_OF_MEMORY,
    DIAG_CODE_INVALID_ARGUMENT,
    DIAG_CODE_OVERFLOW,
    DIAG_CODE_IO,
    DIAG_CODE_LEXICAL,
    DIAG_CODE_PARSE,
    DIAG_CODE_NAME,
    DIAG_CODE_TYPE,
    DIAG_CODE_RUNTIME,
    DIAG_CODE_INTERNAL
} DiagCode;

typedef struct {
    DiagSeverity severity;
    DiagCode code;
    SourceSpan span;
    char *file;
    char *message;
    char *hint;
} Diagnostic;

typedef struct {
    Diagnostic *items;
    size_t length;
    size_t capacity;
} Diagnostics;

void diagnostics_init(Diagnostics *diagnostics);
bool diagnostics_add(Diagnostics *diagnostics, DiagSeverity severity,
                     DiagCode code, const char *file, SourceSpan span,
                     const char *message, const char *hint);
bool diagnostics_print(const Diagnostics *diagnostics, FILE *stream);
void diagnostics_clear(Diagnostics *diagnostics);
void diagnostics_destroy(Diagnostics *diagnostics);

#endif
