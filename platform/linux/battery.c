#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

void platform_get_battery(char *out, size_t size) {
    if (!out || size == 0) return;
    char cap_str[32] = "";
    char status_str[32] = "";

    if (util_read_first_line("/sys/class/power_supply/BAT0/capacity", cap_str, sizeof(cap_str)) ||
        util_read_first_line("/sys/class/power_supply/BAT1/capacity", cap_str, sizeof(cap_str))) {

        util_read_first_line("/sys/class/power_supply/BAT0/status", status_str, sizeof(status_str));
        if (strlen(status_str) == 0) {
            util_read_first_line("/sys/class/power_supply/BAT1/status", status_str, sizeof(status_str));
        }

        if (strlen(status_str) > 0) {
            snprintf(out, size, "%s%% [%s]", cap_str, status_str);
        } else {
            snprintf(out, size, "%s%%", cap_str);
        }
        return;
    }

    snprintf(out, size, "N/A (Desktop/AC)");
}

#endif

