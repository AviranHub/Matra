#include "test.h"

#include "../../src/source/source.h"

#include <assert.h>
#include <string.h>

void test_source(void) {
    SourceBuffer source;
    Diagnostics diagnostics;
    const unsigned char input[] = { 'a', '\0', 'b' };

    diagnostics_init(&diagnostics);
    assert(source_buffer_from_bytes(&source, "memory", input, sizeof(input),
                                    &diagnostics));
    assert(strcmp(source.name, "memory") == 0);
    assert(source.length == sizeof(input));
    assert(memcmp(source.bytes, input, sizeof(input)) == 0);
    assert(source.bytes != input);
    source_buffer_destroy(&source);
    diagnostics_destroy(&diagnostics);
}
