#include "parser.h"

#include <stdint.h>
#include <string.h>

typedef struct {
    AstArena *arena;
    const TokenArray *tokens;
    Diagnostics *diagnostics;
    size_t position;
    size_t expression_depth;
    bool fatal;
} Parser;

static Token parser_eof_token(void) {
    Token token;
    token.kind = TOK_EOF;
    token.span.start_byte = 0U;
    token.span.end_byte = 0U;
    token.span.line = 1U;
    token.span.column = 1U;
    token.lexeme = "";
    token.length = 0U;
    return token;
}

static Token parser_current(const Parser *parser) {
    if (parser->position >= parser->tokens->length) {
        return parser_eof_token();
    }
    return parser->tokens->items[parser->position];
}

static Token parser_peek(const Parser *parser, size_t distance) {
    size_t position;
    if (distance > SIZE_MAX - parser->position) {
        return parser_eof_token();
    }
    position = parser->position + distance;
    if (position >= parser->tokens->length) {
        return parser_eof_token();
    }
    return parser->tokens->items[position];
}

static bool parser_at(const Parser *parser, TokenKind kind) {
    return parser_current(parser).kind == kind;
}

static Token parser_advance(Parser *parser) {
    Token token = parser_current(parser);
    if (token.kind != TOK_EOF && parser->position < parser->tokens->length) {
        parser->position += 1U;
    }
    return token;
}

static bool parser_match(Parser *parser, TokenKind kind) {
    if (!parser_at(parser, kind)) {
        return false;
    }
    (void)parser_advance(parser);
    return true;
}

static void parser_report(Parser *parser, Token token, const char *message) {
    SourceSpan span = token.span;
    const char *file = parser->tokens->source_name == NULL
                           ? "<source>"
                           : parser->tokens->source_name;
    if (parser->diagnostics != NULL &&
        !diagnostics_add(parser->diagnostics, DIAG_ERROR, DIAG_CODE_PARSE, file,
                         span, message, NULL)) {
        parser->fatal = true;
    }
}

static Token parser_expect(Parser *parser, TokenKind kind, const char *message) {
    if (parser_at(parser, kind)) {
        return parser_advance(parser);
    }
    parser_report(parser, parser_current(parser), message);
    return parser_current(parser);
}

static AstNode *parser_new(Parser *parser, AstKind kind, SourceSpan span) {
    AstNode *node = ast_new(parser->arena, kind, span);
    if (node == NULL) {
        parser->fatal = true;
    }
    return node;
}

static AstNode *parser_token_node(Parser *parser, AstKind kind, Token token) {
    AstNode *node = parser_new(parser, kind, token.span);
    if (node != NULL) {
        node->token = token;
    }
    return node;
}

static bool parser_append(Parser *parser, AstList *list, AstNode *node) {
    if (node == NULL || !ast_list_append(parser->arena, list, node)) {
        parser->fatal = true;
        return false;
    }
    return true;
}

static SourceSpan parser_join_span(SourceSpan first, SourceSpan last) {
    SourceSpan span = first;
    span.end_byte = last.end_byte;
    return span;
}

static void parser_skip_newlines(Parser *parser) {
    while (parser_match(parser, TOK_NEWLINE)) {
    }
}

static bool parser_is_terminator(const Parser *parser) {
    TokenKind kind = parser_current(parser).kind;
    return kind == TOK_NEWLINE || kind == TOK_SEMICOLON ||
           kind == TOK_RBRACE || kind == TOK_EOF;
}

static bool parser_consume_terminator(Parser *parser) {
    if (parser_match(parser, TOK_SEMICOLON)) {
        parser_skip_newlines(parser);
        return true;
    }
    if (parser_at(parser, TOK_NEWLINE)) {
        parser_skip_newlines(parser);
        return true;
    }
    if (parser_at(parser, TOK_RBRACE) || parser_at(parser, TOK_EOF)) {
        return true;
    }
    parser_report(parser, parser_current(parser),
                  "expected a newline or semicolon after statement");
    return false;
}

static bool parser_is_declaration_start(TokenKind kind) {
    return kind == TOK_LET || kind == TOK_VAR || kind == TOK_FN ||
           kind == TOK_CLASS || kind == TOK_INTERFACE || kind == TOK_TYPE ||
           kind == TOK_ENUM || kind == TOK_IMPORT || kind == TOK_AT ||
           kind == TOK_PUBLIC || kind == TOK_PRIVATE ||
           kind == TOK_PROTECTED;
}

static void parser_synchronize(Parser *parser) {
    size_t starting_position = parser->position;

    while (!parser_at(parser, TOK_EOF) && !parser_at(parser, TOK_RBRACE)) {
        if (parser_at(parser, TOK_NEWLINE) || parser_at(parser, TOK_SEMICOLON)) {
            (void)parser_advance(parser);
            break;
        }
        if (parser->position > starting_position &&
            parser_is_declaration_start(parser_current(parser).kind)) {
            break;
        }
        (void)parser_advance(parser);
    }
}

static AstNode *parser_parse_expression(Parser *parser, int minimum_binding);
static AstNode *parser_parse_declaration(Parser *parser);
static AstNode *parser_parse_statement(Parser *parser);
static AstNode *parser_parse_type(Parser *parser);
static AstNode *parser_parse_pattern(Parser *parser);
static AstNode *parser_parse_string(Parser *parser);

static AstNode *parser_parse_arguments(Parser *parser, TokenKind closing) {
    AstNode *first = NULL;
    AstNode *last = NULL;

    parser_skip_newlines(parser);
    if (parser_at(parser, closing)) {
        return NULL;
    }
    do {
        AstNode *argument;
        Token start = parser_current(parser);
        bool spread = parser_match(parser, TOK_ELLIPSIS);
        Token label;
        bool named = false;
        AstNode *value;

        parser_skip_newlines(parser);
        if (parser_at(parser, TOK_IDENTIFIER) &&
            parser_peek(parser, 1U).kind == TOK_COLON) {
            label = parser_advance(parser);
            (void)parser_advance(parser);
            named = true;
        } else {
            label = parser_current(parser);
            label.lexeme = NULL;
            label.length = 0U;
        }
        value = parser_parse_expression(parser, 1);
        if (parser_match(parser, TOK_FOR)) {
            AstNode *comprehension = parser_new(
                parser, AST_LIST, value == NULL ? start.span : value->span);
            AstNode *pattern = parser_parse_pattern(parser);
            (void)parser_expect(parser, TOK_IN,
                                "expected 'in' in generator expression");
            if (comprehension != NULL) {
                AstNode *item = parser_new(
                    parser, AST_ARGUMENT,
                    value == NULL ? start.span : value->span);
                comprehension->flag = true;
                comprehension->a = pattern;
                comprehension->b = parser_parse_expression(parser, 1);
                if (item != NULL) {
                    item->a = value;
                    (void)parser_append(parser, &comprehension->items, item);
                }
                value = comprehension;
            }
        }
        argument = parser_new(parser, AST_ARGUMENT,
                              value == NULL ? start.span
                                            : parser_join_span(start.span,
                                                               value->span));
        if (argument == NULL) {
            return first;
        }
        argument->token = label;
        argument->a = value;
        argument->flag = named;
        argument->second_flag = spread;
        if (first == NULL) {
            first = argument;
        } else {
            last->c = argument;
        }
        last = argument;
        if (parser->fatal) {
            return first;
        }
        parser_skip_newlines(parser);
        if (!parser_match(parser, TOK_COMMA)) {
            break;
        }
        parser_skip_newlines(parser);
    } while (!parser_at(parser, closing) && !parser_at(parser, TOK_EOF));
    return first;
}

static bool parser_append_argument_chain(Parser *parser, AstList *list,
                                         AstNode *first) {
    AstNode *argument = first;
    while (argument != NULL) {
        AstNode *next = argument->c;
        argument->c = NULL;
        if (!parser_append(parser, list, argument)) {
            return false;
        }
        argument = next;
    }
    return true;
}

