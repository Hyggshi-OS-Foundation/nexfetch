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
    .show_logo              = 1,
    .custom_logo_path       = "",
    .logo_is_image          = 0,
    .logo_is_video          = 0,
    .logo_width             = 0,
    .logo_animate           = 0,
    .logo_animate_duration  = 0,
    .distro_id              = "tux",
    .color_blocks           = 1,
    .theme                  = "boxed",
    .background_image_path  = "",
    .enabled_module_count   = 0,
    .fast_mode              = 0
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

static int is_video_path(const char *path) {
    if (!path || !*path) return 0;
    const char *dot = strrchr(path, '.');
    if (!dot) return 0;
    const char *ext = dot + 1;
    /* Currently supports MP4; extend this list for mkv/avi/webm etc. */
    const char *video_exts[] = { "mp4", "mkv", "avi", "webm", "mov", NULL };
    for (int i = 0; video_exts[i]; i++) {
        if (strcasecmp(ext, video_exts[i]) == 0) return 1;
    }
    return 0;
}

/*
 * Parse the JSON "modules" array and store each key in g_config.enabled_modules.
 * The array looks like: "modules": ["os", "kernel", "cpu"]
 * Handles both compact and pretty-printed JSON.
 */
static void json_parse_plugins_array(const char *json);

static void json_parse_modules_array(const char *json) {
    /* Locate the "modules" key */
    const char *p = strstr(json, "\"modules\"");
    if (!p) return;
    p += strlen("\"modules\"");

    /* Skip whitespace and colon */
    while (*p && (isspace((unsigned char)*p) || *p == ':')) p++;
    if (*p != '[') return;   /* expected array */
    p++;                      /* skip '[' */

    g_config.enabled_module_count = 0;

    while (*p && *p != ']' && g_config.enabled_module_count < MAX_MODULES) {
        /* Skip whitespace and commas */
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (*p == ']' || *p == '\0') break;

        if (*p != '"') { p++; continue; }   /* skip unexpected chars */
        p++;                                  /* skip opening quote */

        /* Copy the key name */
        char key[32];
        size_t ki = 0;
        while (*p && *p != '"' && ki < sizeof(key) - 1)
            key[ki++] = *p++;
        key[ki] = '\0';
        if (*p == '"') p++;   /* skip closing quote */

        if (ki > 0) {
            snprintf(g_config.enabled_modules[g_config.enabled_module_count],
                     sizeof(g_config.enabled_modules[0]), "%s", key);
            g_config.enabled_module_count++;
        }
    }
}

/*
 * Canonical default configuration. This string is the single source of truth
 * for what a fresh install looks like and stays byte-for-byte identical to
 * config/config.json. It is written out on the very first run of nexfetch on a
 * new machine, and is also parsed in-memory as a last resort so the program
 * works correctly even when no HOME/XDG directory and no system install exist.
 */
static const char NEXFETCH_DEFAULT_CONFIG[] =
"{\n"
"  \"show_logo\": true,\n"
"  \"color_blocks\": true,\n"
"  \"theme\": \"classic\",\n"
"  \"logo\": \"\",\n"
"  \"logo_width\": 32,\n"
"  \"background_image\": \"\",\n"
"  \"plugins\": [],\n"
"  \"modules\": [\n"
"    \"os\",\n"
"    \"kernel\",\n"
"    \"host\",\n"
"    \"uptime\",\n"
"    \"packages\",\n"
"    \"display\",\n"
"    \"shell\",\n"
"    \"de\",\n"
"    \"wm\",\n"
"    \"terminal\",\n"
"    \"cpu\",\n"
"    \"gpu\",\n"
"    \"memory\",\n"
"    \"disk\",\n"
"    \"swap\",\n"
"    \"battery\",\n"
"    \"network\",\n"
"    \"theme\",\n"
"    \"icons\",\n"
"    \"font\",\n"
"    \"locale\"\n"
"  ]\n"
"}\n";

