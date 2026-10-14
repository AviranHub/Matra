#include "ast.h"

#include <stdlib.h>
#include <string.h>

void ast_arena_init(AstArena *arena, size_t block_size) {
    if (arena != NULL) {
        arena_init(&arena->storage, block_size);
    }
}

void ast_arena_destroy(AstArena *arena) {
    if (arena != NULL) {
        arena_destroy(&arena->storage);
    }
}

AstNode *ast_new(AstArena *arena, AstKind kind, SourceSpan span) {
    AstNode *node;

    if (arena == NULL) {
        return NULL;
    }
    node = (AstNode *)arena_calloc(&arena->storage, 1U, sizeof(*node),
                                  _Alignof(AstNode));
    if (node != NULL) {
        node->kind = kind;
        node->span = span;
    }
    return node;
}

bool ast_list_append(AstArena *arena, AstList *list, AstNode *node) {
    AstLink *link;

    if (arena == NULL || list == NULL || node == NULL ||
        list->length == SIZE_MAX) {
        return false;
    }
    link = (AstLink *)arena_alloc(&arena->storage, sizeof(*link),
                                  _Alignof(AstLink));
    if (link == NULL) {
        return false;
    }
    link->node = node;
    link->next = NULL;
    if (list->tail == NULL) {
        list->head = link;
    } else {
        list->tail->next = link;
    }
    list->tail = link;
    list->length += 1U;
    return true;
}

const char *ast_kind_name(AstKind kind) {
    static const char *const names[] = {
        "program", "import", "binding", "function", "parameter", "class",
        "interface", "enum", "enum-case", "type-alias", "type-name",
        "type-apply", "type-union", "type-function", "block", "if", "while",
        "for", "return", "break", "continue", "expression-statement",
        "attribute", "pattern-name", "pattern-wildcard", "pattern-literal",
        "pattern-variant", "pattern-list", "pattern-tuple", "pattern-spread",
        "pattern-range", "pattern-or", "argument", "integer", "float",
        "string", "interpolated-string", "char", "boolean", "name", "list",
        "tuple", "unary", "binary", "assignment", "range", "call", "member",
        "index", "lambda", "match", "match-arm", "spawn", "await", "try",
        "parallel", "kernel", "error"
    };
    size_t count = sizeof(names) / sizeof(names[0]);

    return (size_t)kind < count ? names[(size_t)kind] : "unknown-node";
}

static bool ast_dump_text(FILE *stream, const Token *token) {
    size_t index;

    if (token->length == 0U) {
        return true;
    }
    if (fputc('"', stream) == EOF) {
        return false;
    }
    for (index = 0U; index < token->length; ++index) {
        unsigned char byte = (unsigned char)token->lexeme[index];
        if (byte == '"' || byte == '\\') {
            if (fputc('\\', stream) == EOF || fputc((int)byte, stream) == EOF) {
                return false;
            }
        } else if (byte == '\n') {
            if (fputs("\\n", stream) == EOF) {
                return false;
            }
        } else if (byte == '\r') {
            if (fputs("\\r", stream) == EOF) {
                return false;
            }
        } else if (byte == '\t') {
            if (fputs("\\t", stream) == EOF) {
                return false;
            }
        } else if (byte < 0x20U) {
            if (fprintf(stream, "\\x%02x", (unsigned int)byte) < 0) {
                return false;
            }
        } else if (fputc((int)byte, stream) == EOF) {
            return false;
        }
    }
    return fputc('"', stream) != EOF;
}

static bool ast_dump_node(FILE *stream, const AstNode *node, size_t depth) {
    size_t index;
    const AstLink *link;

    if (node == NULL) {
        return fputs("null", stream) != EOF;
    }
    if (depth > 512U) {
        return fputs("<depth-limit>", stream) != EOF;
    }
    if (fprintf(stream, "(%s", ast_kind_name(node->kind)) < 0) {
        return false;
    }
    if (node->token.lexeme != NULL && node->token.length > 0U) {
        if (fputc(' ', stream) == EOF || !ast_dump_text(stream, &node->token)) {
            return false;
        }
    }
    if (node->op != TOK_ERROR && node->op != TOK_EOF) {
        if (fprintf(stream, " op=%s", token_kind_name(node->op)) < 0) {
            return false;
        }
    }
    if (node->flag && fputs(" flag=true", stream) == EOF) {
        return false;
    }
    if (node->second_flag && fputs(" flag2=true", stream) == EOF) {
        return false;
    }
    if (fprintf(stream, " @%zu:%zu", node->span.line, node->span.column) < 0) {
        return false;
    }
    if (node->a != NULL) {
        if (fputs(" a=", stream) == EOF ||
            !ast_dump_node(stream, node->a, depth + 1U)) {
            return false;
        }
    }
    if (node->b != NULL) {
        if (fputs(" b=", stream) == EOF ||
            !ast_dump_node(stream, node->b, depth + 1U)) {
            return false;
        }
    }
    if (node->c != NULL) {
        if (fputs(" c=", stream) == EOF ||
            !ast_dump_node(stream, node->c, depth + 1U)) {
            return false;
        }
    }
    for (link = node->items.head, index = 0U; link != NULL;
         link = link->next, ++index) {
        if (fprintf(stream, " item%zu=", index) < 0 ||
            !ast_dump_node(stream, link->node, depth + 1U)) {
            return false;
        }
    }
    for (link = node->extra.head, index = 0U; link != NULL;
         link = link->next, ++index) {
        if (fprintf(stream, " extra%zu=", index) < 0 ||
            !ast_dump_node(stream, link->node, depth + 1U)) {
            return false;
        }
    }
    return fputc(')', stream) != EOF;
}

bool ast_dump(FILE *stream, const AstNode *node) {
    return stream != NULL && ast_dump_node(stream, node, 0U) &&
           fputc('\n', stream) != EOF && ferror(stream) == 0;
}
