#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

char *util_read_first_line(const char *filepath, char *buf, size_t size);
char *util_read_file_content(const char *filepath, char *buf, size_t size);
char *util_trim(char *str);
void util_format_bytes(unsigned long long bytes, char *out, size_t max_len);
void util_format_duration(long seconds, char *out, size_t max_len);
int util_execute_cmd(const char *cmd, char *out, size_t max_len);
void util_expand_escapes(char *str);
int util_mkdir_p(const char *path);
int util_copy_file(const char *src, const char *dst);
int util_get_user_config_dir(char *out, size_t size);

/* --- Short-TTL cache for slow lookups (gsettings, dconf, subprocess probes) ---
 * Returns 1 and fills out[] if a fresh (< TTL) cache entry exists, 0 otherwise.
 * cache_write() stores a value for later reads. */
int util_cache_read(const char *key, char *out, size_t size);
void util_cache_write(const char *key, const char *value);

#endif /* UTIL_H */
