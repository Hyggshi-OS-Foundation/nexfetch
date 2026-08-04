#include "presenter.h"
#include "nexfetch.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/ioctl.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

extern size_t ansi_visible_length(const char *str);
extern void ansi_slice_columns(const char *src, size_t start_col, size_t width, char *out, size_t out_size);
extern void ansi_slice_columns_ex(const char *src, size_t start_col, size_t width, char *out, size_t out_size, int append_reset);
extern void ansi_slice_scan_reset(void);
extern void module_detect_color(char *out, size_t max_len);

/* --- Captured Background ---------------------------------------------------
 *
 * render_background() streams chafa's output straight to stdout, then homes
 * the cursor and lets the presenter draw on top. Rather than relying on the
 * terminal to preserve untouched cells when the cursor moves across them
 * (not guaranteed everywhere), the background is captured into s_bg_lines[]
 * (one entry per terminal row, ANSI codes and all), and anything drawn over
 * it -- padding gaps AND the actual field text itself -- explicitly redraws
 * the matching slice of the captured row first. See overlay_text() below for
 * how field text uses this to sit on real image color instead of a flat
 * background.
 */
#define MAX_BG_ROWS 512
#define MAX_BG_LINE_LEN 16384
static char s_bg_lines[MAX_BG_ROWS][MAX_BG_LINE_LEN];
static int s_bg_row_count = 0;
static int s_bg_active = 0;

/*
 * PRESENTER_LINE_BUF sizes every row-composition buffer in the presenters
 * below (info_lines[][], row, header_row, cb_row, top, div, bot).
 *
 * These used to be sized MAX_VAL_LEN, which is fine for plain module text
 * but not for a row that also has captured background slices spliced into
 * it via strcat_forward()/overlay_text()/bg_slice(): a dense truecolor-ANSI
 * background can spend far more bytes per visible column than plain text
 * does. Since overlay_text() splices a background slice in front of EVERY
 * field (not just padding), a row can accumulate several such splices, so
 * this sizing matters even more than before. Sizing these to
 * MAX_BG_LINE_LEN keeps every splice that legitimately fits in a captured
 * background row guaranteed to fit here too.
 */
#define PRESENTER_LINE_BUF MAX_BG_LINE_LEN

/*
 * bg_slice() fills `out` with the captured background's slice for the given
 * row/column window (WITH its trailing reset -- this is the padding-gap
 * variant) and returns 1, or returns 0 (leaving `out` untouched) if there's
 * no active background for that row -- callers fall back to a plain
 * cursor-forward move or plain default background in that case, so behavior
 * without a background is unchanged from before.
 */
static int bg_slice(int row, size_t start_col, size_t width, char *out, size_t out_size) {
    if (!s_bg_active || row < 0 || row >= s_bg_row_count || width == 0) return 0;
    ansi_slice_columns(s_bg_lines[row], start_col, width, out, out_size);
    return 1;
}

/*
 * pad_logo_column() fills the unused part of the logo column for row `row`.
 * If a background was captured, it re-draws that exact slice; otherwise it
 * just moves the cursor forward (nothing to preserve).
 */
static void pad_logo_column(int row, size_t vis_w, size_t max_logo_width) {
    size_t pad = (vis_w < max_logo_width + LOGO_PADDING) ? (max_logo_width + LOGO_PADDING - vis_w) : 0;
    if (pad == 0) return;
    char slice[MAX_BG_LINE_LEN];
    if (bg_slice(row, vis_w, pad, slice, sizeof(slice))) {
        fputs(slice, stdout);
        fputs("\033[0m", stdout);
    } else {
        printf("\033[%zuC", pad);
    }
}

/*
 * strcat_forward() is pad_logo_column()'s counterpart for row-alignment
 * padding appended into a row string being built with snprintf/strcat
 * rather than printed immediately. `abs_col` is the absolute terminal
 * column where this padding begins.
 *
 * Used by render_boxed() for its box-interior padding, and by all three
 * presenters for the TEXT_MARGIN gap after each row's last field. Uses the
 * reset-appending bg_slice() -- a padding gap is meant to end cleanly, with
 * nothing colored relying on its background staying active afterward.
 */
static void strcat_forward(char *dst, size_t dst_size, size_t n, int row, size_t abs_col) {
    if (n == 0) return;
    char slice[MAX_BG_LINE_LEN];
    if (bg_slice(row, abs_col, n, slice, sizeof(slice))) {
        strncat(dst, slice, dst_size - strlen(dst) - 1);
        strncat(dst, "\033[0m", dst_size - strlen(dst) - 1);
    } else {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "\033[%zuC", n);
        strncat(dst, tmp, dst_size - strlen(dst) - 1);
    }
}

