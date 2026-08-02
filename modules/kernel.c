#include "nexfetch.h"
#include <stdio.h>

void module_detect_kernel(char *out, size_t max_len) {
    platform_get_kernel(out, max_len);
}
