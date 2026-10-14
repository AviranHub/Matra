#include "test.h"

#include "../../src/common/str.h"

#include <assert.h>
#include <string.h>

void test_str(void) {
    StrBuf str;
    StrView view;
    const char embedded[] = { 'a', '\0', 'b' };
    const char expected[] = { 'a', 'l', 'p', 'h', 'a', 'a', '\0', 'b',
                              'a', '\0', 'b' };

    str_init(&str);
    assert(strcmp(str_cstr(&str), "") == 0);
    assert(str_append_cstr(&str, "alpha"));
    assert(str_append(&str, embedded, sizeof(embedded)));
    assert(str_append(&str, str.data + 5U, 3U));
    view = str_view(&str);
    assert(view.length == sizeof(expected));
    assert(memcmp(view.data, expected, sizeof(expected)) == 0);
    assert(str_equal(view, str_view(&str)));
    assert(!str_equal(view, (StrView){ "alpha", 5U }));
    str_clear(&str);
    assert(str.length == 0U);
    assert(strcmp(str_cstr(&str), "") == 0);
    str_destroy(&str);
    assert(str.data == NULL);
}
