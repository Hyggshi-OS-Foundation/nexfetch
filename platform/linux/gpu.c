#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

void platform_get_gpu(char *out, size_t size) {
    if (!out || size == 0) return;

    FILE *fp = popen("lspci 2>/dev/null | grep -E 'VGA|3D|Display' | head -n 1", "r");
    if (!fp) {
        snprintf(out, size, "Unknown");
        return;
    }

    char line[512] = "";
    if (fgets(line, sizeof(line), fp) == NULL) {
        pclose(fp);
        snprintf(out, size, "Unknown");
        return;
    }
    pclose(fp);
    util_trim(line);

    /*
     * lspci format:
     *   0000:00:02.0 VGA compatible controller: Intel Corporation ... [Iris Xe Graphics] (rev 01)
     *
     * Strategy: find "controller:" or "adapter:" or simply the LAST colon
     * that ends the device-class description (which always ends in ": ").
     * We scan for ": " which separates the class from the vendor+device name.
     * The bus address part "0000:00:02.0" is followed by a space, not ": ".
     */
    char *name = NULL;
    char *p = line;
    while (*p) {
        if (p[0] == ':' && p[1] == ' ') {
            name = p + 2;   /* candidate: keep looking for a later one */
        }
        p++;
    }

    if (!name || *name == '\0') {
        snprintf(out, size, "%s", line);
        return;
    }

    /* Strip trailing " (rev XX)" */
    char *rev = strstr(name, " (rev ");
    if (rev) *rev = '\0';

    snprintf(out, size, "%s", name);
}
