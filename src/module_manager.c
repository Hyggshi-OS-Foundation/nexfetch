#include "module.h"
#include "nexfetch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static Module s_modules[MAX_MODULES];
static int s_module_count = 0;

void module_manager_init(void) {
    s_module_count = 0;
}

void module_manager_register(const char *name, const char *key, void (*detect)(char *, size_t)) {
    if (s_module_count >= MAX_MODULES) return;
    s_modules[s_module_count].name = name;
    s_modules[s_module_count].key = key;
    s_modules[s_module_count].type = MODULE_TYPE_BUILTIN;
    s_modules[s_module_count].detect = detect;
    s_modules[s_module_count].handle = NULL;
    s_module_count++;
}

int module_manager_load_plugin(const char *so_path) {
    if (!so_path || s_module_count >= MAX_MODULES) return -1;
#ifdef _WIN32
    HMODULE handle = LoadLibraryA(so_path);
    if (!handle) {
        fprintf(stderr, "Failed to load plugin %s\n", so_path);
        return -1;
    }

    const char **name_ptr = (const char **)GetProcAddress(handle, "plugin_name");
    const char **key_ptr = (const char **)GetProcAddress(handle, "plugin_key");
    void (*detect_fn)(char *, size_t) = (void (*)(char *, size_t))GetProcAddress(handle, "plugin_detect");
#else
    void *handle = dlopen(so_path, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Failed to load plugin %s: %s\n", so_path, dlerror());
        return -1;
    }

    const char **name_ptr = (const char **)dlsym(handle, "plugin_name");
    const char **key_ptr = (const char **)dlsym(handle, "plugin_key");
    void (*detect_fn)(char *, size_t) = (void (*)(char *, size_t))dlsym(handle, "plugin_detect");
#endif

    if (!name_ptr || !key_ptr || !detect_fn) {
        fprintf(stderr, "Plugin %s missing required exports\n", so_path);
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        return -1;
    }

    s_modules[s_module_count].name = *name_ptr;
    s_modules[s_module_count].key = *key_ptr;
    s_modules[s_module_count].type = MODULE_TYPE_PLUGIN;
    s_modules[s_module_count].detect = detect_fn;
    s_modules[s_module_count].handle = handle;
    s_module_count++;
    return 0;
}

int module_manager_get_count(void) {
    return s_module_count;
}

Module *module_manager_get(int index) {
    if (index < 0 || index >= s_module_count) return NULL;
    return &s_modules[index];
}

void module_manager_cleanup(void) {
    for (int i = 0; i < s_module_count; i++) {
        if (s_modules[i].handle) {
#ifdef _WIN32
            FreeLibrary((HMODULE)s_modules[i].handle);
#else
            dlclose(s_modules[i].handle);
#endif
            s_modules[i].handle = NULL;
        }
    }
    s_module_count = 0;
}
