#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>

/* --- Desktop Environment -------------------------------------------------- */

static int get_de_version_from_dpkg(const char *pkg_name, char *ver_out, size_t ver_sz) {
    FILE *f = fopen("/var/lib/dpkg/status", "r");
    if (!f) return 0;
    char line[512];
    int match = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Package: ", 9) == 0) {
            match = (strcmp(util_trim(line + 9), pkg_name) == 0);
        } else if (match && strncmp(line, "Version: ", 9) == 0) {
            char *v = util_trim(line + 9);
            char *dash = strchr(v, '-');
            if (dash) *dash = '\0';
            char *colon = strchr(v, ':');
            if (colon) v = colon + 1;
            snprintf(ver_out, ver_sz, "%s", v);
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

void platform_get_de(char *out, size_t size) {
    if (!out || size == 0) return;

    const char *xdg = getenv("XDG_CURRENT_DESKTOP");
    const char *session = getenv("DESKTOP_SESSION");

    if (xdg && xdg[0] != '\0') {
        /* XDG_CURRENT_DESKTOP can be "ubuntu:GNOME" → strip prefix */
        const char *colon = strchr(xdg, ':');
        const char *de = colon ? colon + 1 : xdg;

        /* Try to get version for well-known DEs */
        if (strstr(de, "GNOME") || strstr(de, "gnome")) {
            char ver[64] = "";
            if (get_de_version_from_dpkg("gnome-shell", ver, sizeof(ver)) ||
                (util_execute_cmd("gnome-shell --version 2>/dev/null | awk '{print $NF}'", ver, sizeof(ver)) == 0 && ver[0] != '\0')) {
                snprintf(out, size, "GNOME %s", ver);
                return;
            }
        }
        if (strstr(de, "KDE") || strstr(de, "plasma")) {
            char ver[64] = "";
            if (get_de_version_from_dpkg("plasmashell", ver, sizeof(ver)) ||
                (util_execute_cmd("plasmashell --version 2>/dev/null | awk '{print $NF}'", ver, sizeof(ver)) == 0 && ver[0] != '\0')) {
                snprintf(out, size, "KDE Plasma %s", ver);
                return;
            }
        }
        if (strstr(de, "XFCE") || strstr(de, "xfce")) {
            char ver[64] = "";
            if (get_de_version_from_dpkg("xfce4-session", ver, sizeof(ver)) ||
                (util_execute_cmd("xfce4-session --version 2>/dev/null | head -1 | awk '{print $NF}'", ver, sizeof(ver)) == 0 && ver[0] != '\0')) {
                snprintf(out, size, "XFCE %s", ver);
                return;
            }
        }
        if (strstr(de, "MATE") || strstr(de, "mate")) {
            char ver[64] = "";
            if (get_de_version_from_dpkg("mate-session", ver, sizeof(ver)) ||
                (util_execute_cmd("mate-session --version 2>/dev/null | awk '{print $NF}'", ver, sizeof(ver)) == 0 && ver[0] != '\0')) {
                snprintf(out, size, "MATE %s", ver);
                return;
            }
        }
        if (strstr(de, "Cinnamon") || strstr(de, "cinnamon")) {
            char ver[64] = "";
            if (get_de_version_from_dpkg("cinnamon", ver, sizeof(ver)) ||
                (util_execute_cmd("cinnamon --version 2>/dev/null | awk '{print $NF}'", ver, sizeof(ver)) == 0 && ver[0] != '\0')) {
                snprintf(out, size, "Cinnamon %s", ver);
                return;
            }
        }
        snprintf(out, size, "%s", de);
        return;
    }

    if (session && session[0] != '\0') {
        snprintf(out, size, "%s", session);
        return;
    }

    snprintf(out, size, "Unknown");
}

/* --- Window Manager ------------------------------------------------------- */

void platform_get_wm(char *out, size_t size) {
    if (!out || size == 0) return;

    const char *wmenv = getenv("WINDOW_MANAGER");
    if (wmenv && wmenv[0] != '\0') {
        const char *name = strrchr(wmenv, '/');
        snprintf(out, size, "%s", name ? name + 1 : wmenv);
        return;
    }

    /* On Wayland we can determine the WM from the compositor / DE */
    const char *wayland_disp = getenv("WAYLAND_DISPLAY");
    const char *session_type = getenv("XDG_SESSION_TYPE");
    int is_wayland = (wayland_disp && wayland_disp[0] != '\0') ||
                     (session_type && strcmp(session_type, "wayland") == 0);

    if (is_wayland) {
        const char *xdg_de = getenv("XDG_CURRENT_DESKTOP");
        if (xdg_de) {
            const char *de = strchr(xdg_de, ':');
            de = de ? de + 1 : xdg_de;
            if (strstr(de, "GNOME") || strstr(de, "gnome")) {
                snprintf(out, size, "Mutter (Wayland)"); return;
            }
            if (strstr(de, "KDE") || strstr(de, "plasma")) {
                snprintf(out, size, "KWin (Wayland)"); return;
            }
            if (strstr(de, "sway") || strstr(de, "Sway")) {
                snprintf(out, size, "Sway"); return;
            }
        }
    }

    /* Try common WM detection commands (X11 only -- these tools are
       meaningless without a running X server, so don't waste a fork+exec
       pair probing for them under Wayland/headless where DISPLAY is unset). */
    const char *display_env = getenv("DISPLAY");
    if (display_env && display_env[0] != '\0') {
        const char *wm_cmds[] = {
            "xprop -id $(xprop -root 2>/dev/null | awk '/_NET_SUPPORTING_WM_CHECK/ {print $NF}') 2>/dev/null | awk '/_NET_WM_NAME/ {gsub(/\"/,\"\"); print $NF}'",
            "wmctrl -m 2>/dev/null | awk '/Name:/ {print $2}'",
            NULL
        };

        for (int i = 0; wm_cmds[i] != NULL; i++) {
            char buf[128] = "";
            if (util_execute_cmd(wm_cmds[i], buf, sizeof(buf)) == 0 && buf[0] != '\0' && strcmp(buf, "N/A") != 0) {
                snprintf(out, size, "%s", buf);
                return;
            }
        }
    }

    /* Fallback: check running processes for known WMs. Previously this spawned
       a "pgrep -x <name>" subprocess (fork+exec+shell) per candidate -- up to
       14 sequential subprocesses just to fail on a box with no matching WM.
       A single pass over /proc does the same job with zero forks. */
    const char *known_wms[] = {
        "mutter", "muffin", "kwin_x11", "kwin_wayland", "openbox",
        "i3", "sway", "bspwm", "xfwm4", "fluxbox", "enlightenment",
        "herbstluftwm", "awesome", "dwm"
    };
    /* Friendly display names matching the above */
    const char *wm_display[] = {
        "Mutter (GNOME)", "Muffin (Cinnamon)", "KWin (X11)", "KWin (Wayland)", "Openbox",
        "i3", "Sway", "bspwm", "Xfwm4", "Fluxbox", "Enlightenment",
        "Herbstluftwm", "Awesome", "dwm"
    };
    const int known_wm_count = (int)(sizeof(known_wms) / sizeof(known_wms[0]));

    DIR *proc = opendir("/proc");
    if (proc) {
        struct dirent *ent;
        while ((ent = readdir(proc)) != NULL) {
            if (!isdigit((unsigned char)ent->d_name[0])) continue;

            char path[300];
            snprintf(path, sizeof(path), "/proc/%s/comm", ent->d_name);
            char comm[64] = "";
            if (!util_read_first_line(path, comm, sizeof(comm))) continue;

            for (int i = 0; i < known_wm_count; i++) {
                if (strcmp(comm, known_wms[i]) == 0) {
                    snprintf(out, size, "%s", wm_display[i]);
                    closedir(proc);
                    return;
                }
            }
        }
        closedir(proc);
    }

    snprintf(out, size, "Unknown");
}

/* --- Terminal ------------------------------------------------------------- */

void platform_get_terminal(char *out, size_t size) {
    if (!out || size == 0) return;

    /* $TERM_PROGRAM is set by many terminal emulators */
    const char *tp = getenv("TERM_PROGRAM");
    if (tp && tp[0] != '\0') {
        snprintf(out, size, "%s", tp);
        return;
    }

    /* Walk up the process tree from our PID looking for a known terminal */
    const char *known_terms[] = {
        "gnome-terminal", "konsole", "xterm", "urxvt", "alacritty",
        "kitty", "st", "termite", "tilix", "xfce4-terminal", "lxterminal",
        "mate-terminal", "foot", "wezterm", "hyper", NULL
    };

    /* Read /proc/<ppid>/comm walking up */
    int pid = (int)getppid();
    for (int depth = 0; depth < 8 && pid > 1; depth++) {
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/comm", pid);
        char comm[128] = "";
        if (util_read_first_line(path, comm, sizeof(comm))) {
            for (int i = 0; known_terms[i] != NULL; i++) {
                if (strncmp(comm, known_terms[i], strlen(known_terms[i])) == 0) {
                    snprintf(out, size, "%s", known_terms[i]);
                    return;
                }
            }
        }

        /* Get parent of pid */
        char status_path[64];
        snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);
        FILE *f = fopen(status_path, "r");
        if (!f) break;
        char line[256];
        int ppid = -1;
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "PPid:", 5) == 0) {
                sscanf(line + 5, "%d", &ppid);
                break;
            }
        }
        fclose(f);
        if (ppid <= 0) break;
        pid = ppid;
    }

    /* Only fall back to $TERM if it is a meaningful value */
    const char *term = getenv("TERM");
    if (term && term[0] != '\0' &&
        strcmp(term, "dumb") != 0 &&
        strcmp(term, "unknown") != 0) {
        snprintf(out, size, "%s", term);
        return;
    }

    snprintf(out, size, "Unknown");
}

