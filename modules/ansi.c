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

/*
 * Sequential-scan cache for ansi_slice_columns_ex().
 *
 * presenter.c always requests slices of the SAME captured background row
 * (same `src` pointer into s_bg_lines[row]) in strictly increasing column
 * order while it builds one output row -- key, then " : ", then value,
 * then right-padding, each call's start_col picking up where the previous
 * one's window ended. Only once a row is fully built does presenter.c move
 * on to a different row's `src` pointer.
 *
 * Without this cache, every one of those calls re-walks `src` from byte 0,
 * re-parsing every ANSI escape it has already parsed in prior calls for
 * that same row. That's O(fields_per_row * row_bytes) work per row instead
 * of O(row_bytes) -- and row_bytes can be large, since a densely-colored
 * captured background row carries a full truecolor SGR sequence (~20
 * bytes) per visible cell.
 *
 * Caching the last (src, column, byte-offset) reached lets a call that
 * continues forward on the same src resume scanning from there instead of
 * from the start. A call with a different `src`, or a start_col that goes
 * backward relative to the cached column, simply falls back to scanning
 * from byte 0 as before -- correctness never depends on the cache hitting.
 */
static struct {
    const char *src;
    size_t col;
    size_t byte_offset;
} s_scan_cache = { NULL, 0, 0 };

/*
 * ansi_slice_columns_ex() extracts the byte range of `src` that renders as
 * the visible-column window [start_col, start_col + width), preserving any
 * SGR color/attribute codes so the slice still carries the right colors
 * when printed on its own, and writes it (NUL-terminated) into `out`.
 *
 * `append_reset` controls whether a trailing "\033[0m" is appended:
 *
 *   - append_reset = 1: behaves like the original ansi_slice_columns() --
 *     used wherever the slice is meant to be a self-contained, fully closed
 *     span (padding gaps via strcat_forward()/pad_logo_column(), where
 *     nothing colored is drawn immediately after and any leftover SGR state
 *     should not leak into whatever comes next).
 *
 *   - append_reset = 0: the slice's background color is left ACTIVE in the
 *     terminal's current SGR state after printing. This is required by
 *     overlay_text() in presenter.c, whose entire technique depends on the
 *     redrawn background color still being live when it prints field text
 *     on top with a foreground-only color code -- if this function appended
 *     "\033[0m" here, that reset would fire the instant before the overlay
 *     text is drawn, wiping the real image color back to the terminal's
 *     flat default background and reproducing the exact "opaque block
 *     behind every field" bug this whole mechanism exists to avoid.
 *
 * Truncates silently if `out` is too small.
 */
void ansi_slice_columns_ex(const char *src, size_t start_col, size_t width,
                            char *out, size_t out_size, int append_reset) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!src || width == 0 || out_size < 2) return;

    size_t col = 0;
    size_t out_len = 0;
    size_t cols_written = 0; /* visible columns actually copied into `out` */
    const unsigned char *p = (const unsigned char *)src;

    /* Resume from the cached scan position if this call continues forward
     * over the same source string the previous call was scanning. */
    if (s_scan_cache.src == src && start_col >= s_scan_cache.col) {
        col = s_scan_cache.col;
        p = (const unsigned char *)src + s_scan_cache.byte_offset;
    }

    while (*p) {
        if (col >= start_col + width) break;

        size_t unit_bytes = 0;
        size_t unit_cols = 0;

        if (*p == 0x1B) {
            const unsigned char *q = p + 1;
            if (*q == '[') {
                q++;
                while (*q >= 0x30 && *q <= 0x3F) q++;
                while (*q >= 0x20 && *q <= 0x2F) q++;
                if (*q >= 0x40 && *q <= 0x7E) q++;
            } else if (*q == ']') {
                q++;
                while (*q && *q != 0x07) {
                    if (*q == 0x1B && *(q + 1) == '\\') { q += 2; break; }
                    q++;
                }
                if (*q == 0x07) q++;
            } else if (*q >= 0x40 && *q <= 0x5F) {
                q++;
            } else if (*q) {
                q++;
            }
            unit_bytes = (size_t)(q - p);
            unit_cols = 0;
        } else if ((*p & 0x80) == 0) {
            unit_bytes = 1; unit_cols = 1;
        } else if ((*p & 0xE0) == 0xC0) {
            unit_bytes = 2; unit_cols = 1;
        } else if ((*p & 0xF0) == 0xE0) {
            unit_bytes = 3; unit_cols = 1;
        } else if ((*p & 0xF8) == 0xF0) {
            unit_bytes = 4; unit_cols = 2;
        } else {
            unit_bytes = 1; unit_cols = 0;
        }

        if (unit_bytes == 0) break; /* safety against malformed input */

        int in_window = (unit_cols == 0) || (col >= start_col && col < start_col + width);
        if (in_window && out_len + unit_bytes < out_size - 1) {
            memcpy(out + out_len, p, unit_bytes);
            out_len += unit_bytes;
            if (unit_cols > 0 && col >= start_col) cols_written += unit_cols;
        }

        col += unit_cols;
        p += unit_bytes;
    }

    /*
     * If `src` ran out (or its captured content simply didn't extend far
     * enough) before the requested [start_col, start_col+width) window was
     * fully covered, cols_written < width here. Pad the shortfall with
     * plain spaces so the slice always visually fills its full requested
     * width -- inheriting whatever color state the carried-over escape
     * codes already set, so it reads as a solid block of the last-known
     * color rather than a hole.
     */
    for (; cols_written < width && out_len + 1 < out_size - 1; cols_written++) {
        out[out_len++] = ' ';
    }

    out[out_len] = '\0';
    if (append_reset && out_len > 0) {
        strncat(out, "\033[0m", out_size - out_len - 1);
    }

    /* Save scan position so the next call, if it continues forward on this
     * same `src`, can resume instead of rescanning from byte 0. */
    s_scan_cache.src = src;
    s_scan_cache.col = col;
    s_scan_cache.byte_offset = (size_t)((const char *)p - src);
}

/*
 * ansi_slice_columns() -- original signature, kept for any other caller.
 * Always appends the trailing reset (append_reset = 1).
 */
void ansi_slice_columns(const char *src, size_t start_col, size_t width, char *out, size_t out_size) {
    ansi_slice_columns_ex(src, start_col, width, out, out_size, 1);
}

/*
 * ansi_slice_scan_reset() clears the sequential-scan cache above.
 *
 * The cache matches purely on the `src` pointer value, and s_bg_lines[]'s
 * row buffers live at fixed static addresses for the whole process. If
 * render_background() is ever invoked more than once in the same run
 * (e.g. a future --watch/live-reload mode), a stale cache entry could
 * otherwise match a new call's `src` by address while its byte_offset
 * still refers to the PREVIOUS frame's content at that address, resuming
 * a scan from the wrong point. presenter.c calls this at the start of
 * every render_background() so each captured frame always starts clean.
 */
void ansi_slice_scan_reset(void) {
    s_scan_cache.src = NULL;
    s_scan_cache.col = 0;
    s_scan_cache.byte_offset = 0;
}
