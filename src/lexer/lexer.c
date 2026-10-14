#include "lexer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const SourceBuffer *source;
    TokenArray *tokens;
    Diagnostics *diagnostics;
    size_t offset;
    size_t line;
    size_t column;
    size_t nesting;
    bool failed;
} Lexer;

typedef struct {
    const char *name;
    TokenKind kind;
} Keyword;

static const Keyword lexer_keywords[] = {
    { "let", TOK_LET }, { "var", TOK_VAR }, { "fn", TOK_FN },
    { "class", TOK_CLASS }, { "init", TOK_INIT },
    { "interface", TOK_INTERFACE }, { "type", TOK_TYPE },
    { "enum", TOK_ENUM }, { "match", TOK_MATCH }, { "if", TOK_IF },
    { "else", TOK_ELSE }, { "for", TOK_FOR }, { "in", TOK_IN },
    { "while", TOK_WHILE }, { "return", TOK_RETURN },
    { "break", TOK_BREAK }, { "continue", TOK_CONTINUE },
    { "import", TOK_IMPORT }, { "as", TOK_AS },
    { "public", TOK_PUBLIC }, { "private", TOK_PRIVATE },
    { "protected", TOK_PROTECTED }, { "true", TOK_TRUE },
    { "false", TOK_FALSE }, { "and", TOK_AND }, { "or", TOK_OR },
    { "not", TOK_NOT }, { "parallel", TOK_PARALLEL },
    { "spawn", TOK_SPAWN }, { "await", TOK_AWAIT }, { "try", TOK_TRY },
    { "self", TOK_SELF }
};

static bool lexer_add_token(Lexer *lexer, TokenKind kind, size_t start,
                            size_t start_line, size_t start_column) {
    Token *new_items;
    size_t new_capacity;
    Token token;

    if (lexer->tokens->length == lexer->tokens->capacity) {
        new_capacity = lexer->tokens->capacity == 0U
                           ? 64U
                           : lexer->tokens->capacity * 2U;
        if (new_capacity < lexer->tokens->capacity ||
            new_capacity > SIZE_MAX / sizeof(*new_items)) {
            lexer->failed = true;
            return false;
        }
        new_items = (Token *)realloc(lexer->tokens->items,
                                     new_capacity * sizeof(*new_items));
        if (new_items == NULL) {
            lexer->failed = true;
            return false;
        }
        lexer->tokens->items = new_items;
        lexer->tokens->capacity = new_capacity;
    }
    token.kind = kind;
    token.span.start_byte = start;
    token.span.end_byte = lexer->offset;
    token.span.line = start_line;
    token.span.column = start_column;
    token.lexeme = (const char *)lexer->source->bytes + start;
    token.length = lexer->offset - start;
    lexer->tokens->items[lexer->tokens->length] = token;
    lexer->tokens->length += 1U;
    return true;
}

static void lexer_report_at(Lexer *lexer, DiagCode code, size_t start,
                            size_t end, size_t line, size_t column,
                            const char *message) {
    SourceSpan span;

    span.start_byte = start;
    span.end_byte = end;
    span.line = line;
    span.column = column;
    if (lexer->diagnostics != NULL &&
        !diagnostics_add(lexer->diagnostics, DIAG_ERROR, code,
                         lexer->source->name, span, message, NULL)) {
        lexer->failed = true;
    }
}

static bool lexer_is_digit(unsigned char value) {
    return value >= (unsigned char)'0' && value <= (unsigned char)'9';
}

static bool lexer_is_alpha(unsigned char value) {
    return (value >= (unsigned char)'a' && value <= (unsigned char)'z') ||
           (value >= (unsigned char)'A' && value <= (unsigned char)'Z') ||
           value == (unsigned char)'_';
}

static bool lexer_is_alnum(unsigned char value) {
    return lexer_is_alpha(value) || lexer_is_digit(value);
}

static unsigned char lexer_peek(const Lexer *lexer, size_t lookahead) {
    size_t position;

    if (lexer->offset > lexer->source->length) {
        return 0U;
    }
    if (lookahead > lexer->source->length - lexer->offset) {
        return 0U;
    }
    position = lexer->offset + lookahead;
    return position < lexer->source->length ? lexer->source->bytes[position]
                                            : 0U;
}

