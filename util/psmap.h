#ifndef __PSMAP_H__
#define __PSMAP_H__

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

#endif