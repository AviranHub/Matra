#ifndef MATRA_COMMON_PLATFORM_H
#define MATRA_COMMON_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const char *os_name;
    const char *arch_name;
    size_t pointer_bits;
    bool little_endian;
} PlatformInfo;

PlatformInfo platform_info(void);
bool platform_path_is_absolute(const char *path);
char platform_path_separator(void);

#endif
