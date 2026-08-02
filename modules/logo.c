#include "nexfetch.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern size_t ansi_visible_length(const char *str);

/* Strip ESC [ ? … h/l  (cursor-hide/show and other private-mode codes) */
static void strip_private_modes(char *buf) {
    char out[MAX_LOGO_LINE_LEN];
    char *dst = out;
    unsigned char *src = (unsigned char *)buf;

    while (*src) {
        if (*src == 0x1B && *(src+1) == '[' && *(src+2) == '?') {
            src += 2;
            while (*src >= 0x30 && *src <= 0x3F) src++;
            while (*src >= 0x20 && *src <= 0x2F) src++;
            if (*src >= 0x40 && *src <= 0x7E) src++;
        } else {
            *dst++ = (char)*src++;
        }
    }
    *dst = '\0';
    memcpy(buf, out, (size_t)(dst - out) + 1);
}

/* Store one logo line: strip control codes, expand text escapes, save. */
static int store_line(char *buf, int line_count,
                      char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN]) {
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') { buf[len - 1] = '\0'; len--; }

    strip_private_modes(buf);
    util_expand_escapes(buf);   /* handle literal \033 in text logos */

    /* Check if visible content after stripping private codes */
    const char *check = buf;
    while (*check == ' ') check++;

    if (*check == '\0') {
        /* blank line: keep as spacer only after first content line */
        if (line_count > 0) {
            logo_lines[line_count][0] = '\0';
            return 1;
        }
        return 0;
    }

    snprintf(logo_lines[line_count], MAX_LOGO_LINE_LEN, "%s", buf);
    return 1;
}

/*
 * Load logo from a .txt file (plain ANSI art).
 */
static int load_txt(const char *path,
                    char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN]) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    int count = 0;
    char buf[MAX_LOGO_LINE_LEN * 4];

    while (fgets(buf, sizeof(buf), f) && count < MAX_LOGO_LINES) {
        if (store_line(buf, count, logo_lines)) count++;
    }
    fclose(f);

    /* Trim trailing blank lines */
    while (count > 0 && logo_lines[count - 1][0] == '\0') count--;
    return count;
}

/*
 * Load logo from an image file (PNG/JPG/GIF/…) by piping it through chafa.
 * chafa converts the image to ANSI art on stdout.
 */
static int load_image(const char *path, int logo_width,
                      char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN]) {
    int width = (logo_width > 0) ? logo_width : 32;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "chafa --size %dx%d --format symbols '%s' 2>/dev/null",
        width, MAX_LOGO_LINES, path);

    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;

    int count = 0;
    char buf[MAX_LOGO_LINE_LEN * 4];

    while (fgets(buf, sizeof(buf), fp) && count < MAX_LOGO_LINES) {
        if (store_line(buf, count, logo_lines)) count++;
    }
    pclose(fp);

    /* Trim trailing blank lines */
    while (count > 0 && logo_lines[count - 1][0] == '\0') count--;
    return count;
}

/*
 * Public entry point.
 * Priority:
 *   1. config.json "logo" path (image or txt)
 *   2. --logo CLI flag  (already stored in g_config.custom_logo_path)
 *   3. logos/<distro_id>.txt
 *   4. logos/tux.txt (fallback)
 */
int logo_load(const char *distro_id,
              char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN]) {

    /* --- Custom logo from config or CLI flag -------------------------------- */
    if (g_config.custom_logo_path[0] != '\0') {
        const char *path = g_config.custom_logo_path;

        if (g_config.logo_is_image) {
            int n = load_image(path, g_config.logo_width, logo_lines);
            if (n > 0) return n;
            /* fall through to distro default if chafa fails */
        } else {
            int n = load_txt(path, logo_lines);
            if (n > 0) return n;
        }
    }

    /* --- Distro logo from logos/ directory --------------------------------- */
    char path[512];
    snprintf(path, sizeof(path), "logos/%s.txt",
             distro_id && distro_id[0] ? distro_id : "tux");

    int n = load_txt(path, logo_lines);
    if (n > 0) return n;

    /* Fallback: Tux */
    if (!distro_id || strcmp(distro_id, "tux") != 0) {
        n = load_txt("logos/tux.txt", logo_lines);
        if (n > 0) return n;
    }

    return 0;
}
