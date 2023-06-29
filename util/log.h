#ifndef __LOG_H__
#define __LOG_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern FILE *g_log_fp;

#define LOG(...) do { \
fprintf(g_log_fp, __VA_ARGS__); \
fflush(g_log_fp);\
} while (0)

#endif