static AstNode *parser_parse_type_atom(Parser *parser) {
    Token token = parser_current(parser);
    AstNode *node;

    if (parser_match(parser, TOK_IDENTIFIER) || parser_match(parser, TOK_SELF)) {
        node = parser_token_node(parser, AST_TYPE_NAME, token);
        return node;
    }
    if (parser_match(parser, TOK_LPAREN)) {
        AstList parameters = { NULL, NULL, 0U };
        Token close;
        parser_skip_newlines(parser);
        if (!parser_at(parser, TOK_RPAREN)) {
            do {
                AstNode *parameter_type = parser_parse_type(parser);
                if (parameter_type != NULL &&
                    !parser_append(parser, &parameters, parameter_type)) {
                    return NULL;
                }
                parser_skip_newlines(parser);
                if (!parser_match(parser, TOK_COMMA)) {
                    break;
                }
                parser_skip_newlines(parser);
            } while (!parser_at(parser, TOK_RPAREN) &&
                     !parser_at(parser, TOK_EOF));
        }
        close = parser_expect(parser, TOK_RPAREN,
                              "expected ')' after function type parameters");
        if (parser_match(parser, TOK_ARROW)) {
            node = parser_new(parser, AST_TYPE_FUNCTION,
                              parser_join_span(token.span, close.span));
            if (node != NULL) {
                node->items = parameters;
                node->a = parser_parse_type(parser);
                if (node->a != NULL) {
                    node->span.end_byte = node->a->span.end_byte;
                }
            }
            return node;
        }
        if (parameters.length == 1U) {
            return parameters.head->node;
        }
        node = parser_new(parser, AST_TYPE_APPLY,
                          parser_join_span(token.span, close.span));
        if (node != NULL) {
            Token tuple_name = token;
            tuple_name.kind = TOK_IDENTIFIER;
            tuple_name.lexeme = "Tuple";
            tuple_name.length = 5U;
            node->token = tuple_name;
            node->items = parameters;
        }
        return node;
    }
    parser_report(parser, token, "expected a type");
    if (!parser_at(parser, TOK_EOF) && !parser_at(parser, TOK_COMMA) &&
        !parser_at(parser, TOK_RPAREN) && !parser_at(parser, TOK_GREATER)) {
        (void)parser_advance(parser);
    }
    return parser_token_node(parser, AST_ERROR, token);
}

static AstNode *parser_parse_type(Parser *parser) {
    AstNode *type = parser_parse_type_atom(parser);

    while (!parser->fatal && parser_match(parser, TOK_LBRACKET)) {
        AstNode *applied =
            parser_new(parser, AST_TYPE_APPLY, type == NULL ? parser_current(parser).span
                                                           : type->span);
        if (applied == NULL) {
            return type;
        }
        applied->a = type;
        parser_skip_newlines(parser);
        if (!parser_at(parser, TOK_RBRACKET)) {
            do {
                AstNode *argument = parser_parse_type(parser);
                if (argument != NULL &&
                    !parser_append(parser, &applied->items, argument)) {
                    return applied;
                }
                parser_skip_newlines(parser);
                if (!parser_match(parser, TOK_COMMA)) {
                    break;
                }
                parser_skip_newlines(parser);
            } while (!parser_at(parser, TOK_RBRACKET) &&
                     !parser_at(parser, TOK_EOF));
        }
        {
            Token close = parser_expect(parser, TOK_RBRACKET,
                                        "expected ']' after type arguments");
            applied->span.end_byte = close.span.end_byte;
        }
        type = applied;
    }
    if (parser_match(parser, TOK_PIPE)) {
        AstNode *union_type =
            parser_new(parser, AST_TYPE_UNION, type->span);
        if (union_type == NULL) {
            return type;
        }
        (void)parser_append(parser, &union_type->items, type);
        do {
            AstNode *member = parser_parse_type_atom(parser);
            while (parser_match(parser, TOK_LBRACKET)) {
                AstNode *applied =
                    parser_new(parser, AST_TYPE_APPLY, member->span);
                if (applied == NULL) {
                    return union_type;
                }
                applied->a = member;
                do {
                    AstNode *argument = parser_parse_type(parser);
                    if (argument != NULL) {
                        (void)parser_append(parser, &applied->items, argument);
                    }
                    parser_skip_newlines(parser);
                    if (!parser_match(parser, TOK_COMMA)) {
                        break;
                    }
                } while (!parser_at(parser, TOK_RBRACKET) &&
                         !parser_at(parser, TOK_EOF));
                {
                    Token close = parser_expect(parser, TOK_RBRACKET,
                                                "expected ']' after type arguments");
                    applied->span.end_byte = close.span.end_byte;
                }
                member = applied;
            }
            if (member != NULL) {
                union_type->span.end_byte = member->span.end_byte;
                (void)parser_append(parser, &union_type->items, member);
            }
        } while (parser_match(parser, TOK_PIPE));
        type = union_type;
    }
    return type;
}

static AstNode *parser_parse_pattern_atom(Parser *parser) {
    Token token = parser_current(parser);
    AstNode *node;

    if (parser_match(parser, TOK_ELLIPSIS)) {
        Token name = parser_expect(parser, TOK_IDENTIFIER,
                                   "expected a name after spread pattern");
        return parser_token_node(parser, AST_PATTERN_SPREAD, name);
    }
    if (parser_at(parser, TOK_IDENTIFIER)) {
        token = parser_advance(parser);
        if (token.length == 1U && token.lexeme[0] == '_') {
            return parser_token_node(parser, AST_PATTERN_WILDCARD, token);
        }
        node = parser_token_node(parser, AST_PATTERN_NAME, token);
        if (parser_match(parser, TOK_LPAREN)) {
            AstNode *variant = parser_new(parser, AST_PATTERN_VARIANT,
                                          token.span);
            if (variant == NULL) {
                return node;
            }
            variant->token = token;
            variant->items.head = NULL;
            variant->items.tail = NULL;
            variant->items.length = 0U;
            parser_skip_newlines(parser);
            if (!parser_at(parser, TOK_RPAREN)) {
                do {
                    AstNode *child = parser_parse_pattern(parser);
                    if (child != NULL) {
                        (void)parser_append(parser, &variant->items, child);
                    }
                    parser_skip_newlines(parser);
                    if (!parser_match(parser, TOK_COMMA)) {
                        break;
                    }
                    parser_skip_newlines(parser);
                } while (!parser_at(parser, TOK_RPAREN) &&
                         !parser_at(parser, TOK_EOF));
            }
            {
                Token close = parser_expect(parser, TOK_RPAREN,
                                            "expected ')' after pattern");
                variant->span.end_byte = close.span.end_byte;
            }
            return variant;
        }
        return node;
    }
    if (token.kind == TOK_INTEGER || token.kind == TOK_FLOAT ||
        token.kind == TOK_STRING_BEGIN || token.kind == TOK_CHAR ||
        token.kind == TOK_TRUE || token.kind == TOK_FALSE) {
        AstNode *literal;
        if (token.kind == TOK_STRING_BEGIN) {
            AstNode *string = parser_parse_string(parser);
            literal = parser_new(parser, AST_PATTERN_LITERAL, string->span);
            if (literal != NULL) {
                literal->a = string;
            }
            return literal;
        }
        token = parser_advance(parser);
        literal = parser_token_node(parser, AST_PATTERN_LITERAL, token);
        if (parser_at(parser, TOK_DOT_DOT) ||
            parser_at(parser, TOK_DOT_DOT_EQUAL)) {
            AstNode *range = parser_new(parser, AST_PATTERN_RANGE, token.span);
            Token operation = parser_advance(parser);
            AstNode *end = parser_parse_pattern_atom(parser);
            if (range != NULL) {
                range->a = literal;
                range->b = end;
                range->op = operation.kind;
                if (end != NULL) {
                    range->span.end_byte = end->span.end_byte;
                }
            }
            return range;
        }
        return literal;
    }
    if (parser_match(parser, TOK_LBRACKET)) {
        node = parser_new(parser, AST_PATTERN_LIST, token.span);
        parser_skip_newlines(parser);
        if (!parser_at(parser, TOK_RBRACKET)) {
            do {
                AstNode *child = parser_parse_pattern_atom(parser);
                if (child != NULL) {
                    (void)parser_append(parser, &node->items, child);
                }
                parser_skip_newlines(parser);
                if (!parser_match(parser, TOK_COMMA)) {
                    break;
                }
                parser_skip_newlines(parser);
            } while (!parser_at(parser, TOK_RBRACKET) &&
                     !parser_at(parser, TOK_EOF));
        }
        {
            Token close = parser_expect(parser, TOK_RBRACKET,
                                        "expected ']' after list pattern");
            node->span.end_byte = close.span.end_byte;
        }
        return node;
    }
    if (parser_match(parser, TOK_LPAREN)) {
        node = parser_new(parser, AST_PATTERN_TUPLE, token.span);
        parser_skip_newlines(parser);
        if (!parser_at(parser, TOK_RPAREN)) {
            do {
                AstNode *child = parser_parse_pattern(parser);
                if (child != NULL) {
                    (void)parser_append(parser, &node->items, child);
                }
                parser_skip_newlines(parser);
                if (!parser_match(parser, TOK_COMMA)) {
                    break;
                }
                parser_skip_newlines(parser);
            } while (!parser_at(parser, TOK_RPAREN) &&
                     !parser_at(parser, TOK_EOF));
        }
        {
            Token close = parser_expect(parser, TOK_RPAREN,
                                        "expected ')' after tuple pattern");
            node->span.end_byte = close.span.end_byte;
        }
        return node;
    }
    parser_report(parser, token, "expected a pattern");
    if (!parser_at(parser, TOK_EOF) && !parser_at(parser, TOK_COMMA) &&
        !parser_at(parser, TOK_RBRACKET) && !parser_at(parser, TOK_RPAREN)) {
        (void)parser_advance(parser);
    }
    return parser_token_node(parser, AST_ERROR, token);
}

