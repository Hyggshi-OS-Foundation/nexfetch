#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Each package manager below was previously probed unconditionally by
 * fork+exec'ing a shell that piped the manager's own list command into
 * `wc -l` -- five subprocess spawns on every run, four of which normally
 * fail with "command not found" since most systems only have one package
 * manager installed. That's the dominant cost of this module: forking a
 * shell and letting `sh` search $PATH for a binary that isn't there is not
 * free, and doing it five times adds up fast.
 *
 * These helpers instead stat() for a directory/file that only exists when
 * the corresponding package manager is present, so we skip straight past
 * anything not installed with a single, cheap syscall instead of a process
 * spawn. Only dpkg, which is by far the common case (Debian/Ubuntu), is
 * parsed directly from its status database -- no subprocess at all.
 */

#include <dirent.h>

static int path_exists(const char *path) {
    return access(path, F_OK) == 0;
}

/* Count "Package: " entries directly from dpkg's status file. */
static int count_dpkg_packages(void) {
    FILE *f = fopen("/var/lib/dpkg/status", "r");
    if (!f) return 0;

    int count = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Package:", 8) == 0) count++;
    }
    fclose(f);
    return count;
}

/* Fast dir scan for pacman local packages */
static int count_pacman_packages(void) {
    DIR *dir = opendir("/var/lib/pacman/local");
    if (!dir) return 0;
    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] != '.' && strcmp(ent->d_name, "ALPM_DB_VERSION") != 0) {
            count++;
        }
    }
    closedir(dir);
    return count;
}

/* Fast dir scan for flatpak packages */
static int count_flatpak_dir(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return 0;
    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] != '.') count++;
    }
    closedir(dir);
    return count;
}

static int count_flatpak_packages(void) {
    int count = count_flatpak_dir("/var/lib/flatpak/app");
    const char *home = getenv("HOME");
    if (home) {
        char user_fp[512];
        snprintf(user_fp, sizeof(user_fp), "%s/.local/share/flatpak/app", home);
        count += count_flatpak_dir(user_fp);
    }
    return count;
}

/* Fast dir scan for snap packages (.snap files) */
static int count_snap_packages(void) {
    DIR *dir = opendir("/var/lib/snapd/snaps");
    if (!dir) return 0;
    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        const char *ext = strrchr(ent->d_name, '.');
        if (ext && strcmp(ext, ".snap") == 0) {
            count++;
        }
    }
    closedir(dir);
    return count;
}

void platform_get_packages(char *out, size_t size) {
    if (!out || size == 0) return;
    if (util_cache_read("packages", out, size)) return;

    int total_pkgs = 0;
    char details[256] = "";

    // dpkg -- parsed directly
    if (path_exists("/var/lib/dpkg/status")) {
        int count = count_dpkg_packages();
        if (count > 0) {
            total_pkgs += count;
            snprintf(details + strlen(details), sizeof(details) - strlen(details), "%d (dpkg) ", count);
        }
    }
    // pacman -- parsed via fast dir scan
    if (path_exists("/var/lib/pacman/local")) {
        int count = count_pacman_packages();
        if (count > 0) {
            total_pkgs += count;
            snprintf(details + strlen(details), sizeof(details) - strlen(details), "%d (pacman) ", count);
        }
    }
    // rpm -- probe if rpm db exists
    if (path_exists("/var/lib/rpm")) {
        char buf[64] = "";
        if (util_execute_cmd("rpm -qa 2>/dev/null | wc -l", buf, sizeof(buf)) == 0) {
            int count = atoi(buf);
            if (count > 0) {
                total_pkgs += count;
                snprintf(details + strlen(details), sizeof(details) - strlen(details), "%d (rpm) ", count);
            }
        }
    }
    // flatpak -- parsed via fast dir scan
    if (path_exists("/var/lib/flatpak") || path_exists("/.flatpak-info")) {
        int count = count_flatpak_packages();
        if (count > 0) {
            total_pkgs += count;
            snprintf(details + strlen(details), sizeof(details) - strlen(details), "%d (flatpak) ", count);
        }
    }
    // snap -- parsed via fast dir scan
    if (path_exists("/var/lib/snapd/snaps")) {
        int count = count_snap_packages();
        if (count > 0) {
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

    if (strcmp(out, "Unknown") != 0) util_cache_write("packages", out);
}

/* --- Package Intelligence --- */

/* Count upgradable packages (APT-specific) */
static int count_upgradable_apt(void) {
    char buf[64] = "";
    if (util_execute_cmd("apt list --upgradable 2>/dev/null | grep -c upgradable", buf, sizeof(buf)) == 0) {
        return atoi(buf);
    }
    return 0;
}

/* Count held packages (APT-specific) */
static int count_held_apt(void) {
    char buf[64] = "";
    if (util_execute_cmd("dpkg --get-selections 2>/dev/null | grep -c hold", buf, sizeof(buf)) == 0) {
        return atoi(buf);
    }
    return 0;
}

/* Count broken packages */
static int count_broken_packages(void) {
    char buf[64] = "";
    if (util_execute_cmd("dpkg -l 2>/dev/null | grep -c '^..r'", buf, sizeof(buf)) == 0) {
        return atoi(buf);
    }
    return 0;
}

void platform_get_packages_intel(char *out, size_t size) {
    if (!out || size == 0) return;

    int total = 0;
    int upgradable = 0;
    int held = 0;
    int broken = 0;
    int security_updates = 0;

    /* Get total from existing cache or count */
    char total_buf[256] = "";
    if (util_cache_read("packages", total_buf, sizeof(total_buf))) {
        total = atoi(total_buf);
    } else {
        /* Count from dpkg if available */
        if (path_exists("/var/lib/dpkg/status")) {
            total = count_dpkg_packages();
        }
    }

    /* APT-specific counts (cached separately with 60s TTL) */
    if (path_exists("/var/lib/dpkg/status")) {
        char cached[128] = "";
        if (util_cache_read("packages_intel", cached, sizeof(cached))) {
            sscanf(cached, "%d %d %d %d", &upgradable, &held, &broken, &security_updates);
        } else {
            upgradable = count_upgradable_apt();
            held = count_held_apt();
            broken = count_broken_packages();
            /* Security updates - check for security repo updates */
            char sec_buf[64] = "";
            if (util_execute_cmd("apt list --upgradable 2>/dev/null | grep -c security", sec_buf, sizeof(sec_buf)) == 0) {
                security_updates = atoi(sec_buf);
            }
            snprintf(cached, sizeof(cached), "%d %d %d %d", upgradable, held, broken, security_updates);
            util_cache_write("packages_intel", cached);
        }
    }

    int pos = 0;
    pos += snprintf(out + pos, size - pos, "Installed %d", total);
    if (upgradable > 0)
        pos += snprintf(out + pos, size - pos, "\nUpgradable %d", upgradable);
    if (held > 0)
        pos += snprintf(out + pos, size - pos, "\nHeld %d", held);
    if (broken > 0)
        pos += snprintf(out + pos, size - pos, "\nBroken %d", broken);
    if (security_updates > 0)
        pos += snprintf(out + pos, size - pos, "\nSecurity updates %d", security_updates);
}

#endif
