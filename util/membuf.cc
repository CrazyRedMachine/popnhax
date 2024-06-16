#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#include "membuf.h"

membuf_t* membuf_new(uint32_t size)
{
	char *buffer = (char *)malloc(size);
	membuf_t *res = (membuf_t *)malloc(sizeof(membuf_t));
	
	res->buffer = buffer;
	res->size = 0;
	res->idx = 0;
	
	return res;
}

void membuf_free(membuf_t* membuf)
{
	free(membuf->buffer);
	free(membuf);
}

void membuf_rewind(membuf_t *membuf)
{
	membuf->idx = 0;
}

int membuf_printf(membuf_t *membuf, const char *fmt, ...)
{
	va_list args;
	char line[256];
	va_start(args, fmt);
	vsprintf(line, fmt, args);
	va_end(args);
	strcpy((membuf->buffer)+(membuf->idx), line);
	membuf->size += strlen(line);
	membuf->idx += strlen(line);
	return strlen(line);
}

bool membuf_tofile(membuf_t *membuf, char *filepath)
{
	FILE *file = fopen(filepath, "w");
	if (!file)
		return false;

	int results = fputs(membuf->buffer, file);
	fclose(file);

	if (results == EOF) {
		return false;
	}

	return true;
}