/*
 * append_bounded() is a safe strcat: it appends `src` to `dst` up to the
 * remaining capacity of `dst_size`, truncating rather than overflowing if
 * `src` (typically a module value of unbounded length) doesn't fully fit.
 */
static void append_bounded(char *dst, size_t dst_size, const char *src) {
    size_t used = strlen(dst);
    if (used >= dst_size - 1) return;
    strncat(dst, src, dst_size - used - 1);
}

/*
 * append_repeat_bounded() safely appends `count` copies of `unit` (a short
 * UTF-8 sequence such as the box-drawing "─") to `dst`, stopping early if
 * `dst_size` would be exceeded instead of overflowing the buffer. Used for
 * border-drawing where the repeated unit has nothing behind it to preserve.
 */
static void append_repeat_bounded(char *dst, size_t dst_size, const char *unit, size_t count) {
    size_t unit_len = strlen(unit);
    for (size_t i = 0; i < count; i++) {
        size_t used = strlen(dst);
        if (used + unit_len >= dst_size - 1) break;
        strncat(dst, unit, dst_size - used - 1);
    }
}

/*
 * Upper bound on box_inner_w so the boxed theme's border/row buffers can
 * never be asked to hold more than they physically can.
 */
#define MAX_BOX_INNER_W 140

/*
 * TEXT_MARGIN is a small guaranteed gap appended after each row's last
 * field in the unboxed presenters (classic, modern). It's filled via
 * strcat_forward()/bg_slice() -- an explicit redraw of the real captured
 * background pixels at that row/column -- rather than literal spaces, for
 * the same reason overlay_text() below exists: literal default-background
 * spaces paint an opaque theme-colored block over the chafa image at every
 * gap, instead of showing what the image actually looks like there.
 */
#define TEXT_MARGIN 4

/*
 * overlay_text() appends a text segment to `dst`, first redrawing the
 * captured background pixels for the exact cells the segment is about to
 * occupy, then moving the cursor back over them and drawing the real text
 * on top with `color_prefix` (a foreground-only SGR sequence, or "\033[39m"
 * for "explicit default foreground") -- deliberately NOT resetting the
 * background first.
 *
 * Why this works: SGR attributes (foreground, background, bold, etc.) are
 * independent and persist on the terminal until something explicitly
 * changes them -- they aren't tied to the cell being written. So once the
 * redrawn background slice leaves a background color active, printing new
 * glyphs with an SGR sequence that only touches foreground (never
 * "\033[49m") leaves that background color in place underneath them. The
 * glyph itself still fully occupies its cell (a cell can only hold one
 * character), but the *color it sits on* is now the real image color at
 * that position instead of a flat default background -- this is what turns
 * "opaque maroon block behind every field" into "field text drawn on the
 * actual background image".
 *
 * This depends on the background slice being fetched via
 * ansi_slice_columns_ex(..., append_reset = 0) rather than the normal
 * bg_slice()/ansi_slice_columns() used elsewhere in this file. Those append
 * a trailing "\033[0m", which would reset the background to the terminal's
 * flat default color the instant before the overlay glyphs are drawn --
 * silently recreating the exact bug this function exists to fix. Using the
 * no-reset variant here is not optional.
 *
 * `color_prefix` is required to set foreground explicitly (COLOR_KEY,
 * COLOR_VALUE, COLOR_SEP, or "\033[39m" for plain/uncolored text) rather
 * than being left empty: if we didn't set foreground at all, the field's
 * text would inherit whatever foreground color the redrawn background
 * slice's own glyph happened to leave active (essentially a random color
 * sampled from the image), which is not what any of the callers want for
 * plain text.
 *
 * `vis_width` must be the visible column width of `text` (via
 * ansi_visible_length()) -- it has to match exactly what was redrawn, or
 * the cursor-back distance over/undershoots and either leaves stray
 * background glyphs peeking out to one side of the real text, or clobbers
 * the column just before the field.
 *
 * When there is no captured background for this row (s_bg_active == 0, or
 * this row/column falls outside what was captured), falls back to plain
 * "\033[49m" (default background) before the color prefix -- the original,
 * pre-existing behavior for a plain run with no image involved.
 *
 * Caller is responsible for appending a reset ("\033[49m" and/or
 * COLOR_RESET) after the segment once done with it, same as before -- that
 * reset only fires once the text has already been drawn on the live
 * background color, so it no longer matters that it wipes the background
 * back to default at that point.
 */
