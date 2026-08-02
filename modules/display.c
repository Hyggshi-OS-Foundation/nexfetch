#include "nexfetch.h"

void module_detect_display(char *out, size_t max_len) {
    platform_get_display(out, max_len);
}