static size_t lexer_utf8_width(const unsigned char *bytes, size_t remaining) {
    unsigned char first;

    if (remaining == 0U) {
        return 0U;
    }
    first = bytes[0];
    if (first < 0x80U) {
        return 1U;
    }
    if (first >= 0xC2U && first <= 0xDFU && remaining >= 2U &&
        (bytes[1] & 0xC0U) == 0x80U) {
        return 2U;
    }
    if (first >= 0xE0U && first <= 0xEFU && remaining >= 3U &&
        (bytes[1] & 0xC0U) == 0x80U && (bytes[2] & 0xC0U) == 0x80U &&
        !(first == 0xE0U && bytes[1] < 0xA0U) &&
        !(first == 0xEDU && bytes[1] >= 0xA0U)) {
        return 3U;
    }
    if (first >= 0xF0U && first <= 0xF4U && remaining >= 4U &&
        (bytes[1] & 0xC0U) == 0x80U && (bytes[2] & 0xC0U) == 0x80U &&
        (bytes[3] & 0xC0U) == 0x80U &&
        !(first == 0xF0U && bytes[1] < 0x90U) &&
        !(first == 0xF4U && bytes[1] >= 0x90U)) {
        return 4U;
    }
    return 0U;
}

static void lexer_advance_newline(Lexer *lexer) {
    if (lexer_peek(lexer, 0U) == (unsigned char)'\r' &&
        lexer_peek(lexer, 1U) == (unsigned char)'\n') {
        lexer->offset += 2U;
    } else {
        lexer->offset += 1U;
    }
    lexer->line += 1U;
    lexer->column = 1U;
}

static bool lexer_advance_scalar(Lexer *lexer) {
    size_t width = lexer_utf8_width(lexer->source->bytes + lexer->offset,
                                    lexer->source->length - lexer->offset);
    if (width == 0U) {
        size_t start = lexer->offset;
        size_t line = lexer->line;
        size_t column = lexer->column;
        lexer->offset += 1U;
        lexer->column += 1U;
        lexer_report_at(lexer, DIAG_CODE_LEXICAL, start, lexer->offset, line,
                        column, "invalid UTF-8 sequence");
        return false;
    }
    lexer->offset += width;
    lexer->column += 1U;
    return true;
}

static TokenKind lexer_keyword_kind(const unsigned char *text, size_t length) {
    size_t index;
    for (index = 0U;
         index < sizeof(lexer_keywords) / sizeof(lexer_keywords[0]); ++index) {
        size_t keyword_length = strlen(lexer_keywords[index].name);
        if (length == keyword_length &&
            memcmp(text, lexer_keywords[index].name, length) == 0) {
            return lexer_keywords[index].kind;
        }
    }
    return TOK_IDENTIFIER;
}

static bool lexer_scan_code(Lexer *lexer, bool interpolation,
                            size_t brace_depth);

