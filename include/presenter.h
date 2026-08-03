#ifndef PRESENTER_H
#define PRESENTER_H

#include "nexfetch.h"
#include <stddef.h>

typedef struct ModuleResult {
    const char *key;
    const char *val;
} ModuleResult;

typedef struct Presenter {
    const char *name;         // e.g. "boxed", "classic", "modern", "compact"
    void (*render)(const char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN],
                   int logo_count,
                   size_t max_logo_width,
                   const ModuleResult results[],
                   int result_count,
                   const char *user_host,
                   const char *separator);
} Presenter;

void presenter_manager_init(void);
Presenter *presenter_get_active(void);
void presenter_set_active(const char *name);
void presenter_render(const char logo_lines[MAX_LOGO_LINES][MAX_LOGO_LINE_LEN],
                      int logo_count,
                      size_t max_logo_width,
                      const ModuleResult results[],
                      int result_count,
                      const char *user_host,
                      const char *separator);
void render_background(const char *image_path);

#endif /* PRESENTER_H */