static AstNode *parser_parse_pattern(Parser *parser) {
    AstNode *first = parser_parse_pattern_atom(parser);

    if (parser_match(parser, TOK_PIPE)) {
        AstNode *alternatives =
            parser_new(parser, AST_PATTERN_OR, first->span);
        if (alternatives == NULL) {
            return first;
        }
        (void)parser_append(parser, &alternatives->items, first);
        do {
            AstNode *alternative = parser_parse_pattern_atom(parser);
            if (alternative != NULL) {
                alternatives->span.end_byte = alternative->span.end_byte;
                (void)parser_append(parser, &alternatives->items, alternative);
            }
        } while (parser_match(parser, TOK_PIPE));
        return alternatives;
    }
    return first;
}

static AstNode *parser_parse_attributes(Parser *parser) {
    AstNode *first = NULL;
    AstNode *last = NULL;

    while (parser_match(parser, TOK_AT)) {
        Token name = parser_expect(parser, TOK_IDENTIFIER,
                                   "expected an attribute name after '@'");
        AstNode *attribute = parser_token_node(parser, AST_ATTRIBUTE, name);
        if (parser_match(parser, TOK_LPAREN)) {
            AstNode *arguments = parser_parse_arguments(parser, TOK_RPAREN);
            (void)parser_append_argument_chain(parser, &attribute->items,
                                               arguments);
            (void)parser_expect(parser, TOK_RPAREN,
                                "expected ')' after attribute arguments");
        }
        if (first == NULL) {
            first = attribute;
        } else {
            last->c = attribute;
        }
        last = attribute;
        parser_skip_newlines(parser);
    }
    return first;
}

static bool parser_attach_chain(Parser *parser, AstList *list,
                               AstNode *first) {
    AstNode *node = first;
    while (node != NULL) {
        AstNode *next = node->c;
        node->c = NULL;
        if (!parser_append(parser, list, node)) {
            return false;
        }
        node = next;
    }
    return true;
}

static AstNode *parser_parse_generic_names(Parser *parser) {
    AstNode *first = NULL;
    AstNode *last = NULL;

    if (!parser_match(parser, TOK_LBRACKET)) {
        return NULL;
    }
    parser_skip_newlines(parser);
    if (!parser_at(parser, TOK_RBRACKET)) {
        do {
            Token name = parser_expect(parser, TOK_IDENTIFIER,
                                       "expected a generic parameter name");
            AstNode *parameter = parser_token_node(parser, AST_TYPE_NAME, name);
            if (first == NULL) {
                first = parameter;
            } else {
                last->c = parameter;
            }
            last = parameter;
            parser_skip_newlines(parser);
            if (!parser_match(parser, TOK_COMMA)) {
                break;
            }
            parser_skip_newlines(parser);
        } while (!parser_at(parser, TOK_RBRACKET) &&
                 !parser_at(parser, TOK_EOF));
    }
    (void)parser_expect(parser, TOK_RBRACKET,
                        "expected ']' after generic parameters");
    return first;
}

static AstNode *parser_parse_parameters(Parser *parser, bool allow_inferred) {
    AstNode *first = NULL;
    AstNode *last = NULL;

    parser_skip_newlines(parser);
    if (parser_at(parser, TOK_RPAREN)) {
        return NULL;
    }
    do {
        bool variadic = parser_match(parser, TOK_ELLIPSIS);
        Token name;
        Token label;
        bool has_label = false;
        AstNode *parameter;
        AstNode *type = NULL;
        AstNode *default_value = NULL;

        if (parser_at(parser, TOK_IDENTIFIER) &&
            parser_peek(parser, 1U).kind == TOK_COLON &&
            parser_peek(parser, 2U).kind == TOK_IDENTIFIER &&
            parser_peek(parser, 3U).kind == TOK_COLON) {
            label = parser_advance(parser);
            (void)parser_advance(parser);
            name = parser_expect(parser, TOK_IDENTIFIER,
                                 "expected parameter name after external label");
            has_label = true;
        } else {
            label = parser_current(parser);
            label.lexeme = NULL;
            label.length = 0U;
            name = parser_expect(parser, TOK_IDENTIFIER,
                                 "expected parameter name");
        }
        if (parser_match(parser, TOK_COLON)) {
            type = parser_parse_type(parser);
        } else if (!allow_inferred) {
            parser_report(parser, parser_current(parser),
                          "function parameters require a type annotation");
        }
        if (parser_match(parser, TOK_EQUAL)) {
            default_value = parser_parse_expression(parser, 1);
        }
        parameter = parser_token_node(parser, AST_PARAMETER, name);
        if (parameter == NULL) {
            return first;
        }
        parameter->a = type;
        parameter->b = default_value;
        parameter->flag = variadic;
        parameter->second_flag = has_label;
        if (has_label) {
            (void)parser_append(parser, &parameter->extra,
                                parser_token_node(parser, AST_NAME, label));
        }
        if (first == NULL) {
            first = parameter;
        } else {
            last->c = parameter;
        }
        last = parameter;
        parser_skip_newlines(parser);
        if (!parser_match(parser, TOK_COMMA)) {
            break;
        }
        parser_skip_newlines(parser);
    } while (!parser_at(parser, TOK_RPAREN) && !parser_at(parser, TOK_EOF));
    return first;
}

static AstNode *parser_parse_block(Parser *parser) {
    Token open = parser_expect(parser, TOK_LBRACE, "expected '{' to begin block");
    AstNode *block = parser_new(parser, AST_BLOCK, open.span);

    if (block == NULL) {
        return NULL;
    }
    while (!parser_at(parser, TOK_RBRACE) && !parser_at(parser, TOK_EOF) &&
           !parser->fatal) {
        size_t before;
        AstNode *statement;
        parser_skip_newlines(parser);
        while (parser_match(parser, TOK_SEMICOLON)) {
            parser_skip_newlines(parser);
        }
        if (parser_at(parser, TOK_RBRACE) || parser_at(parser, TOK_EOF)) {
            break;
        }
        before = parser->position;
        statement = parser_parse_declaration(parser);
        if (statement == NULL) {
            statement = parser_parse_statement(parser);
        }
        if (statement != NULL) {
            (void)parser_append(parser, &block->items, statement);
        }
        if (parser->position == before) {
            parser_report(parser, parser_current(parser),
                          "parser could not advance while reading block");
            parser_synchronize(parser);
            if (parser->position == before && !parser_at(parser, TOK_RBRACE) &&
                !parser_at(parser, TOK_EOF)) {
                (void)parser_advance(parser);
            }
        }
    }
    {
        Token close = parser_expect(parser, TOK_RBRACE,
                                    "expected '}' after block");
        block->span.end_byte = close.span.end_byte;
    }
    return block;
}

static AstNode *parser_parse_function(Parser *parser, AstNode *attributes,
                                      Token visibility, bool method) {
    Token start = parser_expect(parser, TOK_FN, "expected 'fn'");
    Token name = parser_expect(parser, TOK_IDENTIFIER,
                               "expected function name");
    AstNode *function = parser_token_node(parser, AST_FUNCTION, name);
    AstNode *generics;
    AstNode *parameters;

    (void)start;
    if (function == NULL) {
        return NULL;
    }
    if (attributes != NULL) {
        (void)parser_attach_chain(parser, &function->extra, attributes);
    }
    function->flag = visibility.kind == TOK_PUBLIC;
    function->is_method = method;
    generics = parser_parse_generic_names(parser);
    (void)parser_expect(parser, TOK_LPAREN, "expected '(' after function name");
    parameters = parser_parse_parameters(parser, false);
    (void)parser_expect(parser, TOK_RPAREN,
                        "expected ')' after function parameters");
    (void)parser_attach_chain(parser, &function->items, parameters);
    if (generics != NULL) {
        (void)parser_attach_chain(parser, &function->extra, generics);
    }
    if (parser_match(parser, TOK_ARROW)) {
        function->a = parser_parse_type(parser);
    }
    if (parser_at(parser, TOK_LBRACE)) {
        function->b = parser_parse_block(parser);
    } else if (parser_match(parser, TOK_EQUAL)) {
        function->is_expression_body = true;
        function->b = parser_parse_expression(parser, 1);
        (void)parser_consume_terminator(parser);
    } else {
        parser_report(parser, parser_current(parser),
                      "expected function body or '=' expression body");
    }
    if (function->b != NULL) {
        function->span = parser_join_span(start.span, function->b->span);
    }
    return function;
}

