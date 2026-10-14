#ifndef MATRA_AST_AST_H
#define MATRA_AST_AST_H

#include "../common/arena.h"
#include "../lexer/token.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum {
    AST_PROGRAM,
    AST_IMPORT,
    AST_BINDING,
    AST_FUNCTION,
    AST_PARAMETER,
    AST_CLASS,
    AST_INTERFACE,
    AST_ENUM,
    AST_ENUM_CASE,
    AST_TYPE_ALIAS,
    AST_TYPE_NAME,
    AST_TYPE_APPLY,
    AST_TYPE_UNION,
    AST_TYPE_FUNCTION,
    AST_BLOCK,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_RETURN,
    AST_BREAK,
    AST_CONTINUE,
    AST_EXPRESSION_STATEMENT,
    AST_ATTRIBUTE,
    AST_PATTERN_NAME,
    AST_PATTERN_WILDCARD,
    AST_PATTERN_LITERAL,
    AST_PATTERN_VARIANT,
    AST_PATTERN_LIST,
    AST_PATTERN_TUPLE,
    AST_PATTERN_SPREAD,
    AST_PATTERN_RANGE,
    AST_PATTERN_OR,
    AST_ARGUMENT,
    AST_INTEGER,
    AST_FLOAT,
    AST_STRING,
    AST_INTERPOLATED_STRING,
    AST_CHAR,
    AST_BOOLEAN,
    AST_NAME,
    AST_LIST,
    AST_TUPLE,
    AST_UNARY,
    AST_BINARY,
    AST_ASSIGNMENT,
    AST_RANGE,
    AST_CALL,
    AST_MEMBER,
    AST_INDEX,
    AST_LAMBDA,
    AST_MATCH,
    AST_MATCH_ARM,
    AST_SPAWN,
    AST_AWAIT,
    AST_TRY,
    AST_PARALLEL,
    AST_KERNEL,
    AST_ERROR
} AstKind;

typedef struct AstNode AstNode;

typedef struct AstLink {
    AstNode *node;
    struct AstLink *next;
} AstLink;

typedef struct {
    AstLink *head;
    AstLink *tail;
    size_t length;
} AstList;

struct AstNode {
    AstKind kind;
    SourceSpan span;
    Token token;
    TokenKind op;
    AstNode *a;
    AstNode *b;
    AstNode *c;
    AstList items;
    AstList extra;
    bool flag;
    bool second_flag;
    bool is_method;
    bool is_expression_body;
    bool is_constructor;
};

typedef struct {
    Arena storage;
} AstArena;

void ast_arena_init(AstArena *arena, size_t block_size);
void ast_arena_destroy(AstArena *arena);
AstNode *ast_new(AstArena *arena, AstKind kind, SourceSpan span);
bool ast_list_append(AstArena *arena, AstList *list, AstNode *node);
bool ast_dump(FILE *stream, const AstNode *node);
const char *ast_kind_name(AstKind kind);

#endif
