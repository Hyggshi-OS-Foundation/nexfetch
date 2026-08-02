#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

void platform_get_swap(char *out, size_t size) {
    if (!out || size == 0) return;
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) {
        snprintf(out, size, "Unknown");
        return;
    }

    char line[256];
    unsigned long long swap_total_kb = 0;
    unsigned long long swap_free_kb = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "SwapTotal:", 10) == 0) sscanf(line + 10, "%llu", &swap_total_kb);
        else if (strncmp(line, "SwapFree:", 9) == 0) sscanf(line + 9, "%llu", &swap_free_kb);
    }
    fclose(f);

    if (swap_total_kb == 0) {
        snprintf(out, size, "Disabled / None");
        return;
    }

    unsigned long long swap_used_kb = swap_total_kb - swap_free_kb;
    int pct = (int)((double)swap_used_kb / (double)swap_total_kb * 100.0);

    if (swap_total_kb < 1024ULL * 1024ULL) {
        unsigned long long used_mib = swap_used_kb / 1024ULL;
        unsigned long long total_mib = swap_total_kb / 1024ULL;
        snprintf(out, size, "%llu MiB / %llu MiB (%d%%)", used_mib, total_mib, pct);
    } else {
        double used_gib = (double)swap_used_kb / (1024.0 * 1024.0);
        double total_gib = (double)swap_total_kb / (1024.0 * 1024.0);
        snprintf(out, size, "%.2f GiB / %.2f GiB (%d%%)", used_gib, total_gib, pct);
    }
}
