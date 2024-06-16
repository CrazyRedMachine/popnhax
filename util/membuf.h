#ifndef __MEMBUF_H__
#define __MEMBUF_H__

#include <stdint.h>

typedef struct membuf_s {
	char *buffer;
	uint32_t size;
	uint32_t idx;
} membuf_t;

membuf_t* membuf_new(uint32_t size);
void membuf_free(membuf_t* membuf);
void membuf_rewind(membuf_t *membuf);
int membuf_printf(membuf_t *membuf, const char *fmt, ...);
bool membuf_tofile(membuf_t *membuf, char *filepath);

#endif
