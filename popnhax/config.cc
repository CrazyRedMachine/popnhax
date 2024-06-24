#include <windows.h>
#include <io.h>

#include "util/crc32.h"
#include "util/log.h"
#include "util/psmap.h"
#include "config.h"

#define F_OK 0

static char *get_opt_filename(const char *xml_filename){
    char *opt_filename = strdup(xml_filename);
    if ( opt_filename != NULL )
        strcpy(opt_filename+strlen(opt_filename)-3, "opt");
    return opt_filename;
}

//works for .opt and .xml
static char *get_option(FILE *handle, char *option_name){
    if (handle == NULL || option_name == NULL)
        return NULL;

    rewind(handle);
    char line[256];
    while (fgets(line, 256, handle))
    {
        char *ptr = strchr(line, '<');
        if ( ptr == NULL )
           continue;

        if ( strncmp(option_name, ptr+1, strlen(option_name)) == 0
         && (*(ptr+1+strlen(option_name)) == ' ') ) //filter out common prefixes
        {
            rewind(handle);
            return strdup(line);
        }
    }

    return NULL;
}

//check .xml crc-32 and compare to .opt entry
static bool config_check_crc(const char *filepath){
    //retrieve xml crc
    uint32_t xmlcrc = 0;
    long charcnt = 0;
    if ( crc32file(filepath, &xmlcrc, &charcnt) == false )
    {
        LOG("popnhax: config update: cannot compute %s CRC-32\n", filepath);
        return false;
    }
    LOG("popnhax: config update: %s CRC-32 is %08X\n", filepath, xmlcrc);

    //retrieve opt crc
    uint32_t optcrc = 0;
    char *opt_filename = get_opt_filename(filepath);
    FILE *opt = fopen(opt_filename, "r");
    if (opt == NULL)
    {
        LOG("popnhax: config update: opt file not found.\n");
        return false;
    }
    char line[256];
    fgets(line, 256, opt); //disclaimer line
    fgets(line, 256, opt); //CRC line
    sscanf(line, "<!-- %08X -->\n", &optcrc);
    fclose(opt);
    LOG("popnhax: config update: %s reference CRC is %08X (matching: %s)\n", opt_filename, optcrc, optcrc==xmlcrc? "true":"false");
    free(opt_filename);

    return (optcrc==xmlcrc);
}

static char *get_option_name(char *line)
{
    if ( line == NULL )
        return NULL;

    char *ptr = line;
    while ( *ptr == ' ' || *ptr == '\t' )
        ptr++;

    if ( *ptr != '<' )
        return NULL;

    if ( *(++ptr) == '!' )
        return NULL;

    if ( strchr(ptr, ' ') == NULL )
        return NULL;

    size_t opt_len = strchr(ptr, ' ') - ptr;
    char *option_name = (char*)malloc(opt_len+1);
    strncpy(option_name, ptr, opt_len);
    option_name[opt_len] = '\0';
    return option_name;
}

//check if _update file is present
static bool config_need_update()
{
    return (access("_update", F_OK) == 0);
}

//for every entry in .opt, overwrite xml value
static bool config_update_xml(const char *xml_filename)
{
    char *opt_filename = get_opt_filename(xml_filename);
    FILE *opt = fopen(opt_filename, "r");
    if ( opt == NULL )
    {
        LOG("popnhax: config_update_xml: cannot open opt file\n");
        free(opt_filename);
        return true; //not a problem if it doesn't exist
    }

    FILE *xml = fopen(xml_filename, "r");
    if ( xml == NULL )
    {
        LOG("popnhax: config_update_xml: cannot open xml file\n");
        return false;
    }

    FILE *update = fopen("update.xml.tmp", "w");
    if ( update == NULL )
    {
        LOG("popnhax: config_update_xml: create update.xml.tmp\n");
        return false;
    }

    // read each line from xml and copy to update unless opt has a value in which case copy from opt instead
    char line[256];
    while (fgets(line, 256, xml))
    {
        char *option_name = get_option_name(line);
        char *option_line = get_option(opt, option_name); //will return NULL if option_name is NULL

        if (option_line == NULL)
        {
            //not an option line, copy to dest
            fprintf(update, "%s", line);
            if (option_name)
                free(option_name);
            continue;
        }

        // opt file has an entry for it, take that one
        fprintf(update, "%s", option_line);
        free(option_line);
        free(option_name);
    }

    fclose(xml);
    fclose(opt);
    fclose(update);
    free(opt_filename);

    if ( remove(xml_filename) == -1 )
    {
        LOG("popnhax: config_update_xml: cannot remove %s (%d)\n", xml_filename, errno);
        return false;
    }
    if ( rename("update.xml.tmp",xml_filename) == -1 )
    {
        LOG("popnhax: config_update_xml: cannot rename update.xml.tmp (%d)\n", errno);
        return false;
    }

    return true;
}

