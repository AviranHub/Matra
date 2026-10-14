#include "test.h"

#include "../../src/common/error.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_error(void) {
    Diagnostics diagnostics;
    SourceSpan span = { 3U, 8U, 2U, 4U };
    char file[] = "sample.lang";
    char message[] = "unknown name";
    FILE *stream;
    char output[256];
    size_t bytes_read;

    diagnostics_init(&diagnostics);
    assert(diagnostics_add(&diagnostics, DIAG_ERROR, DIAG_CODE_NAME, file,
                           span, message, "declare the name first"));
    file[0] = 'X';
    message[0] = 'X';
    assert(diagnostics.length == 1U);
    assert(strcmp(diagnostics.items[0].file, "sample.lang") == 0);
    assert(strcmp(diagnostics.items[0].message, "unknown name") == 0);
    stream = tmpfile();
    assert(stream != NULL);
    assert(diagnostics_print(&diagnostics, stream));
    rewind(stream);
    bytes_read = fread(output, 1U, sizeof(output) - 1U, stream);
    output[bytes_read] = '\0';
    assert(strstr(output, "sample.lang:2:4: error [E0300]: unknown name") != NULL);
    assert(strstr(output, "hint: declare the name first") != NULL);
    assert(fclose(stream) == 0);
    diagnostics_clear(&diagnostics);
    assert(diagnostics.length == 0U);
    diagnostics_destroy(&diagnostics);
    assert(diagnostics.items == NULL);
}