static bool lexer_scan_string(Lexer *lexer) {
    size_t quote_start = lexer->offset;
    size_t quote_line = lexer->line;
    size_t quote_column = lexer->column;
    size_t text_start;
    size_t text_line;
    size_t text_column;

    lexer->offset += 1U;
    lexer->column += 1U;
    if (!lexer_add_token(lexer, TOK_STRING_BEGIN, quote_start, quote_line,
                         quote_column)) {
        return false;
    }
    text_start = lexer->offset;
    text_line = lexer->line;
    text_column = lexer->column;
    while (lexer->offset < lexer->source->length) {
        unsigned char current = lexer_peek(lexer, 0U);
        if (current == (unsigned char)'"') {
            if (lexer->offset > text_start &&
                !lexer_add_token(lexer, TOK_STRING_TEXT, text_start, text_line,
                                 text_column)) {
                return false;
            }
            {
                size_t end_start = lexer->offset;
                size_t end_line = lexer->line;
                size_t end_column = lexer->column;
                lexer->offset += 1U;
                lexer->column += 1U;
                return lexer_add_token(lexer, TOK_STRING_END, end_start,
                                       end_line, end_column);
            }
        }
        if (current == (unsigned char)'\n' || current == (unsigned char)'\r') {
            lexer_report_at(lexer, DIAG_CODE_LEXICAL, quote_start,
                            lexer->offset, quote_line, quote_column,
                            "newline in string literal");
            return lexer_add_token(lexer, TOK_ERROR, quote_start, quote_line,
                                   quote_column);
        }
        if (current == (unsigned char)'{' &&
            lexer_peek(lexer, 1U) == (unsigned char)'{') {
            lexer->offset += 2U;
            lexer->column += 2U;
            continue;
        }
        if (current == (unsigned char)'}' &&
            lexer_peek(lexer, 1U) == (unsigned char)'}') {
            lexer->offset += 2U;
            lexer->column += 2U;
            continue;
        }
        if (current == (unsigned char)'}') {
            size_t start = lexer->offset;
            size_t line = lexer->line;
            size_t column = lexer->column;
            lexer->offset += 1U;
            lexer->column += 1U;
            lexer_report_at(lexer, DIAG_CODE_LEXICAL, start, lexer->offset,
                            line, column, "unmatched closing interpolation brace");
            continue;
        }
        if (current == (unsigned char)'{') {
            size_t interpolation_start = lexer->offset;
            size_t interpolation_line = lexer->line;
            size_t interpolation_column = lexer->column;
            if (lexer->offset > text_start &&
                !lexer_add_token(lexer, TOK_STRING_TEXT, text_start, text_line,
                                 text_column)) {
                return false;
            }
            lexer->offset += 1U;
            lexer->column += 1U;
            if (!lexer_add_token(lexer, TOK_INTERP_BEGIN, interpolation_start,
                                 interpolation_line, interpolation_column)) {
                return false;
            }
            if (lexer->nesting >= 256U) {
                lexer_report_at(lexer, DIAG_CODE_LEXICAL, interpolation_start,
                                lexer->offset, interpolation_line,
                                interpolation_column,
                                "string interpolation nesting limit exceeded");
                return lexer_add_token(lexer, TOK_ERROR, interpolation_start,
                                       interpolation_line,
                                       interpolation_column);
            }
            lexer->nesting += 1U;
            if (!lexer_scan_code(lexer, true, 0U)) {
                return false;
            }
            lexer->nesting -= 1U;
            text_start = lexer->offset;
            text_line = lexer->line;
            text_column = lexer->column;
            continue;
        }
        if (current == (unsigned char)'\\') {
            unsigned char escaped = lexer_peek(lexer, 1U);
            bool valid_escape = escaped == (unsigned char)'\\' ||
                                escaped == (unsigned char)'"' ||
                                escaped == (unsigned char)'n' ||
                                escaped == (unsigned char)'r' ||
                                escaped == (unsigned char)'t' ||
                                escaped == (unsigned char)'0';
            if (!valid_escape) {
                size_t start = lexer->offset;
                size_t line = lexer->line;
                size_t column = lexer->column;
                size_t width = lexer_peek(lexer, 1U) == 0U ? 1U : 2U;
                lexer->offset += width;
                lexer->column += width;
                lexer_report_at(lexer, DIAG_CODE_LEXICAL, start, lexer->offset,
                                line, column, "unsupported string escape");
                continue;
            }
            lexer->offset += 2U;
            lexer->column += 2U;
            continue;
        }
        if (current < 0x80U) {
            lexer->offset += 1U;
            lexer->column += 1U;
        } else {
            (void)lexer_advance_scalar(lexer);
        }
    }
    lexer_report_at(lexer, DIAG_CODE_LEXICAL, quote_start,
                    lexer->source->length, quote_line, quote_column,
                    "unterminated string literal");
    return lexer_add_token(lexer, TOK_ERROR, quote_start, quote_line,
                           quote_column);
}

static bool lexer_scan_char(Lexer *lexer) {
    size_t start = lexer->offset;
    size_t line = lexer->line;
    size_t column = lexer->column;
    size_t scalar_count = 0U;
    bool valid = true;

    lexer->offset += 1U;
    lexer->column += 1U;
    while (lexer->offset < lexer->source->length &&
           lexer_peek(lexer, 0U) != (unsigned char)'\'' &&
           lexer_peek(lexer, 0U) != (unsigned char)'\n' &&
           lexer_peek(lexer, 0U) != (unsigned char)'\r') {
        if (lexer_peek(lexer, 0U) == (unsigned char)'\\') {
            unsigned char escaped = lexer_peek(lexer, 1U);
            if (escaped == 0U) {
                valid = false;
                lexer->offset += 1U;
                lexer->column += 1U;
                break;
            }
            if (escaped != (unsigned char)'\\' &&
                escaped != (unsigned char)'\'' &&
                escaped != (unsigned char)'n' &&
                escaped != (unsigned char)'r' &&
                escaped != (unsigned char)'t' &&
                escaped != (unsigned char)'0') {
                valid = false;
            }
            lexer->offset += 2U;
            lexer->column += 2U;
        } else if (lexer_peek(lexer, 0U) < 0x80U) {
            lexer->offset += 1U;
            lexer->column += 1U;
        } else if (!lexer_advance_scalar(lexer)) {
            valid = false;
        }
        scalar_count += 1U;
    }
    if (lexer_peek(lexer, 0U) == (unsigned char)'\'') {
        lexer->offset += 1U;
        lexer->column += 1U;
    } else {
        valid = false;
    }
    if (scalar_count != 1U) {
        valid = false;
    }
    if (!valid) {
        lexer_report_at(lexer, DIAG_CODE_LEXICAL, start, lexer->offset, line,
                        column, "character literal must contain one valid scalar");
    }
    return lexer_add_token(lexer, valid ? TOK_CHAR : TOK_ERROR, start, line,
                           column);
}