static AstNode *parser_parse_interface(Parser *parser, AstNode *attributes,
                                       Token visibility) {
    Token start = parser_expect(parser, TOK_INTERFACE, "expected 'interface'");
    Token name = parser_expect(parser, TOK_IDENTIFIER,
                               "expected interface name");
    AstNode *interface_node = parser_token_node(parser, AST_INTERFACE, name);
    AstNode *generics;

    if (interface_node == NULL) {
        return NULL;
    }
    interface_node->flag = visibility.kind == TOK_PUBLIC;
    (void)parser_attach_chain(parser, &interface_node->extra, attributes);
    generics = parser_parse_generic_names(parser);
    if (generics != NULL) {
        (void)parser_attach_chain(parser, &interface_node->extra, generics);
    }
    (void)parser_expect(parser, TOK_LBRACE,
                        "expected '{' before interface members");
    while (!parser_at(parser, TOK_RBRACE) && !parser_at(parser, TOK_EOF) &&
           !parser->fatal) {
        AstNode *member_attributes;
        Token member_visibility;
        AstNode *member;
        parser_skip_newlines(parser);
        if (parser_at(parser, TOK_RBRACE)) {
            break;
        }
        member_attributes = parser_parse_attributes(parser);
        member_visibility = parser_current(parser);
        if (member_visibility.kind == TOK_PUBLIC ||
            member_visibility.kind == TOK_PRIVATE ||
            member_visibility.kind == TOK_PROTECTED) {
            (void)parser_advance(parser);
        } else {
            member_visibility.kind = TOK_ERROR;
        }
        if (!parser_at(parser, TOK_FN)) {
            parser_report(parser, parser_current(parser),
                          "interface members must be method signatures");
            parser_synchronize(parser);
            continue;
        }
        {
            Token fn_token = parser_advance(parser);
            Token method_name = parser_expect(parser, TOK_IDENTIFIER,
                                              "expected interface method name");
            member = parser_token_node(parser, AST_FUNCTION, method_name);
            if (member == NULL) {
                break;
            }
            member->second_flag = true;
            member->flag = member_visibility.kind == TOK_PUBLIC;
            (void)parser_attach_chain(parser, &member->extra,
                                      member_attributes);
            {
                AstNode *method_generics = parser_parse_generic_names(parser);
                if (method_generics != NULL) {
                    (void)parser_attach_chain(parser, &member->extra,
                                              method_generics);
                }
            }
            (void)parser_expect(parser, TOK_LPAREN,
                                "expected '(' after method name");
            {
                AstNode *params = parser_parse_parameters(parser, false);
                (void)parser_expect(parser, TOK_RPAREN,
                                    "expected ')' after method parameters");
                (void)parser_attach_chain(parser, &member->items, params);
            }
            if (parser_match(parser, TOK_ARROW)) {
                member->a = parser_parse_type(parser);
            }
            (void)parser_consume_terminator(parser);
            member->span = parser_join_span(fn_token.span,
                                            parser_current(parser).span);
            (void)parser_append(parser, &interface_node->items, member);
        }
    }
    {
        Token close = parser_expect(parser, TOK_RBRACE,
                                    "expected '}' after interface");
        interface_node->span = parser_join_span(start.span, close.span);
    }
    return interface_node;
}

static AstNode *parser_parse_class(Parser *parser, AstNode *attributes,
                                   Token visibility) {
    Token start = parser_expect(parser, TOK_CLASS, "expected 'class'");
    Token name = parser_expect(parser, TOK_IDENTIFIER, "expected class name");
    AstNode *class_node = parser_token_node(parser, AST_CLASS, name);

    if (class_node == NULL) {
        return NULL;
    }
    class_node->flag = visibility.kind == TOK_PUBLIC;
    (void)parser_attach_chain(parser, &class_node->extra, attributes);
    {
        AstNode *generics = parser_parse_generic_names(parser);
        if (generics != NULL) {
            (void)parser_attach_chain(parser, &class_node->extra, generics);
        }
    }
    if (parser_match(parser, TOK_COLON)) {
        class_node->a = parser_parse_type(parser);
    }
    (void)parser_expect(parser, TOK_LBRACE, "expected '{' before class members");
    while (!parser_at(parser, TOK_RBRACE) && !parser_at(parser, TOK_EOF) &&
           !parser->fatal) {
        AstNode *member_attributes;
        Token member_visibility;
        AstNode *member = NULL;
        size_t before = parser->position;

        parser_skip_newlines(parser);
        while (parser_match(parser, TOK_SEMICOLON)) {
            parser_skip_newlines(parser);
        }
        if (parser_at(parser, TOK_RBRACE)) {
            break;
        }
        member_attributes = parser_parse_attributes(parser);
        member_visibility = parser_current(parser);
        if (member_visibility.kind == TOK_PUBLIC ||
            member_visibility.kind == TOK_PRIVATE ||
            member_visibility.kind == TOK_PROTECTED) {
            (void)parser_advance(parser);
        } else {
            member_visibility.kind = TOK_ERROR;
        }
        if (parser_at(parser, TOK_FN)) {
            member = parser_parse_function(parser, member_attributes,
                                           member_visibility, true);
        } else if (parser_at(parser, TOK_INIT)) {
            Token init = parser_advance(parser);
            member = parser_token_node(parser, AST_FUNCTION, init);
            if (member != NULL) {
                member->second_flag = true;
                member->is_constructor = true;
                (void)parser_attach_chain(parser, &member->extra,
                                          member_attributes);
                (void)parser_expect(parser, TOK_LPAREN,
                                    "expected '(' after init");
                parser_skip_newlines(parser);
                if (!parser_at(parser, TOK_RPAREN)) {
                    do {
                        Token parameter_start = parser_current(parser);
                        bool promoted = parser_match(parser, TOK_SELF);
                        Token parameter_name;
                        AstNode *parameter;
                        if (promoted) {
                            (void)parser_expect(parser, TOK_DOT,
                                                "expected '.' after self");
                        }
                        parameter_name = parser_expect(
                            parser, TOK_IDENTIFIER, "expected constructor parameter");
                        (void)parser_expect(parser, TOK_COLON,
                                            "expected ':' before parameter type");
                        parameter = parser_token_node(parser, AST_PARAMETER,
                                                      parameter_name);
                        if (parameter != NULL) {
                            parameter->a = parser_parse_type(parser);
                            parameter->flag = promoted;
                            if (parser_match(parser, TOK_EQUAL)) {
                                parameter->b =
                                    parser_parse_expression(parser, 1);
                            }
                            (void)parser_append(parser, &member->items,
                                                parameter);
                        }
                        parser_skip_newlines(parser);
                        if (!parser_match(parser, TOK_COMMA)) {
                            break;
                        }
                        parser_skip_newlines(parser);
                        (void)parameter_start;
                    } while (!parser_at(parser, TOK_RPAREN) &&
                             !parser_at(parser, TOK_EOF));
                }
                (void)parser_expect(parser, TOK_RPAREN,
                                    "expected ')' after constructor parameters");
                member->b = parser_parse_block(parser);
            }
        } else if (parser_at(parser, TOK_LET) || parser_at(parser, TOK_VAR)) {
            Token binding_start = parser_advance(parser);
            Token field_name = parser_expect(parser, TOK_IDENTIFIER,
                                             "expected field name");
            member = parser_token_node(parser, AST_BINDING, field_name);
            if (member != NULL) {
                member->flag = binding_start.kind == TOK_VAR;
                member->second_flag = member_visibility.kind == TOK_PUBLIC;
                (void)parser_attach_chain(parser, &member->extra,
                                          member_attributes);
                (void)parser_expect(parser, TOK_COLON,
                                    "class fields require a type annotation");
                member->a = parser_parse_type(parser);
                if (parser_match(parser, TOK_EQUAL)) {
                    member->c = parser_parse_expression(parser, 1);
                }
                (void)parser_consume_terminator(parser);
            }
        } else {
            parser_report(parser, parser_current(parser),
                          "expected class field, method, or constructor");
            parser_synchronize(parser);
        }
        if (member != NULL) {
            (void)parser_append(parser, &class_node->items, member);
        }
        if (before == parser->position && !parser_at(parser, TOK_RBRACE) &&
            !parser_at(parser, TOK_EOF)) {
            (void)parser_advance(parser);
        }
    }
    {
        Token close = parser_expect(parser, TOK_RBRACE,
                                    "expected '}' after class");
        class_node->span = parser_join_span(start.span, close.span);
    }
    return class_node;
}

