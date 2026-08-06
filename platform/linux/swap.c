#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

void platform_get_swap(char *out, size_t size) {
    if (!out || size == 0) return;

    PlatformMeminfo mi;
    platform_read_meminfo(&mi);

    if (mi.swap_total_kb == 0) {
        snprintf(out, size, "Disabled / None");
        return;
    }

    unsigned long long swap_used_kb = mi.swap_total_kb - mi.swap_free_kb;
    int pct = (int)((double)swap_used_kb / (double)mi.swap_total_kb * 100.0);

    if (mi.swap_total_kb < 1024ULL * 1024ULL) {
        unsigned long long used_mib = swap_used_kb / 1024ULL;
        unsigned long long total_mib = mi.swap_total_kb / 1024ULL;
        snprintf(out, size, "%llu MiB / %llu MiB (%d%%)", used_mib, total_mib, pct);
    } else {
        double used_gib = (double)swap_used_kb / (1024.0 * 1024.0);
        double total_gib = (double)mi.swap_total_kb / (1024.0 * 1024.0);
        snprintf(out, size, "%.2f GiB / %.2f GiB (%d%%)", used_gib, total_gib, pct);
    }
}

#endif
