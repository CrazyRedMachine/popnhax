#define READ_HEX(_xml, _prop, _prop_name, _var_output) \
    {unsigned char _temp[256] = {}; \
    property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_STR, _temp, sizeof(_temp)); \
    _var_output = strtol((const char*)_temp, NULL, 16); \
    } \

#define READ_STR_RAW(_xml, _prop, _prop_name, _var_name) \
    unsigned char _var_name[256] = {}; \
    property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_STR, _var_name, sizeof(_var_name)); \

#define READ_STR(_xml, _prop, _prop_name, _var_name, _var_output) \
    unsigned char _var_name[256] = {}; \
    property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_STR, _var_name, sizeof(_var_name)); \
    uint8_t *_var_output = add_string(_var_name);

#define READ_U8_ARR(_xml, _prop, _prop_name, _var_name, _elm_cnt) \
    uint8_t _var_name[_elm_cnt] = {}; \
    property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_U8, (char*)&_var_name, sizeof(_var_name[0]) * _elm_cnt);

#define READ_U8(_xml, _prop, _prop_name, _var_name) \
    uint8_t _var_name = 0; \
    property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_U8, (char*)&_var_name, sizeof(_var_name));

#define READ_S8(_xml, _prop, _prop_name, _var_name) \
    int8_t _var_name = 0; \
    property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_U8, (char*)&_var_name, sizeof(_var_name));

#define READ_U16_ARR(_xml, _prop, _prop_name, _var_name, _elm_cnt) \
    uint16_t _var_name[_elm_cnt] = {}; \
    property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_U16, (char*)&_var_name, sizeof(_var_name[0]) * _elm_cnt);

#define READ_U16(_xml, _prop, _prop_name, _var_name) \
    uint16_t _var_name = 0; \
    property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_U16, (char*)&_var_name, sizeof(_var_name));

#define READ_S16(_xml, _prop, _prop_name, _var_name) \
    int16_t _var_name = 0; \
    property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_S16, (char*)&_var_name, sizeof(_var_name));

#define READ_U32(_xml, _prop, _prop_name, _var_name) \
    uint32_t _var_name = 0; \
    property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_U32, (char*)&_var_name, sizeof(_var_name));

#define READ_S32(_xml, _prop, _prop_name, _var_name) \
    uint32_t _var_name = 0; \
    property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_S32, (char*)&_var_name, sizeof(_var_name));


// Optionals: Update _var_output if _prop_name exists
#define READ_STR_OPT(_xml, _prop, _prop_name, _var_output) \
{ \
    if (property_search(_xml, _prop, _prop_name)) { \
        unsigned char _var_tempname[256] = {}; \
        property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_STR, _var_tempname, sizeof(_var_tempname)); \
        _var_output = add_string(_var_tempname); \
    } \
}

#define READ_U32_OPT(_xml, _prop, _prop_name, _var_output) \
{ \
    if (property_search(_xml, _prop, _prop_name)) { \
        property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_U32, (char*)&_var_output, sizeof(_var_output)); \
    } \
}

#define READ_S32_OPT(_xml, _prop, _prop_name, _var_output) \
{ \
    if (property_search(_xml, _prop, _prop_name)) { \
        property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_S32, (char*)&_var_output, sizeof(_var_output)); \
    } \
}

#define READ_U16_OPT(_xml, _prop, _prop_name, _var_output) \
{ \
    if (property_search(_xml, _prop, _prop_name)) { \
        property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_U16, (char*)&_var_output, sizeof(_var_output)); \
    } \
}

#define READ_S16_OPT(_xml, _prop, _prop_name, _var_output) \
{ \
    if (property_search(_xml, _prop, _prop_name)) { \
        property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_S16, (char*)&_var_output, sizeof(_var_output)); \
    } \
}

#define READ_U8_OPT(_xml, _prop, _prop_name, _var_output) \
{ \
    if (property_search(_xml, _prop, _prop_name)) { \
        property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_U8, (char*)&_var_output, sizeof(_var_output)); \
    } \
}

#define READ_U8_ARR_OPT(_xml, _prop, _prop_name, _var_output, _elm_cnt) \
{ \
    if (property_search(_xml, _prop, _prop_name)) { \
        property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_U8, (char*)&_var_output, sizeof(_var_output[0]) * _elm_cnt); \
    } \
}

#define READ_U16_ARR_OPT(_xml, _prop, _prop_name, _var_output, _elm_cnt) \
{ \
    if (property_search(_xml, _prop, _prop_name)) { \
        property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_U16, (char*)&_var_output, sizeof(_var_output[0]) * _elm_cnt); \
    } \
}


#define READ_CHARA_OPT(_xml, _prop, _prop_name, _var_output) \
{ \
    if (property_search(_xml, _prop, _prop_name)) { \
        unsigned char _var_tempname[256] = {}; \
        property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_STR, _var_tempname, sizeof(_var_tempname)); \
        _var_output = get_chara_idx(_var_tempname); \
    } \
}

#define READ_LAPIS_SHAPE_OPT(_xml, _prop, _prop_name, _var_output) \
{ \
    if (property_search(_xml, _prop, _prop_name)) { \
        unsigned char _var_tempname[256] = {}; \
        property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_STR, _var_tempname, sizeof(_var_tempname)); \
        _var_output = get_lapis_shape_id(_var_tempname); \
    } \
}

#define READ_LAPIS_COLOR_OPT(_xml, _prop, _prop_name, _var_output) \
{ \
    if (property_search(_xml, _prop, _prop_name)) { \
        unsigned char _var_tempname[256] = {}; \
        property_node_refer(_xml, _prop, _prop_name, PROPERTY_TYPE_STR, _var_tempname, sizeof(_var_tempname)); \
        _var_output = get_lapis_color_id(_var_tempname); \
    } \
}
