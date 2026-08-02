#include "nexfetch.h"

void module_detect_locale(char *out, size_t max_len) {
    platform_get_locale(out, max_len);
}
