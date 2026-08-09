#include "nexfetch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void explain_memory(const char *val, char *out, size_t max_len) {
    if (!val || !out || max_len == 0) return;

    int pct = 0;
    /* Try to extract percentage from "X GiB / Y GiB (Z%)" format */
    const char *pct_str = strrchr(val, '(');
    if (pct_str && pct_str[1] >= '0' && pct_str[1] <= '9') {
        pct = atoi(pct_str + 1);
    }

    if (pct >= 90) {
        snprintf(out, max_len, "  \033[31mHigh memory pressure: %d%% RAM used.\033[0m\n  Consider adding swap or upgrading RAM.", pct);
    } else if (pct >= 70) {
        snprintf(out, max_len, "  \033[33mModerate memory pressure: %d%% RAM used.\033[0m", pct);
    } else {
        snprintf(out, max_len, "  \033[32mLow memory pressure: %d%% RAM used.\033[0m", pct);
    }
}

static void explain_cpu(const char *val, char *out, size_t max_len) {
    if (!val || !out || max_len == 0) return;

    /* Check for common CPU patterns */
    if (strstr(val, "Intel")) {
        snprintf(out, max_len, "  Intel processor detected.");
    } else if (strstr(val, "AMD")) {
        snprintf(out, max_len, "  AMD processor detected.");
    } else if (strstr(val, "Apple")) {
        snprintf(out, max_len, "  Apple Silicon detected.");
    } else {
        snprintf(out, max_len, "  Processor detected.");
    }
}

static void explain_uptime(const char *val, char *out, size_t max_len) {
    if (!val || !out || max_len == 0) return;

    /* Check for days */
    if (strstr(val, "day")) {
        int days = atoi(val);
        if (days > 30) {
            snprintf(out, max_len, "  \033[33mSystem up for %d days. Consider rebooting for kernel updates.\033[0m", days);
        } else if (days > 7) {
            snprintf(out, max_len, "  System up for %d days.", days);
        } else {
            snprintf(out, max_len, "  System up for %d days.", days);
        }
    } else if (strstr(val, "hour")) {
        snprintf(out, max_len, "  System up for less than a day.");
    } else {
        snprintf(out, max_len, "  System uptime detected.");
    }
}

static void explain_packages(const char *val, char *out, size_t max_len) {
    if (!val || !out || max_len == 0) return;

    int total = atoi(val);
    if (total > 3000) {
        snprintf(out, max_len, "  \033[33mLarge package count: %d installed. Consider removing unused packages.\033[0m", total);
    } else if (total > 1500) {
        snprintf(out, max_len, "  Moderate package count: %d installed.", total);
    } else {
        snprintf(out, max_len, "  Lean package count: %d installed.", total);
    }
}

static void explain_disk(const char *val, char *out, size_t max_len) {
    if (!val || !out || max_len == 0) return;

    int pct = 0;
    const char *pct_str = strrchr(val, '(');
    if (pct_str && pct_str[1] >= '0' && pct_str[1] <= '9') {
        pct = atoi(pct_str + 1);
    }

    if (pct >= 90) {
        snprintf(out, max_len, "  \033[31mDisk nearly full: %d%% used. Clean up cached packages or logs.\033[0m", pct);
    } else if (pct >= 75) {
        snprintf(out, max_len, "  \033[33mDisk usage moderate: %d%% used.\033[0m", pct);
    } else {
        snprintf(out, max_len, "  \033[32mDisk usage healthy: %d%% used.\033[0m", pct);
    }
}

static void explain_battery(const char *val, char *out, size_t max_len) {
    if (!val || !out || max_len == 0) return;

    if (strstr(val, "Charging")) {
        snprintf(out, max_len, "  Battery is charging.");
    } else if (strstr(val, "Full")) {
        snprintf(out, max_len, "  Battery fully charged.");
    } else {
        int pct = atoi(val);
        if (pct > 0 && pct <= 20) {
            snprintf(out, max_len, "  \033[31mBattery low: %d%%. Connect charger to prevent data loss.\033[0m", pct);
        } else if (pct > 20 && pct <= 50) {
            snprintf(out, max_len, "  \033[33mBattery at %d%%.\033[0m", pct);
        } else if (pct > 50) {
            snprintf(out, max_len, "  \033[32mBattery at %d%%.\033[0m", pct);
        }
    }
}

static void explain_network(const char *val, char *out, size_t max_len) {
    if (!val || !out || max_len == 0) return;

    if (strstr(val, "lo") || strstr(val, "loopback")) {
        snprintf(out, max_len, "  Loopback interface only. No external network detected.");
    } else {
        snprintf(out, max_len, "  Network interface detected.");
    }
}

void explain_generate(const ModuleResult *results, int count, char *out, size_t max_len) {
    if (!results || !out || max_len == 0 || count == 0) return;

    int pos = 0;
    int has_explanation = 0;

    pos += snprintf(out + pos, max_len - pos, "\n\033[1mExplanation\033[0m\n");

    for (int i = 0; i < count; i++) {
        char explanation[256] = "";
        const char *key = results[i].key;
        const char *val = results[i].val;

        if (!val || val[0] == '\0') continue;

        if (strcmp(key, "Memory") == 0) {
            explain_memory(val, explanation, sizeof(explanation));
        } else if (strcmp(key, "CPU") == 0) {
            explain_cpu(val, explanation, sizeof(explanation));
        } else if (strcmp(key, "Uptime") == 0) {
            explain_uptime(val, explanation, sizeof(explanation));
        } else if (strcmp(key, "Packages") == 0) {
            explain_packages(val, explanation, sizeof(explanation));
        } else if (strcmp(key, "Disk") == 0) {
            explain_disk(val, explanation, sizeof(explanation));
        } else if (strcmp(key, "Battery") == 0) {
            explain_battery(val, explanation, sizeof(explanation));
        } else if (strcmp(key, "Network") == 0) {
            explain_network(val, explanation, sizeof(explanation));
        }

        if (explanation[0] != '\0') {
            has_explanation = 1;
            pos += snprintf(out + pos, max_len - pos, "\n\033[1m%s:\033[0m %s\n%s\n", key, val, explanation);
        }
    }

    if (!has_explanation) {
        pos += snprintf(out + pos, max_len - pos, "\nNo specific explanations available for this configuration.\n");
    }
}
