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

/* Version symbol exported to plugins via dlsym(RTLD_DEFAULT, "nexfetch_version") */
const char *nexfetch_version = NEXFETCH_VERSION;

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

#include "util.h"
#include <dirent.h>
#include <unistd.h>

static const char *get_filename_basename(const char *path) {
    const char *p = strrchr(path, '/');
#ifdef _WIN32
    const char *p2 = strrchr(path, '\\');
    if (p2 && (!p || p2 > p)) p = p2;
#endif
    return p ? p + 1 : path;
}

int module_manager_load_plugin(const char *so_path) {
    if (!so_path || s_module_count >= MAX_MODULES) return -1;

    char resolved_path[512] = "";
    char user_dir[512] = "";
    const char *base = get_filename_basename(so_path);

    /* Priority 1: ~/.config/nexfetch/modules/ */
    if (util_get_user_config_dir(user_dir, sizeof(user_dir)) == 0) {
        char candidate[512];
        snprintf(candidate, sizeof(candidate), "%s/modules/%s", user_dir, base);
        if (access(candidate, F_OK) == 0) {
            snprintf(resolved_path, sizeof(resolved_path), "%s", candidate);
        } else {
            snprintf(candidate, sizeof(candidate), "%s/modules/%s", user_dir, so_path);
            if (access(candidate, F_OK) == 0) {
                snprintf(resolved_path, sizeof(resolved_path), "%s", candidate);
            }
        }
    }

    /* Priority 2: /usr/lib/nexfetch/modules/ */
    if (resolved_path[0] == '\0') {
        char candidate[512];
        snprintf(candidate, sizeof(candidate), "/usr/lib/nexfetch/modules/%s", base);
        if (access(candidate, F_OK) == 0) {
            snprintf(resolved_path, sizeof(resolved_path), "%s", candidate);
        }
    }

    /* Priority 3: specified path as-is (e.g. ./plugins/...) */
    if (resolved_path[0] == '\0') {
        if (access(so_path, F_OK) == 0) {
            snprintf(resolved_path, sizeof(resolved_path), "%s", so_path);
        }
    }

    /* Priority 4: /usr/share/nexfetch/plugins/ */
    if (resolved_path[0] == '\0') {
        char candidate[512];
        snprintf(candidate, sizeof(candidate), "/usr/share/nexfetch/plugins/%s", base);
        if (access(candidate, F_OK) == 0) {
            snprintf(resolved_path, sizeof(resolved_path), "%s", candidate);
        }
    }

    if (resolved_path[0] == '\0') {
        snprintf(resolved_path, sizeof(resolved_path), "%s", so_path);
    }

#ifdef _WIN32
    HMODULE handle = LoadLibraryA(resolved_path);
    if (!handle) {
        return -1;
    }

    const char **name_ptr = (const char **)GetProcAddress(handle, "plugin_name");
    const char **key_ptr = (const char **)GetProcAddress(handle, "plugin_key");
    void (*detect_fn)(char *, size_t) = (void (*)(char *, size_t))GetProcAddress(handle, "plugin_detect");
#else
    void *handle = dlopen(resolved_path, RTLD_LAZY);
    if (!handle) {
        return -1;
    }

    const char **name_ptr = (const char **)dlsym(handle, "plugin_name");
    const char **key_ptr = (const char **)dlsym(handle, "plugin_key");
    void (*detect_fn)(char *, size_t) = (void (*)(char *, size_t))dlsym(handle, "plugin_detect");
#endif

    if (!name_ptr || !key_ptr || !detect_fn) {
        fprintf(stderr, "Plugin %s missing required exports\n", resolved_path);
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        return -1;
    }

    /* Avoid duplicate registration if key already loaded */
    for (int i = 0; i < s_module_count; i++) {
        if (s_modules[i].key && strcmp(s_modules[i].key, *key_ptr) == 0) {
#ifdef _WIN32
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
            return 0;
        }
    }

    s_modules[s_module_count].name = *name_ptr;
    s_modules[s_module_count].key = *key_ptr;
    s_modules[s_module_count].type = MODULE_TYPE_PLUGIN;
    s_modules[s_module_count].detect = detect_fn;
    s_modules[s_module_count].handle = handle;
    s_module_count++;
    return 0;
}

static void module_manager_scan_and_load(const char *dir_path) {
    if (!dir_path || !*dir_path) return;
    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        const char *ext = strrchr(ent->d_name, '.');
        if (ext && (strcmp(ext, ".so") == 0 || strcmp(ext, ".dll") == 0)) {
            char full[512];
            snprintf(full, sizeof(full), "%s/%s", dir_path, ent->d_name);
            module_manager_load_plugin(full);
        }
    }
    closedir(dir);
}

void module_manager_load_from_dirs(void) {
    char user_dir[512] = "";
    if (util_get_user_config_dir(user_dir, sizeof(user_dir)) == 0) {
        char user_mod_dir[512];
        snprintf(user_mod_dir, sizeof(user_mod_dir), "%s/modules", user_dir);
        module_manager_scan_and_load(user_mod_dir);
    }
    module_manager_scan_and_load("/usr/lib/nexfetch/modules");
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
