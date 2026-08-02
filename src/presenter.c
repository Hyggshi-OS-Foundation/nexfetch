#include "presenter.h"
#include "nexfetch.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern size_t ansi_visible_length(const char *str);
extern void module_detect_color(char *out, size_t max_len);

/* --- Classic Presenter ----------------------------------------------------- */

static void render_classic(const char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN],
                           int logo_count, size_t max_logo_width,
                           const ModuleResult results[], int result_count,
                           const char *user_host, const char *separator) {
    char info_lines[MAX_MODULES + 6][MAX_VAL_LEN];
    memset(info_lines, 0, sizeof(info_lines));
    int info_count = 0;

    snprintf(info_lines[info_count++], MAX_VAL_LEN, "%s", user_host);
    snprintf(info_lines[info_count++], MAX_VAL_LEN, COLOR_SEP "%s" COLOR_RESET, separator);

    for (int i = 0; i < result_count; i++) {
        snprintf(info_lines[info_count++], MAX_VAL_LEN,
            COLOR_KEY "%s" COLOR_RESET ": " COLOR_VALUE "%s" COLOR_RESET,
            results[i].key, results[i].val);
    }

    if (g_config.color_blocks) {
        snprintf(info_lines[info_count++], MAX_VAL_LEN, "%s", "");
        char color_bar[MAX_VAL_LEN] = "";
        module_detect_color(color_bar, sizeof(color_bar));
        snprintf(info_lines[info_count++], MAX_VAL_LEN, "%s", color_bar);
    }

    int total_rows = logo_count > info_count ? logo_count : info_count;
    for (int r = 0; r < total_rows; r++) {
        if (g_config.show_logo) {
            const char *l_line = r < logo_count ? logo_lines[r] : "";
            size_t vis_w = ansi_visible_length(l_line);
            printf("%s" COLOR_RESET, l_line);
            for (size_t pad = vis_w; pad < max_logo_width + LOGO_PADDING; pad++) putchar(' ');
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

    char info_lines[MAX_MODULES + 8][MAX_VAL_LEN];
    memset(info_lines, 0, sizeof(info_lines));
    int info_count = 0;

    /* Top border: ╭────────────────────────────╮ */
    char top[MAX_VAL_LEN] = "\033[1;36m╭";
    for (size_t i = 0; i < box_inner_w; i++) {
        strcat(top, "─");
    }
    strcat(top, "╮\033[0m");
    snprintf(info_lines[info_count++], MAX_VAL_LEN, "%s", top);

    /* Header row: │ hyggshi@hostname          │ */
    char header_row[MAX_VAL_LEN];
    size_t u_vis = ansi_visible_length(user_host);
    snprintf(header_row, sizeof(header_row), "\033[1;36m│\033[0m %s", user_host);
    size_t h_vis_pad = box_inner_w > (u_vis + 1) ? box_inner_w - (u_vis + 1) : 0;
    for (size_t i = 0; i < h_vis_pad; i++) strcat(header_row, " ");
    strcat(header_row, "\033[1;36m│\033[0m");
    snprintf(info_lines[info_count++], MAX_VAL_LEN, "%s", header_row);

    /* Divider row: ├────────────────────────────┤ */
    char div[MAX_VAL_LEN] = "\033[1;36m├";
    for (size_t i = 0; i < box_inner_w; i++) strcat(div, "─");
    strcat(div, "┤\033[0m");
    snprintf(info_lines[info_count++], MAX_VAL_LEN, "%s", div);

    /* Data rows: │ OS         : Ubuntu 26.04   │ */
    for (int i = 0; i < result_count; i++) {
        char row[MAX_VAL_LEN];
        size_t kw = ansi_visible_length(results[i].key);
        size_t vw = ansi_visible_length(results[i].val);

        snprintf(row, sizeof(row), "\033[1;36m│\033[0m " COLOR_KEY "%s" COLOR_RESET, results[i].key);

        /* Pad key space */
        for (size_t k = kw; k < max_key_w; k++) strcat(row, " ");
        strcat(row, " : " COLOR_VALUE);
        strcat(row, results[i].val);
        strcat(row, COLOR_RESET);

        size_t cur_vis = 1 + 1 + kw + (max_key_w - kw) + 3 + vw;
        size_t r_pad = box_inner_w > cur_vis ? box_inner_w - cur_vis : 0;
        for (size_t p = 0; p < r_pad; p++) strcat(row, " ");
        strcat(row, "\033[1;36m│\033[0m");

        snprintf(info_lines[info_count++], MAX_VAL_LEN, "%s", row);
    }

    /* Color bar row if enabled */
    if (g_config.color_blocks) {
        char color_bar[MAX_VAL_LEN] = "";
        module_detect_color(color_bar, sizeof(color_bar));
        size_t cb_vis = ansi_visible_length(color_bar);

        char cb_row[MAX_VAL_LEN];
        snprintf(cb_row, sizeof(cb_row), "\033[1;36m│\033[0m %s" COLOR_RESET, color_bar);
        size_t cb_pad = box_inner_w > (cb_vis + 1) ? box_inner_w - (cb_vis + 1) : 0;
        for (size_t i = 0; i < cb_pad; i++) strcat(cb_row, " ");
        strcat(cb_row, "\033[1;36m│\033[0m");
        snprintf(info_lines[info_count++], MAX_VAL_LEN, "%s", cb_row);
    }

    /* Bottom border: ╰────────────────────────────╯ */
    char bot[MAX_VAL_LEN] = "\033[1;36m╰";
    for (size_t i = 0; i < box_inner_w; i++) strcat(bot, "─");
    strcat(bot, "╯\033[0m");
    snprintf(info_lines[info_count++], MAX_VAL_LEN, "%s", bot);

    /* Render logo side-by-side with boxed frame */
    int total_rows = logo_count > info_count ? logo_count : info_count;
    for (int r = 0; r < total_rows; r++) {
        if (g_config.show_logo) {
            const char *l_line = r < logo_count ? logo_lines[r] : "";
            size_t vis_w = ansi_visible_length(l_line);
            printf("%s" COLOR_RESET, l_line);
            for (size_t pad = vis_w; pad < max_logo_width + LOGO_PADDING; pad++) putchar(' ');
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
    char info_lines[MAX_MODULES + 6][MAX_VAL_LEN];
    memset(info_lines, 0, sizeof(info_lines));
    int info_count = 0;

    snprintf(info_lines[info_count++], MAX_VAL_LEN, "%s", user_host);

    for (int i = 0; i < result_count; i++) {
        const char *connector = (i == result_count - 1) ? "\033[1;36m╰─\033[0m " : "\033[1;36m├─\033[0m ";
        snprintf(info_lines[info_count++], MAX_VAL_LEN,
            "%s" COLOR_KEY "%s" COLOR_RESET ": " COLOR_VALUE "%s" COLOR_RESET,
            connector, results[i].key, results[i].val);
    }

    if (g_config.color_blocks) {
        char color_bar[MAX_VAL_LEN] = "";
        module_detect_color(color_bar, sizeof(color_bar));
        snprintf(info_lines[info_count++], MAX_VAL_LEN, "   %s", color_bar);
    }

    int total_rows = logo_count > info_count ? logo_count : info_count;
    for (int r = 0; r < total_rows; r++) {
        if (g_config.show_logo) {
            const char *l_line = r < logo_count ? logo_lines[r] : "";
            size_t vis_w = ansi_visible_length(l_line);
            printf("%s" COLOR_RESET, l_line);
            for (size_t pad = vis_w; pad < max_logo_width + LOGO_PADDING; pad++) putchar(' ');
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
