#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