static AstNode *parser_parse_enum(Parser *parser, AstNode *attributes,
                                  Token visibility) {
    Token start = parser_expect(parser, TOK_ENUM, "expected 'enum'");
    Token name = parser_expect(parser, TOK_IDENTIFIER, "expected enum name");
    AstNode *enum_node = parser_token_node(parser, AST_ENUM, name);

    if (enum_node == NULL) {
        return NULL;
    }
    enum_node->flag = visibility.kind == TOK_PUBLIC;
    (void)parser_attach_chain(parser, &enum_node->extra, attributes);
    (void)parser_expect(parser, TOK_LBRACE, "expected '{' before enum cases");
    while (!parser_at(parser, TOK_RBRACE) && !parser_at(parser, TOK_EOF) &&
           !parser->fatal) {
        AstNode *item;
        parser_skip_newlines(parser);
        while (parser_match(parser, TOK_COMMA)) {
            parser_skip_newlines(parser);
        }
        if (parser_at(parser, TOK_RBRACE)) {
            break;
        }
        if (parser_at(parser, TOK_FN)) {
            item = parser_parse_function(parser, NULL,
                                         (Token){ TOK_ERROR, { 0U, 0U, 0U, 0U },
                                                  NULL, 0U }, true);
        } else {
            Token case_name = parser_expect(parser, TOK_IDENTIFIER,
                                            "expected enum case or method");
            item = parser_token_node(parser, AST_ENUM_CASE, case_name);
            if (parser_match(parser, TOK_LPAREN)) {
                parser_skip_newlines(parser);
                if (!parser_at(parser, TOK_RPAREN)) {
                    do {
                        AstNode *payload = parser_parse_type(parser);
                        if (payload != NULL) {
                            (void)parser_append(parser, &item->items, payload);
                        }
                        parser_skip_newlines(parser);
                        if (!parser_match(parser, TOK_COMMA)) {
                            break;
                        }
                        parser_skip_newlines(parser);
                    } while (!parser_at(parser, TOK_RPAREN) &&
                             !parser_at(parser, TOK_EOF));
                }
                {
                    Token close = parser_expect(parser, TOK_RPAREN,
                                                "expected ')' after enum payload");
                    item->span.end_byte = close.span.end_byte;
                }
            }
        }
        if (item != NULL) {
            (void)parser_append(parser, &enum_node->items, item);
        }
        if (!parser_match(parser, TOK_COMMA)) {
            parser_skip_newlines(parser);
        }
    }
    {
        Token close = parser_expect(parser, TOK_RBRACE, "expected '}' after enum");
        enum_node->span = parser_join_span(start.span, close.span);
    }
    return enum_node;
}

static AstNode *parser_parse_type_alias(Parser *parser, AstNode *attributes,
                                        Token visibility) {
    Token start = parser_expect(parser, TOK_TYPE, "expected 'type'");
    Token name = parser_expect(parser, TOK_IDENTIFIER, "expected type name");
    AstNode *alias = parser_token_node(parser, AST_TYPE_ALIAS, name);

    if (alias == NULL) {
        return NULL;
    }
    alias->flag = visibility.kind == TOK_PUBLIC;
    (void)parser_attach_chain(parser, &alias->extra, attributes);
    {
        AstNode *generics = parser_parse_generic_names(parser);
        if (generics != NULL) {
            (void)parser_attach_chain(parser, &alias->extra, generics);
        }
    }
    (void)parser_expect(parser, TOK_EQUAL, "expected '=' in type alias");
    do {
        Token variant_name = parser_expect(parser, TOK_IDENTIFIER,
                                           "expected variant name");
        AstNode *variant = parser_token_node(parser, AST_ENUM_CASE,
                                             variant_name);
        if (variant != NULL && parser_match(parser, TOK_LPAREN)) {
            parser_skip_newlines(parser);
            if (!parser_at(parser, TOK_RPAREN)) {
                do {
                    AstNode *payload = parser_parse_type(parser);
                    if (payload != NULL) {
                        (void)parser_append(parser, &variant->items, payload);
                    }
                    parser_skip_newlines(parser);
                    if (!parser_match(parser, TOK_COMMA)) {
                        break;
                    }
                } while (!parser_at(parser, TOK_RPAREN) &&
                         !parser_at(parser, TOK_EOF));
            }
            {
                Token close = parser_expect(parser, TOK_RPAREN,
                                            "expected ')' after variant payload");
                variant->span.end_byte = close.span.end_byte;
            }
        }
        if (variant != NULL) {
            (void)parser_append(parser, &alias->items, variant);
        }
    } while (parser_match(parser, TOK_PIPE));
    (void)parser_consume_terminator(parser);
    alias->span = parser_join_span(
        start.span, alias->items.tail == NULL
                        ? name.span : alias->items.tail->node->span);
    return alias;
}

static AstNode *parser_parse_import(Parser *parser) {
    Token start = parser_expect(parser, TOK_IMPORT, "expected 'import'");
    AstNode *import_node = parser_new(parser, AST_IMPORT, start.span);
    Token part;

    if (import_node == NULL) {
        return NULL;
    }
    part = parser_expect(parser, TOK_IDENTIFIER, "expected module path");
    (void)parser_append(parser, &import_node->items,
                        parser_token_node(parser, AST_NAME, part));
    while (parser_match(parser, TOK_DOT) ||
           parser_match(parser, TOK_DOUBLE_COLON)) {
        part = parser_expect(parser, TOK_IDENTIFIER,
                             "expected name after module separator");
        (void)parser_append(parser, &import_node->items,
                            parser_token_node(parser, AST_NAME, part));
    }
    if (parser_match(parser, TOK_AS)) {
        part = parser_expect(parser, TOK_IDENTIFIER,
                             "expected alias after 'as'");
        import_node->a = parser_token_node(parser, AST_NAME, part);
    }
    (void)parser_consume_terminator(parser);
    import_node->span.end_byte = parser_peek(parser, 0U).span.end_byte;
    return import_node;
}

static AstNode *parser_parse_binding(Parser *parser) {
    Token start = parser_advance(parser);
    AstNode *binding = parser_new(parser, AST_BINDING, start.span);

    if (binding == NULL) {
        return NULL;
    }
    binding->flag = start.kind == TOK_VAR;
    binding->a = parser_parse_pattern(parser);
    if (parser_match(parser, TOK_COLON)) {
        binding->b = parser_parse_type(parser);
    }
    (void)parser_expect(parser, TOK_EQUAL,
                        "expected '=' and initializer in binding");
    binding->c = parser_parse_expression(parser, 1);
    (void)parser_consume_terminator(parser);
    if (binding->c != NULL) {
        binding->span = parser_join_span(start.span, binding->c->span);
    }
    return binding;
}

static AstNode *parser_parse_declaration(Parser *parser) {
    AstNode *attributes = parser_parse_attributes(parser);
    Token visibility = parser_current(parser);
    if (visibility.kind == TOK_PUBLIC || visibility.kind == TOK_PRIVATE ||
        visibility.kind == TOK_PROTECTED) {
        (void)parser_advance(parser);
    } else {
        visibility.kind = TOK_ERROR;
    }
    switch (parser_current(parser).kind) {
        case TOK_IMPORT:
            return parser_parse_import(parser);
        case TOK_LET:
        case TOK_VAR:
            if (attributes != NULL || visibility.kind != TOK_ERROR) {
                parser_report(parser, parser_current(parser),
                              "bindings cannot have attributes or visibility");
            }
            return parser_parse_binding(parser);
        case TOK_FN:
            return parser_parse_function(parser, attributes, visibility, false);
        case TOK_CLASS:
            return parser_parse_class(parser, attributes, visibility);
        case TOK_INTERFACE:
            return parser_parse_interface(parser, attributes, visibility);
        case TOK_ENUM:
            return parser_parse_enum(parser, attributes, visibility);
        case TOK_TYPE:
            return parser_parse_type_alias(parser, attributes, visibility);
        default:
            if (attributes != NULL || visibility.kind != TOK_ERROR) {
                parser_report(parser, parser_current(parser),
                              "expected declaration after modifier");
            }
            return NULL;
    }
}

