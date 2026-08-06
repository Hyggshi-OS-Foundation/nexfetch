// vision_nexfetch.c — Plugin "Vision for Nexfetch"
#define _GNU_SOURCE
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

const char *plugin_name = "Vision for Nexfetch";
const char *plugin_key  = "vision_nexfetch";

void plugin_detect(char *out, size_t max_len)
{
    const char *version = "unknown";

#ifdef _WIN32
    /* Windows: plugin DLL can't easily resolve symbols from the host exe,
       so read the version string exported from the host via GetProcAddress. */
    HMODULE host = GetModuleHandleA(NULL);
    if (host) {
        const char **ver_ptr = (const char **)GetProcAddress(host, "nexfetch_version");
        if (ver_ptr && *ver_ptr)
            version = *ver_ptr;
    }
#else
    /* Linux/macOS: resolve the version symbol exported by the nexfetch
       executable (built with -rdynamic). */
    const char **ver_ptr = (const char **)dlsym(RTLD_DEFAULT, "nexfetch_version");
    if (ver_ptr && *ver_ptr)
        version = *ver_ptr;
#endif

    snprintf(out, max_len, "v%s", version);
}