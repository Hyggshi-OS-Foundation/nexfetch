#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

void platform_get_host(char *out, size_t size) {
    if (!out || size == 0) return;
    char name[128] = "";
    char vendor[128] = "";

    if (util_read_first_line("/sys/class/dmi/id/product_name", name, sizeof(name))) {
        util_read_first_line("/sys/class/dmi/id/sys_vendor", vendor, sizeof(vendor));
        if (strlen(vendor) > 0 && strstr(name, vendor) == NULL) {
            snprintf(out, size, "%s %s", vendor, name);
        } else {
            snprintf(out, size, "%s", name);
        }
        return;
    }

    if (util_read_first_line("/sys/firmware/devicetree/base/model", name, sizeof(name))) {
        snprintf(out, size, "%s", name);
        return;
    }

    snprintf(out, size, "PC/Generic");
}