static void overlay_text(char *dst, size_t dst_size, int row, size_t abs_col,
                         size_t vis_width, const char *color_prefix, const char *text) {
    if (vis_width > 0 && s_bg_active && row >= 0 && row < s_bg_row_count) {
        char slice[MAX_BG_LINE_LEN];
        ansi_slice_columns_ex(s_bg_lines[row], abs_col, vis_width, slice, sizeof(slice), 0 /* leave bg active */);
        if (slice[0] != '\0') {
            strncat(dst, slice, dst_size - strlen(dst) - 1);
            char back[32];
            snprintf(back, sizeof(back), "\033[%zuD", vis_width);
            strncat(dst, back, dst_size - strlen(dst) - 1);
        } else {
            append_bounded(dst, dst_size, "\033[49m");
        }
    } else {
        append_bounded(dst, dst_size, "\033[49m");
    }
    append_bounded(dst, dst_size, color_prefix);
    append_bounded(dst, dst_size, text);
}

/* --- Background Image Renderer --------------------------------------------- */

/*
 * Chafa (and most ANSI-art generators) optimize their own output by only
 * re-emitting an SGR color code when the color actually changes from the
 * previous cell -- including across line breaks, since a real terminal
 * never resets SGR state on '\n'. For a large uniformly-colored region of
 * an image (or an entirely solid-color image), this means only the FIRST
 * line touching that color carries the escape code; every following line
 * in that same-colored run is emitted as bare text with no color info of
 * its own, relying on the previous line's SGR state still being active.
 *
 * render_background() below captures chafa's output one line at a time
 * into s_bg_lines[], treating each line as an independent, self-contained
 * string. Read in isolation later (by bg_slice()/overlay_text(), long
 * after the original multi-line stream context is gone), a line that
 * chafa emitted "bare" like this has genuinely no color to give -- it
 * reads back as plain, uncolored text, which is what produced the black/
 * flat-default boxes: not because overlay_text's logic was wrong, but
 * because the captured data it was reading from was already colorless for
 * those rows.
 *
 * The fix: track the last-seen foreground/background SGR state as chafa's
 * lines are consumed, and prepend that carried state onto any line before
 * storing it, so every entry in s_bg_lines[] is self-contained regardless
 * of whether chafa itself repeated the color code for that particular row.
 */
typedef struct {
    char fg[24]; /* e.g. "38;2;255;255;255" or "37", empty = default */
    char bg[24]; /* e.g. "48;2;20;30;40" or "40", empty = default */
} SgrCarry;

static void sgr_carry_reset(SgrCarry *c) {
    c->fg[0] = '\0';
    c->bg[0] = '\0';
}

/* Parses one SGR "ESC [ params m" sequence's param list (already split out
   as the raw bytes between '[' and the final 'm') and updates *c. */
static void sgr_carry_apply_params(SgrCarry *c, const char *params, size_t len) {
    size_t i = 0;
    /* Reuse a local copy so we can safely tokenize without touching src. */
    char buf[256];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, params, len);
    buf[len] = '\0';

    /* Split on ';' into integer tokens, walking with an index so 38/48
       can greedily consume their trailing 2;r;g;b or 5;n sub-params. */
    int vals[32];
    int n = 0;
    char *tok = buf;
    while (*tok && n < 32) {
        char *end;
        long v = strtol(tok, &end, 10);
        vals[n++] = (*tok == '\0' || tok == end) ? 0 : (int)v; /* "" or empty field = 0 (reset) */
        if (*end == ';') { tok = end + 1; } else { break; }
    }
    if (n == 0) { vals[n++] = 0; } /* bare "\033[m" means reset */

    while (i < (size_t)n) {
        int v = vals[i];
        if (v == 0) {
            sgr_carry_reset(c);
            i++;
        } else if (v == 38 || v == 48) {
            char *dst = (v == 38) ? c->fg : c->bg;
            size_t dst_sz = 24;
            if (i + 1 < (size_t)n && vals[i + 1] == 2 && i + 4 < (size_t)n) {
                snprintf(dst, dst_sz, "%d;2;%d;%d;%d", v, vals[i+2], vals[i+3], vals[i+4]);
                i += 5;
            } else if (i + 1 < (size_t)n && vals[i + 1] == 5 && i + 2 < (size_t)n) {
                snprintf(dst, dst_sz, "%d;5;%d", v, vals[i+2]);
                i += 3;
            } else {
                i++; /* malformed, skip just the introducer */
            }
        } else if (v == 39) {
            c->fg[0] = '\0';
            i++;
        } else if (v == 49) {
            c->bg[0] = '\0';
            i++;
        } else if ((v >= 30 && v <= 37) || (v >= 90 && v <= 97)) {
            snprintf(c->fg, sizeof(c->fg), "%d", v);
            i++;
        } else if ((v >= 40 && v <= 47) || (v >= 100 && v <= 107)) {
            snprintf(c->bg, sizeof(c->bg), "%d", v);
            i++;
        } else {
            i++; /* bold/underline/etc. -- not tracked, doesn't affect fg/bg carry */
        }
    }
}

