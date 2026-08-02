#include "nexfetch.h"
#include <stdio.h>

void module_detect_os(char *out, size_t max_len) {
    platform_get_os(out, max_len, g_config.distro_id, sizeof(g_config.distro_id));
}
