#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_get_packages(char *out, size_t size) {
    if (!out || size == 0) return;
    char buf[128] = "";
    int total_pkgs = 0;
    char details[256] = "";

    // dpkg
    if (util_execute_cmd("dpkg-query -f '${binary:Package}\n' -W 2>/dev/null | wc -l", buf, sizeof(buf)) == 0) {
        int count = atoi(buf);
        if (count > 0) {
            total_pkgs += count;
            snprintf(details + strlen(details), sizeof(details) - strlen(details), "%d (dpkg) ", count);
        }
    }
    // pacman
    if (util_execute_cmd("pacman -Qq 2>/dev/null | wc -l", buf, sizeof(buf)) == 0) {
        int count = atoi(buf);
        if (count > 0) {
            total_pkgs += count;
            snprintf(details + strlen(details), sizeof(details) - strlen(details), "%d (pacman) ", count);
        }
    }
    // rpm
    if (util_execute_cmd("rpm -qa 2>/dev/null | wc -l", buf, sizeof(buf)) == 0) {
        int count = atoi(buf);
        if (count > 0) {
            total_pkgs += count;
            snprintf(details + strlen(details), sizeof(details) - strlen(details), "%d (rpm) ", count);
        }
    }
    // flatpak
    if (util_execute_cmd("flatpak list 2>/dev/null | wc -l", buf, sizeof(buf)) == 0) {
        int count = atoi(buf);
        if (count > 0) {
            total_pkgs += count;
            snprintf(details + strlen(details), sizeof(details) - strlen(details), "%d (flatpak) ", count);
        }
    }
    // snap
    if (util_execute_cmd("snap list 2>/dev/null | wc -l", buf, sizeof(buf)) == 0) {
        int count = atoi(buf);
        if (count > 1) { // line 1 is header
            count--;
            total_pkgs += count;
            snprintf(details + strlen(details), sizeof(details) - strlen(details), "%d (snap) ", count);
        }
    }

    if (total_pkgs > 0) {
        util_trim(details);
        snprintf(out, size, "%d [%s]", total_pkgs, details);
    } else {
        snprintf(out, size, "Unknown");
    }
}

#endif