/* --- Theme / Icons / Font ------------------------------------------------- */

static int get_gtk_setting(const char *key, char *out, size_t size) {
    const char *home = getenv("HOME");
    if (!home || !*home) return 0;

    const char *files[] = {
        "/.config/gtk-3.0/settings.ini",
        "/.config/gtk-4.0/settings.ini",
        "/.gtkrc-2.0",
        NULL
    };

    for (int i = 0; files[i]; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s%s", home, files[i]);
        FILE *f = fopen(path, "r");
        if (!f) continue;

        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == '=') {
                char *val = line + strlen(key) + 1;
                util_trim(val);
                if (*val == '"' || *val == '\'') val++;
                size_t len = strlen(val);
                if (len > 0 && (val[len - 1] == '"' || val[len - 1] == '\'')) val[len - 1] = '\0';
                if (*val != '\0') {
                    snprintf(out, size, "%s", val);
                    fclose(f);
                    return 1;
                }
            }
        }
        fclose(f);
    }
    return 0;
}

void platform_get_theme(char *out, size_t size) {
    if (!out || size == 0) return;
    char buf[256] = "";

    if (get_gtk_setting("gtk-theme-name", buf, sizeof(buf))) {
        snprintf(out, size, "%s [GTK]", buf);
        return;
    }

    /* gsettings fallback */
    if (util_execute_cmd("gsettings get org.gnome.desktop.interface gtk-theme 2>/dev/null", buf, sizeof(buf)) == 0 && buf[0] != '\0') {
        char *s = buf;
        if (*s == '\'' || *s == '"') s++;
        size_t len = strlen(s);
        if (len > 0 && (s[len - 1] == '\'' || s[len - 1] == '"')) s[len - 1] = '\0';
        snprintf(out, size, "%s [GTK]", s);
        return;
    }

    /* KDE / Plasma */
    if (util_execute_cmd("kreadconfig5 --group 'General' --key 'ColorScheme' 2>/dev/null", buf, sizeof(buf)) == 0 && buf[0] != '\0') {
        snprintf(out, size, "%s [KDE]", buf);
        return;
    }

    snprintf(out, size, "Unknown");
}

