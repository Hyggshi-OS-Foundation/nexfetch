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
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#include <pwd.h>
#include <sys/ioctl.h>
#endif

extern void config_init(void);
extern int logo_load(const char *distro_id, char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN]);
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
            snprintf(g_config.custom_logo_path, sizeof(g_config.custom_logo_path), "%s", argv[++i]);
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("nexfetch - Modern Modular System Fetch CLI\n");
            printf("Usage: nexfetch [options]\n\n");
            printf("Options:\n");
            printf("  -h, --help           Show help options\n");
            printf("  -v, --version        Show version information\n");
            printf("  --no-logo            Disable ASCII logo display\n");
            printf("  --logo <path>        Use custom ASCII or PNG logo file\n");
            printf("  --theme <name>       Set presentation theme (boxed, classic, modern)\n");
            printf("  --list-modules       List all registered modules\n");
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

    /* Load logo */
    char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN];
    memset(logo_lines, 0, sizeof(logo_lines));
    int logo_count = 0;
    size_t max_logo_width = 0;

    if (g_config.show_logo) {
        logo_count = logo_load(g_config.distro_id, logo_lines);
        for (int i = 0; i < logo_count; i++) {
            size_t w = ansi_visible_length(logo_lines[i]);
            if (w > max_logo_width) max_logo_width = w;
        }

        /* Auto-fallback if logo + info is wider than terminal */
        int term_width = get_terminal_width();
        if ((int)(max_logo_width + LOGO_PADDING + 40) > term_width) {
            g_config.show_logo = 0;
            logo_count = 0;
            max_logo_width = 0;
        }
    }

    /* Collect raw system information results (independent of UI presentation) */
    ModuleResult results[MAX_MODULES];
    char val_buffers[MAX_MODULES][MAX_VAL_LEN];
    int result_count = 0;

    for (int i = 0; i < module_manager_get_count(); i++) {
        Module *m = module_manager_get(i);
        val_buffers[result_count][0] = '\0';

        if (m->detect) {
            m->detect(val_buffers[result_count], MAX_VAL_LEN);
        }

        if (val_buffers[result_count][0] == '\0' ||
            strcmp(val_buffers[result_count], "Unknown") == 0 ||
            strcmp(val_buffers[result_count], "N/A") == 0) {
            continue;
        }

        results[result_count].key = m->name;
        results[result_count].val = val_buffers[result_count];
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

    /* Delegate presentation rendering to the active Presenter Plugin/Theme */
    presenter_render(logo_lines, logo_count, max_logo_width,
                     results, result_count,
                     user_host, sep);

    module_manager_cleanup();
    return 0;
}
