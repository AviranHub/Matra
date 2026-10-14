#include "test.h"

#include "../../src/ast/ast.h"
#include "../../src/lexer/lexer.h"
#include "../../src/parser/parser.h"
#include "../../src/sema/sema.h"
#include "../../src/source/source.h"

#include <assert.h>

void test_calculator_example(void) {
    SourceBuffer source;
    TokenArray tokens = { 0 };
    Diagnostics diagnostics;
    AstArena arena;
    SemaContext sema;
    AstNode *program = NULL;

    diagnostics_init(&diagnostics);
    ast_arena_init(&arena, 4096U);
    sema_context_init(&sema);
    assert(source_buffer_load(&source, "examples/calculator.mat",
                              &diagnostics));
    assert(lexer_scan(&source, &tokens, &diagnostics));
    assert(parser_parse(&arena, &tokens, &diagnostics, &program));
    assert(sema_analyze(program, &sema, &diagnostics));
    assert(diagnostics.length == 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);
}