/* Scans a raw captured line for every SGR sequence in it, updating *c to
   reflect whatever color state is active by the END of the line -- this
   becomes the carried state going into the NEXT line. */
static void sgr_carry_scan_line(SgrCarry *c, const char *line) {
    const unsigned char *p = (const unsigned char *)line;
    while (*p) {
        if (*p == 0x1B && *(p + 1) == '[') {
            const unsigned char *start = p + 2;
            const unsigned char *q = start;
            while (*q && *q != 'm' && *q != 0x1B) q++;
            if (*q == 'm') {
                sgr_carry_apply_params(c, (const char *)start, (size_t)(q - start));
                p = q + 1;
                continue;
            }
        }
        p++;
    }
}

/* Builds the "\033[<fg>;<bg>m" prefix for the state carried INTO a line
   (i.e. the state as of the end of the previous line), or an empty string
   if both are at default. */
static void sgr_carry_prefix(const SgrCarry *c, char *out, size_t out_size) {
    out[0] = '\0';
    if (c->fg[0] == '\0' && c->bg[0] == '\0') return;
    if (c->fg[0] && c->bg[0]) {
        snprintf(out, out_size, "\033[%s;%sm", c->fg, c->bg);
    } else if (c->fg[0]) {
        snprintf(out, out_size, "\033[%sm", c->fg);
    } else {
        snprintf(out, out_size, "\033[%sm", c->bg);
    }
}

static int has_txt_extension(const char *path) {
    if (!path) return 0;
    const char *dot = strrchr(path, '.');
    if (!dot) return 0;
    const char *ext = dot + 1;
#ifdef _WIN32
    return _stricmp(ext, "txt") == 0;
#else
    return strcasecmp(ext, "txt") == 0;
#endif
}

void render_background(const char *image_path, int content_rows) {
    s_bg_active = 0;
    s_bg_row_count = 0;
    memset(s_bg_lines, 0, sizeof(s_bg_lines));
    ansi_slice_scan_reset();
    if (!image_path || !image_path[0]) return;

    int cols = 80, rows = 24;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        cols = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
        rows = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        if (ws.ws_col > 0) cols = (int)ws.ws_col;
        if (ws.ws_row > 0) rows = (int)ws.ws_row;
    }
