#include "nexfetch.h"
#include <string.h>
#include <stddef.h>

/*
 * Compute the visible (on-screen) column width of a string that may contain
 * ANSI/VT escape sequences and UTF-8 multibyte characters.
 *
 * Handles:
 *   - CSI sequences:  ESC [ <params> <final>   (includes [?25l, [38;2;…m, etc.)
 *   - OSC sequences:  ESC ] … BEL / ESC\
 *   - Other 2-char:   ESC <single-byte>
 *   - UTF-8 1–4 byte codepoints (multibyte counted as 1 column; surrogate/4-byte as 2)
 */
size_t ansi_visible_length(const char *str) {
    if (!str) return 0;
    size_t len = 0;
    const unsigned char *p = (const unsigned char *)str;

    while (*p) {
        unsigned char c = *p;

        if (c == 0x1B) { /* ESC */
            p++;
            if (*p == '[') {
                /* CSI: ESC [ {param bytes 0x30–0x3F} {interm bytes 0x20–0x2F} {final 0x40–0x7E} */
                p++;
                while (*p >= 0x30 && *p <= 0x3F) p++;
                while (*p >= 0x20 && *p <= 0x2F) p++;
                if (*p >= 0x40 && *p <= 0x7E) p++;
            } else if (*p == ']') {
                /* OSC: ESC ] … BEL or ESC\ */
                p++;
                while (*p && *p != 0x07) {
                    if (*p == 0x1B && *(p + 1) == '\\') { p += 2; break; }
                    p++;
                }
                if (*p == 0x07) p++;
            } else if (*p >= 0x40 && *p <= 0x5F) {
                /* Two-char Fe escape (e.g. ESC M, ESC c) */
                p++;
            } else if (*p) {
                p++;
            }
            /* zero visible columns */

        } else if (c >= 0xF0) {
            /* 4-byte UTF-8 (rare in ASCII art, count as 2 cols to be safe) */
            p++;
            for (int k = 0; k < 3 && (*p & 0xC0) == 0x80; k++) p++;
            len += 2;

        } else if (c >= 0xE0) {
            /* 3-byte UTF-8 (e.g. box-drawing, block chars U+2580–U+259F) – 1 col */
            p++;
            if ((*p & 0xC0) == 0x80) { p++; if ((*p & 0xC0) == 0x80) p++; }
            len++;

        } else if (c >= 0xC0) {
            /* 2-byte UTF-8 – 1 col */
            p++;
            if ((*p & 0xC0) == 0x80) p++;
            len++;

        } else if (c >= 0x80) {
            /* Stray continuation byte – skip */
            p++;

        } else {
            /* Plain ASCII */
            p++;
            len++;
        }
    }
    return len;
}
