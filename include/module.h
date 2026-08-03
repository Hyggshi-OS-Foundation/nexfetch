#ifndef MODULE_H
#define MODULE_H

#include <stddef.h>

typedef enum ModuleType {
    MODULE_TYPE_BUILTIN,
    MODULE_TYPE_PLUGIN,
    MODULE_TYPE_CUSTOM
} ModuleType;

typedef struct Module {
    const char *name;       // Display key e.g. "OS", "CPU", "Memory"
    const char *key;        // Config key e.g. "os", "cpu", "memory"
    ModuleType type;
    void (*detect)(char *out_val, size_t max_len);
    void *handle;           // dlopen handle if dynamic plugin
} Module;

void module_manager_init(void);
void module_manager_register(const char *name, const char *key, void (*detect)(char *, size_t));
int module_manager_load_plugin(const char *so_path);
void module_manager_run_all(void);
int module_manager_get_count(void);
Module *module_manager_get(int index);
void module_manager_cleanup(void);

/* Global version symbol exported to dynamic plugins via dlsym(RTLD_DEFAULT, "nexfetch_version") */
extern const char *nexfetch_version;

#endif /* MODULE_H */
