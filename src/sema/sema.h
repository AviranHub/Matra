#ifndef MATRA_SEMA_SEMA_H
#define MATRA_SEMA_SEMA_H

#include "../ast/ast.h"

typedef enum {
    SEMA_TYPE_ERROR,
    SEMA_TYPE_UNKNOWN,
    SEMA_TYPE_UNIT,
    SEMA_TYPE_BOOL,
    SEMA_TYPE_INT,
    SEMA_TYPE_FLOAT,
    SEMA_TYPE_STRING,
    SEMA_TYPE_CHAR,
    SEMA_TYPE_FUNCTION,
    SEMA_TYPE_COLLECTION,
    SEMA_TYPE_NOMINAL,
    SEMA_TYPE_ADT,
    SEMA_TYPE_GENERIC,
    SEMA_TYPE_UNION
} SemaTypeKind;

typedef enum {
    SEMA_ESCAPE_UNKNOWN,
    SEMA_ESCAPE_NON_ESCAPING,
    SEMA_ESCAPE_ESCAPES
} SemaEscape;

typedef struct {
    SemaTypeKind kind;
    const char *name;
} SemaType;

typedef struct {
    void *implementation;
} SemaContext;

void sema_context_init(SemaContext *context);
void sema_context_destroy(SemaContext *context);
bool sema_analyze(const AstNode *program, SemaContext *context,
                  Diagnostics *diagnostics);
const SemaType *sema_type_of(const SemaContext *context, const AstNode *node);
SemaEscape sema_escape_of(const SemaContext *context, const AstNode *node);
const char *sema_type_name(const SemaType *type);

#endif
