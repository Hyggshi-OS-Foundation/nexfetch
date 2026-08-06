#include "nexfetch.h"
#include "module.h"
#include "platform.h"
#include "util.h"
#include "presenter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <lmcons.h>
#include <io.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#endif

extern void config_init(void);
extern int  logo_load(const char *distro_id, char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN]);
extern void logo_gif_animate(const char *path, int logo_width, int logo_height, int duration_secs, int fps);
extern size_t ansi_visible_length(const char *str);

/* Module detectors */
extern void module_detect_os(char *out, size_t max_len);
extern void module_detect_kernel(char *out, size_t max_len);
extern void module_detect_host(char *out, size_t max_len);
extern void module_detect_uptime(char *out, size_t max_len);
extern void module_detect_packages(char *out, size_t max_len);
extern void module_detect_shell(char *out, size_t max_len);
extern void module_detect_de(char *out, size_t max_len);
extern void module_detect_wm(char *out, size_t max_len);
extern void module_detect_terminal(char *out, size_t max_len);
extern void module_detect_cpu(char *out, size_t max_len);
extern void module_detect_gpu(char *out, size_t max_len);
extern void module_detect_memory(char *out, size_t max_len);
extern void module_detect_disk(char *out, size_t max_len);
extern void module_detect_battery(char *out, size_t max_len);
extern void module_detect_network(char *out, size_t max_len);
extern void module_detect_theme(char *out, size_t max_len);
extern void module_detect_icons(char *out, size_t max_len);
extern void module_detect_font(char *out, size_t max_len);

/* New module detectors */
extern void module_detect_locale(char *out, size_t max_len);
extern void module_detect_swap(char *out, size_t max_len);
extern void module_detect_display(char *out, size_t max_len);

#ifndef _WIN32
typedef struct {
    void (*detect)(char *, size_t);
    char *buf;
    size_t max_len;

    /* Completion signal, used instead of pthread_timedjoin_np() (a glibc
       extension not available on macOS/BSD pthreads) so the timeout logic
       below works on every POSIX platform. mutex/cond point at the shared
       pair set up by the caller for this whole batch of module threads. */
    pthread_mutex_t *mutex;
    pthread_cond_t  *cond;
    volatile int     done;
} ModuleWorkerTask;

static void *module_worker_runner(void *arg) {
    ModuleWorkerTask *t = (ModuleWorkerTask *)arg;
    if (t && t->detect) {
        t->detect(t->buf, t->max_len);
    }
    if (t) {
        pthread_mutex_lock(t->mutex);
        t->done = 1;
        pthread_cond_broadcast(t->cond);
        pthread_mutex_unlock(t->mutex);
    }
    return NULL;
}

typedef struct {
    const char *distro_id;
    char (*lines)[MAX_LOGO_LINE_LEN];
    int count;
    size_t max_width;
} LogoLoadTask;

static void *logo_load_runner(void *arg) {
    LogoLoadTask *t = (LogoLoadTask *)arg;
    if (!t || !t->lines) return NULL;
    t->count = logo_load(t->distro_id, t->lines);
    t->max_width = 0;
    for (int i = 0; i < t->count; i++) {
        size_t w = ansi_visible_length(t->lines[i]);
        if (w > t->max_width) t->max_width = w;
    }
    return NULL;
}

static int module_is_slow(const char *key) {
    return key && (
        strcmp(key, "packages") == 0 ||
        strcmp(key, "theme")    == 0 ||
        strcmp(key, "icons")    == 0 ||
        strcmp(key, "font")     == 0 ||
        strcmp(key, "display")  == 0
    );
}

/* Bound total run time so one misbehaving module -- a plugin waiting on a
 * daemon socket, a subprocess fallback stuck on a slow D-Bus/dconf
 * round-trip, an unreachable network share, etc. -- can never stall the
 * whole program. Modules already run concurrently (one thread each), so
 * this timeout is the ceiling on the ENTIRE run, not per-module-additive.
 *
 * On timeout we abandon the thread (detach it) rather than waiting
 * further, and drop whatever partial data it wrote so it's simply skipped
 * from the output, like any other module that came back empty. The
 * detached thread keeps running invisibly in the background, but since it
 * is never joined again, main() is free to finish and exit -- the process
 * exit takes it down with it. */
