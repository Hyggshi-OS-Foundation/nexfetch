#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>

/* Returns 1 if any AC/USB power-supply node under /sys/class/power_supply
 * reports online == 1. Battery "status" files are notoriously unreliable
 * across vendors (some report "Discharging"/"Unknown" even while plugged
 * in, e.g. when a charge threshold is capping charge), so this is used as
 * a corroborating signal rather than trusting the BATn status file blindly. */
static int ac_is_online(void) {
    DIR *d = opendir("/sys/class/power_supply");
    if (!d) return -1; /* unknown */

    int found_adapter = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        const char *name = entry->d_name;
        if (name[0] == '.') continue;

        char type_path[300];
        snprintf(type_path, sizeof(type_path), "/sys/class/power_supply/%s/type", name);
        char type_str[32] = "";
        util_read_first_line(type_path, type_str, sizeof(type_str));

        /* AC adapters/USB-PD inputs report type "Mains" or "USB" */
        if (strcmp(type_str, "Mains") != 0 && strcmp(type_str, "USB") != 0) continue;

        char online_path[300];
        snprintf(online_path, sizeof(online_path), "/sys/class/power_supply/%s/online", name);
        char online_str[8] = "";
        if (util_read_first_line(online_path, online_str, sizeof(online_str))) {
            found_adapter = 1;
            if (strcmp(online_str, "1") == 0) {
                closedir(d);
                return 1;
            }
        }
    }
    closedir(d);
    return found_adapter ? 0 : -1;
}

void platform_get_battery(char *out, size_t size) {
    if (!out || size == 0) return;
    char cap_str[32] = "";
    char status_str[32] = "";

    /* Try each battery index and keep capacity/status paired to the SAME
     * battery, instead of mixing BAT0's status with BAT1's capacity. */
    const char *indices[] = { "BAT0", "BAT1" };
    int found = 0;
    for (size_t i = 0; i < sizeof(indices) / sizeof(indices[0]); i++) {
        char cap_path[300], status_path[300];
        snprintf(cap_path, sizeof(cap_path), "/sys/class/power_supply/%s/capacity", indices[i]);
        snprintf(status_path, sizeof(status_path), "/sys/class/power_supply/%s/status", indices[i]);

        if (util_read_first_line(cap_path, cap_str, sizeof(cap_str))) {
            util_read_first_line(status_path, status_str, sizeof(status_str));
            found = 1;
            break;
        }
    }

    if (!found) {
        snprintf(out, size, "N/A (Desktop/AC)");
        return;
    }

    /* Corroborate against AC/USB online state. Some kernels/drivers report
     * a stale or wrong status string (e.g. "Not charging" while plugged in
     * and actively drawing power) - when AC is online but the battery
     * status doesn't already say so, trust the AC signal instead. */
    int ac_online = ac_is_online();
    if (ac_online == 1 &&
        strcmp(status_str, "Charging") != 0 &&
        strcmp(status_str, "Full") != 0) {
        snprintf(status_str, sizeof(status_str), "Charging");
    } else if (ac_online == 0 && strlen(status_str) == 0) {
        snprintf(status_str, sizeof(status_str), "Discharging");
    }

    if (strlen(status_str) > 0) {
        snprintf(out, size, "%s%% [%s]", cap_str, status_str);
    } else {
        snprintf(out, size, "%s%%", cap_str);
    }
}

#endif

