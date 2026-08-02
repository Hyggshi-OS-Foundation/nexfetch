#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <sys/sysinfo.h>

void platform_get_uptime(char *out, size_t size) {
    if (!out || size == 0) return;
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        util_format_duration(info.uptime, out, size);
    } else {
        snprintf(out, size, "Unknown");
    }
}
