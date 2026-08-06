#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <pthread.h>
#endif

#ifndef _WIN32
static pthread_mutex_t s_meminfo_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
static PlatformMeminfo s_meminfo;
static int s_meminfo_loaded = 0;

void platform_read_meminfo(PlatformMeminfo *info) {
    if (!info) return;

#ifndef _WIN32
    pthread_mutex_lock(&s_meminfo_mutex);
#endif
    if (!s_meminfo_loaded) {
        memset(&s_meminfo, 0, sizeof(s_meminfo));
        FILE *f = fopen("/proc/meminfo", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if      (strncmp(line, "MemTotal:",     9)  == 0) sscanf(line + 9,  "%llu", &s_meminfo.mem_total_kb);
                else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line + 13, "%llu", &s_meminfo.mem_avail_kb);
                else if (strncmp(line, "MemFree:",      8)  == 0) sscanf(line + 8,  "%llu", &s_meminfo.mem_free_kb);
                else if (strncmp(line, "Buffers:",      8)  == 0) sscanf(line + 8,  "%llu", &s_meminfo.buffers_kb);
                else if (strncmp(line, "Cached:",       7)  == 0) sscanf(line + 7,  "%llu", &s_meminfo.cached_kb);
                else if (strncmp(line, "SwapTotal:",   10)  == 0) sscanf(line + 10, "%llu", &s_meminfo.swap_total_kb);
                else if (strncmp(line, "SwapFree:",     9)  == 0) sscanf(line + 9,  "%llu", &s_meminfo.swap_free_kb);
            }
            fclose(f);
        }
        s_meminfo_loaded = 1;
    }
    *info = s_meminfo;
#ifndef _WIN32
    pthread_mutex_unlock(&s_meminfo_mutex);
#endif
}

void platform_get_memory(char *out, size_t size) {
    if (!out || size == 0) return;

    PlatformMeminfo mi;
    platform_read_meminfo(&mi);

    if (mi.mem_total_kb == 0) {
        snprintf(out, size, "Unknown");
        return;
    }

    unsigned long long mem_used_kb;
    if (mi.mem_avail_kb > 0) {
        mem_used_kb = mi.mem_total_kb - mi.mem_avail_kb;
    } else {
        mem_used_kb = mi.mem_total_kb - (mi.mem_free_kb + mi.buffers_kb + mi.cached_kb);
    }

    int pct = (int)((double)mem_used_kb / (double)mi.mem_total_kb * 100.0);

    if (mi.mem_total_kb < 1024ULL * 1024ULL) {
        unsigned long long used_mib  = mem_used_kb / 1024ULL;
        unsigned long long total_mib = mi.mem_total_kb / 1024ULL;
        snprintf(out, size, "%llu MiB / %llu MiB (%d%%)", used_mib, total_mib, pct);
    } else {
        double used_gib  = (double)mem_used_kb / (1024.0 * 1024.0);
        double total_gib = (double)mi.mem_total_kb / (1024.0 * 1024.0);
        snprintf(out, size, "%.2f GiB / %.2f GiB (%d%%)", used_gib, total_gib, pct);
    }
}

#endif
