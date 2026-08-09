#include "nexfetch.h"

void module_detect_security(char *out, size_t max_len) {
    platform_get_security(out, max_len);
}
