#include "test.h"

#include "../../src/ast/ast.h"
#include "../../src/lexer/lexer.h"
#include "../../src/parser/parser.h"
#include "../../src/sema/sema.h"

#include <assert.h>
#include <string.h>

static void analyze_text(const char *text, Diagnostics *diagnostics,
                         SemaContext *sema, AstArena *arena,
                         SourceBuffer *source, TokenArray *tokens,
                         AstNode **program) {
    diagnostics_init(diagnostics);
    ast_arena_init(arena, 4096U);
    assert(source_buffer_from_bytes(source, "sema-test",
                                    (const unsigned char *)text, strlen(text),
                                    diagnostics));
    assert(lexer_scan(source, tokens, diagnostics));
    assert(parser_parse(arena, tokens, diagnostics, program));
    sema_context_init(sema);
    assert(sema->implementation != NULL);
    assert(sema_analyze(*program, sema, diagnostics));
}

void test_sema(void) {
    Diagnostics diagnostics;
    SemaContext sema;
    AstArena arena;
    SourceBuffer source;
    TokenArray tokens = { 0 };
    AstNode *program = NULL;

    analyze_text("let count: Int = 4\nvar total = count + 3\n"
                 "let doubled = double(count)\n"
                 "fn double(x: Int) -> Int = x * 2\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    assert(sema_type_of(&sema, program->items.head->node->c)->kind ==
           SEMA_TYPE_INT);
    assert(sema_escape_of(&sema, program->items.head->node->c) ==
           SEMA_ESCAPE_ESCAPES);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("fn identity[T](value: T) -> T = value\n"
                 "let copied = identity(5)\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    assert(sema_type_of(&sema, program->items.head->next->node->c)->kind ==
           SEMA_TYPE_INT);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("fn combine(first: Int, second: Int) -> Int = first + second\n"
                 "let answer = combine(second: 2, first: 1)\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("fn echo(value: Int | String) -> Int | String = value\n"
                 "let result: Int | String = echo(7)\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("let invalid: Int | String = true\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length > 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("let value: Bool | Int = true\n"
                 "let result = match value { true => 1, false => 0 }\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length > 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("let value: Bool | Int = true\n"
                 "let result = match value { true => 1, false => 0, _ => 2 }\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("fn same[T](left: T, right: T) -> T = left\n"
                 "let invalid = same(1, true)\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length > 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("fn first(...values: Int) -> Int = 1\n"
                 "let selected = first(1, 2, 3)\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("fn combine(first: Int, second: Int) -> Int = first + second\n"
                 "let answer = combine(first: true, first: 1, extra: 3)\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length >= 3U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("let [first, second] = [1, 2]\n"
                 "for value in [first, second] { print(value) }\n"
                 "let chosen = match true { true => 1, false => 0 }\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("class Point { let x: Float64\n }\n"
                 "let point: Point = Point()\n"
                 "let invalid = point.missing\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length > 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("class Point { init(self.x: Float64) {} }\n"
                 "let invalid: Point = Point(true)\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length > 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("class Point {\n"
                 "    init(self.x: Float64) {}\n"
                 "    public fn value() -> Float64 = self.x\n"
                 "}\n"
                 "let point: Point = Point(2.0)\n"
                 "let result: Float64 = point.value()\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("type Choice = Yes(Int) | No\n"
                 "let selected: Choice = Yes(7)\n"
                 "let result = match selected { Yes(value) => value, No => 0 }\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("class Point {\n"
                 "    init(self.x: Float64) {}\n"
                 "    fn value() -> Float64 = self.x\n"
                 "}\n"
                 "let point: Point = Point(2.0)\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("type Choice = Yes(Int) | No\n"
                 "let selected: Choice = Yes(7)\n"
                 "let result = match selected { Yes(value) => value, No => 0 }\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("type Choice = Yes(Int) | No\n"
                 "let selected: Choice = Yes(7)\n"
                 "let result = match selected { Yes => 1, No => 0 }\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length > 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("type Choice = Yes | No\n"
                 "let selected: Choice = Yes\n"
                 "let result = match selected { Yes => 1 }\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length > 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("let values: List[Int] = [1]\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("let broken = match true { true => 1 }\n"
                 "let invalid: Int = false\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length >= 2U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("let value = 2\nvalue = 3\nmissing + true\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length >= 2U);
    assert(strcmp(diagnostics.items[0].file, "sema-test") == 0);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("class Point {\n"
                 "    let x: Int = \"bad\"\n"
                 "    fn value() = 1\n"
                 "    init(self.y: Int) {}\n"
                 "}\n"
                 "let point = Point(true)\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length >= 2U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("let empty = ()\n"
                 "let pair = (1, \"two\")\n"
                 "let transform = (x: Int) => x + 1\n"
                 "let doubled = [x * 2 for x in [1, 2]]\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    assert(sema_type_of(&sema, program->items.head->node->c)->kind ==
           SEMA_TYPE_UNIT);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("let values = [1, 2]\n"
                 "let invalid = values[true]\n"
                 "let also_invalid = 1 .. \"end\"\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length >= 2U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("interface Measure { fn value() -> Int }\n"
                 "class Counter: Measure { public fn value() -> Int = 1 }\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("interface Measure { fn value() -> Int }\n"
                 "class Counter: Measure {}\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length > 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("class Counter { let value: Int }\n"
                 "let counter = Counter()\n"
                 "let value = counter.value\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length > 0U);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);

    tokens = (TokenArray){ 0 };
    program = NULL;
    analyze_text("fn answer() { return 42 }\n"
                 "let result = answer()\n",
                 &diagnostics, &sema, &arena, &source, &tokens, &program);
    assert(diagnostics.length == 0U);
    assert(sema_type_of(&sema, program->items.head->next->node->c)->kind ==
           SEMA_TYPE_INT);
    sema_context_destroy(&sema);
    ast_arena_destroy(&arena);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);
}
