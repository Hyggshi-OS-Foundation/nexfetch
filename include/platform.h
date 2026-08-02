#ifndef PLATFORM_H
#define PLATFORM_H

#include <stddef.h>

void platform_get_os(char *out, size_t size, char *distro_id, size_t distro_id_size);
void platform_get_kernel(char *out, size_t size);
void platform_get_host(char *out, size_t size);
void platform_get_uptime(char *out, size_t size);
void platform_get_packages(char *out, size_t size);
void platform_get_shell(char *out, size_t size);
void platform_get_de(char *out, size_t size);
void platform_get_wm(char *out, size_t size);
void platform_get_terminal(char *out, size_t size);
void platform_get_cpu(char *out, size_t size);
void platform_get_gpu(char *out, size_t size);
void platform_get_memory(char *out, size_t size);
void platform_get_disk(char *out, size_t size);
void platform_get_theme(char *out, size_t size);
void platform_get_icons(char *out, size_t size);
void platform_get_font(char *out, size_t size);
void platform_get_battery(char *out, size_t size);
void platform_get_network(char *out, size_t size);

/* New modules */
void platform_get_locale(char *out, size_t size);
void platform_get_swap(char *out, size_t size);
void platform_get_display(char *out, size_t size);

#endif /* PLATFORM_H */
