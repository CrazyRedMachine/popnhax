#include <windows.h>
#include <io.h>

#include "util/crc32.h"
#include "util/log.h"
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
