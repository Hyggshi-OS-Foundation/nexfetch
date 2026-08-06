#ifndef NEXFETCH_H
#define NEXFETCH_H

#include "module.h"
#include "platform.h"
#include "util.h"

#define NEXFETCH_VERSION "1.1.0"
#define MAX_MODULES 48
#define MAX_PLUGINS 16
#define MAX_VAL_LEN 512
#define MAX_LOGO_LINES 32
#define MAX_LOGO_LINE_LEN 2048   /* must fit full chafa-rendered lines (up to ~700 raw bytes) */
#define LOGO_PADDING 1          /* gap columns between logo and info box */

typedef struct NexfetchConfig {
    int show_logo;
    char custom_logo_path[512];        /* path from config.json "logo" field or --logo flag */
    int  logo_is_image;                /* 1 = PNG/JPG/GIF → convert via chafa; 0 = .txt */
    int  logo_is_video;                /* 1 = MP4 → extract first frame via ffmpeg, then chafa */
    int  logo_width;                   /* columns for chafa conversion (0 = auto 32) */
    int  logo_animate;                 /* 1 = animate GIF logos in-place after initial render */
    int  logo_animate_duration;        /* seconds to animate; 0 = loop until Ctrl+C */
    int  logo_fps;                       /* animation FPS: 30, 60, etc. (0 = default 60) */
    int  logo_border;                      /* 1 = show border, 0 = no border (default) */
    char distro_id[64];
    int color_blocks;
    char theme[64];                    /* active presentation theme: "boxed", "classic", "modern" */
    char background_image_path[512];   /* path to background image rendered as full-terminal ANSI art */

    /* Module filter: keys listed in config.json "modules" array.
     * When enabled_module_count == 0 the filter is inactive and all modules run. */
    char enabled_modules[MAX_MODULES][32];
    int  enabled_module_count;

    /* Plugin paths listed in config.json "plugins" array.
     * Loaded at startup via module_manager_load_plugin(). */
    char plugin_paths[MAX_PLUGINS][512];
    int  plugin_count;

    /* Set by --fast: skip slow modules/plugins and defer expensive work. */
    int  fast_mode;
} NexfetchConfig;

extern NexfetchConfig g_config;

/* ANSI color macros */
#define COLOR_RESET    "\033[39m"
#define COLOR_BOLD     "\033[3m"
#define COLOR_KEY      "\033[34m"   /* Bold Blue   – key labels  */
#define COLOR_VALUE    "\033[37m"     /* Light White – values (no \033[0m: overlay_text keeps bg active) */
#define COLOR_USER     "\033[32m"   /* Bold Green  – user@host   */
#define COLOR_SEP      "\033[30m"   /* Dark Gray   – separator   */
#define COLOR_TITLE    "\033[37m"   /* Bold White  – section     */

#endif /* NEXFETCH_H */
