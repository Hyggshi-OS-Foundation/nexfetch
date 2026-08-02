#include "nexfetch.h"

void module_detect_battery(char *out, size_t max_len) {
    platform_get_battery(out, max_len);
}
