#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <sys/statvfs.h>

void platform_get_disk(char *out, size_t size) {
    if (!out || size == 0) return;
    struct statvfs stat;
    if (statvfs("/", &stat) == 0) {
        unsigned long long total = stat.f_blocks * stat.f_frsize;
        unsigned long long free_bytes = stat.f_bavail * stat.f_frsize;
        unsigned long long used = total - free_bytes;

        char total_str[32], used_str[32];
        util_format_bytes(total, total_str, sizeof(total_str));
        util_format_bytes(used, used_str, sizeof(used_str));

        int percentage = (total > 0) ? (int)(((double)used / (double)total) * 100.0) : 0;
        snprintf(out, size, "%s / %s (%d%%) [/]", used_str, total_str, percentage);
    } else {
        snprintf(out, size, "Unknown");
    }
}

#endif

