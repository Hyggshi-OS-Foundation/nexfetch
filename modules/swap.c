#include "nexfetch.h"

void module_detect_swap(char *out, size_t max_len) {
    platform_get_swap(out, max_len);
}
