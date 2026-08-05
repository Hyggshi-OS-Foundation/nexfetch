#include "nexfetch.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>   /* getpid(), access(), usleep() */
#include <time.h>     /* time() */
#include <signal.h>   /* sigaction() */
#else
#include <process.h>  /* _getpid() */
#define getpid _getpid
#endif

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

/* -------------------------------------------------------------------------
 * logo_gif_animate() -- in-place animated GIF playback in the logo area
 *
 * Called AFTER the initial static nexfetch render. Uses ffmpeg to extract
 * every frame of the GIF into a temp directory, then loops through them via
 * chafa, repositioning the cursor to the logo area for each frame so the
 * info text panel to the right stays visible and untouched.
 *
 * Parameters:
 *   path          - path to the .gif file (same as custom_logo_path)
 *   logo_width    - target column width for chafa (must match load_image's width)
 *   logo_height   - number of terminal rows the logo occupies (= logo_count)
 *   duration_secs - seconds to play; 0 = loop indefinitely until Ctrl+C
 * -------------------------------------------------------------------------*/
#ifndef _WIN32
static volatile sig_atomic_t s_anim_stop = 0;

static void anim_sigint_handler(int sig) {
    (void)sig;
    s_anim_stop = 1;
}

/* Run a shell cleanup command and discard the return value deliberately.
 * Using "if (system()) {}" satisfies -Wunused-result (the return value is
 * read by the condition) while making it obvious we don't act on it. */
static void run_shell(const char *cmd) { if (system(cmd)) { /* ignored */ } }

void logo_gif_animate(const char *path, int logo_width, int logo_height,
                      int duration_secs) {
    if (!path || !path[0] || logo_width <= 0 || logo_height <= 0) return;

    /* Unique temp directory so concurrent nexfetch invocations don't collide */
    char tmp_dir[256];
    snprintf(tmp_dir, sizeof(tmp_dir), "/tmp/nexfetch_anim_%d", (int)getpid());

    char cmd[1280];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s' 2>/dev/null", tmp_dir);
    if (system(cmd) != 0) return;

    /* Extract every frame of the GIF as a separate PNG using ffmpeg */
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -loglevel quiet -i '%s' -vsync 0 '%s/f%%04d.png' 2>/dev/null",
        path, tmp_dir);
    if (system(cmd) != 0) {
        snprintf(cmd, sizeof(cmd), "rm -rf '%s' 2>/dev/null", tmp_dir);
        run_shell(cmd);
        return;
    }

    /* Count how many frames were extracted */
    int frame_count = 0;
    char frame_path[512];
    for (;;) {
        snprintf(frame_path, sizeof(frame_path), "%s/f%04d.png", tmp_dir, frame_count + 1);
        if (access(frame_path, F_OK) != 0) break;
        if (++frame_count >= 999) break;
    }
    if (frame_count == 0) {
        snprintf(cmd, sizeof(cmd), "rm -rf '%s' 2>/dev/null", tmp_dir);
        run_shell(cmd);
        return;
    }

    /* Install a SIGINT handler so Ctrl+C restores the cursor cleanly */
    struct sigaction sa_old, sa_new;
    sa_new.sa_handler = anim_sigint_handler;
    sigemptyset(&sa_new.sa_mask);
    sa_new.sa_flags = 0;
    sigaction(SIGINT, &sa_new, &sa_old);
    s_anim_stop = 0;

    /* Hide cursor for flicker-free animation */
    fputs("\033[?25l", stdout);
    fflush(stdout);

    time_t start_t = time(NULL);
    int frame_idx  = 1;

    while (!s_anim_stop) {
        if (duration_secs > 0 && (int)(time(NULL) - start_t) >= duration_secs) break;

        snprintf(frame_path, sizeof(frame_path), "%s/f%04d.png", tmp_dir, frame_idx);

        /* Render this frame via chafa (symbols, no animation) */
        char chafa_cmd[1024];
        snprintf(chafa_cmd, sizeof(chafa_cmd),
            "chafa --animate=off --size %dx%d --format symbols '%s' 2>/dev/null",
            logo_width, logo_height, frame_path);

        FILE *fp = popen(chafa_cmd, "r");
        if (fp) {
            char line[MAX_LOGO_LINE_LEN * 4];
            int row = 1;
            while (fgets(line, sizeof(line), fp) && row <= logo_height) {
                size_t len = strlen(line);
                if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
                strip_private_modes(line);
                /* Move cursor to absolute (row, col 1) and paint the logo line */
                printf("\033[%d;1H%s\033[0m", row, line);
                row++;
            }
            pclose(fp);
        }
        fflush(stdout);

        usleep(83000); /* ~83 ms ≈ 12 fps */

        /* Advance frame, wrapping at the end */
        frame_idx = (frame_idx % frame_count) + 1;
    }

    /* Restore cursor visibility and move below the animated area */
    fputs("\033[?25h", stdout);
    printf("\033[%d;1H\n", logo_height + 1);
    fflush(stdout);

    /* Restore original SIGINT handler */
    sigaction(SIGINT, &sa_old, NULL);

    /* Remove the temp frames */
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' 2>/dev/null", tmp_dir);
    run_shell(cmd);
}
#else
/* Stub: animated GIF logos not yet supported on Windows */
void logo_gif_animate(const char *path, int logo_width, int logo_height,
                      int duration_secs) {
    (void)path; (void)logo_width; (void)logo_height; (void)duration_secs;
}
#endif

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
 *
 * --animate=off ensures animated GIFs are rendered as a single still frame
 * (the first frame) instead of attempting terminal animation, which would
 * conflict with nexfetch's static overlay layout and leave the terminal in
 * an inconsistent state after exit.
 */