static AstNode *parser_parse_if(Parser *parser) {
    Token start = parser_expect(parser, TOK_IF, "expected 'if'");
    AstNode *node = parser_new(parser, AST_IF, start.span);

    if (node == NULL) {
        return NULL;
    }
    node->a = parser_parse_expression(parser, 1);
    node->b = parser_parse_block(parser);
    parser_skip_newlines(parser);
    if (parser_match(parser, TOK_ELSE)) {
        parser_skip_newlines(parser);
        node->c = parser_at(parser, TOK_IF) ? parser_parse_if(parser)
                                            : parser_parse_block(parser);
    }
    if (node->c != NULL) {
        node->span = parser_join_span(start.span, node->c->span);
    } else if (node->b != NULL) {
        node->span = parser_join_span(start.span, node->b->span);
    }
    return node;
}

static AstNode *parser_parse_statement(Parser *parser) {
    Token token = parser_current(parser);
    AstNode *node;

    switch (token.kind) {
        case TOK_LBRACE:
            return parser_parse_block(parser);
        case TOK_IF:
            return parser_parse_if(parser);
        case TOK_WHILE: {
            Token start = parser_advance(parser);
            node = parser_new(parser, AST_WHILE, start.span);
            if (node != NULL) {
                node->a = parser_parse_expression(parser, 1);
                node->b = parser_parse_block(parser);
                node->span = parser_join_span(start.span, node->b->span);
            }
            return node;
        }
        case TOK_FOR: {
            Token start = parser_advance(parser);
            node = parser_new(parser, AST_FOR, start.span);
            if (node != NULL) {
                node->a = parser_parse_pattern(parser);
                (void)parser_expect(parser, TOK_IN, "expected 'in' in for loop");
                node->b = parser_parse_expression(parser, 1);
                node->c = parser_parse_block(parser);
                node->span = parser_join_span(start.span, node->c->span);
            }
            return node;
        }
        case TOK_RETURN: {
            Token start = parser_advance(parser);
            node = parser_new(parser, AST_RETURN, start.span);
            if (node != NULL) {
                if (!parser_is_terminator(parser)) {
                    node->a = parser_parse_expression(parser, 1);
                }
                (void)parser_consume_terminator(parser);
                if (node->a != NULL) {
                    node->span = parser_join_span(start.span, node->a->span);
                }
            }
            return node;
        }
        case TOK_BREAK:
        case TOK_CONTINUE: {
            Token start = parser_advance(parser);
            node = parser_new(parser,
                              start.kind == TOK_BREAK ? AST_BREAK : AST_CONTINUE,
                              start.span);
            (void)parser_consume_terminator(parser);
            return node;
        }
        default:
            node = parser_new(parser, AST_EXPRESSION_STATEMENT, token.span);
            if (node != NULL) {
                node->a = parser_parse_expression(parser, 1);
                (void)parser_consume_terminator(parser);
                if (node->a != NULL) {
                    node->span = node->a->span;
                }
            }
            return node;
    }
}

static bool parser_is_lambda_start(const Parser *parser) {
    size_t index = parser->position;
    size_t depth = 0U;

    if (!parser_at(parser, TOK_LPAREN)) {
        return false;
    }
    for (; index < parser->tokens->length; ++index) {
        TokenKind kind = parser->tokens->items[index].kind;
        if (kind == TOK_LPAREN) {
            depth += 1U;
        } else if (kind == TOK_RPAREN) {
            if (depth == 0U) {
                return false;
            }
            depth -= 1U;
            if (depth == 0U) {
                return index + 1U < parser->tokens->length &&
                       parser->tokens->items[index + 1U].kind == TOK_FAT_ARROW;
            }
        }
    }
    return false;
}

static bool parser_token_continues_expression(TokenKind kind) {
    switch (kind) {
        case TOK_LPAREN:
        case TOK_LBRACKET:
        case TOK_DOT:
        case TOK_DOUBLE_COLON:
        case TOK_POWER:
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT:
        case TOK_AT:
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_DOT_DOT:
        case TOK_DOT_DOT_EQUAL:
        case TOK_LESS:
        case TOK_LESS_EQUAL:
        case TOK_GREATER:
        case TOK_GREATER_EQUAL:
        case TOK_EQUAL_EQUAL:
        case TOK_BANG_EQUAL:
        case TOK_AND:
        case TOK_OR:
        case TOK_EQUAL:
        case TOK_PLUS_EQUAL:
        case TOK_MINUS_EQUAL:
        case TOK_STAR_EQUAL:
        case TOK_SLASH_EQUAL:
        case TOK_PERCENT_EQUAL:
        case TOK_POWER_EQUAL:
            return true;
        default:
            return false;
    }
}

static void parser_skip_continuation_newlines(Parser *parser) {
    size_t saved = parser->position;

    if (!parser_at(parser, TOK_NEWLINE)) {
        return;
    }
    parser_skip_newlines(parser);
    if (!parser_token_continues_expression(parser_current(parser).kind)) {
        parser->position = saved;
    }
}

static int parser_infix_binding(TokenKind kind, int *right_binding,
                                AstKind *node_kind) {
    int binding;

    switch (kind) {
        case TOK_EQUAL:
        case TOK_PLUS_EQUAL:
        case TOK_MINUS_EQUAL:
        case TOK_STAR_EQUAL:
        case TOK_SLASH_EQUAL:
        case TOK_PERCENT_EQUAL:
        case TOK_POWER_EQUAL:
            binding = 1;
            *right_binding = binding;
            *node_kind = AST_ASSIGNMENT;
            return binding;
        case TOK_OR:
            binding = 2;
            *node_kind = AST_BINARY;
            break;
        case TOK_AND:
            binding = 3;
            *node_kind = AST_BINARY;
            break;
        case TOK_EQUAL_EQUAL:
        case TOK_BANG_EQUAL:
            binding = 4;
            *node_kind = AST_BINARY;
            break;
        case TOK_LESS:
        case TOK_LESS_EQUAL:
        case TOK_GREATER:
        case TOK_GREATER_EQUAL:
            binding = 5;
            *node_kind = AST_BINARY;
            break;
        case TOK_DOT_DOT:
        case TOK_DOT_DOT_EQUAL:
            binding = 6;
            *node_kind = AST_RANGE;
            break;
        case TOK_AT:
            binding = 9;
            *node_kind = AST_BINARY;
            break;
        case TOK_PLUS:
        case TOK_MINUS:
            binding = 8;
            *node_kind = AST_BINARY;
            break;
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT:
            binding = 9;
            *node_kind = AST_BINARY;
            break;
        case TOK_POWER:
            binding = 11;
            *node_kind = AST_BINARY;
            *right_binding = binding;
            return binding;
        default:
            return 0;
    }
    *right_binding = binding + 1;
    return binding;
}

static bool parser_is_comparison(TokenKind kind) {
    return kind == TOK_LESS || kind == TOK_LESS_EQUAL ||
           kind == TOK_GREATER || kind == TOK_GREATER_EQUAL;
}

static bool parser_is_place(const AstNode *node) {
    return node != NULL &&
           (node->kind == AST_NAME || node->kind == AST_MEMBER ||
            node->kind == AST_INDEX);
}

static AstNode *parser_parse_string(Parser *parser) {
    Token opening = parser_advance(parser);
    AstNode *string_node =
        parser_new(parser, AST_INTERPOLATED_STRING, opening.span);
    Token closing = opening;
    bool interpolated = false;

    if (string_node == NULL) {
        return NULL;
    }
    while (!parser_at(parser, TOK_EOF) && !parser_at(parser, TOK_STRING_END) &&
           !parser->fatal) {
        if (parser_at(parser, TOK_STRING_TEXT)) {
            Token text = parser_advance(parser);
            (void)parser_append(parser, &string_node->items,
                                parser_token_node(parser, AST_STRING, text));
        } else if (parser_match(parser, TOK_INTERP_BEGIN)) {
            AstNode *expression;
            interpolated = true;
            parser_skip_newlines(parser);
            expression = parser_parse_expression(parser, 1);
            (void)parser_expect(parser, TOK_INTERP_END,
                                "expected '}' after interpolation expression");
            if (expression != NULL) {
                (void)parser_append(parser, &string_node->items, expression);
            }
        } else if (parser_at(parser, TOK_ERROR)) {
            (void)parser_advance(parser);
        } else {
            parser_report(parser, parser_current(parser),
                          "unexpected token inside string literal");
            (void)parser_advance(parser);
        }
    }
    if (parser_at(parser, TOK_STRING_END)) {
        closing = parser_advance(parser);
    } else {
        parser_report(parser, parser_current(parser),
                      "expected closing quote for string");
    }
    string_node->span = parser_join_span(opening.span, closing.span);
    string_node->token = opening;
    string_node->token.span = string_node->span;
    string_node->token.length =
        string_node->span.end_byte - string_node->span.start_byte;
    if (!interpolated) {
        string_node->kind = AST_STRING;
        string_node->items.head = NULL;
        string_node->items.tail = NULL;
        string_node->items.length = 0U;
    }
    return string_node;
}

