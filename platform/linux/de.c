#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* --- Desktop Environment -------------------------------------------------- */

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
            if (util_execute_cmd("gnome-shell --version 2>/dev/null | awk '{print $NF}'", ver, sizeof(ver)) == 0 && ver[0] != '\0') {
                snprintf(out, size, "GNOME %s", ver);
                return;
            }
        }
        if (strstr(de, "KDE") || strstr(de, "plasma")) {
            char ver[64] = "";
            if (util_execute_cmd("plasmashell --version 2>/dev/null | awk '{print $NF}'", ver, sizeof(ver)) == 0 && ver[0] != '\0') {
                snprintf(out, size, "KDE Plasma %s", ver);
                return;
            }
        }
        if (strstr(de, "XFCE") || strstr(de, "xfce")) {
            char ver[64] = "";
            if (util_execute_cmd("xfce4-session --version 2>/dev/null | head -1 | awk '{print $NF}'", ver, sizeof(ver)) == 0 && ver[0] != '\0') {
                snprintf(out, size, "XFCE %s", ver);
                return;
            }
        }
        if (strstr(de, "MATE") || strstr(de, "mate")) {
            char ver[64] = "";
            if (util_execute_cmd("mate-session --version 2>/dev/null | awk '{print $NF}'", ver, sizeof(ver)) == 0 && ver[0] != '\0') {
                snprintf(out, size, "MATE %s", ver);
                return;
            }
        }
        if (strstr(de, "Cinnamon") || strstr(de, "cinnamon")) {
            char ver[64] = "";
            if (util_execute_cmd("cinnamon --version 2>/dev/null | awk '{print $NF}'", ver, sizeof(ver)) == 0 && ver[0] != '\0') {
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

    /* Try common WM detection commands (X11) */
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

    /* Fallback: check running processes for known WMs */
    const char *known_wms[] = {
        "mutter", "muffin", "kwin_x11", "kwin_wayland", "openbox",
        "i3", "sway", "bspwm", "xfwm4", "fluxbox", "enlightenment",
        "herbstluftwm", "awesome", "dwm", NULL
    };
    /* Friendly display names matching the above */
    const char *wm_display[] = {
        "Mutter (GNOME)", "Muffin (Cinnamon)", "KWin (X11)", "KWin (Wayland)", "Openbox",
        "i3", "Sway", "bspwm", "Xfwm4", "Fluxbox", "Enlightenment",
        "Herbstluftwm", "Awesome", "dwm", NULL
    };
    for (int i = 0; known_wms[i] != NULL; i++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "pgrep -x %s 2>/dev/null", known_wms[i]);
        char buf[32] = "";
        if (util_execute_cmd(cmd, buf, sizeof(buf)) == 0 && buf[0] != '\0') {
            snprintf(out, size, "%s", wm_display[i]);
            return;
        }
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

void platform_get_theme(char *out, size_t size) {
    if (!out || size == 0) return;
    char buf[256] = "";

    /* gsettings (GTK / GNOME) */
    if (util_execute_cmd("gsettings get org.gnome.desktop.interface gtk-theme 2>/dev/null", buf, sizeof(buf)) == 0 && buf[0] != '\0') {
        /* Strip surrounding quotes */
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

