#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_get_shell(char *out, size_t size) {
    if (!out || size == 0) return;
    const char *shell_env = getenv("SHELL");
    if (shell_env) {
        const char *name = strrchr(shell_env, '/');
        if (name) name++;
        else name = shell_env;

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
    } else {
        snprintf(out, size, "Unknown");
    }
}
