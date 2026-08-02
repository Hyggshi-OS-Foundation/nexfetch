#include "nexfetch.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef _WIN32
#define strcasecmp _stricmp
#endif

NexfetchConfig g_config = {
    .show_logo         = 1,
    .custom_logo_path  = "",
    .logo_is_image     = 0,
    .logo_width        = 0,
    .distro_id         = "tux",
    .color_blocks      = 1,
    .theme             = "boxed"
};

static int json_get_string(const char *json, const char *key, char *out, size_t out_size) {
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);

    const char *p = strstr(json, needle);
    if (!p) return 0;
    p += strlen(needle);

    while (*p && (isspace((unsigned char)*p) || *p == ':')) p++;
    if (*p != '"') return 0;
    p++;

    size_t i = 0;
    while (*p && *p != '"' && i < out_size - 1) {
        if (*p == '\\' && *(p+1)) {
            p++;
            switch (*p) {
                case 'n': out[i++] = '\n'; break;
                case 't': out[i++] = '\t'; break;
                case '"': out[i++] = '"';  break;
                case '\\': out[i++] = '\\'; break;
                case '/': out[i++] = '/';  break;
                default:  out[i++] = *p;   break;
            }
        } else {
            out[i++] = *p;
        }
        p++;
    }
    out[i] = '\0';
    return 1;
}

static int is_image_path(const char *path) {
    if (!path || !*path) return 0;
    const char *dot = strrchr(path, '.');
    if (!dot) return 0;
    const char *ext = dot + 1;
    const char *image_exts[] = { "png", "jpg", "jpeg", "gif", "bmp", "webp", "svg", NULL };
    for (int i = 0; image_exts[i]; i++) {
        if (strcasecmp(ext, image_exts[i]) == 0) return 1;
    }
    return 0;
}

void config_init(void) {
    char buf[4096] = "";
    /* Try local config first, then system-wide locations */
    if (!util_read_file_content("config/config.json", buf, sizeof(buf))) {
        if (!util_read_file_content("/etc/nexfetch/config.json", buf, sizeof(buf))) {
            if (!util_read_file_content("/usr/share/nexfetch/config/config.json", buf, sizeof(buf)))
                return;
        }
    }

    if (strstr(buf, "\"show_logo\": false") || strstr(buf, "\"show_logo\":false"))
        g_config.show_logo = 0;

    if (strstr(buf, "\"color_blocks\": false") || strstr(buf, "\"color_blocks\":false"))
        g_config.color_blocks = 0;

    char logo_path[512] = "";
    if (json_get_string(buf, "logo", logo_path, sizeof(logo_path)) && logo_path[0] != '\0') {
        snprintf(g_config.custom_logo_path, sizeof(g_config.custom_logo_path), "%s", logo_path);
        g_config.logo_is_image = is_image_path(logo_path);
    }

    char theme_val[64] = "";
    if (json_get_string(buf, "theme", theme_val, sizeof(theme_val)) && theme_val[0] != '\0') {
        snprintf(g_config.theme, sizeof(g_config.theme), "%s", theme_val);
    }

    char lw_str[16] = "";
    if (json_get_string(buf, "logo_width", lw_str, sizeof(lw_str)) && lw_str[0] != '\0') {
        int w = atoi(lw_str);
        if (w > 0) g_config.logo_width = w;
    } else {
        const char *lw_key = strstr(buf, "\"logo_width\"");
        if (lw_key) {
            const char *col = strchr(lw_key, ':');
            if (col) {
                int w = atoi(col + 1);
                if (w > 0) g_config.logo_width = w;
            }
        }
    }
}
