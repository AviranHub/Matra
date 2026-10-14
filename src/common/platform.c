#include "platform.h"

#include <limits.h>
#include <stdint.h>

_Static_assert(sizeof(void *) * CHAR_BIT == 64U,
               "Matra compiler host must use a 64-bit architecture");

static const char *platform_os_name(void) {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__) && defined(__MACH__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#elif defined(__FreeBSD__)
    return "freebsd";
#else
    return "unknown";
#endif
}

static const char *platform_arch_name(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#else
    return "unknown";
#endif
}

PlatformInfo platform_info(void) {
    const uint16_t endian_probe = UINT16_C(1);
    const unsigned char *bytes = (const unsigned char *)&endian_probe;
    PlatformInfo info;

    info.os_name = platform_os_name();
    info.arch_name = platform_arch_name();
    info.pointer_bits = sizeof(void *) * (size_t)CHAR_BIT;
    info.little_endian = bytes[0] == 1U;
    return info;
}

bool platform_path_is_absolute(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return false;
    }
#if defined(_WIN32)
    if ((path[0] == '\\' || path[0] == '/') &&
        (path[1] == '\\' || path[1] == '/')) {
        return true;
    }
    if (((path[0] >= 'A' && path[0] <= 'Z') ||
         (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
        return true;
    }
    return false;
#else
    return path[0] == '/';
#endif
}

char platform_path_separator(void) {
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}
