#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

void platform_get_os(char *out, size_t size, char *distro_id, size_t distro_id_size) {
    if (!out || size == 0) return;
    snprintf(out, size, "Linux");
    if (distro_id && distro_id_size > 0) {
        snprintf(distro_id, distro_id_size, "tux");
    }

    FILE *f = fopen("/etc/os-release", "r");
    if (!f) f = fopen("/usr/lib/os-release", "r");
    if (!f) return;

    char line[256];
    char pretty_name[128] = "";
    char id[64] = "";

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
            char *val = line + 12;
            if (*val == '"' || *val == '\'') val++;
            size_t len = strlen(val);
            if (len > 0 && (val[len - 1] == '"' || val[len - 1] == '\'' || val[len - 1] == '\n')) {
                val[len - 1] = '\0';
            }
            if (len > 1 && (val[len - 2] == '"' || val[len - 2] == '\'')) {
                val[len - 2] = '\0';
            }
            snprintf(pretty_name, sizeof(pretty_name), "%s", val);
        } else if (strncmp(line, "ID=", 3) == 0) {
            char *val = line + 3;
            if (*val == '"' || *val == '\'') val++;
            size_t len = strlen(val);
            if (len > 0 && (val[len - 1] == '"' || val[len - 1] == '\'' || val[len - 1] == '\n')) {
                val[len - 1] = '\0';
            }
            snprintf(id, sizeof(id), "%s", val);
        }
    }
    fclose(f);

    if (strlen(pretty_name) > 0) {
        snprintf(out, size, "%s", pretty_name);
    }
    if (distro_id && strlen(id) > 0) {
        snprintf(distro_id, distro_id_size, "%s", id);
    }
}