void config_init(void) {
    char buf[8192] = "";
    char user_dir[512] = "";
    char user_cfg[512] = "";
    char user_mod_dir[512] = "";
    int loaded = 0;

    if (util_get_user_config_dir(user_dir, sizeof(user_dir)) == 0) {
        snprintf(user_cfg, sizeof(user_cfg), "%s/config.json", user_dir);
        snprintf(user_mod_dir, sizeof(user_mod_dir), "%s/modules", user_dir);

        /* Automatically create user config and module directories */
        util_mkdir_p(user_dir);
        util_mkdir_p(user_mod_dir);

        /* Auto-generate config.json on first run only */
        FILE *chk = fopen(user_cfg, "r");
        if (chk) {
            fclose(chk);
        } else {
            int copied = 0;

            /* 1. Running from the source tree / release folder, which ships a
             *    canonical config.json next to the binary. */
            if (util_copy_file("config/config.json", user_cfg) == 0) copied = 1;

            /* 2. Installed via package: system-wide locations. */
            if (!copied && util_copy_file("/etc/nexfetch/config.json", user_cfg) == 0)
                copied = 1;
            if (!copied && util_copy_file("/usr/share/nexfetch/config/config.json", user_cfg) == 0)
                copied = 1;

            /* 3. None of the above (freely downloaded binary placed anywhere):
             *    write the bundled canonical default so the first run of a
             *    fresh download is always fully configured. */
            if (!copied) {
                FILE *fw = fopen(user_cfg, "w");
                if (fw) {
                    fputs(NEXFETCH_DEFAULT_CONFIG, fw);
                    fclose(fw);
                    copied = 1;
                }
            }
        }

        /* 1. High priority: User config file */
        if (util_read_file_content(user_cfg, buf, sizeof(buf))) {
            loaded = 1;
        }
    }

    /* Fallback search order if user config couldn't be loaded */
    if (!loaded) {
        if (util_read_file_content("config/config.json", buf, sizeof(buf))) {
            loaded = 1;
        } else if (util_read_file_content("/etc/nexfetch/config.json", buf, sizeof(buf))) {
            loaded = 1;
        } else if (util_read_file_content("/usr/share/nexfetch/config/config.json", buf, sizeof(buf))) {
            loaded = 1;
        }
    }

    /* Brand-new, fully bare environment (no HOME/XDG, no system install):
     * still parse the canonical default so first-run behavior is correct. */
    if (!loaded) {
        snprintf(buf, sizeof(buf), "%s", NEXFETCH_DEFAULT_CONFIG);
        loaded = 1;
    }

    if (strstr(buf, "\"show_logo\": false") || strstr(buf, "\"show_logo\":false"))
        g_config.show_logo = 0;

    if (strstr(buf, "\"color_blocks\": false") || strstr(buf, "\"color_blocks\":false"))
        g_config.color_blocks = 0;

    char logo_path[512] = "";
    if (json_get_string(buf, "logo", logo_path, sizeof(logo_path)) && logo_path[0] != '\0') {
        snprintf(g_config.custom_logo_path, sizeof(g_config.custom_logo_path), "%s", logo_path);
        if (is_video_path(logo_path)) {
            g_config.logo_is_video = 1;
            g_config.logo_is_image = 0;
        } else {
            g_config.logo_is_image = is_image_path(logo_path);
            g_config.logo_is_video = 0;
        }
    }

    char bg_path[512] = "";
    if (json_get_string(buf, "background_image", bg_path, sizeof(bg_path)) && bg_path[0] != '\0') {
        snprintf(g_config.background_image_path, sizeof(g_config.background_image_path), "%s", bg_path);
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

    /* Parse the module filter list */
    json_parse_modules_array(buf);

    /* Parse the plugin path list */
    json_parse_plugins_array(buf);

    /* Animated GIF logo settings */
    if (strstr(buf, "\"logo_animate\": true") || strstr(buf, "\"logo_animate\":true"))
        g_config.logo_animate = 1;

    {
        const char *k = strstr(buf, "\"logo_animate_duration\"");
        if (k) {
            const char *col = strchr(k, ':');
            if (col) {
                int d = atoi(col + 1);
                if (d >= 0) g_config.logo_animate_duration = d;
            }
        }
    }
}

/*
 * Parse the JSON "plugins" array and store each path in g_config.plugin_paths.
 * The array looks like: "plugins": ["plugins/myplugin.so"]
 * Handles both compact and pretty-printed JSON.
 */
static void json_parse_plugins_array(const char *json) {
    /* Locate the "plugins" key */
    const char *p = strstr(json, "\"plugins\"");
    if (!p) return;
    p += strlen("\"plugins\"");

    /* Skip whitespace and colon */
    while (*p && (isspace((unsigned char)*p) || *p == ':')) p++;
    if (*p != '[') return;   /* expected array */
    p++;                      /* skip '[' */

    g_config.plugin_count = 0;

    while (*p && *p != ']' && g_config.plugin_count < MAX_PLUGINS) {
        /* Skip whitespace and commas */
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (*p == ']' || *p == '\0') break;

        if (*p != '"') { p++; continue; }   /* skip unexpected chars */
        p++;                                  /* skip opening quote */

        /* Copy the path */
        char path[512];
        size_t pi = 0;
        while (*p && *p != '"' && pi < sizeof(path) - 1)
            path[pi++] = *p++;
        path[pi] = '\0';
        if (*p == '"') p++;   /* skip closing quote */

        if (pi > 0) {
            snprintf(g_config.plugin_paths[g_config.plugin_count],
                     sizeof(g_config.plugin_paths[0]), "%s", path);
            g_config.plugin_count++;
        }
    }
}
