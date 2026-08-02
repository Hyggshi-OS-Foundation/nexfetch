#include "nexfetch.h"

void module_detect_disk(char *out, size_t max_len) {
    platform_get_disk(out, max_len);
}
