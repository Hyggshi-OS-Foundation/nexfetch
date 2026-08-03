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
extern void module_detect_color(char *out, size_t max_len);

/* --- Captured Background ---------------------------------------------------
 *
 * render_background() used to stream chafa's output straight to stdout, then
 * home the cursor and let the presenter draw on top, skipping the padding
 * cells via cursor-forward moves so the background would "show through" the
 * gaps. That relies on the terminal preserving a cell's content when the
 * cursor is moved across it without writing anything -- true for classic
 * VT100-style emulators, but not guaranteed everywhere, and some terminals
 * render an untouched cell as blank/default rather than whatever was already
 * drawn there.
 *
 * To not depend on that, the background is captured into s_bg_lines[] (one
 * entry per terminal row, ANSI codes and all) and every padding gap is filled
 * by re-printing the exact matching slice of the captured row via
 * ansi_slice_columns(), rather than skipped. That way the "gap" is an actual
 * explicit redraw of the same pixels, not a bet on terminal behavior.
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
 * but not for a row that also has a captured background slice spliced into
 * it via strcat_forward()/bg_slice(): a dense truecolor-ANSI background can
 * spend far more bytes per visible column than plain text does. When that
 * splice happened during the *key padding* step (before " : " + the value
 * had been appended), it could consume most of a MAX_VAL_LEN buffer right
 * there -- and append_bounded()'s bounds check would then silently no-op
 * on the value append, since the buffer already looked "full". Net effect:
 * the key label would print, but the value after it would just vanish,
 * with no error anywhere. Rows whose key length happened to equal the
 * longest key (no padding needed, so no splice call) were unaffected --
 * which is why some rows always rendered fine and others never did.
 *
 * Sizing these to MAX_BG_LINE_LEN removes the case entirely: any splice
 * that legitimately fit in a captured background row is now guaranteed to
 * fit here too, so the value is never pushed out.
 */
#define PRESENTER_LINE_BUF MAX_BG_LINE_LEN

/*
 * bg_slice() fills `out` with the captured background's slice for the given
 * row/column window and returns 1, or returns 0 (leaving `out` untouched) if
 * there's no active background for that row -- callers fall back to a plain
 * cursor-forward move in that case, so behavior without a background is
 * unchanged from before.
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
    } else {
        printf("\033[%zuC", pad);
    }
}

/*
 * strcat_forward() is pad_logo_column()'s counterpart for the boxed theme's
 * row-alignment padding, appended into a row string being built with
 * snprintf/strcat rather than printed immediately. `abs_col` is the absolute
 * terminal column where this padding begins (logo column width, if any, plus
 * however much of the box row has been assembled so far).
 */
