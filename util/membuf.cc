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

	if (buffer == NULL || res == NULL)
	{
		free(buffer);
		free(res);
		return NULL;
	}

	res->buffer = buffer;
	res->size = 0;
	res->idx = 0;
	res->capacity = size;

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
	vsnprintf(line, sizeof(line), fmt, args);
	va_end(args);

	uint32_t len = strlen(line);

	// grow the buffer as needed (+1 for the NUL terminator)
	if (membuf->idx + len + 1 > membuf->capacity)
	{
		uint32_t new_capacity = membuf->capacity * 2;
		while (membuf->idx + len + 1 > new_capacity)
			new_capacity *= 2;

		char *new_buffer = (char *)realloc(membuf->buffer, new_capacity);
		if (new_buffer == NULL)
		{
			printf("membuf: couldn't grow buffer from %u to %u bytes, dropping write\n",
			       membuf->capacity, new_capacity);
			return 0;
		}

		membuf->buffer = new_buffer;
		membuf->capacity = new_capacity;
	}

	strcpy((membuf->buffer)+(membuf->idx), line);
	membuf->size += len;
	membuf->idx += len;
	return len;
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