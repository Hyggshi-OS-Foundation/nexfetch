#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

void platform_get_cpu(char *out, size_t size) {
    if (!out || size == 0) return;
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        snprintf(out, size, "Unknown");
        return;
    }

    char line[256];
    char model[128] = "";
    int cores = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "model name", 10) == 0 || strncmp(line, "Hardware", 8) == 0 || strncmp(line, "Processor", 9) == 0) {
            char *colon = strchr(line, ':');
            if (colon && strlen(model) == 0) {
                snprintf(model, sizeof(model), "%s", util_trim(colon + 1));
            }
        }
        if (strncmp(line, "processor", 9) == 0) {
            cores++;
        }
    }
    fclose(f);

    if (strlen(model) > 0) {
        if (cores > 0) {
            snprintf(out, size, "%s (%d)", model, cores);
        } else {
            snprintf(out, size, "%s", model);
        }
    } else {
        snprintf(out, size, "Unknown CPU");
    }
}
