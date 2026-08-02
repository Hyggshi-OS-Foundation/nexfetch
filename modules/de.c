#include "nexfetch.h"

void module_detect_de(char *out, size_t max_len) {
    platform_get_de(out, max_len);
}

void module_detect_wm(char *out, size_t max_len) {
    platform_get_wm(out, max_len);
}

void module_detect_terminal(char *out, size_t max_len) {
    platform_get_terminal(out, max_len);
}

void module_detect_theme(char *out, size_t max_len) {
    platform_get_theme(out, max_len);
}

void module_detect_icons(char *out, size_t max_len) {
    platform_get_icons(out, max_len);
}

void module_detect_font(char *out, size_t max_len) {
    platform_get_font(out, max_len);
}