void platform_get_icons(char *out, size_t size) {
    if (!out || size == 0) return;
    char buf[256] = "";

    if (get_gtk_setting("gtk-icon-theme-name", buf, sizeof(buf))) {
        snprintf(out, size, "%s", buf);
        return;
    }

    if (util_execute_cmd("gsettings get org.gnome.desktop.interface icon-theme 2>/dev/null", buf, sizeof(buf)) == 0 && buf[0] != '\0') {
        char *s = buf;
        if (*s == '\'' || *s == '"') s++;
        size_t len = strlen(s);
        if (len > 0 && (s[len - 1] == '\'' || s[len - 1] == '"')) s[len - 1] = '\0';
        snprintf(out, size, "%s", s);
        return;
    }

    snprintf(out, size, "Unknown");
}

void platform_get_font(char *out, size_t size) {
    if (!out || size == 0) return;
    char buf[256] = "";

    if (get_gtk_setting("gtk-font-name", buf, sizeof(buf))) {
        snprintf(out, size, "%s", buf);
        return;
    }

    if (util_execute_cmd("gsettings get org.gnome.desktop.interface font-name 2>/dev/null", buf, sizeof(buf)) == 0 && buf[0] != '\0') {
        char *s = buf;
        if (*s == '\'' || *s == '"') s++;
        size_t len = strlen(s);
        if (len > 0 && (s[len - 1] == '\'' || s[len - 1] == '"')) s[len - 1] = '\0';
        snprintf(out, size, "%s", s);
        return;
    }

    snprintf(out, size, "Unknown");
}

#endif
