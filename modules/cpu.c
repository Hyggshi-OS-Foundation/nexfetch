#include "nexfetch.h"

void module_detect_cpu(char *out, size_t max_len) {
    platform_get_cpu(out, max_len);
}
