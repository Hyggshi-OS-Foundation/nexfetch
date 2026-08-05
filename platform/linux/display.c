#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <glob.h>

void platform_get_display(char *out, size_t size) {
    if (!out || size == 0) return;

    /* Try cache first -- display resolution rarely changes between runs */
    if (util_cache_read("display", out, size)) return;

    char res[128] = "";

    /* Try drm sysfs modes first */
    glob_t g;
    if (glob("/sys/class/drm/*/modes", 0, NULL, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; i++) {
            if (util_read_first_line(g.gl_pathv[i], res, sizeof(res))) {
                if (strlen(res) > 0) {
                    snprintf(out, size, "%s", res);
                    globfree(&g);
                    util_cache_write("display", out);
                    return;
                }
            }
        }
        globfree(&g);
    }

    /* Try xrandr if sysfs is empty */
    if (util_execute_cmd("xrandr 2>/dev/null | grep '\\*' | head -n 1 | awk '{print $1, \"@\", $2}'", res, sizeof(res)) == 0 && strlen(res) > 0) {
        char *p = strchr(res, '*');
        if (p) *p = '\0';
        p = strchr(res, '+');
        if (p) *p = '\0';
        util_trim(res);
        snprintf(out, size, "%s", res);
        util_cache_write("display", out);
        return;
    }

    snprintf(out, size, "Unknown");
}

#endif
