#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_get_shell(char *out, size_t size) {
    if (!out || size == 0) return;

    /* Try cache first -- shell version rarely changes between runs */
    if (util_cache_read("shell", out, size)) return;

    const char *shell_env = getenv("SHELL");
    if (shell_env) {
        const char *name = strrchr(shell_env, '/');
        if (name) name++;
        else name = shell_env;

        /* Try environment variables first for instant lookup */
        const char *ver_env = NULL;
        if (strcmp(name, "bash") == 0) ver_env = getenv("BASH_VERSION");
        else if (strcmp(name, "zsh") == 0) ver_env = getenv("ZSH_VERSION");
        else if (strcmp(name, "fish") == 0) ver_env = getenv("FISH_VERSION");

        if (ver_env && *ver_env) {
            char ver_clean[64];
            snprintf(ver_clean, sizeof(ver_clean), "%s", ver_env);
            char *paren = strchr(ver_clean, '(');
            if (paren) *paren = '\0';
            util_trim(ver_clean);
            snprintf(out, size, "%s (%s)", name, ver_clean);
            util_cache_write("shell", out);
            return;
        }

        char version[64] = "";
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "%s --version 2>/dev/null", shell_env);
        if (util_execute_cmd(cmd, version, sizeof(version)) == 0) {
            char *space = strchr(version, ',');
            if (space) *space = '\0';
            snprintf(out, size, "%s (%s)", name, version);
        } else {
            snprintf(out, size, "%s", name);
        }
        util_cache_write("shell", out);
    } else {
        snprintf(out, size, "Unknown");
    }
}

#endif
