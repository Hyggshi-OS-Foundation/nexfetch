#include "nexfetch.h"

void module_detect_uptime(char *out, size_t max_len) {
    platform_get_uptime(out, max_len);
}