#endif

    /*
     * bg_rows used to be hard-capped at (terminal_rows - 1), regardless of
     * how many rows the actual logo/info frame needs. Whenever the frame
     * (logo lines, or key:value rows -- easily 20-30+ once several plugin
     * modules like Docker/Camera/Vision are enabled) was taller than the
     * *current* terminal window, every row beyond that cap had no captured
     * background in s_bg_lines[] at all. overlay_text()/bg_slice() then
     * silently fell back to a flat default background for those rows --
     * this is what produced the plain uncolored/black blocks behind fields
     * like "MyModule", "Vision Docker", "Vision Camera", etc. sitting below
     * the terminal's visible height at capture time.
     *
     * The caller (main.c) already knows exactly how many rows the frame it
     * is about to draw needs -- logo_count and result_count are computed
     * before render_background() runs -- so it passes that in as
     * `content_rows`. We capture at least that many lines from chafa, even
     * if it's more than the terminal currently shows; the extra rows just
     * scroll into view like any other tall terminal output, instead of
     * silently losing their background color.
     */
    int bg_rows = rows > 1 ? rows - 1 : rows;
    if (content_rows > bg_rows) bg_rows = content_rows;
    if (bg_rows > MAX_BG_ROWS) bg_rows = MAX_BG_ROWS;

    int is_txt = has_txt_extension(image_path);
    FILE *fp = NULL;

    if (is_txt) {
        fp = fopen(image_path, "r");
    } else {
        char cmd[1280];
        snprintf(cmd, sizeof(cmd),
            "chafa --stretch --margin-bottom=0 --size %dx%d --format symbols '%s' 2>/dev/null",
            cols, bg_rows, image_path);
        fp = popen(cmd, "r");
    }
    if (!fp) return;

    SgrCarry carry;
    sgr_carry_reset(&carry);

    char line[MAX_BG_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        if (s_bg_row_count >= bg_rows) break;

        fputs(line, stdout);

        size_t len = strlen(line);
        int has_newline = (len > 0 && line[len - 1] == '\n');
        if (has_newline) line[--len] = '\0';

        if (s_bg_row_count < MAX_BG_ROWS) {
            char *dst = s_bg_lines[s_bg_row_count];
            size_t used = strlen(dst);

            /* Prepend whatever color state carried over from the end of
               the previous line, BEFORE this line's own bytes, so this
               row is self-contained even if chafa itself emitted no color
               code here (relying on the just-ended previous line's state
               still being active, which is true on a real terminal but
               not true once this line is later read back in isolation). */
            char prefix[48];
            sgr_carry_prefix(&carry, prefix, sizeof(prefix));
            if (prefix[0] && used < MAX_BG_LINE_LEN - 1) {
                size_t room = MAX_BG_LINE_LEN - 1 - used;
                size_t plen = strlen(prefix);
                size_t copy_len = plen < room ? plen : room;
                memcpy(dst + used, prefix, copy_len);
                used += copy_len;
                dst[used] = '\0';
            }

            if (used < MAX_BG_LINE_LEN - 1) {
                size_t room = MAX_BG_LINE_LEN - 1 - used;
                size_t copy_len = len < room ? len : room;
                memcpy(dst + used, line, copy_len);
                dst[used + copy_len] = '\0';
            }

            /* Update carried state from this line's OWN color codes so it's
               ready for whatever the NEXT line needs. */
            sgr_carry_scan_line(&carry, line);

            if (has_newline) s_bg_row_count++;
        }
    }
    if (is_txt) fclose(fp);
    else        pclose(fp);

    if (s_bg_row_count > 0) s_bg_active = 1;

    fputs("\033[0m", stdout);
    fputs("\033[H", stdout);
    fflush(stdout);
}

/* --- Classic Presenter ----------------------------------------------------- */

static void render_classic(const char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN],
                           int logo_count, size_t max_logo_width,
                           const ModuleResult results[], int result_count,
                           const char *user_host, const char *separator) {
    static char info_lines[MAX_MODULES + 6][PRESENTER_LINE_BUF];
    memset(info_lines, 0, sizeof(info_lines));
    int info_count = 0;

    size_t logo_offset = g_config.show_logo ? (max_logo_width + LOGO_PADDING) : 0;

    /* user_host row */
    {
        size_t uh_w = ansi_visible_length(user_host);
        overlay_text(info_lines[info_count], PRESENTER_LINE_BUF, info_count, logo_offset,
                     uh_w, "\033[39m", user_host);
        append_bounded(info_lines[info_count], PRESENTER_LINE_BUF, "\033[49m");
        strcat_forward(info_lines[info_count], PRESENTER_LINE_BUF, TEXT_MARGIN,
                       info_count, logo_offset + uh_w);
        info_count++;
    }

    /* separator row */
    {
        size_t sep_w = ansi_visible_length(separator);
        overlay_text(info_lines[info_count], PRESENTER_LINE_BUF, info_count, logo_offset,
                     sep_w, COLOR_SEP, separator);
        append_bounded(info_lines[info_count], PRESENTER_LINE_BUF, "\033[49m" COLOR_RESET);
        strcat_forward(info_lines[info_count], PRESENTER_LINE_BUF, TEXT_MARGIN,
                       info_count, logo_offset + sep_w);
        info_count++;
    }

    /* key : value rows */
    for (int i = 0; i < result_count; i++) {
        size_t kw = ansi_visible_length(results[i].key);
        size_t vw = ansi_visible_length(results[i].val);
        size_t col = logo_offset;

        overlay_text(info_lines[info_count], PRESENTER_LINE_BUF, info_count, col,
                     kw, COLOR_KEY, results[i].key);
        append_bounded(info_lines[info_count], PRESENTER_LINE_BUF, "\033[49m" COLOR_RESET);
        col += kw;

        overlay_text(info_lines[info_count], PRESENTER_LINE_BUF, info_count, col,
                     2, "\033[39m", ": ");
        append_bounded(info_lines[info_count], PRESENTER_LINE_BUF, "\033[49m");
        col += 2;

        overlay_text(info_lines[info_count], PRESENTER_LINE_BUF, info_count, col,
                     vw, COLOR_VALUE, results[i].val);
        append_bounded(info_lines[info_count], PRESENTER_LINE_BUF, "\033[49m" COLOR_RESET);
        col += vw;

        strcat_forward(info_lines[info_count], PRESENTER_LINE_BUF, TEXT_MARGIN, info_count, col);
        info_count++;
    }

    if (g_config.color_blocks) {
        snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", "");
        char color_bar[PRESENTER_LINE_BUF] = "";
        module_detect_color(color_bar, sizeof(color_bar));
        size_t cb_w = ansi_visible_length(color_bar);
        overlay_text(info_lines[info_count], PRESENTER_LINE_BUF, info_count, logo_offset,
                     cb_w, "\033[39m", color_bar);
        append_bounded(info_lines[info_count], PRESENTER_LINE_BUF, "\033[49m");
        strcat_forward(info_lines[info_count], PRESENTER_LINE_BUF, TEXT_MARGIN,
                       info_count, logo_offset + cb_w);
        info_count++;
    }

    int total_rows = logo_count > info_count ? logo_count : info_count;
    for (int r = 0; r < total_rows; r++) {
        if (g_config.show_logo) {
            const char *l_line = r < logo_count ? logo_lines[r] : "";
            size_t vis_w = ansi_visible_length(l_line);
            printf("%s" COLOR_RESET, l_line);
            pad_logo_column(r, vis_w, max_logo_width);
        }
        if (r < info_count) printf("%s", info_lines[r]);
        printf("\n");
    }
    printf("\n");
}

