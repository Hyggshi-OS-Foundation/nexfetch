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
}

#endif
