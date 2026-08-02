#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_get_locale(char *out, size_t size) {
    if (!out || size == 0) return;

    const char *lc_all = getenv("LC_ALL");
    const char *lang = getenv("LANG");

    if (lc_all && lc_all[0] != '\0') {
        snprintf(out, size, "%s", lc_all);
        return;
    }

    if (lang && lang[0] != '\0') {
        snprintf(out, size, "%s", lang);
        return;
    }

    char buf[128] = "";
    if (util_read_first_line("/etc/default/locale", buf, sizeof(buf))) {
        char *eq = strchr(buf, '=');
        if (eq) {
            char *val = eq + 1;
            if (*val == '"' || *val == '\'') val++;
            size_t len = strlen(val);
            if (len > 0 && (val[len - 1] == '"' || val[len - 1] == '\'')) val[len - 1] = '\0';
            snprintf(out, size, "%s", val);
            return;
        }
    }

    snprintf(out, size, "Unknown");
}

#endif