static bool lexer_scan_number(Lexer *lexer) {
    size_t start = lexer->offset;
    size_t line = lexer->line;
    size_t column = lexer->column;
    bool is_float = false;

    while (lexer_is_digit(lexer_peek(lexer, 0U))) {
        lexer->offset += 1U;
        lexer->column += 1U;
    }
    if (lexer_peek(lexer, 0U) == (unsigned char)'.' &&
        lexer_peek(lexer, 1U) != (unsigned char)'.' &&
        lexer_is_digit(lexer_peek(lexer, 1U))) {
        is_float = true;
        lexer->offset += 1U;
        lexer->column += 1U;
        while (lexer_is_digit(lexer_peek(lexer, 0U))) {
            lexer->offset += 1U;
            lexer->column += 1U;
        }
    }
    if (lexer_peek(lexer, 0U) == (unsigned char)'e' ||
        lexer_peek(lexer, 0U) == (unsigned char)'E') {
        size_t exponent_start = lexer->offset;
        lexer->offset += 1U;
        lexer->column += 1U;
        if (lexer_peek(lexer, 0U) == (unsigned char)'+' ||
            lexer_peek(lexer, 0U) == (unsigned char)'-') {
            lexer->offset += 1U;
            lexer->column += 1U;
        }
        if (!lexer_is_digit(lexer_peek(lexer, 0U))) {
            lexer_report_at(lexer, DIAG_CODE_LEXICAL, exponent_start,
                            lexer->offset, line, column,
                            "floating exponent requires at least one digit");
            return lexer_add_token(lexer, TOK_ERROR, start, line, column);
        }
        is_float = true;
        while (lexer_is_digit(lexer_peek(lexer, 0U))) {
            lexer->offset += 1U;
            lexer->column += 1U;
        }
    }
    return lexer_add_token(lexer, is_float ? TOK_FLOAT : TOK_INTEGER, start,
                           line, column);
}

static bool lexer_scan_identifier(Lexer *lexer) {
    size_t start = lexer->offset;
    size_t line = lexer->line;
    size_t column = lexer->column;
    TokenKind kind;

    while (lexer_is_alnum(lexer_peek(lexer, 0U))) {
        lexer->offset += 1U;
        lexer->column += 1U;
    }
    kind = lexer_keyword_kind(lexer->source->bytes + start,
                              lexer->offset - start);
    return lexer_add_token(lexer, kind, start, line, column);
}

