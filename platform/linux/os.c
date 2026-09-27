#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static void to_lower_inplace(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

void platform_get_os(char *out, size_t size,
                      char *distro_id, size_t distro_id_size,
                      char *distro_id_like, size_t distro_id_like_size) {
    if (!out || size == 0) return;
    const char *fallback_os = "Linux";
    if (distro_id && distro_id_size > 0) {
        snprintf(distro_id, distro_id_size, "tux");
    }
    if (distro_id_like && distro_id_like_size > 0) {
        distro_id_like[0] = '\0';
    }

    FILE *f = fopen("/etc/os-release", "r");
    if (!f) f = fopen("/usr/lib/os-release", "r");

    if (!f) {
        snprintf(out, size, "%s", fallback_os);
        return;
    }

    char pretty[160] = "";
    char name[128] = "";
    char version[128] = "";
    char id[64] = "";
    char id_like[256] = "";
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;

        size_t keylen = (size_t)(eq - line);
        if (keylen >= sizeof(line)) keylen = sizeof(line) - 1;
        char key[64] = "";
        if (keylen >= sizeof(key)) keylen = sizeof(key) - 1;
        memcpy(key, line, keylen);
        key[keylen] = '\0';

        /* Value: strip trailing newline/CR and surrounding quotes. */
        char *val = eq + 1;
        size_t vlen = strlen(val);
        if (vlen > 0 && (val[vlen-1] == '\n' || val[vlen-1] == '\r')) {
            val[vlen-1] = '\0';
            vlen--;
        }
        if (vlen >= 2 && val[0] == '"' && val[vlen-1] == '"') {
            val[vlen-1] = '\0';
            val++;
            vlen -= 2;
        }

        if (keylen == 11 && strcmp(key, "PRETTY_NAME") == 0) {
            snprintf(pretty, sizeof(pretty), "%s", val);
        } else if (keylen == 4 && strcmp(key, "NAME") == 0) {
            snprintf(name, sizeof(name), "%s", val);
        } else if (keylen == 7 && strcmp(key, "VERSION") == 0) {
            snprintf(version, sizeof(version), "%s", val);
        } else if (keylen == 2 && strcmp(key, "ID") == 0) {
            snprintf(id, sizeof(id), "%s", val);
        } else if (keylen == 7 && strcmp(key, "ID_LIKE") == 0) {
            snprintf(id_like, sizeof(id_like), "%s", val);
        }
    }
    fclose(f);

    /* Preferred: PRETTY_NAME. Otherwise compose NAME + VERSION. */
    if (pretty[0] != '\0') {
        snprintf(out, size, "%s", pretty);
    } else if (name[0] != '\0') {
        if (version[0] != '\0')
            snprintf(out, size, "%s %s", name, version);
        else
            snprintf(out, size, "%s", name);
    } else {
        snprintf(out, size, "%s", fallback_os);
    }

    /* ID drives the logo database lookup. Normalize to lowercase since the
     * os-release spec requires lowercase IDs, but be defensive about
     * malformed files. */
    if (distro_id && distro_id_size > 0 && id[0] != '\0') {
        snprintf(distro_id, distro_id_size, "%s", id);
        to_lower_inplace(distro_id);
    }

    /* ID_LIKE is carried separately and verbatim (still space-separated).
     * It is a fallback list only -- logo_load() must try distro_id first
     * and only walk this list if that lookup misses. It must never be
     * written into distro_id or otherwise override it. */
    if (distro_id_like && distro_id_like_size > 0 && id_like[0] != '\0') {
        snprintf(distro_id_like, distro_id_like_size, "%s", id_like);
        to_lower_inplace(distro_id_like);
    }
}

#endif
