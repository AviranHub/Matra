#include "token.h"

#include <stdlib.h>

const char *token_kind_name(TokenKind kind) {
    static const char *const names[] = {
        "error", "eof", "newline", "identifier", "integer", "float",
        "string-begin", "string-text", "interpolation-begin",
        "interpolation-end", "string-end", "char", "let", "var", "fn",
        "class", "init", "interface", "type", "enum", "match", "if",
        "else", "for", "in", "while", "return", "break", "continue",
        "import", "as", "public", "private", "protected", "true", "false",
        "and", "or", "not", "parallel", "spawn", "await", "try", "self",
        "(", ")", "{", "}", "[", "]", ",", ":", ";", ".", "..", "..=",
        "@", "->", "=>", "...", "+", "-", "*", "/", "%", "**", "+=",
        "-=", "*=", "/=", "%=", "**=", "=", "==", "!=", "<", "<=",
        ">", ">=", "@", "|", "::"
    };
    size_t count = sizeof(names) / sizeof(names[0]);

    if ((size_t)kind >= count) {
        return "unknown-token";
    }
    return names[(size_t)kind];
}

void token_array_destroy(TokenArray *tokens) {
    if (tokens == NULL) {
        return;
    }
    free(tokens->items);
    tokens->items = NULL;
    tokens->length = 0U;
    tokens->capacity = 0U;
    tokens->source_name = NULL;
}
