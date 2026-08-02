#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

void platform_get_memory(char *out, size_t size) {
    if (!out || size == 0) return;
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) {
        snprintf(out, size, "Unknown");
        return;
    }

    char line[256];
    unsigned long long mem_total_kb = 0;
    unsigned long long mem_avail_kb = 0;
    unsigned long long mem_free_kb = 0;
    unsigned long long buffers_kb = 0;
    unsigned long long cached_kb = 0;

    while (fgets(line, sizeof(line), f)) {
        if      (strncmp(line, "MemTotal:",     9)  == 0) sscanf(line + 9,  "%llu", &mem_total_kb);
        else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line + 13, "%llu", &mem_avail_kb);
        else if (strncmp(line, "MemFree:",      8)  == 0) sscanf(line + 8,  "%llu", &mem_free_kb);
        else if (strncmp(line, "Buffers:",      8)  == 0) sscanf(line + 8,  "%llu", &buffers_kb);
        else if (strncmp(line, "Cached:",       7)  == 0) sscanf(line + 7,  "%llu", &cached_kb);
    }
    fclose(f);

    if (mem_total_kb == 0) {
        snprintf(out, size, "Unknown");
        return;
    }

    unsigned long long mem_used_kb;
    if (mem_avail_kb > 0) {
        mem_used_kb = mem_total_kb - mem_avail_kb;
    } else {
        mem_used_kb = mem_total_kb - (mem_free_kb + buffers_kb + cached_kb);
    }

    int pct = (int)((double)mem_used_kb / (double)mem_total_kb * 100.0);

    /* Auto-select unit: show MiB if used < 1 GiB, else GiB */
    if (mem_total_kb < 1024ULL * 1024ULL) {
        /* Small system: MiB */
        unsigned long long used_mib  = mem_used_kb  / 1024ULL;
        unsigned long long total_mib = mem_total_kb / 1024ULL;
        snprintf(out, size, "%llu MiB / %llu MiB (%d%%)", used_mib, total_mib, pct);
    } else {
        /* Large system: GiB with 2 decimals */
        double used_gib  = (double)mem_used_kb  / (1024.0 * 1024.0);
        double total_gib = (double)mem_total_kb / (1024.0 * 1024.0);
        snprintf(out, size, "%.2f GiB / %.2f GiB (%d%%)", used_gib, total_gib, pct);
    }
}

#endif