//regen .opt file (comprised of disclaimer, crc, and all options from xml)
static bool config_make_opt(const char *xml_filename){
    uint32_t crc = 0;
    long charcnt = 0;
    if ( crc32file(xml_filename, &crc, &charcnt) == false )
    {
        LOG("Cannot compute %s CRC-32\n", xml_filename);
        return false;
    }

    FILE *xml = fopen(xml_filename, "r");
    if ( xml == NULL )
    {
        LOG("popnhax: config update: cannot open config xml file\n");
        return false;
    }

    char *opt_filename = get_opt_filename(xml_filename);
    FILE *opt = fopen(opt_filename, "w");
    free(opt_filename);
    if ( opt == NULL )
    {
        LOG("popnhax: config update: cannot create config opt file\n");
        free(opt_filename);
        fclose(xml);
        return false;
    }

    fprintf(opt, "<!-- GENERATED BY POPNHAX. DO NOT EDIT THIS FILE -->\n");
    fprintf(opt, "<!-- %08X -->\n", crc);

    char line[256];
    while (fgets(line, 256, xml))
    {
        char *ptr = line;
        while ( *ptr == ' ' || *ptr == '\t' )
            ptr++;

        if ( *ptr != '<' )
            continue;

        if ( *(ptr+1) == '!' )
            continue;

        fprintf(opt, "%s", line);
    }

    fclose(opt);
    fclose(xml);
    return true;
}

static bool type_to_str(enum psmap_property_type type, char* expected_type)
{
    bool res = true;
    switch (type)
    {
        case PSMAP_PROPERTY_TYPE_S8:
            strcpy(expected_type, "s8");
            break;
        case PSMAP_PROPERTY_TYPE_U8:
            strcpy(expected_type, "u8");
            break;
        case PSMAP_PROPERTY_TYPE_S16:
            strcpy(expected_type, "s16");
            break;
        case PSMAP_PROPERTY_TYPE_U16:
            strcpy(expected_type, "u16");
            break;
        case PSMAP_PROPERTY_TYPE_S32:
            strcpy(expected_type, "s32");
            break;
        case PSMAP_PROPERTY_TYPE_U32:
            strcpy(expected_type, "u32");
            break;
        case PSMAP_PROPERTY_TYPE_S64:
            strcpy(expected_type, "s64");
            break;
        case PSMAP_PROPERTY_TYPE_U64:
            strcpy(expected_type, "u64");
            break;
        case PSMAP_PROPERTY_TYPE_STR:
            strcpy(expected_type, "str");
            break;
        case PSMAP_PROPERTY_TYPE_FLOAT:
            strcpy(expected_type, "float");
            break;
        case PSMAP_PROPERTY_TYPE_ATTR:
            strcpy(expected_type, "attr");
            break;
        case PSMAP_PROPERTY_TYPE_BOOL:
            strcpy(expected_type, "bool");
            break;
        default:
            strcpy(expected_type, "INVALID");
            res = false;
            break;
    }
    return res;
}

