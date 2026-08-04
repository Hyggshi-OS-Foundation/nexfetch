#include "nexfetch.h"
#include <stdio.h>

/*
 * module_detect_color() renders the "color blocks" row shown at the bottom
 * of the fetch output. It used to draw only the 8 normal ANSI background
 * colors (40-47). This now also appends the 8 "bright"/high-intensity
 * background colors (100-107) right after them, so the bar shows the full
 * 16-color palette instead of just the normal half -- `max_len` was already
 * enough room for this (snprintf still truncates safely if a caller ever
 * passes a smaller buffer).
 */
void module_detect_color(char *out, size_t max_len) {
    snprintf(out, max_len,
        "\033[40m   \033[41m   \033[42m   \033[43m   \033[44m   \033[45m   \033[46m   \033[47m   \033[0m "
        "\033[100m   \033[101m   \033[102m   \033[103m   \033[104m   \033[105m   \033[106m   \033[107m   \033[0m");
}