/* --- Boxed Aesthetic Presenter --------------------------------------------- */

static void render_boxed(const char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN],
                         int logo_count, size_t max_logo_width,
                         const ModuleResult results[], int result_count,
                         const char *user_host, const char *separator) {
    (void)separator;

    size_t max_key_w = 0;
    size_t max_val_w = 0;

    size_t user_host_w = ansi_visible_length(user_host);
    if (user_host_w > max_val_w) max_val_w = user_host_w;

    for (int i = 0; i < result_count; i++) {
        size_t kw = ansi_visible_length(results[i].key);
        size_t vw = ansi_visible_length(results[i].val);
        if (kw > max_key_w) max_key_w = kw;
        if (vw > max_val_w) max_val_w = vw;
    }

    size_t box_inner_w = max_key_w + 3 + max_val_w + 2;
    if (box_inner_w < user_host_w + 4) box_inner_w = user_host_w + 4;
    if (box_inner_w < 36) box_inner_w = 36;
    if (box_inner_w > MAX_BOX_INNER_W) box_inner_w = MAX_BOX_INNER_W;

    size_t logo_offset = g_config.show_logo ? (max_logo_width + LOGO_PADDING) : 0;

    static char info_lines[MAX_MODULES + 8][PRESENTER_LINE_BUF];
    memset(info_lines, 0, sizeof(info_lines));
    int info_count = 0;

    /* Top border -- border glyphs aren't field text, left unchanged (no
       image content to blend into that matters for a solid border line). */
    char top[PRESENTER_LINE_BUF] = "\033[49m\033[1;36m╭";
    append_repeat_bounded(top, sizeof(top), "─", box_inner_w);
    strncat(top, "╮\033[0m", sizeof(top) - strlen(top) - 1);
    snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", top);

    /* Header row: │ hyggshi@hostname          │
       Built directly: left border, a leading space, then user_host drawn
       over the real background, then padding to the box edge, then the
       right border. (No placeholder/no-op calls here -- build it once.) */
    char header_row[PRESENTER_LINE_BUF] = "";
    append_bounded(header_row, sizeof(header_row), "\033[49m\033[1;36m│\033[0m\033[49m ");
    size_t h_col = logo_offset + 2; /* "│ " */
    overlay_text(header_row, sizeof(header_row), info_count, h_col, user_host_w, "\033[39m", user_host);
    append_bounded(header_row, sizeof(header_row), "\033[49m");
    size_t h_vis_pad = box_inner_w > (user_host_w + 1) ? box_inner_w - (user_host_w + 1) : 0;
    strcat_forward(header_row, sizeof(header_row), h_vis_pad, info_count, h_col + user_host_w);
    append_bounded(header_row, sizeof(header_row), "\033[1;36m│\033[0m");
    snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", header_row);

    /* Divider row -- border glyphs, unchanged. */
    char div[PRESENTER_LINE_BUF] = "\033[49m\033[1;36m├";
    append_repeat_bounded(div, sizeof(div), "─", box_inner_w);
    strncat(div, "┤\033[0m", sizeof(div) - strlen(div) - 1);
    snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", div);

    /* Data rows: │ OS         : Ubuntu 26.04   │ */
    for (int i = 0; i < result_count; i++) {
        char row[PRESENTER_LINE_BUF] = "\033[49m\033[1;36m│\033[0m\033[49m ";
        size_t kw = ansi_visible_length(results[i].key);
        size_t vw = ansi_visible_length(results[i].val);
        size_t col = logo_offset + 2; /* "│ " */

        overlay_text(row, sizeof(row), info_count, col, kw, COLOR_KEY, results[i].key);
        append_bounded(row, sizeof(row), "\033[49m" COLOR_RESET);
        col += kw;

        strcat_forward(row, sizeof(row), max_key_w - kw, info_count, col);
        col += (max_key_w - kw);

        overlay_text(row, sizeof(row), info_count, col, 3, "\033[39m", " : ");
        append_bounded(row, sizeof(row), "\033[49m");
        col += 3;

        overlay_text(row, sizeof(row), info_count, col, vw, COLOR_VALUE, results[i].val);
        append_bounded(row, sizeof(row), "\033[49m" COLOR_RESET);
        col += vw;

        size_t cur_vis = 1 + 1 + kw + (max_key_w - kw) + 3 + vw;
        size_t r_pad = box_inner_w > cur_vis ? box_inner_w - cur_vis : 0;
        strcat_forward(row, sizeof(row), r_pad, info_count, col);
        append_bounded(row, sizeof(row), "\033[1;36m│\033[0m");

        snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", row);
    }

    if (g_config.color_blocks) {
        char color_bar[PRESENTER_LINE_BUF] = "";
        module_detect_color(color_bar, sizeof(color_bar));
        size_t cb_vis = ansi_visible_length(color_bar);

        char cb_row[PRESENTER_LINE_BUF] = "\033[49m\033[1;36m│\033[0m\033[49m ";
        size_t cb_col = logo_offset + 2;
        overlay_text(cb_row, sizeof(cb_row), info_count, cb_col, cb_vis, "\033[39m", color_bar);
        append_bounded(cb_row, sizeof(cb_row), "\033[49m" COLOR_RESET);
        size_t cb_pad = box_inner_w > (cb_vis + 1) ? box_inner_w - (cb_vis + 1) : 0;
        strcat_forward(cb_row, sizeof(cb_row), cb_pad, info_count, cb_col + cb_vis);
        append_bounded(cb_row, sizeof(cb_row), "\033[1;36m│\033[0m");
        snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", cb_row);
    }

    /* Bottom border -- border glyphs, unchanged. */
    char bot[PRESENTER_LINE_BUF] = "\033[49m\033[1;36m╰";
    append_repeat_bounded(bot, sizeof(bot), "─", box_inner_w);
    strncat(bot, "╯\033[0m", sizeof(bot) - strlen(bot) - 1);
    snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", bot);

    int total_rows = logo_count > info_count ? logo_count : info_count;
    for (int r = 0; r < total_rows; r++) {
        if (g_config.show_logo) {
            const char *l_line = r < logo_count ? logo_lines[r] : "";
            size_t vis_w = ansi_visible_length(l_line);
            printf("%s" COLOR_RESET, l_line);
            pad_logo_column(r, vis_w, max_logo_width);
        }
        if (r < info_count) printf("%s", info_lines[r]);
        printf("\n");
    }
    printf("\n");
}