#define NEXFETCH_MODULE_TIMEOUT_MS 80

static void compute_deadline(struct timespec *ts, int timeout_ms) {
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec  += timeout_ms / 1000;
    ts->tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

/* IMPORTANT: `deadline` must be ONE absolute point in time shared across
 * every call in the join loop, not a fresh "timeout_ms from now" per
 * thread. Modules are joined one at a time in a plain for-loop, so a
 * per-thread relative timeout would let each slow module in turn eat up
 * to timeout_ms more, stacking additively (N slow modules -> N *
 * timeout_ms total) instead of bounding the whole run. Sharing one
 * deadline means the *combined* time spent waiting across all modules is
 * capped at timeout_ms, however many of them are slow. */
static int join_with_deadline(ModuleWorkerTask *t, pthread_t thread,
                               const struct timespec *deadline) {
    int timed_out = 0;

    pthread_mutex_lock(t->mutex);
    while (!t->done) {
        int wr = pthread_cond_timedwait(t->cond, t->mutex, deadline);
        if (wr == ETIMEDOUT) {
            timed_out = !t->done; /* re-check: may have finished right as we woke */
            break;
        }
        /* Spurious wakeup with the flag still unset: loop again. The
           deadline is absolute, so this can't run past it. */
    }
    pthread_mutex_unlock(t->mutex);

    if (timed_out) {
        pthread_detach(thread);
        return -1;
    }
    /* Thread has already finished (or is finishing this instant), so this
       join is effectively non-blocking. */
    pthread_join(thread, NULL);
    return 0;
}
#endif

static void get_user_host_str(char *out, size_t size,
                               char *user_raw, size_t user_sz,
                               char *host_raw, size_t host_sz) {
#ifdef _WIN32
    DWORD u_len = (DWORD)user_sz;
    if (!GetUserNameA(user_raw, &u_len)) {
        const char *u_env = getenv("USERNAME");
        snprintf(user_raw, user_sz, "%s", u_env ? u_env : "user");
    }
    DWORD h_len = (DWORD)host_sz;
    if (!GetComputerNameA(host_raw, &h_len)) {
        const char *h_env = getenv("COMPUTERNAME");
        snprintf(host_raw, host_sz, "%s", h_env ? h_env : "localhost");
    }
#else
    struct passwd *pw = getpwuid(geteuid());
    if (pw) snprintf(user_raw, user_sz, "%s", pw->pw_name);
    else    snprintf(user_raw, user_sz, "user");
    if (gethostname(host_raw, (int)host_sz) != 0)
        snprintf(host_raw, host_sz, "localhost");
#endif

    snprintf(out, size,
        COLOR_USER "%s" COLOR_RESET "@" COLOR_USER "%s" COLOR_RESET,
        user_raw, host_raw);
}

/*
 * clear_full_screen() clears the visible terminal viewport and homes the
 * cursor before nexfetch draws its background/box. Without this, re-running
 * nexfetch in the same terminal -- or shrinking the box by disabling a
 * module/the logo between runs -- left stray fragments of the previous,
 * larger frame (leftover border characters) visible around the new, smaller
 * one. Only emitted when stdout is an actual terminal, so piping/redirecting
 * output stays clean.
 *
 * NOTE: We intentionally do NOT use \033[3J (clear scrollback buffer) here.
 * Clearing the scrollback wipes the user's command prompt and all previous
 * terminal history, making it look like the cursor "disappeared" and causing
 * the top of the fetch output to be permanently lost when the output is
 * taller than the terminal viewport (there's no scrollback to scroll up to).
 * \033[2J (clear visible screen) + \033[H (home cursor) is sufficient to
 * remove stray fragments from a previous run while preserving scrollback.
 */
static void clear_full_screen(void) {
#ifdef _WIN32
    if (!_isatty(_fileno(stdout))) return;
#else
    if (!isatty(STDOUT_FILENO)) return;
#endif
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
}

static int get_terminal_width(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        int width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        if (width > 0) return width;
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (int)ws.ws_col;
#endif
    const char *cols_env = getenv("COLUMNS");
    if (cols_env && cols_env[0] != '\0') {
        int c = atoi(cols_env);
        if (c > 0) return c;
    }
    return 80;
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
    config_init();

    /* CLI flags */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("nexfetch %s\n", NEXFETCH_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--no-logo") == 0) {
            g_config.show_logo = 0;
        }
        if (strcmp(argv[i], "--theme") == 0 && i + 1 < argc) {
            snprintf(g_config.theme, sizeof(g_config.theme), "%s", argv[++i]);
        }
        if (strcmp(argv[i], "--logo") == 0 && i + 1 < argc) {
            const char *logo_arg = argv[++i];
            snprintf(g_config.custom_logo_path, sizeof(g_config.custom_logo_path), "%s", logo_arg);
            /* Detect logo type from extension */
            const char *dot = strrchr(logo_arg, '.');
            if (dot) {
                const char *ext = dot + 1;
                /* Video extensions */
                const char *vexts[] = { "mp4", "mkv", "avi", "webm", "mov", NULL };
                int is_vid = 0;
                for (int j = 0; vexts[j]; j++) {
#ifdef _WIN32
                    if (_stricmp(ext, vexts[j]) == 0) { is_vid = 1; break; }
#else
                    if (strcasecmp(ext, vexts[j]) == 0) { is_vid = 1; break; }
#endif
                }
                /* Image extensions */
                const char *iexts[] = { "png", "jpg", "jpeg", "gif", "bmp", "webp", "svg", NULL };
                int is_img = 0;
                for (int j = 0; iexts[j]; j++) {
#ifdef _WIN32
                    if (_stricmp(ext, iexts[j]) == 0) { is_img = 1; break; }
#else
                    if (strcasecmp(ext, iexts[j]) == 0) { is_img = 1; break; }
#endif
                }
                g_config.logo_is_video = is_vid;
                g_config.logo_is_image = is_vid ? 0 : is_img;
            }
        }
        if (strcmp(argv[i], "--bg") == 0 && i + 1 < argc) {
            snprintf(g_config.background_image_path,
                     sizeof(g_config.background_image_path), "%s", argv[++i]);
        }
        if (strcmp(argv[i], "--no-bg") == 0) {
            /* Explicitly clear any background image path set by config.json
               or an earlier --bg, since previously there was no CLI flag
               capable of turning a configured background back off. */
            g_config.background_image_path[0] = '\0';
        }
        if (strcmp(argv[i], "--animate") == 0) {
            g_config.logo_animate = 1;
        }
        if (strcmp(argv[i], "--animate-duration") == 0 && i + 1 < argc) {
            int d = atoi(argv[++i]);
            if (d >= 0) g_config.logo_animate_duration = d;
        }
        if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            int fps = atoi(argv[++i]);
            if (fps > 0) g_config.logo_fps = fps;
        }
        if (strcmp(argv[i], "--fast") == 0) {
            g_config.fast_mode = 1;
            g_config.logo_animate = 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("nexfetch - Modern Modular System Fetch CLI\n");
            printf("Usage: nexfetch [options]\n\n");
            printf("Options:\n");
            printf("  -h, --help                     Show help options\n");
            printf("  -v, --version                  Show version information\n");
            printf("  --no-logo                      Disable ASCII logo display\n");
            printf("  --logo <path>                  Use custom ASCII, PNG/JPG/GIF or MP4 logo\n");
            printf("  --animate                      Animate GIF logo after initial render\n");
            printf("  --animate-duration <secs>      Seconds to animate (0 = until Ctrl+C)\n");
            printf("  --fps <n>                      Animation FPS (default 60)\n");
            printf("  --fast                         Skip animation, minimize modules\n");
            printf("  --bg <path>                    Render image as full-terminal background\n");
            printf("  --no-bg                        Disable background image\n");
            printf("  --theme <name>                 Set theme (boxed, classic, modern)\n");
            printf("  --list-modules                 List all registered modules\n");
            return 0;
        }
    }

    module_manager_init();
    presenter_manager_init();
    presenter_set_active(g_config.theme);

    /* Register built-in modules in display order */
    module_manager_register("OS",       "os",       module_detect_os);
    module_manager_register("Kernel",   "kernel",   module_detect_kernel);
    module_manager_register("Host",     "host",     module_detect_host);
    module_manager_register("Uptime",   "uptime",   module_detect_uptime);
    module_manager_register("Packages", "packages", module_detect_packages);
    module_manager_register("Display",  "display",  module_detect_display);
    module_manager_register("Shell",    "shell",    module_detect_shell);
    module_manager_register("DE",       "de",       module_detect_de);
    module_manager_register("WM",       "wm",       module_detect_wm);
    module_manager_register("Terminal", "terminal", module_detect_terminal);
    module_manager_register("CPU",      "cpu",      module_detect_cpu);
    module_manager_register("GPU",      "gpu",      module_detect_gpu);
    module_manager_register("Memory",   "memory",   module_detect_memory);
    module_manager_register("Disk",     "disk",     module_detect_disk);
    module_manager_register("Swap",     "swap",     module_detect_swap);
    module_manager_register("Battery",  "battery",  module_detect_battery);
    module_manager_register("Network",  "network",  module_detect_network);
    module_manager_register("Theme",    "theme",    module_detect_theme);
    module_manager_register("Icons",    "icons",    module_detect_icons);
    module_manager_register("Font",     "font",     module_detect_font);
    module_manager_register("Locale",   "locale",   module_detect_locale);

    /* Load dynamic plugins listed in config.json "plugins" array */
    if (!g_config.fast_mode) {
        for (int i = 0; i < g_config.plugin_count; i++) {
            module_manager_load_plugin(g_config.plugin_paths[i]);
        }

        /* Auto-load plugins from user and system module directories */
        module_manager_load_from_dirs();
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--list-modules") == 0) {
            printf("Registered Modules (%d total):\n", module_manager_get_count());
            for (int j = 0; j < module_manager_get_count(); j++) {
                Module *m = module_manager_get(j);
                printf("  [%d] %-10s (key: %s, type: %s)\n",
                    j + 1, m->name, m->key,
                    m->type == MODULE_TYPE_BUILTIN ? "builtin" : "plugin");
            }
            return 0;
        }
    }

    /* Detect OS first (populates g_config.distro_id for logo) */
    char os_val[MAX_VAL_LEN] = "";
    module_detect_os(os_val, sizeof(os_val));

    /* Load logo concurrently with module detection (POSIX only). */
    char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN];
    memset(logo_lines, 0, sizeof(logo_lines));
    int logo_count = 0;
    size_t max_logo_width = 0;

