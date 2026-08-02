#include "nexfetch.h"
#include <stdio.h>

void module_detect_color(char *out, size_t max_len) {
    snprintf(out, max_len,
        "\033[40m   \033[41m   \033[42m   \033[43m   \033[44m   \033[45m   \033[46m   \033[47m   \033[0m");
}
