#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <windows.h>

void *xmalloc(size_t nbytes) {
    void *mem;

    mem = malloc(nbytes);

    if (mem == NULL) {
        return NULL;
    }

    return mem;
}

void *xrealloc(void *mem, size_t nbytes) {
    void *newmem;

    newmem = realloc(mem, nbytes);

    if (newmem == NULL) {
        return NULL;
    }

    return newmem;
}

static void push_argv(int *argc, char ***argv, const char *begin, const char *end) {
    size_t nchars;
    char *str;

    (*argc)++;
    *argv = (char **)xrealloc(*argv, *argc * sizeof(char **));

    nchars = end - begin;
    str = (char *)xmalloc(nchars + 1);
    memcpy(str, begin, nchars);
    str[nchars] = '\0';

    (*argv)[*argc - 1] = str;
}

void args_recover(int *argc_out, char ***argv_out) {
    int argc;
    char **argv;
    char *begin;
    char *pos;
    bool quote;

    argc = 0;
    argv = NULL;
    quote = false;

    for (begin = pos = GetCommandLine(); *pos; pos++) {
        switch (*pos) {
        case '"':
            if (!quote) {
                quote = true;
                begin = pos + 1;
            } else {
                push_argv(&argc, &argv, begin, pos);

                quote = false;
                begin = NULL;
            }

            break;

        case ' ':
            if (!quote && begin != NULL) {
                push_argv(&argc, &argv, begin, pos);
                begin = NULL;
            }

            break;

        default:
            if (begin == NULL) {
                begin = pos;
            }

            break;
        }
    }

    if (begin != NULL && !quote) {
        push_argv(&argc, &argv, begin, pos);
    }

    *argc_out = argc;
    *argv_out = argv;
}