static int64_t get_min(enum psmap_property_type type)
{
    switch (type)
    {
        case PSMAP_PROPERTY_TYPE_S8:
            return -128;
        case PSMAP_PROPERTY_TYPE_S16:
            return -32768;
        case PSMAP_PROPERTY_TYPE_S32:
            return -2147483648;
        case PSMAP_PROPERTY_TYPE_S64:
            return LLONG_MIN;
        case PSMAP_PROPERTY_TYPE_U8:
        case PSMAP_PROPERTY_TYPE_U16:
        case PSMAP_PROPERTY_TYPE_U32:
        case PSMAP_PROPERTY_TYPE_U64:
            return 0;
        default:
            return -1;
    }
}
static int64_t get_max(enum psmap_property_type type)
{
    switch (type)
    {
        case PSMAP_PROPERTY_TYPE_S8:
            return 127;
        case PSMAP_PROPERTY_TYPE_S16:
            return 32767;
        case PSMAP_PROPERTY_TYPE_S32:
            return 2147483647;
        case PSMAP_PROPERTY_TYPE_S64:
            return 9223372036854775807;
        case PSMAP_PROPERTY_TYPE_U8:
            return 255;
        case PSMAP_PROPERTY_TYPE_U16:
            return 65535;
        case PSMAP_PROPERTY_TYPE_U32:
            return 4294967295;
        default:
            return -1;
    }
}

static bool is_valid(const char *option_line, uint8_t type, uint32_t member_width, char **reason)
{
    char tmp_reason[256] = {0};
    char open[64] = {0};
    char expected_type[8] = {0};
    char option_type[64] = {0};
    char value[64] = {0};
    char close[64] = {0};
    if (!type_to_str((enum psmap_property_type) type, expected_type))
    {
        sprintf(tmp_reason, "bad type value"); // shouldn't happen as it's psmap struct type
        *reason = strdup(tmp_reason);
        return false;
    }

    /* attempt to scan line */
    int num = sscanf(option_line, " %s __type=\"%[^<>\"]\">%[^><\"]%[^\"\r\n] ", open, option_type, value, close);
    if ( num == 2 && strcmp(option_type, "str")==0 )
    {
        num = sscanf(option_line, " %s __type=\"%[^<>\"]\">%[^\"\r\n] ", open, option_type, close);
        if (num != 3)
        {
            sprintf(tmp_reason, "malformed line (should be <option_name __type=\"option_type\">option_value</option_name>)");
            *reason = strdup(tmp_reason);
            return false;
        }
    }
    else if ( num != 4 )
    {
        sprintf(tmp_reason, "malformed line (should be <option_name __type=\"option_type\">option_value</option_name>)");
        *reason = strdup(tmp_reason);
        return false;
    }

    /* tag errors */
    if ( open[0] != '<' )
    {
        sprintf(tmp_reason, "malformed opening tag (should be <%s __type\"option_type\">)", open);
        *reason = strdup(tmp_reason);
        return false;
    }
    if ( close[0] != '<' || close[1] != '/' || close[strlen(close)-1] != '>' )
    {
        sprintf(tmp_reason, "malformed closing tag (should be </%s>)", open+1);
        *reason = strdup(tmp_reason);
        return false;
    }
    if ( close[strlen(open)+1] != '>' || strncmp(open+1, close+2, strlen(open+1)) != 0 )
    {
        sprintf(tmp_reason, "opening tag doesn't match closing tag (%s> vs %s)", open, close);
        *reason = strdup(tmp_reason);
        return false;
    }

    /* type errors */
    if ( strcmp(option_type, expected_type) != 0 )
    {
        sprintf(tmp_reason, "unexpected type (expected %s, got %s)", expected_type, option_type);
        *reason = strdup(tmp_reason);
        return false;
    }

    if ( type == PSMAP_PROPERTY_TYPE_STR )
    {
        if ( strlen(value) > member_width-1 )
        {
            sprintf(tmp_reason, "value %s too long (max length is %d, got %d)", value, member_width-1, strlen(value));
            *reason = strdup(tmp_reason);
            return false;
        }
        return true;
    }

    if ( type == PSMAP_PROPERTY_TYPE_ATTR )
        return true; // can't do further checks

    if ( type == PSMAP_PROPERTY_TYPE_FLOAT )
    {
        char *endPtr;
        (void)strtof(value, &endPtr);
        if ( endPtr != (value+strlen(value)) || errno == ERANGE )
        {
            sprintf(tmp_reason, "invalid %s value %s (should be a valid IEEE 754 floating point number)", option_type, value);
            *reason = strdup(tmp_reason);
            return false;
        }
    }

    if ( type == PSMAP_PROPERTY_TYPE_BOOL )
    {
        if ( strlen(value) != 1 || (value[0] != '0' && value[0] != '1') )
        {
            sprintf(tmp_reason, "invalid bool value (should be 0 or 1, got %s)", value);
            *reason = strdup(tmp_reason);
            return false;
        }
        return true;
    }

    if ( type == PSMAP_PROPERTY_TYPE_U64 )
    {
        char *endPtr;
        (void)strtoull(value, &endPtr, 10);
        if ( endPtr != (value+strlen(value)) || errno == ERANGE )
        {
            sprintf(tmp_reason, "invalid %s value %s (should be an integer between 0 and 18446744073709551615)", option_type, value);
            *reason = strdup(tmp_reason);
            return false;
        }
    }
    else // only numeric types up to S64 remain
    {
        char *endPtr;
        int64_t parsed = strtoull(value, &endPtr, 10);
        if ( type == PSMAP_PROPERTY_TYPE_S64 && errno == ERANGE )
        {
            sprintf(tmp_reason, "invalid %s value (should be an integer between -9223372036854775808 and 9223372036854775807)", option_type);
            *reason = strdup(tmp_reason);
            return false;
        }
        else if ( endPtr != (value+strlen(value)) )
        {
            sprintf(tmp_reason, "invalid value %s (should be an integer and nothing else)", value);
            *reason = strdup(tmp_reason);
            return false;
        }

        int64_t min = get_min((enum psmap_property_type)type);
        int64_t max = get_max((enum psmap_property_type)type);
        if ( parsed < min || parsed > max )
        {
            sprintf(tmp_reason, "invalid value (should be an integer between %lld and %lld, got %s)", min, max, value);
            *reason = strdup(tmp_reason);
            return false;
        }
    }

    return true;
}

