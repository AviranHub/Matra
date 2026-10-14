#include "test.h"

#include "../../src/ast/ast.h"
#include "../../src/lexer/lexer.h"
#include "../../src/parser/parser.h"

#include <assert.h>

void test_parser(void) {
    static const unsigned char input[] =
        "let value = 1 + 2 * 3\n"
        "fn square(n: Int) -> Int = n ** 2\n"
        "let total = sum(x for x in values)\n"
        "match value { 404, 410 => 1, _ => 0 }\n"
        "let grouped = a + b @ c\n"
        "let pair = (1, 2)\n";
    SourceBuffer source;
    TokenArray tokens = { 0 };
    Diagnostics diagnostics;
    AstArena arena;
    AstNode *program = NULL;

    diagnostics_init(&diagnostics);
    ast_arena_init(&arena, 4096U);
    assert(source_buffer_from_bytes(&source, "parser-test", input,
                                    sizeof(input) - 1U, &diagnostics));
    assert(lexer_scan(&source, &tokens, &diagnostics));
    assert(parser_parse(&arena, &tokens, &diagnostics, &program));
    assert(program != NULL && program->kind == AST_PROGRAM);
    assert(program->items.length == 6U);
    assert(diagnostics.length == 0U);
    assert(program->items.head->node->kind == AST_BINDING);
    assert(program->items.head->node->c->kind == AST_BINARY);
    assert(program->items.head->node->c->b->kind == AST_BINARY);
    {
        const AstNode *grouped = program->items.head->next->next->next->next->node->c;
        const AstNode *pair = program->items.tail->node->c;
        assert(grouped->kind == AST_BINARY && grouped->op == TOK_PLUS);
        assert(grouped->b->kind == AST_BINARY && grouped->b->op == TOK_AT);
        assert(pair->kind == AST_TUPLE && pair->items.length == 2U);
        assert(pair->items.head->node->kind == AST_ARGUMENT);
    }
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);
}
