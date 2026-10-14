#ifndef MATRA_LEXER_TOKEN_H
#define MATRA_LEXER_TOKEN_H

#include "../common/error.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TOK_ERROR,
    TOK_EOF,
    TOK_NEWLINE,
    TOK_IDENTIFIER,
    TOK_INTEGER,
    TOK_FLOAT,
    TOK_STRING_BEGIN,
    TOK_STRING_TEXT,
    TOK_INTERP_BEGIN,
    TOK_INTERP_END,
    TOK_STRING_END,
    TOK_CHAR,
    TOK_LET,
    TOK_VAR,
    TOK_FN,
    TOK_CLASS,
    TOK_INIT,
    TOK_INTERFACE,
    TOK_TYPE,
    TOK_ENUM,
    TOK_MATCH,
    TOK_IF,
    TOK_ELSE,
    TOK_FOR,
    TOK_IN,
    TOK_WHILE,
    TOK_RETURN,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_IMPORT,
    TOK_AS,
    TOK_PUBLIC,
    TOK_PRIVATE,
    TOK_PROTECTED,
    TOK_TRUE,
    TOK_FALSE,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_PARALLEL,
    TOK_SPAWN,
    TOK_AWAIT,
    TOK_TRY,
    TOK_SELF,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_COMMA,
    TOK_COLON,
    TOK_SEMICOLON,
    TOK_DOT,
    TOK_DOT_DOT,
    TOK_DOT_DOT_EQUAL,
    TOK_AT,
    TOK_PIPE,
    TOK_ARROW,
    TOK_FAT_ARROW,
    TOK_ELLIPSIS,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_POWER,
    TOK_PLUS_EQUAL,
    TOK_MINUS_EQUAL,
    TOK_STAR_EQUAL,
    TOK_SLASH_EQUAL,
    TOK_PERCENT_EQUAL,
    TOK_POWER_EQUAL,
    TOK_EQUAL,
    TOK_EQUAL_EQUAL,
    TOK_BANG_EQUAL,
    TOK_LESS,
    TOK_LESS_EQUAL,
    TOK_GREATER,
    TOK_GREATER_EQUAL,
    TOK_DOUBLE_COLON
} TokenKind;

typedef struct {
    TokenKind kind;
    SourceSpan span;
    const char *lexeme;
    size_t length;
} Token;

typedef struct {
    Token *items;
    size_t length;
    size_t capacity;
    const char *source_name;
} TokenArray;

const char *token_kind_name(TokenKind kind);
void token_array_destroy(TokenArray *tokens);

#endif
