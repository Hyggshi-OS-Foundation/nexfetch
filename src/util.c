#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#include <direct.h>
#define sys_mkdir(path) _mkdir(path)
#else
#include <unistd.h>
#define sys_mkdir(path) mkdir(path, 0755)
#endif

char *util_read_first_line(const char *filepath, char *buf, size_t size) {
    if (!filepath || !buf || size == 0) return NULL;
    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;
    if (fgets(buf, (int)size, f) == NULL) {
        fclose(f);
        return NULL;
    }
    fclose(f);
    return util_trim(buf);
}

char *util_read_file_content(const char *filepath, char *buf, size_t size) {
    if (!filepath || !buf || size == 0) return NULL;
    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;
    size_t bytes_read = fread(buf, 1, size - 1, f);
    buf[bytes_read] = '\0';
    fclose(f);
    return buf;
}

char *util_trim(char *str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return str;
}

void util_format_bytes(unsigned long long bytes, char *out, size_t max_len) {
    if (!out || max_len == 0) return;
    const char *units[] = {"B", "MiB", "GiB", "TiB"};
    double size = (double)bytes;
    int unit_idx = 0;

    if (bytes >= 1024 * 1024) {
        size = (double)bytes / (1024.0 * 1024.0);
        unit_idx = 1;
    }
    if (size >= 1024.0) {
        size /= 1024.0;
        unit_idx = 2;
    }
    if (size >= 1024.0) {
        size /= 1024.0;
        unit_idx = 3;
    }

    if (unit_idx == 0) {
        snprintf(out, max_len, "%llu B", bytes);
    } else {
        snprintf(out, max_len, "%.2f %s", size, units[unit_idx]);
    }
}

void util_format_duration(long seconds, char *out, size_t max_len) {
    if (!out || max_len == 0) return;
    long days = seconds / 86400;
    long hours = (seconds % 86400) / 3600;
    long mins = (seconds % 3600) / 60;

    if (days > 0) {
        snprintf(out, max_len, "%ldd %ldh %ldm", days, hours, mins);
    } else if (hours > 0) {
        snprintf(out, max_len, "%ldh %ldm", hours, mins);
    } else {
        snprintf(out, max_len, "%ldm", mins);
    }
}

int util_execute_cmd(const char *cmd, char *out, size_t max_len) {
    if (!cmd || !out || max_len == 0) return -1;
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    if (fgets(out, (int)max_len, fp) != NULL) {
        util_trim(out);
        pclose(fp);
        return 0;
    }
    pclose(fp);
    return -1;
}

void util_expand_escapes(char *str) {
    if (!str) return;
    char *src = str;
    char *dst = str;
    while (*src) {
        if (src[0] == '\\' && src[1] == '0' && src[2] == '3' && src[3] == '3') {
            *dst++ = '\033';
            src += 4;
        } else if (src[0] == '\\' && src[1] == 'e') {
            *dst++ = '\033';
            src += 2;
        } else if (src[0] == '\\' && src[1] == 'x' && src[2] == '1' && (src[3] == 'b' || src[3] == 'B')) {
            *dst++ = '\033';
            src += 4;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

#include <sys/types.h>

int util_mkdir_p(const char *path) {
    if (!path || !*path) return -1;
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) return -1;

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = '\0';
            sys_mkdir(tmp);
            *p = c;
        }
    }
    return sys_mkdir(tmp);
}

int util_copy_file(const char *src, const char *dst) {
    if (!src || !dst) return -1;
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(in);
    fclose(out);
    return 0;
}

int util_get_user_config_dir(char *out, size_t size) {
    if (!out || size == 0) return -1;
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        snprintf(out, size, "%s/nexfetch", xdg);
        return 0;
    }
    const char *home = getenv("HOME");
    if (home && *home) {
        snprintf(out, size, "%s/.config/nexfetch", home);
        return 0;
    }
#ifdef _WIN32
    const char *appdata = getenv("APPDATA");
    if (appdata && *appdata) {
        snprintf(out, size, "%s/nexfetch", appdata);
        return 0;
    }
#endif
    return -1;
}

/* --- Short-TTL cache for slow lookups (gsettings, dconf, subprocess probes) ---
 * Spawning gsettings/dconf is the single biggest, most variable cost in a
 * nexfetch run (D-Bus activation latency swings wildly run to run). These
 * values essentially never change between runs, so cache each result for a
 * few seconds instead of re-querying D-Bus on every single invocation. */
#define NEXFETCH_CACHE_TTL_SECS 30

static int util_cache_path(const char *key, char *out, size_t size) {
    const char *xdg = getenv("XDG_CACHE_HOME");
    const char *home = getenv("HOME");
    char dir[400];
    if (xdg && xdg[0] != '\0')
        snprintf(dir, sizeof(dir), "%s/nexfetch", xdg);
    else if (home && home[0] != '\0')
        snprintf(dir, sizeof(dir), "%s/.cache/nexfetch", home);
    else
        return 0;

    util_mkdir_p(dir);
    snprintf(out, size, "%s/%s.cache", dir, key);
    return 1;
}

int util_cache_read(const char *key, char *out, size_t size) {
    char path[512];
    if (!util_cache_path(key, path, sizeof(path))) return 0;

    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (time(NULL) - st.st_mtime > NEXFETCH_CACHE_TTL_SECS) return 0;

    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int ok = (fgets(out, (int)size, f) != NULL);
    fclose(f);
    if (ok) util_trim(out);
    return ok && out[0] != '\0';
}

void util_cache_write(const char *key, const char *value) {
    char path[512];
    if (!util_cache_path(key, path, sizeof(path))) return;
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(value, f);
    fclose(f);
}