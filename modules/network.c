#include "nexfetch.h"

void module_detect_network(char *out, size_t max_len) {
    platform_get_network(out, max_len);
}