static AstNode *parser_parse_list(Parser *parser) {
    Token opening = parser_advance(parser);
    AstNode *list = parser_new(parser, AST_LIST, opening.span);

    if (list == NULL) {
        return NULL;
    }
    parser_skip_newlines(parser);
    if (!parser_at(parser, TOK_RBRACKET)) {
        bool spread = parser_match(parser, TOK_ELLIPSIS);
        AstNode *first = parser_parse_expression(parser, 1);
        AstNode *item = parser_new(parser, AST_ARGUMENT,
                                   first == NULL ? opening.span : first->span);
        if (item != NULL) {
            item->a = first;
            item->second_flag = spread;
            (void)parser_append(parser, &list->items, item);
        }
        parser_skip_newlines(parser);
        if (parser_match(parser, TOK_FOR)) {
            list->flag = true;
            list->a = parser_parse_pattern(parser);
            (void)parser_expect(parser, TOK_IN,
                                "expected 'in' in collection comprehension");
            list->b = parser_parse_expression(parser, 1);
        } else {
            while (parser_match(parser, TOK_COMMA)) {
                parser_skip_newlines(parser);
                if (parser_at(parser, TOK_RBRACKET)) {
                    break;
                }
                spread = parser_match(parser, TOK_ELLIPSIS);
                first = parser_parse_expression(parser, 1);
                item = parser_new(parser, AST_ARGUMENT,
                                  first == NULL ? parser_current(parser).span
                                                : first->span);
                if (item != NULL) {
                    item->a = first;
                    item->second_flag = spread;
                    (void)parser_append(parser, &list->items, item);
                }
                parser_skip_newlines(parser);
            }
        }
    }
    {
        Token close = parser_expect(parser, TOK_RBRACKET,
                                    "expected ']' after collection literal");
        list->span.end_byte = close.span.end_byte;
    }
    return list;
}

static AstNode *parser_parse_match(Parser *parser) {
    Token start = parser_expect(parser, TOK_MATCH, "expected 'match'");
    AstNode *match = parser_new(parser, AST_MATCH, start.span);

    if (match == NULL) {
        return NULL;
    }
    match->a = parser_parse_expression(parser, 1);
    parser_skip_newlines(parser);
    (void)parser_expect(parser, TOK_LBRACE, "expected '{' before match arms");
    parser_skip_newlines(parser);
    while (!parser_at(parser, TOK_RBRACE) && !parser_at(parser, TOK_EOF) &&
           !parser->fatal) {
        Token arm_start = parser_current(parser);
        AstNode *arm = parser_new(parser, AST_MATCH_ARM, arm_start.span);
        if (arm == NULL) {
            break;
        }
        arm->a = parser_parse_pattern(parser);
        while (parser_match(parser, TOK_COMMA)) {
            AstNode *alternative = parser_parse_pattern(parser);
            if (alternative == NULL || !parser_append(parser, &arm->items,
                                                      alternative)) {
                break;
            }
        }
        if (parser_match(parser, TOK_IF)) {
            arm->b = parser_parse_expression(parser, 1);
        }
        (void)parser_expect(parser, TOK_FAT_ARROW,
                            "expected '=>' after match pattern");
        arm->c = parser_parse_expression(parser, 1);
        if (arm->c != NULL) {
            arm->span = parser_join_span(arm_start.span, arm->c->span);
        }
        (void)parser_append(parser, &match->items, arm);
        if (parser_match(parser, TOK_COMMA)) {
            parser_skip_newlines(parser);
        } else if (parser_at(parser, TOK_NEWLINE)) {
            parser_skip_newlines(parser);
        } else if (!parser_at(parser, TOK_RBRACE)) {
            parser_report(parser, parser_current(parser),
                          "expected ',' or newline between match arms");
            parser_synchronize(parser);
        }
    }
    {
        Token close = parser_expect(parser, TOK_RBRACE,
                                    "expected '}' after match arms");
        match->span = parser_join_span(start.span, close.span);
    }
    return match;
}

static AstNode *parser_parse_kernel(Parser *parser) {
    Token start = parser_advance(parser);
    AstNode *kernel = parser_new(parser, AST_KERNEL, start.span);
    AstNode *domain;
    Token parameter;

    (void)parser_expect(parser, TOK_LPAREN, "expected '(' after kernel");
    domain = parser_parse_expression(parser, 1);
    (void)parser_expect(parser, TOK_RPAREN,
                        "expected ')' after kernel domain");
    (void)parser_expect(parser, TOK_LBRACE,
                        "expected '{' before kernel lambda");
    parameter = parser_expect(parser, TOK_IDENTIFIER,
                              "expected kernel parameter name");
    (void)parser_expect(parser, TOK_FAT_ARROW,
                        "expected '=>' after kernel parameter");
    if (kernel != NULL) {
        kernel->a = domain;
        kernel->token = parameter;
        kernel->b = parser_parse_expression(parser, 1);
    }
    {
        Token close = parser_expect(parser, TOK_RBRACE,
                                    "expected '}' after kernel body");
        if (kernel != NULL) {
            kernel->span = parser_join_span(start.span, close.span);
        }
    }
    return kernel;
}

static AstNode *parser_parse_primary(Parser *parser) {
    Token token = parser_current(parser);
    AstNode *node;

    switch (token.kind) {
        case TOK_INTEGER:
            (void)parser_advance(parser);
            return parser_token_node(parser, AST_INTEGER, token);
        case TOK_FLOAT:
            (void)parser_advance(parser);
            return parser_token_node(parser, AST_FLOAT, token);
        case TOK_CHAR:
            (void)parser_advance(parser);
            return parser_token_node(parser, AST_CHAR, token);
        case TOK_TRUE:
        case TOK_FALSE:
            (void)parser_advance(parser);
            return parser_token_node(parser, AST_BOOLEAN, token);
        case TOK_STRING_BEGIN:
            return parser_parse_string(parser);
        case TOK_IDENTIFIER:
        case TOK_SELF:
            if (token.kind == TOK_IDENTIFIER && token.length == 6U &&
                memcmp(token.lexeme, "kernel", 6U) == 0 &&
                parser_peek(parser, 1U).kind == TOK_LPAREN) {
                return parser_parse_kernel(parser);
            }
            (void)parser_advance(parser);
            return parser_token_node(parser, AST_NAME, token);
        case TOK_LBRACKET:
            return parser_parse_list(parser);
        case TOK_MATCH:
            return parser_parse_match(parser);
        case TOK_LBRACE:
            return parser_parse_block(parser);
        case TOK_LPAREN:
            if (parser_is_lambda_start(parser)) {
                Token opening = parser_advance(parser);
                node = parser_new(parser, AST_LAMBDA, opening.span);
                {
                    AstNode *parameters = parser_parse_parameters(parser, true);
                    (void)parser_expect(parser, TOK_RPAREN,
                                        "expected ')' after lambda parameters");
                    (void)parser_expect(parser, TOK_FAT_ARROW,
                                        "expected '=>' after lambda parameters");
                    (void)parser_attach_chain(parser,
                                              node == NULL ? &(AstList){ 0 }
                                                           : &node->items,
                                              parameters);
                }
                if (node != NULL) {
                    node->a = parser_parse_expression(parser, 1);
                    if (node->a != NULL) {
                        node->span = parser_join_span(opening.span,
                                                      node->a->span);
                    }
                }
                return node;
            }
            (void)parser_advance(parser);
            parser_skip_newlines(parser);
            if (parser_match(parser, TOK_RPAREN)) {
                return parser_new(parser, AST_TUPLE, token.span);
            }
            node = parser_parse_expression(parser, 1);
            parser_skip_newlines(parser);
            if (parser_match(parser, TOK_COMMA)) {
                AstNode *tuple = parser_new(parser, AST_TUPLE, token.span);
                if (tuple != NULL) {
                    AstNode *item = parser_new(parser, AST_ARGUMENT,
                                               node == NULL ? token.span : node->span);
                    if (item != NULL) {
                        item->a = node;
                        (void)parser_append(parser, &tuple->items, item);
                    }
                    parser_skip_newlines(parser);
                    while (!parser_at(parser, TOK_RPAREN) &&
                           !parser_at(parser, TOK_EOF)) {
                        AstNode *item = parser_parse_expression(parser, 1);
                        if (item != NULL) {
                            AstNode *argument = parser_new(parser, AST_ARGUMENT,
                                                           item->span);
                            if (argument != NULL) {
                                argument->a = item;
                                (void)parser_append(parser, &tuple->items,
                                                    argument);
                            }
                        }
                        parser_skip_newlines(parser);
                        if (!parser_match(parser, TOK_COMMA)) {
                            break;
                        }
                        parser_skip_newlines(parser);
                    }
                    {
                        Token close = parser_expect(
                            parser, TOK_RPAREN, "expected ')' after tuple");
                        tuple->span = parser_join_span(token.span, close.span);
                    }
                }
                return tuple;
            }
            {
                Token close = parser_expect(parser, TOK_RPAREN,
                                            "expected ')' after expression");
                if (node != NULL) {
                    node->span = parser_join_span(token.span, close.span);
                }
            }
            return node;
        default:
            parser_report(parser, token, "expected an expression");
            if (!parser_at(parser, TOK_EOF) && !parser_at(parser, TOK_NEWLINE) &&
                !parser_at(parser, TOK_RBRACE) &&
                !parser_at(parser, TOK_RPAREN) &&
                !parser_at(parser, TOK_RBRACKET) &&
                !parser_at(parser, TOK_COMMA)) {
                (void)parser_advance(parser);
            }
            return parser_token_node(parser, AST_ERROR, token);
    }
}