static int load_image(const char *path, int logo_width,
                      char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN]) {
    int width = (logo_width > 0) ? logo_width : 32;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "chafa --animate=off --size %dx%d --format symbols '%s' 2>/dev/null",
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
 * Load logo from a video file (MP4/MKV/…) by extracting the first frame
 * with ffmpeg, then rendering it through chafa like a regular image.
 * The temp file is cleaned up after conversion.
 */
static int load_video(const char *path, int logo_width,
                      char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN]) {
    /* Build a unique temp path to avoid clashes when running concurrently */
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/nexfetch_logo_%d.png", (int)getpid());

    /* Extract the very first video frame as PNG via ffmpeg */
    char cmd[1280];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -loglevel quiet -i '%s' -vframes 1 '%s' 2>/dev/null",
        path, tmp_path);

    int rc = system(cmd);
    if (rc != 0) return 0;   /* ffmpeg not found or failed */

    int n = load_image(tmp_path, logo_width, logo_lines);

    /* Clean up temp file regardless of result */
    remove(tmp_path);
    return n;
}

/*
 * Public entry point.
 * Priority:
 *   1. config.json "logo" path (image or txt)
 *   2. --logo CLI flag  (already stored in g_config.custom_logo_path)
 *   3. logos/<distro_id>.txt
 *   4. Fallback: logos/tux.txt for unmatched Linux distros,
 *      logos/nexfetch.txt for unmatched non-Linux platforms (macOS, Windows)
 */
int logo_load(const char *distro_id,
              char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN]) {

    /* --- Custom logo from config or CLI flag -------------------------------- */
    if (g_config.custom_logo_path[0] != '\0') {
        const char *path = g_config.custom_logo_path;

        if (g_config.logo_is_video) {
            int n = load_video(path, g_config.logo_width, logo_lines);
            if (n <= 0 && path[0] != '/') {
                char sys_path[1024];
                snprintf(sys_path, sizeof(sys_path), "/usr/share/nexfetch/%s", path);
                n = load_video(sys_path, g_config.logo_width, logo_lines);
            }
            if (n > 0) return n;
            /* fall through to distro default if ffmpeg/chafa fails */
        } else if (g_config.logo_is_image) {
            int n = load_image(path, g_config.logo_width, logo_lines);
            if (n <= 0 && path[0] != '/') {
                char sys_path[1024];
                snprintf(sys_path, sizeof(sys_path), "/usr/share/nexfetch/%s", path);
                n = load_image(sys_path, g_config.logo_width, logo_lines);
            }
            if (n > 0) return n;
            /* fall through to distro default if chafa fails */
        } else {
            int n = load_txt(path, logo_lines);
            if (n <= 0 && path[0] != '/') {
                char sys_path[1024];
                snprintf(sys_path, sizeof(sys_path), "/usr/share/nexfetch/%s", path);
                n = load_txt(sys_path, logo_lines);
            }
            if (n > 0) return n;
        }
    }

    /* --- Distro logo from logos/ directory --------------------------------- */
    char path[512];
    const char *distro = distro_id && distro_id[0] ? distro_id : "tux";

    /* Try local logos/ first, then system-wide /usr/share/nexfetch/logos/ */
    snprintf(path, sizeof(path), "logos/%s.txt", distro);
    int n = load_txt(path, logo_lines);
    if (n <= 0) {
        snprintf(path, sizeof(path), "/usr/share/nexfetch/logos/%s.txt", distro);
        n = load_txt(path, logo_lines);
    }
    if (n > 0) return n;

    /* Fallback:
     *   - Linux distros with no dedicated art (e.g. an uncommon distro not
     *     yet in logos/) fall back to Tux, the mascot for the whole family.
     *   - Non-Linux platforms (macOS, Windows) have no such family mascot
     *     to borrow -- showing Tux for them is actively misleading (a
     *     Windows or macOS run should never look like Linux). Fall back to
     *     nexfetch's own project logo instead. */
    if (strcmp(distro, "macos") == 0 || strcmp(distro, "windows") == 0) {
        n = load_txt("logos/nexfetch.txt", logo_lines);
        if (n <= 0) {
            n = load_txt("/usr/share/nexfetch/logos/nexfetch.txt", logo_lines);
        }
        if (n > 0) return n;
    } else if (strcmp(distro, "tux") != 0) {
        n = load_txt("logos/tux.txt", logo_lines);
        if (n <= 0) {
            n = load_txt("/usr/share/nexfetch/logos/tux.txt", logo_lines);
        }
        if (n > 0) return n;
    }

    return 0;
}