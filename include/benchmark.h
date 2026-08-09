#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <stddef.h>

typedef struct BenchmarkResult {
    double startup_ms;
    double module_loading_ms;
    double logo_rendering_ms;
    double config_parsing_ms;
    double total_ms;
    char grade[4]; /* "A+", "A", "B", etc. */
} BenchmarkResult;

void benchmark_run(int iterations, BenchmarkResult *result);
void benchmark_print(const BenchmarkResult *result);
void benchmark_compare_run(void);

#endif /* BENCHMARK_H */