#ifndef _WIN32
    LogoLoadTask logo_task = {
        .distro_id = g_config.distro_id,
        .lines = logo_lines,
        .count = 0,
        .max_width = 0
    };
    pthread_t logo_thread;
    int logo_thread_active = 0;
    if (g_config.show_logo) {
        if (pthread_create(&logo_thread, NULL, logo_load_runner, &logo_task) == 0) {
            logo_thread_active = 1;
        }
    }
#else
    if (g_config.show_logo) {
        logo_count = logo_load(g_config.distro_id, logo_lines);
        for (int i = 0; i < logo_count; i++) {
            size_t w = ansi_visible_length(logo_lines[i]);
            if (w > max_logo_width) max_logo_width = w;
        }
    }
#endif

    /* Collect raw system information results (independent of UI presentation) */
    ModuleResult results[MAX_MODULES];
    char val_buffers[MAX_MODULES][MAX_VAL_LEN];
    int result_count = 0;

    int active_indices[MAX_MODULES];
    int active_count = 0;

    for (int i = 0; i < module_manager_get_count(); i++) {
        Module *m = module_manager_get(i);
        val_buffers[i][0] = '\0';

        /* If the config specifies a module list, skip anything not in it */
        if (g_config.enabled_module_count > 0) {
            int allowed = 0;
            for (int j = 0; j < g_config.enabled_module_count; j++) {
#ifdef _WIN32
                if (_stricmp(m->key, g_config.enabled_modules[j]) == 0) {
#else
                if (strcasecmp(m->key, g_config.enabled_modules[j]) == 0) {
#endif
                    allowed = 1;
                    break;
                }
            }
            if (!allowed) continue;
        }

        if (g_config.fast_mode && module_is_slow(m->key)) continue;

        active_indices[active_count++] = i;
    }

#ifndef _WIN32
    pthread_t threads[MAX_MODULES];
    ModuleWorkerTask tasks[MAX_MODULES];
    int thread_started[MAX_MODULES];
    memset(thread_started, 0, sizeof(thread_started));

    /* Shared by every task in this batch: each worker thread signals its
       own completion on this pair, and the join loop below waits on it
       bounded by a single absolute deadline (see join_with_deadline()). */
    pthread_mutex_t join_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  join_cond  = PTHREAD_COND_INITIALIZER;

    for (int k = 0; k < active_count; k++) {
        int idx = active_indices[k];
        Module *m = module_manager_get(idx);
        tasks[k].detect = m ? m->detect : NULL;
        tasks[k].buf = val_buffers[idx];
        tasks[k].max_len = MAX_VAL_LEN;
        tasks[k].mutex = &join_mutex;
        tasks[k].cond = &join_cond;
        tasks[k].done = 0;

        if (m && strcmp(m->key, "os") == 0) {
            snprintf(val_buffers[idx], MAX_VAL_LEN, "%s", os_val);
            continue;
        }

        if (tasks[k].detect &&
            pthread_create(&threads[k], NULL, module_worker_runner, &tasks[k]) == 0) {
            thread_started[k] = 1;
        }
    }

    struct timespec join_deadline;
    compute_deadline(&join_deadline, NEXFETCH_MODULE_TIMEOUT_MS);

    for (int k = 0; k < active_count; k++) {
        if (!thread_started[k]) continue;
        Module *m = module_manager_get(active_indices[k]);
        if (m && m->detect) {
            if (join_with_deadline(&tasks[k], threads[k], &join_deadline) != 0) {
                /* Timed out: discard whatever it may have partially written
                   so it's treated the same as any other empty result. */
                val_buffers[active_indices[k]][0] = '\0';
            }
        }
    }

    if (logo_thread_active) {
        pthread_join(logo_thread, NULL);
        logo_count = logo_task.count;
        max_logo_width = logo_task.max_width;
    }
#else
    for (int k = 0; k < active_count; k++) {
        int idx = active_indices[k];
        Module *m = module_manager_get(idx);
        if (m && strcmp(m->key, "os") == 0) {
            snprintf(val_buffers[idx], MAX_VAL_LEN, "%s", os_val);
            continue;
        }
        if (m && m->detect) {
            m->detect(val_buffers[idx], MAX_VAL_LEN);
        }
    }
#endif

    if (g_config.show_logo) {
        int term_width = get_terminal_width();
        if ((int)(max_logo_width + LOGO_PADDING + 40) > term_width) {
            g_config.show_logo = 0;
            logo_count = 0;
            max_logo_width = 0;
        }
    }

    for (int k = 0; k < active_count; k++) {
        int idx = active_indices[k];
        Module *m = module_manager_get(idx);
        if (val_buffers[idx][0] == '\0' ||
            strcmp(val_buffers[idx], "Unknown") == 0 ||
            strcmp(val_buffers[idx], "N/A") == 0) {
            continue;
        }

        results[result_count].key = m->name;
        results[result_count].val = val_buffers[idx];
        result_count++;
    }

    /* Build header strings */
    char user_raw[64] = "user", host_raw[64] = "localhost";
    char user_host[MAX_VAL_LEN];
    get_user_host_str(user_host, sizeof(user_host), user_raw, sizeof(user_raw), host_raw, sizeof(host_raw));

    size_t sep_len = strlen(user_raw) + 1 + strlen(host_raw);
    char sep[256] = "";
    for (size_t i = 0; i < sep_len && i < sizeof(sep) - 1; i++) sep[i] = '-';
    sep[sep_len] = '\0';

    /* Wipe the whole viewport first so nothing from a previous, larger
       frame (or a prior run) can peek out from behind the new one. */
    clear_full_screen();

    /* Hide cursor and clear scrollback to eliminate shell prompt artifacts
     * when rendering background images. The prompt line would otherwise
     * remain visible at the bottom, mixed into the chafa output. */
#ifdef _WIN32
    if (_isatty(_fileno(stdout)))
#else
    if (isatty(STDOUT_FILENO))
#endif
    {
        fputs("\033[?25l", stdout);  /* hide cursor */
        /* Note: we intentionally do NOT use \033[3J (clear scrollback) here
         * as it would wipe terminal history. The clear_full_screen() above
         * already cleared the visible viewport which is sufficient. */
        fflush(stdout);
    }

    /* Delegate presentation rendering to the active Presenter Plugin/Theme */
    /* Render background image first if configured (renders behind fetch output) */
    if (g_config.background_image_path[0] != '\0' && !g_config.fast_mode) {
        /*
         * render_background() needs to know how many rows the frame we're
         * about to draw actually needs, so it can capture at least that
         * many background rows instead of clamping to the current
         * terminal height (see the comment in render_background() for why
         * that clamp was leaving later rows with no background at all).
         *
         * Each presenter wraps `result_count` data rows in a few extra
         * decorative rows of its own -- boxed adds the most (top border,
         * header, divider, optional color-bar row, bottom border = +5),
         * classic and modern add fewer. Since the active theme is just a
         * config string here, add the boxed worst case (+5) unconditionally
         * rather than special-casing each theme name; a few possibly-unused
         * extra background rows cost nothing but a couple more chafa output
         * lines, while under-counting is what causes the uncolored-row bug.
         */
        int content_rows = result_count + 5;
        if (logo_count > content_rows) content_rows = logo_count;
        render_background(g_config.background_image_path, content_rows);
    }

    presenter_render(logo_lines, logo_count, max_logo_width,
                     results, result_count,
                     user_host, sep);

#ifndef _WIN32
    /* --- Animated GIF logo ---------------------------------------------------
     * If the user requested animation (--animate or config logo_animate:true)
     * and the logo is a GIF, play the animation in-place in the logo area.
     * The info text panel to the right is untouched during playback.
     * logo_gif_animate() blocks until duration_secs elapses or the user
     * presses Ctrl+C; it also handles cursor hide/restore automatically. */
    if (g_config.logo_animate && g_config.logo_is_image && logo_count > 0
            && g_config.custom_logo_path[0] != '\0') {
        /* Quick extension check for .gif without adding a new header */
        const char *_lp  = g_config.custom_logo_path;
        const char *_dot = strrchr(_lp, '.');
        int _is_gif = 0;
        if (_dot) {
            const char *_e = _dot + 1;
            char _lo[5] = {0};
            for (int _i = 0; _i < 4 && _e[_i]; _i++)
                _lo[_i] = (char)((_e[_i] >= 'A' && _e[_i] <= 'Z') ? _e[_i] + 32 : _e[_i]);
            _is_gif = (strcmp(_lo, "gif") == 0);
        }
        if (_is_gif) {
            int _lw = g_config.logo_width > 0 ? g_config.logo_width : 32;
            int _fps = g_config.logo_fps > 0 ? g_config.logo_fps : 60;
            logo_gif_animate(_lp, _lw, logo_count, g_config.logo_animate_duration, _fps);
        }
    }
#endif

    /* Ensure all output is flushed and the cursor is visible before exiting.
       The chafa background renderer (or a plugin) may have emitted cursor-hide
       sequences (\033[?25l) that were never restored, leaving the terminal
       without a visible cursor. \033[?25h forces it back on. Only done when
       stdout is a real terminal so piped/redirected output stays clean. */
    fflush(stdout);
#ifdef _WIN32
    if (_isatty(_fileno(stdout)))
#else
    if (isatty(STDOUT_FILENO))
#endif
    {
        fputs("\033[?25h", stdout);
        fflush(stdout);
    }

    module_manager_cleanup();
    return 0;
}
