#include "test.h"

#include "../../src/lexer/lexer.h"

#include <assert.h>
#include <string.h>

void test_lexer(void) {
    static const unsigned char input[] =
        "let value = 2 ** 3\r\nprint(\"v={value}\")";
    SourceBuffer source;
    TokenArray tokens = { 0 };
    Diagnostics diagnostics;
    size_t index;
    bool found_interpolation = false;

    diagnostics_init(&diagnostics);
    assert(source_buffer_from_bytes(&source, "lexer-test", input,
                                    sizeof(input) - 1U, &diagnostics));
    assert(lexer_scan(&source, &tokens, &diagnostics));
    assert(diagnostics.length == 0U);
    assert(tokens.length > 10U);
    assert(tokens.items[0].kind == TOK_LET);
    assert(tokens.items[1].kind == TOK_IDENTIFIER);
    assert(tokens.items[1].span.line == 1U);
    for (index = 0U; index < tokens.length; ++index) {
        if (tokens.items[index].kind == TOK_INTERP_BEGIN) {
            found_interpolation = true;
            break;
        }
    }
    assert(found_interpolation);
    token_array_destroy(&tokens);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);
}
