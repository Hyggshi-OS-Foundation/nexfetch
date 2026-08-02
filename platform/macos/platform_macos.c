#include "platform.h"
#include <stdio.h>

void platform_get_os(char *out, size_t size, char *distro_id, size_t distro_id_size) {
    snprintf(out, size, "macOS");
    if (distro_id) snprintf(distro_id, distro_id_size, "macos");
}
void platform_get_kernel(char *out, size_t size) { snprintf(out, size, "Darwin"); }
void platform_get_host(char *out, size_t size) { snprintf(out, size, "Mac"); }
void platform_get_uptime(char *out, size_t size) { snprintf(out, size, "Unknown"); }
void platform_get_packages(char *out, size_t size) { snprintf(out, size, "Homebrew"); }
void platform_get_shell(char *out, size_t size) { snprintf(out, size, "zsh"); }
void platform_get_de(char *out, size_t size) { snprintf(out, size, "Aqua"); }
void platform_get_wm(char *out, size_t size) { snprintf(out, size, "Quartz Compositor"); }
void platform_get_terminal(char *out, size_t size) { snprintf(out, size, "Terminal"); }
void platform_get_cpu(char *out, size_t size) { snprintf(out, size, "Apple Silicon / Intel"); }
void platform_get_gpu(char *out, size_t size) { snprintf(out, size, "Apple GPU"); }
void platform_get_memory(char *out, size_t size) { snprintf(out, size, "Unknown"); }
void platform_get_disk(char *out, size_t size) { snprintf(out, size, "Unknown"); }
void platform_get_theme(char *out, size_t size) { snprintf(out, size, "Unknown"); }
void platform_get_icons(char *out, size_t size) { snprintf(out, size, "Unknown"); }
void platform_get_font(char *out, size_t size) { snprintf(out, size, "Unknown"); }
void platform_get_battery(char *out, size_t size) { snprintf(out, size, "N/A"); }
void platform_get_network(char *out, size_t size) { snprintf(out, size, "Unknown"); }
void platform_get_locale(char *out, size_t size) { snprintf(out, size, "en_US.UTF-8"); }
void platform_get_swap(char *out, size_t size) { snprintf(out, size, "N/A"); }
void platform_get_display(char *out, size_t size) { snprintf(out, size, "Retina Display"); }