static AstNode *parser_parse_prefix(Parser *parser) {
    Token token = parser_current(parser);
    AstNode *node;

    if (token.kind == TOK_NOT || token.kind == TOK_PLUS ||
        token.kind == TOK_MINUS || token.kind == TOK_SPAWN ||
        token.kind == TOK_AWAIT || token.kind == TOK_TRY ||
        token.kind == TOK_PARALLEL) {
        (void)parser_advance(parser);
        if (token.kind == TOK_PARALLEL) {
            node = parser_new(parser, AST_PARALLEL, token.span);
        } else if (token.kind == TOK_SPAWN) {
            node = parser_new(parser, AST_SPAWN, token.span);
        } else if (token.kind == TOK_AWAIT) {
            node = parser_new(parser, AST_AWAIT, token.span);
        } else if (token.kind == TOK_TRY) {
            node = parser_new(parser, AST_TRY, token.span);
        } else {
            node = parser_new(parser, AST_UNARY, token.span);
            if (node != NULL) {
                node->op = token.kind;
            }
        }
        if (node != NULL) {
            node->a = parser_parse_expression(
                parser, token.kind == TOK_PARALLEL ? 2 : 10);
            if (node->a != NULL) {
                node->span = parser_join_span(token.span, node->a->span);
            }
        }
        return node;
    }
    return parser_parse_primary(parser);
}

static AstNode *parser_parse_expression(Parser *parser, int minimum_binding) {
    AstNode *left;

    if (parser->expression_depth >= 512U) {
        parser_report(parser, parser_current(parser),
                      "expression nesting limit exceeded");
        return parser_token_node(parser, AST_ERROR, parser_current(parser));
    }
    parser->expression_depth += 1U;
    left = parser_parse_prefix(parser);
    while (left != NULL && !parser->fatal) {
        Token token;
        AstNode *node;
        int right_binding = 0;
        int left_binding;
        AstKind node_kind = AST_BINARY;

        parser_skip_continuation_newlines(parser);
        token = parser_current(parser);
        if (token.kind == TOK_LPAREN && 12 >= minimum_binding) {
            AstNode *call;
            (void)parser_advance(parser);
            call = parser_new(parser, AST_CALL, left->span);
            if (call == NULL) {
                break;
            }
            call->a = left;
            (void)parser_append_argument_chain(
                parser, &call->items, parser_parse_arguments(parser, TOK_RPAREN));
            {
                Token close = parser_expect(parser, TOK_RPAREN,
                                            "expected ')' after arguments");
                call->span.end_byte = close.span.end_byte;
            }
            left = call;
            continue;
        }
        if ((token.kind == TOK_DOT || token.kind == TOK_DOUBLE_COLON) &&
            12 >= minimum_binding) {
            AstNode *member;
            (void)parser_advance(parser);
            token = parser_expect(parser, TOK_IDENTIFIER,
                                  "expected member name after access operator");
            member = parser_token_node(parser, AST_MEMBER, token);
            if (member != NULL) {
                member->a = left;
                member->span.start_byte = left->span.start_byte;
                member->span.line = left->span.line;
                member->span.column = left->span.column;
                left = member;
            }
            continue;
        }
        if (token.kind == TOK_LBRACKET && 12 >= minimum_binding) {
            AstNode *index = parser_new(parser, AST_INDEX, left->span);
            (void)parser_advance(parser);
            if (index == NULL) {
                break;
            }
            index->a = left;
            parser_skip_newlines(parser);
            if (!parser_at(parser, TOK_RBRACKET)) {
                do {
                    AstNode *element = parser_parse_expression(parser, 1);
                    if (element != NULL) {
                        (void)parser_append(parser, &index->items, element);
                    }
                    parser_skip_newlines(parser);
                    if (!parser_match(parser, TOK_COMMA)) {
                        break;
                    }
                    parser_skip_newlines(parser);
                } while (!parser_at(parser, TOK_RBRACKET) &&
                         !parser_at(parser, TOK_EOF));
            }
            {
                Token close = parser_expect(parser, TOK_RBRACKET,
                                            "expected ']' after index");
                index->span.end_byte = close.span.end_byte;
            }
            left = index;
            continue;
        }
        left_binding = parser_infix_binding(token.kind, &right_binding,
                                            &node_kind);
        if (left_binding == 0 || left_binding < minimum_binding) {
            break;
        }
        (void)parser_advance(parser);
        if (node_kind == AST_ASSIGNMENT && !parser_is_place(left)) {
            parser_report(parser, token,
                          "left side of assignment must be a mutable place");
        }
        if (parser_is_comparison(token.kind) &&
            left->kind == AST_BINARY && parser_is_comparison(left->op)) {
            parser_report(parser, token,
                          "comparison operators cannot be chained");
        }
        node = parser_new(parser, node_kind,
                          left->span);
        if (node == NULL) {
            break;
        }
        node->op = token.kind;
        node->a = left;
        node->b = parser_parse_expression(parser, right_binding);
        if (node->b != NULL) {
            node->span = parser_join_span(left->span, node->b->span);
        }
        left = node;
    }
    parser->expression_depth -= 1U;
    return left;
}

bool parser_parse(AstArena *arena, const TokenArray *tokens,
                 Diagnostics *diagnostics, AstNode **program) {
    Parser parser;
    AstNode *root;

    if (arena == NULL || tokens == NULL || tokens->items == NULL ||
        tokens->length == 0U || program == NULL) {
        return false;
    }
    parser.arena = arena;
    parser.tokens = tokens;
    parser.diagnostics = diagnostics;
    parser.position = 0U;
    parser.expression_depth = 0U;
    parser.fatal = false;
    root = parser_new(&parser, AST_PROGRAM, parser_current(&parser).span);
    if (root == NULL) {
        return false;
    }
    root->token.lexeme = parser.tokens->source_name;
    root->token.length = parser.tokens->source_name == NULL
                             ? 0U : strlen(parser.tokens->source_name);
    while (!parser_at(&parser, TOK_EOF) && !parser.fatal) {
        size_t before;
        AstNode *declaration;
        parser_skip_newlines(&parser);
        while (parser_match(&parser, TOK_SEMICOLON)) {
            parser_skip_newlines(&parser);
        }
        if (parser_at(&parser, TOK_EOF)) {
            break;
        }
        before = parser.position;
        declaration = parser_parse_declaration(&parser);
        if (declaration == NULL) {
            declaration = parser_parse_statement(&parser);
        }
        if (declaration != NULL) {
            (void)parser_append(&parser, &root->items, declaration);
        }
        if (parser.position == before) {
            parser_report(&parser, parser_current(&parser),
                          "parser could not advance at top level");
            parser_synchronize(&parser);
            if (parser.position == before && !parser_at(&parser, TOK_EOF)) {
                (void)parser_advance(&parser);
            }
        }
    }
    root->span.end_byte = parser_current(&parser).span.end_byte;
    *program = root;
    return !parser.fatal;
}
