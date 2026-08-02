#if defined(__linux__) || defined(__gnu_linux__)

#include "platform.h"
#include <stdio.h>
#include <sys/utsname.h>

void platform_get_kernel(char *out, size_t size) {
    if (!out || size == 0) return;
    struct utsname buffer;
    if (uname(&buffer) == 0) {
        snprintf(out, size, "%s %s", buffer.release, buffer.machine);
    } else {
        snprintf(out, size, "Unknown");
    }
}

#endif

