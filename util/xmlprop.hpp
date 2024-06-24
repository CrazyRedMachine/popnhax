#ifndef __XML_H__
#define __XML_H__

#include <stdio.h>

#include "imports/avs.h"
#include "util/membuf.h"
#include "util/psmap.h"

int reader_callback(uint32_t context, void *bytes, size_t nbytes) {
    return fread(bytes, 1, nbytes, (FILE *)TlsGetValue(context));
}

int membuf_reader_callback(uint32_t context, void *bytes, size_t nbytes) {
    int written = nbytes;
    membuf_t *membuf = (membuf_t *)TlsGetValue(context);
    if ( membuf->idx + nbytes > membuf->size )
        written = membuf->size - membuf->idx;

    memcpy(bytes, (membuf->buffer)+membuf->idx, written);
    membuf->idx += written;

    return written;
}

struct property *load_prop_membuf(membuf_t *membuf) {
    if (!membuf) {
        printf("Could not open membuf %p\n", membuf);
        return nullptr;
    }
    DWORD tlsIndex = TlsAlloc();
    TlsSetValue(tlsIndex, (void *)membuf);

    const size_t size = property_read_query_memsize(membuf_reader_callback, tlsIndex, NULL, NULL);
    membuf_rewind(membuf);

    struct property_node *buffer =
        (struct property_node *)calloc(size + 0x10000, sizeof(unsigned char));
    struct property *config_data = property_create(0x17, buffer, size + 0x10000);

    if (size > 0) {
        const int ret = property_insert_read(config_data, 0, membuf_reader_callback, tlsIndex);

        if (!ret) {
            printf("Could not load buffer\n");
            exit(-6);
        }
    }

    return config_data;
}

struct property *load_prop_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    DWORD tlsIndex = TlsAlloc();

    TlsSetValue(tlsIndex, (void *)file);

    if (!file) {
        printf("Could not open config file: %s\n", filename);
        return nullptr;
    }

    const size_t size = property_read_query_memsize(reader_callback, tlsIndex, NULL, NULL);
    rewind(file);

    struct property_node *buffer =
        (struct property_node *)calloc(size + 0x10000, sizeof(unsigned char));
    struct property *config_data = property_create(0x17, buffer, size + 0x10000);

    if (size > 0) {
        const int ret = property_insert_read(config_data, 0, reader_callback, tlsIndex);

        if (ret) {
            fclose(file);
        } else {
            printf("Could not load %s\n", filename);
            exit(-6);
        }
    }

    return config_data;
}

bool _load_config(const char *filename, void *dest, const struct property_psmap *psmap) {
    struct property *config_xml = load_prop_file(filename);

    if (!config_xml || !(property_psmap_import(config_xml, nullptr, dest, psmap))) {
        return false;
    }

    return true;
}

#endif