#ifndef IMPORTS_AVS_H
#define IMPORTS_AVS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <windows.h>

#if !defined(AVS_VERSION)

#error "Can't build AVS-dependent project using AVS-independent make rules"

#elif AVS_VERSION == 1508 || AVS_VERSION == 1509

#define avs_thread_delay                   XCd229cc00012b
#define property_search                    XCd229cc00012e
#define boot                               XCd229cc0000aa
#define shutdown                           XCd229cc00001d
#define property_desc_to_buffer            XCd229cc0000fd
#define property_destroy                   XCd229cc00013c
#define property_read_query_memsize        XCd229cc0000ff
#define property_create                    XCd229cc000126
#define property_insert_read               XCd229cc00009a
#define property_node_create               XCd229cc00002c
#define property_node_remove               XCd229cc000028
#define property_node_refer                XCd229cc000009
#define std_setenv                         XCd229cc000094
#define avs_fs_open                        XCd229cc000090
#define avs_fs_copy                        XCd229cc0000eb
#define avs_fs_close                       XCd229cc00011f
#define avs_fs_dump_mountpoint             XCd229cc0000e9
#define avs_fs_mount                       XCd229cc0000ce
#define avs_fs_fstat                       XCd229cc0000c3
#define avs_fs_lstat                       XCd229cc0000c0
#define avs_fs_lseek                       XCd229cc00004d
#define avs_fs_read                        XCd229cc00010d
#define avs_fs_opendir                     XCd229cc0000f0
#define avs_fs_readdir                     XCd229cc0000bb
#define avs_fs_closedir                    XCd229cc0000b8
#define cstream_create                     XCd229cc000141
#define cstream_operate                    XCd229cc00008c
#define cstream_finish                     XCd229cc000025
#define cstream_destroy                    XCd229cc0000e3
#define property_node_read                 XCd229cc0000f3
#define property_node_write                XCd229cc00002d
#define property_file_write                XCd229cc000052
#define property_node_traversal            XCd229cc000046
#define property_psmap_export              XCd229cc000006
#define property_psmap_import              XCd229cc000005
#define property_node_name                 XCd229cc000049
#define property_node_get_desc             XCd229cc000165
#define property_get_error                 XCd229cc0000b5
#define property_node_clone                XCd229cc00010a
#define property_query_size                XCd229cc000032
#define property_node_query_stat           XCd229cc0000b1
#define property_node_datasize             XCd229cc000083
#define property_mem_write                 XCd229cc000033
#define property_part_write                XCd229cc000024
#define property_node_absolute_path        XCd229cc00007c
#define property_node_has                  XCd229cc00008a
#define property_node_is_array             XCd229cc000142
#define property_node_type                 XCd229cc000071
#define property_get_attribute_bool        XCd229cc000043
#define property_node_get_attribute_bool   XCd229cc000110
#define property_node_get_attribute_u32    XCd229cc0000db
#define property_node_get_attribute_s32    XCd229cc00011a
#define property_node_rename               XCd229cc0000af
#define property_query_freesize            XCd229cc000144
#define property_clear_error               XCd229cc00014b
#define property_lookup_encode             XCd229cc0000fc
#define property_unlock_flag               XCd229cc000145
#define property_lock_flag                 XCd229cc000121
#define property_set_flag                  XCd229cc000035
#define property_part_write_meta           XCd229cc00004f
#define property_part_write_meta2          XCd229cc000107
#define property_read_data                 XCd229cc0000de
#define property_read_meta                 XCd229cc00010e
#define property_get_attribute_u32         XCd229cc000148
#define property_get_attribute_s32         XCd229cc00005f
#define property_get_fingerprint           XCd229cc000057
#define property_node_refdata              XCd229cc00009f
#define property_insert_read_with_filename XCd229cc0000cd
#define property_mem_read                  XCd229cc000039
#define property_read_query_memsize_long   XCd229cc00002b
#define property_clear                     XCd229cc0000c2
#define avs_net_add_protocol               XCd229cc000156
#define avs_net_del_protocol               XCd229cc00000f
#define avs_net_addrinfobyaddr             XCd229cc000040
#define avs_net_socket                     XCd229cc000026
#define avs_net_setsockopt                 XCd229cc000092
#define avs_net_getsockopt                 XCd229cc000084
#define avs_net_connect                    XCd229cc000038
#define avs_net_send                       XCd229cc00011d
#define avs_net_recv                       XCd229cc000131
#define avs_net_pollfds_add                XCd229cc00004b
#define avs_net_pollfds_get                XCd229cc000105
#define avs_net_bind                       XCd229cc00007e
#define avs_net_close                      XCd229cc00009c
#define avs_net_shutdown                   XCd229cc0000ac
#define avs_net_get_peername               XCd229cc000085
#define avs_net_get_sockname               XCd229cc0000b0

