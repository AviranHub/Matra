#include "test.h"

#include "../../src/common/platform.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>

void test_platform(void) {
    PlatformInfo info = platform_info();

    assert(info.os_name != NULL);
    assert(info.arch_name != NULL);
    assert(info.pointer_bits == sizeof(void *) * (size_t)CHAR_BIT);
    assert(info.pointer_bits == 64U);
    assert(!platform_path_is_absolute(""));
    assert(!platform_path_is_absolute(NULL));
#if defined(_WIN32)
    assert(platform_path_is_absolute("C:\\source.lang"));
    assert(platform_path_is_absolute("\\\\server\\share"));
    assert(!platform_path_is_absolute("\\rooted-only"));
    assert(platform_path_separator() == '\\');
#else
    assert(platform_path_is_absolute("/tmp/source.lang"));
    assert(!platform_path_is_absolute("relative/source.lang"));
    assert(platform_path_separator() == '/');
#endif
}
