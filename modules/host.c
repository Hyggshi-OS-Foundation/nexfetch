#include "nexfetch.h"

void module_detect_host(char *out, size_t max_len) {
    platform_get_host(out, max_len);
}
