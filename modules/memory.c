#include "nexfetch.h"

void module_detect_memory(char *out, size_t max_len) {
    platform_get_memory(out, max_len);
}
