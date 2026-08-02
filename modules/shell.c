#include "nexfetch.h"

void module_detect_shell(char *out, size_t max_len) {
    platform_get_shell(out, max_len);
}
