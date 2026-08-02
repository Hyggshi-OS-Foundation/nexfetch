#include "nexfetch.h"
#include <stdio.h>

void module_detect_custom(char *out, size_t max_len) {
    snprintf(out, max_len, "Nexfetch Modular CLI v%s", NEXFETCH_VERSION);
}
