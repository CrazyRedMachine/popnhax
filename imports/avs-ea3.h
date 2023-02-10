#ifndef IMPORTS_AVS_EA3_H
#define IMPORTS_AVS_EA3_H

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(AVS_VERSION)

#error "Can't build AVS-dependent project using AVS-independent make rules"

#elif AVS_VERSION == 1509

#define ea3_xrpc_apply XE592acd000057
#define ea3_xrpc_module_register XE592acd000060
#define ea3_xrpc_new XE592acd000052
#define ea3_xrpc_destroy XE592acd000007
#define ea3_boot XE592acd00008c
#define ea3_shutdown XE592acd00005a

#else

#error AVS obfuscated import macros have not been declared for this version

#endif

#define XRPC_STATUS_SERVER_FAULT_ERROR (-17)
#define XRPC_STATUS_SERVER_RESPONSE_ERROR (-18)

void ea3_boot(struct property_node *conf);
void ea3_shutdown(void);

struct xrpc_handle;
struct xrpc_server_handle;

struct xrpc_status {
    int16_t status;
    int16_t subcode;
    int16_t status_code;
    int16_t fault_code;
} __attribute__((gcc_struct, packed));

struct xrpc_arg_list {
    const char *name;
    const char *property_path;
    bool omittable;
} __attribute__((gcc_struct, packed));

/*
 * The following defines the codes used inside the 'status@' attribute
 * used in a xrpc response.
 */

enum ea3_general_status_codes {
    XRPC_OK = 0, /* No error */
};

enum xrpc_method_types {
    HTTPAC_HTTP10 = 0x1,
};

struct xrpc_method {
    const char *xrpc_meth_name;
    bool(__cdecl *xrpc_cb_init)(void *shmem, char *buffer);
    bool(__cdecl *xrpc_cb_sender)(void *shmem, struct property_node *node);
    bool(__cdecl *xrpc_cb_receiver)(void *shmem, struct property_node *node);
    char crypt_level;
    char padding_00;
    bool use_xrpc11;
    bool use_esign;
    bool use_ssl;
    char compress_type;
    char method_type;
    char padding_01;
    struct xrpc_arg_list *arg_list;
} __attribute__((gcc_struct, packed));

typedef int (*xrpc_apply_exit_callback_t)(void *buffer, struct xrpc_status status, void *param);

int ea3_xrpc_apply(struct xrpc_handle *handle, const char *name, void *shmem,
                   xrpc_apply_exit_callback_t cbexit, void *cbexit_data, ...);

struct xrpc_handle *ea3_xrpc_new(size_t sz_xrpc_buf, const char *xrpc_encoding, uint32_t flags);
void ea3_xrpc_destroy(struct xrpc_handle *handle);
int ea3_xrpc_module_register(const char *xrpc_endpoint, const char *services_url, uint32_t flags,
                             struct xrpc_method *method_array);

#ifdef __cplusplus
}
#endif

#endif
