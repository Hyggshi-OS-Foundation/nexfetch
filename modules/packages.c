#include "nexfetch.h"

void module_detect_packages(char *out, size_t max_len) {
    platform_get_packages(out, max_len);
}