#elif AVS_VERSION == 1700

#define property_create XCgsqzn0000090
#define property_insert_read XCgsqzn0000094
#define property_read_query_memsize XCgsqzn00000b0
#define property_psmap_import XCgsqzn00000b2

#else

#error AVS obfuscated import macros have not been declared for this version

#endif

enum property_type {
    PROPERTY_TYPE_VOID = 1,
    PROPERTY_TYPE_S8 = 2,
    PROPERTY_TYPE_U8 = 3,
    PROPERTY_TYPE_S16 = 4,
    PROPERTY_TYPE_U16 = 5,
    PROPERTY_TYPE_S32 = 6,
    PROPERTY_TYPE_U32 = 7,
    PROPERTY_TYPE_S64 = 8,
    PROPERTY_TYPE_U64 = 9,
    PROPERTY_TYPE_BIN = 10,
    PROPERTY_TYPE_STR = 11,
    PROPERTY_TYPE_FLOAT = 14,
    PROPERTY_TYPE_ATTR = 46,
    PROPERTY_TYPE_BOOL = 52,
};

enum property_node_traversal {
    TRAVERSE_PARENT = 0,
    TRAVERSE_FIRST_CHILD = 1,
    TRAVERSE_FIRST_ATTR = 2,
    TRAVERSE_FIRST_SIBLING = 3,
    TRAVERSE_NEXT_SIBLING = 4,
    TRAVERSE_PREVIOUS_SIBLING = 5,
    TRAVERSE_LAST_SIBLING = 6,
    TRAVERSE_NEXT_SEARCH_RESULT = 7,
    TRAVERSE_PREV_SEARCH_RESULT = 8,
};

struct property;
struct property_node;
struct property_psmap;

typedef int (*avs_reader_t)(uint32_t context, void *bytes, size_t nbytes);

uint32_t property_read_query_memsize(avs_reader_t reader, uint32_t context, int *nodes, int *total);
struct property *property_create(int flags, void *buffer, uint32_t buffer_size);
int property_insert_read(struct property *prop, struct property_node *node, avs_reader_t reader,
                         uint32_t context);
int property_psmap_import(struct property *prop, struct property_node *root, void *dest,
                          const struct property_psmap *psmap);
struct property_node *property_node_create(struct property *prop, struct property_node *parent,
                                           int type, const char *key, ...);
struct property_node *property_search(
    struct property *prop, struct property_node *root, const char *path);

int property_mem_write(struct property *prop, void *bytes, int nbytes);
void *property_desc_to_buffer(struct property *prop);
void property_file_write(struct property *prop, const char *path);
int property_set_flag(struct property *prop, int flags, int mask);
void property_destroy(struct property *prop);

int property_node_refer(
    struct property *prop,
    struct property_node *node,
    const char *name,
    enum property_type type,
    void *bytes,
    uint32_t nbytes);
struct property_node *property_node_traversal(
    struct property_node *node, enum property_node_traversal direction);
void avs_thread_delay(size_t ms, int zero);

#ifdef __cplusplus
}
#endif

#endif