/* --- Modern Presenter (Tree connectors) ----------------------------------- */

static void render_modern(const char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN],
                          int logo_count, size_t max_logo_width,
                          const ModuleResult results[], int result_count,
                          const char *user_host, const char *separator) {
    (void)separator;
    static char info_lines[MAX_MODULES + 6][PRESENTER_LINE_BUF];
    memset(info_lines, 0, sizeof(info_lines));
    int info_count = 0;

    size_t logo_offset = g_config.show_logo ? (max_logo_width + LOGO_PADDING) : 0;

    {
        size_t uh_w = ansi_visible_length(user_host);
        overlay_text(info_lines[info_count], PRESENTER_LINE_BUF, info_count, logo_offset,
                     uh_w, "\033[39m", user_host);
        append_bounded(info_lines[info_count], PRESENTER_LINE_BUF, "\033[49m");
        strcat_forward(info_lines[info_count], PRESENTER_LINE_BUF, TEXT_MARGIN,
                       info_count, logo_offset + uh_w);
        info_count++;
    }

    for (int i = 0; i < result_count; i++) {
        const char *connector = (i == result_count - 1) ? "\033[1;36m╰─\033[0m " : "\033[1;36m├─\033[0m ";
        size_t conn_w = ansi_visible_length(connector);
        size_t kw = ansi_visible_length(results[i].key);
        size_t vw = ansi_visible_length(results[i].val);
        size_t col = logo_offset;

        /* Connector glyphs kept as literal output (unchanged) -- they're
           short box-drawing symbols, not the kind of field text this fix
           targets, and always have their own explicit color already. */
        append_bounded(info_lines[info_count], PRESENTER_LINE_BUF, connector);
        col += conn_w;

        overlay_text(info_lines[info_count], PRESENTER_LINE_BUF, info_count, col,
                     kw, COLOR_KEY, results[i].key);
        append_bounded(info_lines[info_count], PRESENTER_LINE_BUF, "\033[49m" COLOR_RESET);
        col += kw;

        overlay_text(info_lines[info_count], PRESENTER_LINE_BUF, info_count, col,
                     2, "\033[39m", ": ");
        append_bounded(info_lines[info_count], PRESENTER_LINE_BUF, "\033[49m");
        col += 2;

        overlay_text(info_lines[info_count], PRESENTER_LINE_BUF, info_count, col,
                     vw, COLOR_VALUE, results[i].val);
        append_bounded(info_lines[info_count], PRESENTER_LINE_BUF, "\033[49m" COLOR_RESET);
        col += vw;

        strcat_forward(info_lines[info_count], PRESENTER_LINE_BUF, TEXT_MARGIN, info_count, col);
        info_count++;
    }

    if (g_config.color_blocks) {
        char color_bar[PRESENTER_LINE_BUF] = "";
        module_detect_color(color_bar, sizeof(color_bar));
        size_t cb_w = ansi_visible_length(color_bar);
        size_t col = logo_offset + 3; /* matches original "   " lead-in */
        append_bounded(info_lines[info_count], PRESENTER_LINE_BUF, "\033[49m   ");
        overlay_text(info_lines[info_count], PRESENTER_LINE_BUF, info_count, col,
                     cb_w, "\033[39m", color_bar);
        append_bounded(info_lines[info_count], PRESENTER_LINE_BUF, "\033[49m");
        strcat_forward(info_lines[info_count], PRESENTER_LINE_BUF, TEXT_MARGIN,
                       info_count, col + cb_w);
        info_count++;
    }

    int total_rows = logo_count > info_count ? logo_count : info_count;
    for (int r = 0; r < total_rows; r++) {
        if (g_config.show_logo) {
            const char *l_line = r < logo_count ? logo_lines[r] : "";
            size_t vis_w = ansi_visible_length(l_line);
            printf("%s" COLOR_RESET, l_line);
            pad_logo_column(r, vis_w, max_logo_width);
        }
        if (r < info_count) printf("%s", info_lines[r]);
        printf("\n");
    }
    printf("\n");
}

/* --- Registry & Presenter Dispatcher ------------------------------------- */

static Presenter s_presenters[] = {
    { "boxed", render_boxed },
    { "classic", render_classic },
    { "modern", render_modern },
    { NULL, NULL }
};

static Presenter *s_active_presenter = &s_presenters[0];

void presenter_manager_init(void) {
    s_active_presenter = &s_presenters[0];
}

Presenter *presenter_get_active(void) {
    return s_active_presenter;
}

void presenter_set_active(const char *name) {
    if (!name) return;
    for (int i = 0; s_presenters[i].name != NULL; i++) {
        if (strcmp(s_presenters[i].name, name) == 0) {
            s_active_presenter = &s_presenters[i];
            return;
        }
    }
}

void presenter_render(const char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN],
                      int logo_count, size_t max_logo_width,
                      const ModuleResult results[], int result_count,
                      const char *user_host, const char *separator) {
    if (s_active_presenter && s_active_presenter->render) {
        s_active_presenter->render(logo_lines, logo_count, max_logo_width,
                                  results, result_count, user_host, separator);
    }
}