static void strcat_forward(char *dst, size_t dst_size, size_t n, int row, size_t abs_col) {
    if (n == 0) return;
    char slice[MAX_BG_LINE_LEN];
    if (bg_slice(row, abs_col, n, slice, sizeof(slice))) {
        strncat(dst, slice, dst_size - strlen(dst) - 1);
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
 * `dst_size` would be exceeded instead of overflowing the buffer. Replaces
 * the old raw strcat() loops, which had no bound on `count` and could write
 * past a fixed MAX_VAL_LEN buffer whenever a module value was long enough
 * (this was the cause of the segfault: box_inner_w is derived from module
 * output width with no upper limit, and each "─" is 3 raw bytes).
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
 * Upper bound on box_inner_w so the boxed theme's border/row buffers (sized
 * MAX_VAL_LEN) can never be asked to hold more than they physically can, and
 * so an unusually long module value (long GPU/disk/network strings, etc.)
 * degrades into a clamped box instead of crashing. "─" is 3 UTF-8 bytes, so
 * this keeps worst-case border length safely under MAX_VAL_LEN.
 */
#define MAX_BOX_INNER_W 140

/* --- Background Image Renderer --------------------------------------------- */

/*
 * render_background() fills the terminal with an ANSI-art render of the given
 * image (using chafa), then moves the cursor back to the top-left so that the
 * normal fetch output is drawn on top as an overlay.
 *
 * Gracefully degrades: if chafa is not installed or the image cannot be read,
 * the function returns silently and fetch continues without a background.
 */
void render_background(const char *image_path) {
    s_bg_active = 0;
    s_bg_row_count = 0;
    memset(s_bg_lines, 0, sizeof(s_bg_lines));
    if (!image_path || !image_path[0]) return;

    /* Determine terminal dimensions */
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

    /* Build chafa command sized to the full terminal */
    char cmd[1280];
    snprintf(cmd, sizeof(cmd),
        "chafa --size %dx%d --format symbols '%s' 2>/dev/null",
        cols, rows, image_path);

    FILE *fp = popen(cmd, "r");
    if (!fp) return;

    char line[MAX_BG_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        /* Print immediately so anything the presenter never touches (rows
           below the box, columns past its right edge, etc.) still shows
           the image, then keep a de-newlined copy for later splicing. */
        fputs(line, stdout);

        size_t len = strlen(line);
        int has_newline = (len > 0 && line[len - 1] == '\n');
        if (has_newline) line[--len] = '\0';

        if (s_bg_row_count < MAX_BG_ROWS) {
            /* A single visual chafa row can exceed MAX_BG_LINE_LEN when
               truecolor SGR codes are dense (wide terminal + 24-bit fg/bg
               per cell), in which case fgets() returns it across multiple
               reads with no '\n' in between. Append each read onto the
               *current* row instead of starting a new one, and only
               advance to the next row index once a real newline is seen --
               otherwise a mid-escape-sequence split gets stored as a bogus
               separate "row" and later leaks as literal garbage text when
               spliced into padding. */
            char *dst = s_bg_lines[s_bg_row_count];
            size_t used = strlen(dst);
            if (used < MAX_BG_LINE_LEN - 1) {
                size_t room = MAX_BG_LINE_LEN - 1 - used;
                size_t copy_len = len < room ? len : room;
                memcpy(dst + used, line, copy_len);
                dst[used + copy_len] = '\0';
            }
            if (has_newline) s_bg_row_count++;
        }
    }
    pclose(fp);

    if (s_bg_row_count > 0) s_bg_active = 1;

    /* Reposition cursor to top-left so fetch output overlays the background */
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

    snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", user_host);
    snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, COLOR_SEP "%s" COLOR_RESET, separator);

    for (int i = 0; i < result_count; i++) {
        snprintf(info_lines[info_count++], PRESENTER_LINE_BUF,
            COLOR_KEY "%s" COLOR_RESET ": " COLOR_VALUE "%s" COLOR_RESET,
            results[i].key, results[i].val);
    }

    if (g_config.color_blocks) {
        snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", "");
        char color_bar[PRESENTER_LINE_BUF] = "";
        module_detect_color(color_bar, sizeof(color_bar));
        snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", color_bar);
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

    /* Compute max key and value width to format a clean box */
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

    /* Absolute terminal column where the box itself begins (past the logo
       column, if one is being shown alongside it). Needed so padding
       splices can look up the right slice of the captured background row. */
    size_t logo_offset = g_config.show_logo ? (max_logo_width + LOGO_PADDING) : 0;

    static char info_lines[MAX_MODULES + 8][PRESENTER_LINE_BUF];
    memset(info_lines, 0, sizeof(info_lines));
    int info_count = 0;

    /* Top border: ╭────────────────────────────╮ */
    char top[PRESENTER_LINE_BUF] = "\033[1;36m╭";
    append_repeat_bounded(top, sizeof(top), "─", box_inner_w);
    strncat(top, "╮\033[0m", sizeof(top) - strlen(top) - 1);
    snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", top);

    /* Header row: │ hyggshi@hostname          │ */
    char header_row[PRESENTER_LINE_BUF];
    size_t u_vis = ansi_visible_length(user_host);
    snprintf(header_row, sizeof(header_row), "\033[1;36m│\033[0m %s", user_host);
    size_t h_vis_pad = box_inner_w > (u_vis + 1) ? box_inner_w - (u_vis + 1) : 0;
    strcat_forward(header_row, sizeof(header_row), h_vis_pad, info_count, logo_offset + ansi_visible_length(header_row));
    append_bounded(header_row, sizeof(header_row), "\033[1;36m│\033[0m");
    snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", header_row);

    /* Divider row: ├────────────────────────────┤ */
    char div[PRESENTER_LINE_BUF] = "\033[1;36m├";
    append_repeat_bounded(div, sizeof(div), "─", box_inner_w);
    strncat(div, "┤\033[0m", sizeof(div) - strlen(div) - 1);
    snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", div);

    /* Data rows: │ OS         : Ubuntu 26.04   │ */
    for (int i = 0; i < result_count; i++) {
        char row[PRESENTER_LINE_BUF];
        size_t kw = ansi_visible_length(results[i].key);
        size_t vw = ansi_visible_length(results[i].val);

        snprintf(row, sizeof(row), "\033[1;36m│\033[0m " COLOR_KEY "%s" COLOR_RESET, results[i].key);

        /* Pad key space */
        strcat_forward(row, sizeof(row), max_key_w - kw, info_count, logo_offset + ansi_visible_length(row));
        append_bounded(row, sizeof(row), " : " COLOR_VALUE);
        append_bounded(row, sizeof(row), results[i].val);
        append_bounded(row, sizeof(row), COLOR_RESET);

        size_t cur_vis = 1 + 1 + kw + (max_key_w - kw) + 3 + vw;
        size_t r_pad = box_inner_w > cur_vis ? box_inner_w - cur_vis : 0;
        strcat_forward(row, sizeof(row), r_pad, info_count, logo_offset + ansi_visible_length(row));
        append_bounded(row, sizeof(row), "\033[1;36m│\033[0m");

        snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", row);
    }

    /* Color bar row if enabled */
    if (g_config.color_blocks) {
        char color_bar[PRESENTER_LINE_BUF] = "";
        module_detect_color(color_bar, sizeof(color_bar));
        size_t cb_vis = ansi_visible_length(color_bar);

        char cb_row[PRESENTER_LINE_BUF];
        snprintf(cb_row, sizeof(cb_row), "\033[1;36m│\033[0m %s" COLOR_RESET, color_bar);
        size_t cb_pad = box_inner_w > (cb_vis + 1) ? box_inner_w - (cb_vis + 1) : 0;
        strcat_forward(cb_row, sizeof(cb_row), cb_pad, info_count, logo_offset + ansi_visible_length(cb_row));
        append_bounded(cb_row, sizeof(cb_row), "\033[1;36m│\033[0m");
        snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", cb_row);
    }

    /* Bottom border: ╰────────────────────────────╯ */
    char bot[PRESENTER_LINE_BUF] = "\033[1;36m╰";
    append_repeat_bounded(bot, sizeof(bot), "─", box_inner_w);
    strncat(bot, "╯\033[0m", sizeof(bot) - strlen(bot) - 1);
    snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", bot);

    /* Render logo side-by-side with boxed frame */
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

    snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "%s", user_host);

    for (int i = 0; i < result_count; i++) {
        const char *connector = (i == result_count - 1) ? "\033[1;36m╰─\033[0m " : "\033[1;36m├─\033[0m ";
        snprintf(info_lines[info_count++], PRESENTER_LINE_BUF,
            "%s" COLOR_KEY "%s" COLOR_RESET ": " COLOR_VALUE "%s" COLOR_RESET,
            connector, results[i].key, results[i].val);
    }

    if (g_config.color_blocks) {
        char color_bar[PRESENTER_LINE_BUF] = "";
        module_detect_color(color_bar, sizeof(color_bar));
        snprintf(info_lines[info_count++], PRESENTER_LINE_BUF, "   %s", color_bar);
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

static Presenter *s_active_presenter = &s_presenters[0]; // Default to boxed

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
