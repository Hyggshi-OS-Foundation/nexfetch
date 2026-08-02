#include "nexfetch.h"

void module_detect_gpu(char *out, size_t max_len) {
    platform_get_gpu(out, max_len);
}
