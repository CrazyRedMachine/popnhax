#ifndef __XML_H__
#define __XML_H__

#include <stdio.h>

#include "imports/avs.h"
#include "util/membuf.h"

struct property_psmap {
    uint8_t type;
    uint8_t has_default;
    uint16_t field_offset;
    uint32_t member_width;
    const char *path;
    uintptr_t default_value;
} __attribute__((packed));

enum psmap_property_type {
    PSMAP_PROPERTY_TYPE_S8 = 0x02,
    PSMAP_PROPERTY_TYPE_U8 = 0x03,
    PSMAP_PROPERTY_TYPE_S16 = 0x04,
    PSMAP_PROPERTY_TYPE_U16 = 0x05,
    PSMAP_PROPERTY_TYPE_S32 = 0x06,
    PSMAP_PROPERTY_TYPE_U32 = 0x07,
    PSMAP_PROPERTY_TYPE_S64 = 0x08,
    PSMAP_PROPERTY_TYPE_U64 = 0x09,
    PSMAP_PROPERTY_TYPE_STR = 0x0A,
    PSMAP_PROPERTY_TYPE_FLOAT = 0x0D,
    PSMAP_PROPERTY_TYPE_ATTR = 0x2D,
    PSMAP_PROPERTY_TYPE_BOOL = 0x32,
};

#ifndef __offsetof
#define __offsetof(_s, _f) __builtin_offsetof(_s, _f)
#endif

#ifndef __fieldwidth
#define __fieldwidth(_s, _f) sizeof((((_s *)NULL)->_f))
#endif

#define PSMAP_BEGIN(_name, _type) const _type struct property_psmap _name[] = {
#define PSMAP_MEMBER_REQ(_type, _struct, _member, _path)                                           \
    {_type, false, __offsetof(_struct, _member), __fieldwidth(_struct, _member), _path, 0},
#define PSMAP_MEMBER_OPT(_type, _struct, _member, _path, _def)                                     \
    {_type, true,           __offsetof(_struct, _member), __fieldwidth(_struct, _member),          \
     _path, (uintptr_t)_def},
#define PSMAP_END                                                                                  \
    { 0xff, false, 0, 0, "NULL", 0 }                                                               \
    }                                                                                              \
    ;

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

    if (!config_xml) {
        printf("Couldn't load xml file: %s\n", filename);
        return false;
    }

    if (!(property_psmap_import(config_xml, nullptr, dest, psmap))) {
        printf("Couldn't parse psmap\n");
        return false;
    }

    return true;
}

#endif