void config_diag(const char *opt_filepath, const struct property_psmap *psmap) {
    int i = 0;
    int shift = strlen("/popnhax/");
    FILE *opt = fopen(opt_filepath, "r");
    if ( opt == NULL )
    {
        LOG("FATAL ERROR: cannot open %s\n", opt_filepath);
    }

    while ( psmap[i].type != 0xFF )
    {
        bool required = !psmap[i].has_default;
        const char *option_name = psmap[i].path+shift;
        //LOG("checking %s option %s\n", required ? "REQUIRED" : "OPTIONAL", option_name);
        char *option_line = get_option(opt, (char*)option_name); //will return NULL if option_name is NULL
        if (option_line)
        {
            char *reason;
            if (!is_valid(option_line, psmap[i].type, psmap[i].member_width, &reason))
            {
                LOG("\t%s", option_line);
                LOG("ERROR: %s: %s\n", option_name, reason);
            }
        }
        else if (required)
        {
            LOG("ERROR: required option %s not found\n", option_name);
        }
        free(option_line);
        i++;
    }
}

// take care of updating .xml/.opt files if needed
bool config_process(const char *xml_filepath){
    // look for _update marker file
    if ( config_need_update() )
    {
        //update xml with values from opt
        LOG("popnhax: config update: popnhax update detected. Updating xml with your previous settings\n");
        config_update_xml(xml_filepath);
        remove("_update");
    } else if ( config_check_crc(xml_filepath) ) {
        LOG("popnhax: config update: nothing to do.\n");
        return true;
    }

    // regen opt from xml file
    LOG("popnhax: config update: xml has changed. Regenerating opt file\n");
    bool res = config_make_opt(xml_filepath);

    return res;
}