static bool lexer_scan_code(Lexer *lexer, bool interpolation,
                            size_t brace_depth) {
    while (lexer->offset < lexer->source->length && !lexer->failed) {
        unsigned char current = lexer_peek(lexer, 0U);
        size_t start = lexer->offset;
        size_t line = lexer->line;
        size_t column = lexer->column;
        TokenKind kind = TOK_ERROR;
        size_t token_width = 1U;

        if (current == (unsigned char)' ' || current == (unsigned char)'\t' ||
            current == (unsigned char)'\v' || current == (unsigned char)'\f') {
            lexer->offset += 1U;
            lexer->column += 1U;
            continue;
        }
        if (current == (unsigned char)'\n' || current == (unsigned char)'\r') {
            lexer_advance_newline(lexer);
            if (!lexer_add_token(lexer, TOK_NEWLINE, start, line, column)) {
                return false;
            }
            continue;
        }
        if (current == (unsigned char)'/' &&
            lexer_peek(lexer, 1U) == (unsigned char)'/') {
            lexer->offset += 2U;
            lexer->column += 2U;
            while (lexer->offset < lexer->source->length &&
                   lexer_peek(lexer, 0U) != (unsigned char)'\n' &&
                   lexer_peek(lexer, 0U) != (unsigned char)'\r') {
                if (lexer_peek(lexer, 0U) < 0x80U) {
                    lexer->offset += 1U;
                    lexer->column += 1U;
                } else {
                    (void)lexer_advance_scalar(lexer);
                }
            }
            continue;
        }
        if (interpolation && current == (unsigned char)'}') {
            if (brace_depth == 0U) {
                lexer->offset += 1U;
                lexer->column += 1U;
                return lexer_add_token(lexer, TOK_INTERP_END, start, line,
                                       column);
            }
            brace_depth -= 1U;
            kind = TOK_RBRACE;
        } else if (current == (unsigned char)'{') {
            if (interpolation) {
                brace_depth += 1U;
            }
            kind = TOK_LBRACE;
        } else if (current == (unsigned char)'"') {
            if (!lexer_scan_string(lexer)) {
                return false;
            }
            continue;
        } else if (current == (unsigned char)'\'') {
            if (!lexer_scan_char(lexer)) {
                return false;
            }
            continue;
        } else if (lexer_is_digit(current)) {
            if (!lexer_scan_number(lexer)) {
                return false;
            }
            continue;
        } else if (lexer_is_alpha(current)) {
            if (!lexer_scan_identifier(lexer)) {
                return false;
            }
            continue;
        } else {
            switch (current) {
                case '(':
                    kind = TOK_LPAREN;
                    break;
                case ')':
                    kind = TOK_RPAREN;
                    break;
                case '}':
                    kind = TOK_RBRACE;
                    break;
                case '[':
                    kind = TOK_LBRACKET;
                    break;
                case ']':
                    kind = TOK_RBRACKET;
                    break;
                case ',':
                    kind = TOK_COMMA;
                    break;
                case ':':
                    kind = lexer_peek(lexer, 1U) == (unsigned char)':'
                               ? TOK_DOUBLE_COLON
                               : TOK_COLON;
                    token_width = kind == TOK_DOUBLE_COLON ? 2U : 1U;
                    break;
                case ';':
                    kind = TOK_SEMICOLON;
                    break;
                case '.':
                    if (lexer_peek(lexer, 1U) == (unsigned char)'.' &&
                        lexer_peek(lexer, 2U) == (unsigned char)'=') {
                        kind = TOK_DOT_DOT_EQUAL;
                        token_width = 3U;
                    } else if (lexer_peek(lexer, 1U) == (unsigned char)'.' &&
                               lexer_peek(lexer, 2U) == (unsigned char)'.') {
                        kind = TOK_ELLIPSIS;
                        token_width = 3U;
                    } else if (lexer_peek(lexer, 1U) == (unsigned char)'.') {
                        kind = TOK_DOT_DOT;
                        token_width = 2U;
                    } else {
                        kind = TOK_DOT;
                    }
                    break;
                case '@':
                    kind = TOK_AT;
                    break;
                case '|':
                    kind = TOK_PIPE;
                    break;
                case '+':
                    kind = lexer_peek(lexer, 1U) == (unsigned char)'='
                               ? TOK_PLUS_EQUAL
                               : TOK_PLUS;
                    token_width = kind == TOK_PLUS_EQUAL ? 2U : 1U;
                    break;
                case '-':
                    if (lexer_peek(lexer, 1U) == (unsigned char)'>') {
                        kind = TOK_ARROW;
                        token_width = 2U;
                    } else if (lexer_peek(lexer, 1U) == (unsigned char)'=') {
                        kind = TOK_MINUS_EQUAL;
                        token_width = 2U;
                    } else {
                        kind = TOK_MINUS;
                    }
                    break;
                case '*':
                    if (lexer_peek(lexer, 1U) == (unsigned char)'*' &&
                        lexer_peek(lexer, 2U) == (unsigned char)'=') {
                        kind = TOK_POWER_EQUAL;
                        token_width = 3U;
                    } else if (lexer_peek(lexer, 1U) == (unsigned char)'*') {
                        kind = TOK_POWER;
                        token_width = 2U;
                    } else if (lexer_peek(lexer, 1U) == (unsigned char)'=') {
                        kind = TOK_STAR_EQUAL;
                        token_width = 2U;
                    } else {
                        kind = TOK_STAR;
                    }
                    break;
                case '/':
                    kind = lexer_peek(lexer, 1U) == (unsigned char)'='
                               ? TOK_SLASH_EQUAL
                               : TOK_SLASH;
                    token_width = kind == TOK_SLASH_EQUAL ? 2U : 1U;
                    break;
                case '%':
                    kind = lexer_peek(lexer, 1U) == (unsigned char)'='
                               ? TOK_PERCENT_EQUAL
                               : TOK_PERCENT;
                    token_width = kind == TOK_PERCENT_EQUAL ? 2U : 1U;
                    break;
                case '=':
                    if (lexer_peek(lexer, 1U) == (unsigned char)'=') {
                        kind = TOK_EQUAL_EQUAL;
                        token_width = 2U;
                    } else if (lexer_peek(lexer, 1U) == (unsigned char)'>') {
                        kind = TOK_FAT_ARROW;
                        token_width = 2U;
                    } else {
                        kind = TOK_EQUAL;
                    }
                    break;
                case '!':
                    if (lexer_peek(lexer, 1U) == (unsigned char)'=') {
                        kind = TOK_BANG_EQUAL;
                        token_width = 2U;
                    }
                    break;
                case '<':
                    kind = lexer_peek(lexer, 1U) == (unsigned char)'='
                               ? TOK_LESS_EQUAL
                               : TOK_LESS;
                    token_width = kind == TOK_LESS_EQUAL ? 2U : 1U;
                    break;
                case '>':
                    kind = lexer_peek(lexer, 1U) == (unsigned char)'='
                               ? TOK_GREATER_EQUAL
                               : TOK_GREATER;
                    token_width = kind == TOK_GREATER_EQUAL ? 2U : 1U;
                    break;
                default:
                    kind = TOK_ERROR;
                    break;
            }
        }
        if (kind == TOK_ERROR) {
            size_t width = lexer_utf8_width(lexer->source->bytes + start,
                                            lexer->source->length - start);
            if (current >= 0x80U && width == 0U) {
                lexer->offset += 1U;
                lexer->column += 1U;
                lexer_report_at(lexer, DIAG_CODE_LEXICAL, start, lexer->offset,
                                line, column, "invalid UTF-8 sequence");
            } else {
                if (current >= 0x80U && width > 0U) {
                    lexer->offset += width;
                    lexer->column += 1U;
                } else {
                    lexer->offset += token_width;
                    lexer->column += token_width;
                }
                lexer_report_at(lexer, DIAG_CODE_LEXICAL, start, lexer->offset,
                                line, column, "unexpected character");
            }
            if (!lexer_add_token(lexer, TOK_ERROR, start, line, column)) {
                return false;
            }
            continue;
        }
        lexer->offset += token_width;
        lexer->column += token_width;
        if (!lexer_add_token(lexer, kind, start, line, column)) {
            return false;
        }
    }
    if (interpolation) {
        lexer_report_at(lexer, DIAG_CODE_LEXICAL, lexer->offset, lexer->offset,
                        lexer->line, lexer->column,
                        "unterminated string interpolation");
        return lexer_add_token(lexer, TOK_INTERP_END, lexer->offset,
                               lexer->line, lexer->column);
    }
    return true;
}

bool lexer_scan(const SourceBuffer *source, TokenArray *tokens,
                Diagnostics *diagnostics) {
    Lexer lexer;
    size_t start;

    if (source == NULL || tokens == NULL || source->bytes == NULL) {
        return false;
    }
    tokens->items = NULL;
    tokens->length = 0U;
    tokens->capacity = 0U;
    tokens->source_name = source->name;
    lexer.source = source;
    lexer.tokens = tokens;
    lexer.diagnostics = diagnostics;
    lexer.offset = 0U;
    lexer.line = 1U;
    lexer.column = 1U;
    lexer.nesting = 0U;
    lexer.failed = false;
    if (source->length >= 3U && source->bytes[0] == 0xEFU &&
        source->bytes[1] == 0xBBU && source->bytes[2] == 0xBFU) {
        lexer.offset = 3U;
    }
    if (!lexer_scan_code(&lexer, false, 0U)) {
        token_array_destroy(tokens);
        return false;
    }
    if (lexer.failed) {
        token_array_destroy(tokens);
        return false;
    }
    start = lexer.offset;
    if (!lexer_add_token(&lexer, TOK_EOF, start, lexer.line, lexer.column)) {
        token_array_destroy(tokens);
        return false;
    }
    return true;
}
