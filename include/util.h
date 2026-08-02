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

#endif /* UTIL_H */
