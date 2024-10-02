#include <cstdio>
#include <cstring>
#include <io.h>
#include <winsock2.h>
#include <windows.h>

#include <process.h>

#include "util/jsmn.h"
#include "util/jsmn-find.h"

#include "util/log.h"
#include "util/patch.h"

#include "popnhax/custom_categs.h"

#include "libcurl/curl/curl.h"

#define DEBUG_CURL 0

#if DEBUG_CURL == 1
FILE *g_curl_log_fp;
#define CURL_PRINT(...) do { \
if (g_curl_log_fp != NULL) { \
fprintf(g_curl_log_fp, __VA_ARGS__); \
fflush(g_curl_log_fp);\
 }\
fprintf(stderr, __VA_ARGS__); \
} while (0)
#else
#define CURL_PRINT(...) do { \
fprintf(stderr, __VA_ARGS__); \
} while (0)
#endif

#define MAX_RIVALS 4

#define SCORE_TABLE_HEAP 1
#define SCORE_TABLE_TYPE struct ScoreHashtable
#define SCORE_TABLE_BUCKET struct ScoreBucket
#define SCORE_TABLE_FREE_KEY(key) free(key)
#define SCORE_TABLE_HASH(key, hash) chash_string_hash(key, hash)
#define SCORE_TABLE_FREE_VALUE(value) free(value)
#define SCORE_TABLE_COMPARE(cmp_a, cmp_b) chash_string_compare(cmp_a, cmp_b)
#define SCORE_TABLE_INIT(bucket, _key, _value) chash_default_init(bucket, _key, _value)

/* libcurl imports */
typedef CURL* (*curl_easy_init_fn_t)(void);
typedef CURLcode (*curl_easy_setopt_fn_t)(CURL *curl, CURLoption option, ...);
typedef CURLcode (*curl_easy_perform_fn_t)(CURL *curl);
typedef void (*curl_easy_cleanup_fn_t)(CURL *curl);
typedef struct curl_slist* (*curl_slist_append_fn_t)(struct curl_slist *, const char *);
typedef void (*curl_slist_free_all_fn_t)(struct curl_slist *);
curl_slist_append_fn_t _curl_slist_append = NULL;
curl_slist_free_all_fn_t _curl_slist_free_all = NULL;
curl_easy_init_fn_t _curl_easy_init = NULL;
curl_easy_setopt_fn_t _curl_easy_setopt = NULL;
curl_easy_perform_fn_t _curl_easy_perform = NULL;
curl_easy_cleanup_fn_t _curl_easy_cleanup = NULL;
HINSTANCE g_libcurl = NULL;
/*  */

bool load_libcurl()
{
	if ( g_libcurl != NULL )
		return true; //already done
	
	g_libcurl = LoadLibrary("libcurl.dll");
	if ( g_libcurl == NULL )
		return false;

	_curl_easy_init = (curl_easy_init_fn_t)GetProcAddress(g_libcurl, "curl_easy_init");
	_curl_easy_setopt = (curl_easy_setopt_fn_t)GetProcAddress(g_libcurl, "curl_easy_setopt");
	_curl_easy_perform = (curl_easy_perform_fn_t)GetProcAddress(g_libcurl, "curl_easy_perform");
	_curl_easy_cleanup = (curl_easy_cleanup_fn_t)GetProcAddress(g_libcurl, "curl_easy_cleanup");
	_curl_slist_append = (curl_slist_append_fn_t)GetProcAddress(g_libcurl, "curl_slist_append");
	_curl_slist_free_all = (curl_slist_free_all_fn_t)GetProcAddress(g_libcurl, "curl_slist_free_all");

	return ( _curl_easy_init != NULL 
	      && _curl_easy_setopt != NULL 
		  && _curl_easy_perform != NULL 
		  && _curl_easy_cleanup != NULL 
		  && _curl_slist_append != NULL 
		  && _curl_slist_free_all != NULL);
}

#pragma pack(1)
typedef struct score_info_s {
    uint32_t score;
    uint16_t gauge;
    uint16_t unk;
    uint32_t max_combo;
    uint16_t nb_cool;
    uint16_t nb_great;
    uint16_t nb_good;
    uint16_t nb_bad;
    uint32_t padding1;
    uint16_t nb_fast;
    uint16_t nb_slow;
    uint32_t unk2;
    uint32_t unk3;
    uint8_t clear_type;
    uint8_t clear_rank;
} score_info_t;

#pragma pack(1)
typedef struct song_info_s {
    uint16_t music_id;
    uint8_t sheet_id;
    uint8_t sheet_id2;
    uint32_t unk;
    uint16_t unk2;
    uint8_t  hispeed; // c'est x10.. va de 0x0A à 0x64 pour 1.0 à 10.0
    uint8_t  unk3;
    uint8_t  popkun; //0: normal, 1:beat, 2:you, 3:rival
    uint8_t  hidden; // on/off
    int16_t hidden_value;
    uint8_t sudden;
    uint8_t padding;
    int16_t sudden_value;
    uint8_t random; // 0:non  1:mirror  2:random 3: srandom
    uint8_t judge;
    uint8_t gauge_type; //0 normal 1: easy 2: hard 3: danger
    /*
    uint8_t unk;
    uint8_t ojama1;
    uint8_t ojama2;

    uint8_t unk;
    uint8_t unk;
    uint8_t ojama1_state;
    uint8_t ojama2_state;

    uint8_t guidese;
    uint8_t lift_state;
    int16_t lift;
 */
} song_info_t;

#pragma pack(1)
typedef struct rival_info_s {
    int16_t no;       // rival index [0;3]
    char g_pm_id[13]; // friendid
    char name[13];    // username, 6 unicode or 12 ascii
    int16_t chara_num;
    int8_t is_open;   // always 1?
    int8_t padding;   // 00
} rival_info_t;

#pragma pack(1)
typedef struct rival_score_s {
    uint32_t music_num;
    uint32_t sheet_num;
    uint32_t score;
    uint32_t clear_rank;
    uint32_t clear_type;
} rival_score_t;

struct ScoreBucket {
    char *k;
    rival_score_t *v;
    int state;
};

struct ScoreHashtable {
    int size;
    int capacity;
    struct ScoreBucket *fields;
};

struct MemoryStruct {
  char *memory;
  size_t size;
};

bool g_skip_omni = false;

struct ScoreHashtable *g_score_hashtable = NULL;
rival_score_t *g_rival_score_current = NULL;
int g_rival_current_idx = 0;
int g_rival_score_count = 0;

/* will be loaded from file */
char g_api_key[64] = {0};
char g_base_url[128] = {0};
char g_import_url[128] = {0};
char g_status_url[128] = {0};
char g_users_url_prefix[128] = {0};
char g_rivals_url[128] = {0};
char g_pbs_url_suffix[128] = {0};

char g_rivals[5][32]; //to store usernames

void (*write_rival_score)();
uint32_t* (*get_rivals_ptr)();
uint32_t g_rival_entry_size;
uint32_t g_rival_count_offset;
rival_info_t g_curr_rival_info;
rival_score_t g_curr_rival_score;
uint32_t g_curr_rival_score_count = 1;
uint32_t g_rival_count = 1;

struct MemoryStruct g_curl_data;
score_info_t *g_score_info;
song_info_t *g_song_info;
uint32_t *g_song_info_zone;
uint32_t g_playerdata_zone_addr; //pointer to the playerdata memory zone (offset 0x08 is popn friend ID as ascii (12 char long), offset 0x44 is rival count, g_tachi_loggedin_offset is "is logged in" flag)
uint32_t g_tachi_loggedin_offset;
uint16_t g_stage_offset = 0x504; // 0x508 in later games, overwritten when setting up the patches
uint32_t g_need_conf_load = 1;
uint32_t g_hidden_is_offset = 0; // mask "hidden" value behavior in score send

const char *g_clear_medal[] =
{
    "none", //shouldn't happen
    "failedCircle",     //1
    "failedDiamond",    //2
    "failedStar",       //3
    "easyClear",        //4
    "clearCircle",      //5
    "clearDiamond",     //6
    "clearStar",        //7
    "fullComboCircle",  //8
    "fullComboDiamond", //9
    "fullComboStar",    //A
    "perfect"           //B
};

const char *g_random_type[] =
{
    "NONRAN",
    "MIRROR",
    "RANDOM",
    "S-RANDOM"
};

const char *g_gauge_type[] =
{
    "NORMAL",
    "EASY",
    "HARD",
    "DANGER"
};

const char *g_difficulty[] =
{
    "Easy",
    "Normal",
    "Hyper",
    "EX"
};

/* curl callback function */
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  char *ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);
  if(!ptr) {
    /* out of memory! */
    CURL_PRINT("[tachi] (WriteMemoryCallback) not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

rival_info_t g_rival_data = {.no=0,.g_pm_id={'1','2','3','4','1','2','3','4','1','2','3','4','\0'},.name={'Y','T','B','N','\0'},.chara_num=5555,.is_open=1,.padding=0};
rival_info_t *g_rival_current = NULL;

// make g_rival_current point to the next entry so that the asm hook can inject it
// return false when the last rival has been retrieved
bool get_next_rival(){
    static uint32_t idx = 0;
    if ( g_rival_current == NULL ) // need reinit
    {
        idx = 0;
    }
    if ( idx >= g_rival_count )
    {
        //end reached
        g_rival_current = NULL;
        idx = 0;
    }
    sprintf( g_rival_data.name, "%.*s", 12, g_rivals[idx]);

    g_rival_current = &g_rival_data;
    idx++; //for next call
    return true;
}


// make g_rival_score_current point to the next hashmap entry so that the asm hook can inject it
// return false when the last score has been retrieved
bool get_next_score(){
    static int idx = 0;
    if ( g_rival_score_current == NULL ) // need reinit
    {
        idx = 0;
    }

    while ( g_score_hashtable->fields[idx].state != CHASH_FILLED )
    {
        idx++;
        if ( idx >= g_score_hashtable->capacity )
        {
            //end reached
            chash_free(g_score_hashtable, SCORE_TABLE);
            g_rival_score_current = NULL;
            return false;
        }
    }

    g_rival_score_current = g_score_hashtable->fields[idx].v;
    idx++; //for next call
    return true;
}

/* debug function, not used by the hook */
void iterate_hashtable(){
    g_rival_score_current = NULL;
    int count = 0;
    while ( get_next_score() )
    {
        printf("music_num = %u\nsheet_num = %u\nscore = %u\nclear_type = %u\nclear_rank = %u\n\n----\n", g_rival_score_current->music_num, g_rival_score_current->sheet_num, g_rival_score_current->score, g_rival_score_current->clear_type, g_rival_score_current->clear_rank);
        count++;
    }
    fprintf(stderr,"retrieved %d values in hashmap\n", count);
}

// parse curl-retrieved json and build the score hashmap
int parse_rival_pbs(){
    int r = 0;
    (void)r;
    jsmn_parser parser;
    jsmntok_t *toks = NULL;
    jsmnf_pair *f;

    unsigned num_tokens = 0;
    uint32_t score_entries = 0;
    uint32_t chart_entries = 0;

    char * const pbs_path[] = { (char*)"body", (char*)"pbs" };
    char * const chartID_path[] = { (char*)"chartID" };
    char * const score_path[] = { (char*)"scoreData", (char*)"score" };
    char * const clearMedal_path[] = { (char*)"scoreData", (char*)"enumIndexes", (char*)"clearMedal" };
    char * const grade_path[] = { (char*)"scoreData", (char*)"enumIndexes", (char*)"grade" };

    char * const charts_path[] = {(char*) "body", (char*)"charts" };
    char * const inGameID_path[] = { (char*)"data", (char*)"inGameID" };
    char * const difficulty_path[] = { (char*)"difficulty" };
    char *chartID_str = NULL;

    char* js = g_curl_data.memory;
    int jslen = g_curl_data.size;

    if ( js == NULL )
    {
        CURL_PRINT("[tachi] (parse_rival_pbs) no curl data to parse\n");
        return false;
    }
    jsmn_init(&parser);
    r = jsmn_parse_auto(&parser, js, jslen, &toks, &num_tokens);

    /* populate jsmnf_pairs with the jsmn tokens */
    jsmnf_loader loader;
    jsmnf_pair *pairs = NULL;
    unsigned num_pairs = 0;

    jsmnf_init(&loader);
    r = jsmnf_load_auto(&loader, js, toks, num_tokens, &pairs, &num_pairs);

    /* prepare hashtable */
    if ( g_score_hashtable != NULL )
    {
            chash_free(g_score_hashtable, SCORE_TABLE);
    }
    g_rival_score_current = NULL;
    g_rival_score_count = 0;
    g_score_hashtable = chash_init(g_score_hashtable, SCORE_TABLE);

    /* first part, pbs */
    if ((f = jsmnf_find_path(pairs, js, pbs_path, sizeof(pbs_path) / sizeof *pbs_path)))
    {
        score_entries = f->size;
        for (int i = 0; i < f->size; ++i)
        {
            rival_score_t *rival_score = (rival_score_t *)malloc(sizeof(rival_score_t));

            jsmnf_pair *g = &f->fields[i];
            if ((g = jsmnf_find_path(g, js, clearMedal_path, sizeof(clearMedal_path) / sizeof *clearMedal_path))) {
                rival_score->clear_type = strtol(js + g->v.pos, NULL, 10) + 1;
            }
            g = &f->fields[i];
            if ((g = jsmnf_find_path(g, js, grade_path, sizeof(grade_path) / sizeof *grade_path))) {
                rival_score->clear_rank = strtol(js + g->v.pos, NULL, 10) + 1;

            }
            g = &f->fields[i];
            if ((g = jsmnf_find_path(g, js, score_path, sizeof(score_path) / sizeof *score_path))) {
                rival_score->score = strtol(js + g->v.pos, NULL, 10);
            }
            g = &f->fields[i];
            if ((g = jsmnf_find_path(g, js, chartID_path, sizeof(chartID_path) / sizeof *chartID_path))) {
                chartID_str = (char*)malloc((int)g->v.len+1);
                sprintf(chartID_str, "%.*s", (int)g->v.len, js + g->v.pos);
            }
            chash_assign(g_score_hashtable, chartID_str, rival_score, SCORE_TABLE);
        }
    }

    /* find chartID in body.charts.n.chartID to retrieve body.charts.n.data.inGameID and body.charts.n.difficulty */
    if ((f = jsmnf_find_path(pairs, js, charts_path, sizeof(charts_path) / sizeof *charts_path))) {
        chart_entries = f->size;
        if ( score_entries != chart_entries )
            CURL_PRINT("WARNING: found %u score entries and %u chart entries\n", score_entries, chart_entries);

        int entries=0;
        for (int i = 0; i < f->size; ++i)
        {
            jsmnf_pair *g = &f->fields[i];
            if ((g = jsmnf_find_path(g, js, chartID_path, sizeof(chartID_path) / sizeof *chartID_path))) {
                chartID_str = (char*)malloc((int)g->v.len+1);
                sprintf(chartID_str, "%.*s", (int)g->v.len, js + g->v.pos);
            }

            rival_score_t *value;
            chash_lookup(g_score_hashtable, chartID_str, value, SCORE_TABLE);

            g = &f->fields[i];
            if ((g = jsmnf_find_path(g, js, inGameID_path, sizeof(inGameID_path) / sizeof *inGameID_path))) {
                value->music_num = strtol(js + g->v.pos, NULL, 10);
            }
            g = &f->fields[i];
            if ((g = jsmnf_find_path(g, js, difficulty_path, sizeof(difficulty_path) / sizeof *difficulty_path)))
            {
                /* filter on 2nd char since E is common to Easy and EX */
                switch ( (js + g->v.pos)[1] )
                {
                    case 'o': //Normal
                    case 'O':
                        value->sheet_num = 1;
                        break;
                    case 'y': //Hyper
                    case 'Y':
                        value->sheet_num = 2;
                        break;
                    case 'X': //EX
                    case 'x':
                        value->sheet_num = 3;
                        break;
                    case 'a': //Easy
                    case 'A':
                    default:
                        value->sheet_num = 0;
                        break;
                }
            }
            entries++;
        }
        CURL_PRINT("[%s] parsed %d scores\n", g_rivals[g_rival_current_idx], entries);
        g_rival_score_count = entries;
    }

    return true;
}

/* curl callback */
static size_t parse_rivals(void *ptr, size_t size, size_t nmemb, void *data)
{
    char *match = strstr((char*)ptr, "\"username\":\"");
    while (match!= NULL && g_rival_count < MAX_RIVALS)
    {
        char *match_username = match+strlen("\"username\":\"");
        strncpy( g_rivals[g_rival_count], match_username, strchr(match_username, '"')-match_username);
        g_rivals[g_rival_count][strchr(match_username, '"')-match_username] = '\0';
        ptr = match+1;
        match = strstr((char*)ptr, "\"username\":\"");
        g_rival_count++;
    };
    if (match != NULL)
        CURL_PRINT("WARNING: pop'n only supports %d rivals, skipping the rest\n", MAX_RIVALS);

    return size*nmemb;
}

bool tachi_get_status()
{
    CURL *curl = _curl_easy_init();
    if(!curl)
        return false;

    _curl_easy_setopt(curl, CURLOPT_URL, g_status_url);
    _curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

    _curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    _curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&g_curl_data);
    CURLcode res = _curl_easy_perform(curl);
    _curl_easy_cleanup(curl);
    return (res == CURLE_OK);
}

static char *load_whole_file(char *filename, long *p_fsize)
{
    long fsize;
    char *str;
    FILE *f;

    f = fopen(filename, "rb");
    if ( f == NULL )
    {
        CURL_PRINT("[tachi] (load_whole_file) cannot open file\n");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    str = (char*) malloc(fsize + 1);

    str[fsize] = '\0';
    fread(str, 1, fsize, f);

    fclose(f);

    if (p_fsize) *p_fsize = fsize;

    return str;
}

bool load_conf(char *friendid)
{
    char filename[30];
    FILE *conf = NULL;
    if ( strlen(friendid) != 0 )
    {
        sprintf(filename, "_tachi.%s.conf", friendid);
        conf = fopen(filename, "r");
    }

    if ( conf == NULL )
    {
        sprintf(filename, "_tachi.default.conf");
        conf = fopen(filename, "r");
    }

    if ( conf == NULL )
    {
        CURL_PRINT("[tachi] no config file found\n");
        return false;
    }

    CURL_PRINT("[tachi] using file %s\n", filename);

    int r;
    jsmn_parser parser;
    jsmntok_t *toks = NULL;
    unsigned num_tokens = 0;

    char* js = load_whole_file(filename, NULL);
    if ( js == NULL )
    {
        CURL_PRINT("[tachi] cannot open config file\n");
        return false;
    }
    jsmn_init(&parser);
    r = jsmn_parse_auto(&parser, js, strlen(js), &toks, &num_tokens);
    if (r <= 0)
    {
        CURL_PRINT("[tachi] couldn't parse config file\n");
        return false;
    }

    // populate jsmnf_pairs with the jsmn tokens
    jsmnf_loader loader;
    jsmnf_pair *pairs = NULL;
    unsigned num_pairs = 0;

    jsmnf_init(&loader);
    r = jsmnf_load_auto(&loader, js, toks, num_tokens, &pairs, &num_pairs);
    if (r <= 0)
    {
        CURL_PRINT("[tachi] couldn't parse config file\n");
        return false;
    }

    char base[64];
    bool has_api = false;
    bool missing_param = false;
    jsmnf_pair *f;

    if ((f = jsmnf_find(pairs, js, "key", strlen("key")))) {
        has_api = true;
        sprintf(g_api_key, "%.*s", (int)f->v.len, js + f->v.pos);
    } else {
        missing_param = true;
    }
    if ((f = jsmnf_find(pairs, js, "base", strlen("base")))) {
        sprintf(base, "%.*s", (int)f->v.len, js + f->v.pos);
    } else {
        CURL_PRINT("[tachi] 'base' param not found\n");
        missing_param = true;
    }

    if ( missing_param )
    {
        if ( has_api && (f = jsmnf_find(pairs, js, "url", strlen("url")))) {
            sprintf(g_import_url, "%.*s", (int)f->v.len, js + f->v.pos);
        } else {
            CURL_PRINT("[tachi] 'url' param not found\n");
            CURL_PRINT("[tachi] missing field in config file. please download a proper config file for your tachi account.\n");
            return false;
        }

        int len = strstr(g_import_url+strlen("https://"), "/") - g_import_url;
        sprintf(base, "%.*s", len, g_import_url);
        CURL_PRINT("[tachi] using legacy mode (extracted base '%s' from 'url' param), please update your config file.\n", base);
    }

    sprintf(g_import_url, "%s%s", base, "/ir/direct-manual/import");
    sprintf(g_status_url, "%s%s", base, "/api/v1/status");
    sprintf(g_users_url_prefix, "%s%s", base, "/api/v1/users");
    sprintf(g_rivals_url, "%s%s", base, "/api/v1/users/me/games/popn/9B/rivals");
    sprintf(g_pbs_url_suffix, "%s", "/games/popn/9B/pbs/all");

    return true;
}

bool load_profile_if_needed()
{
    if ( g_need_conf_load )
    {
        /* retrieve friend ID and open config file */
        char *friendid = (char*)(*(uint32_t*)g_playerdata_zone_addr+0x08);
        if (!load_conf(friendid))
        {
            CURL_PRINT("[tachi] failed to parse config\n");
            return false;
        }

        if ( g_curl_data.memory == NULL )
        {
            g_curl_data.memory = (char*)malloc(1);  /* grown as needed by realloc above */
            g_curl_data.size = 0;
        }

        //CURL_PRINT("[profile info]\nfriend_id = %s\napi_key = %s\nimport url = %s\nstatus url = %s\nrivals url = %s\npbs url = %s/%s%s\n", friendid, g_api_key, g_import_url, g_status_url, g_rivals_url, g_users_url_prefix, "<username>", g_pbs_url_suffix);

        if (!tachi_get_status() || g_curl_data.memory == NULL )
        {
            CURL_PRINT("[tachi] ERROR: status check request failed (server down?)\n");
            return false;
        }

        if ( strstr(g_curl_data.memory, "\"success\":true") != NULL )
        {
            CURL_PRINT("[tachi] status check: OK\n");
        }
        else
        {
            CURL_PRINT("[tachi] WARNING: status check failed (got success=false)\n");
            CURL_PRINT("%s\n", g_curl_data.memory);
        }
        g_need_conf_load = 0;

        if ( g_curl_data.memory != NULL )
        {
            free(g_curl_data.memory);
            g_curl_data.memory = NULL;
            g_curl_data.size = 0;
        }
    }

    return true;
}

bool tachi_send_score()
{
    char score_json[1024];

    /* retrieve song id memory zone */
    uint8_t stage_number = *(uint8_t*)((uint8_t*)g_song_info_zone+0x0E);
    uint32_t *song_info = (uint32_t*)((uint32_t)g_song_info_zone+g_stage_offset*stage_number);
    g_song_info = (song_info_t*)((uint8_t*)song_info+0x20);
    uint16_t song_id = g_song_info->music_id;

    uint16_t attract_marker = *(uint16_t*)((uint8_t*)g_song_info_zone+0x1C);
    if ( attract_marker == 0x0101 )
    {
        CURL_PRINT("[tachi_send_score] skipping attract mode demo score\n");
        return false;
    }

    if ( song_id > 3400 || ( g_skip_omni && is_a_custom(song_id) ) ) // only skip customs, not proper omni
    {
        CURL_PRINT("[tachi_send_score] skipping %s songid %u\n", g_skip_omni ? "omni/custom" : "custom", song_id);
        return false;
    }

    /* prepare POST data */
    if ( !load_profile_if_needed() )
        return false;

    uint8_t diff_idx = g_song_info->sheet_id;
    const char *diff = g_difficulty[diff_idx];
    double gauge = g_score_info->gauge * 100. / 1023.;
    double hispeed = (g_song_info->hispeed)/10.;
    uint64_t curr_time = (uint64_t)time(NULL)*1000;

    sprintf(score_json, "{ \
        \"meta\": {\
        \"game\": \"popn\",\
        \"playtype\": \"9B\",\
        \"service\": \"popnhax\"\
        },\
        \"scores\":\
        [{\
            \"score\": %u,\
            \"clearMedal\": \"%s\",\
            \"matchType\": \"inGameID\",\
            \"identifier\": \"%u\",\
            \"timeAchieved\": %llu,\
            \"difficulty\": \"%s\",\
            \"optional\": {\
            \"fast\": %u,\
            \"slow\": %u,\
            \"maxCombo\": %u,\
            \"gauge\": %f\
            },\
            \"judgements\": {\
                \"cool\": %u,\
                \"great\": %u,\
                \"good\": %u,\
                \"bad\": %u\
            },\
            \"scoreMeta\": {\
                \"hiSpeed\": %f,\
                \"hidden\": %d,\
                \"sudden\": %d,\
                \"random\": \"%s\",\
                \"gauge\": \"%s\"\
            }\
        }]\
    }", g_score_info->score, g_clear_medal[g_score_info->clear_type], song_id, curr_time, diff, g_score_info->nb_fast, g_score_info->nb_slow, g_score_info->max_combo, gauge, g_score_info->nb_cool, g_score_info->nb_great, g_score_info->nb_good, g_score_info->nb_bad, hispeed, (g_hidden_is_offset) ? 0 : g_song_info->hidden_value, g_song_info->sudden_value, g_random_type[g_song_info->random], g_gauge_type[g_song_info->gauge_type]);

    /* curl request */
    CURL *curl = _curl_easy_init();

    struct curl_slist *list = NULL;

    char auth_header[128];
    sprintf(auth_header, "Authorization: Bearer %s", g_api_key);

    CURL_PRINT("\n\nscorejson data is \n%s\n", score_json);

    if(curl)
    {
        _curl_easy_setopt(curl, CURLOPT_URL, g_import_url);
        _curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

        list = _curl_slist_append(list, auth_header);
        list = _curl_slist_append(list, "Content-Type: application/json");
        _curl_easy_setopt(curl, CURLOPT_POSTFIELDS, score_json);
        _curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

        _curl_easy_perform(curl);
        _curl_slist_free_all(list);
        _curl_easy_cleanup(curl);
    }

    return false;
}

bool tachi_get_rival_scores(int idx)
{
    if ( !load_profile_if_needed() )
        return false;

    CURL *curl = _curl_easy_init();
    if(!curl)
        return false;

    struct curl_slist *list = NULL;

    char auth_header[128];
    char rival_url[128];
    sprintf(auth_header, "Authorization: Bearer %s", g_api_key);

    sprintf(rival_url, "%s/%s%s", g_users_url_prefix, g_rivals[idx], g_pbs_url_suffix);

    if ( g_curl_data.memory == NULL )
    {
        g_curl_data.memory = (char*)malloc(1);  /* grown as needed by realloc above */
        g_curl_data.size = 0;
    }

    _curl_easy_setopt(curl, CURLOPT_URL, rival_url);
    _curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

    list = _curl_slist_append(list, auth_header);
    _curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    _curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    _curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&g_curl_data);

    CURLcode res = _curl_easy_perform(curl);
    _curl_easy_cleanup(curl);

    return (res == CURLE_OK);
}

void tachi_get_rival_scores_and_parse()
{

    if (tachi_get_rival_scores(g_rival_current_idx))
    {
        parse_rival_pbs();
        if ( g_curl_data.memory != NULL )
        {
            free(g_curl_data.memory);
            g_curl_data.memory = NULL;
            g_curl_data.size = 0;
       }
    }

    g_rival_current_idx++;
}

bool tachi_get_rivals()
{
    /* reset data for the hook that is to come */
    g_rival_count = 0;
    g_rival_current = NULL;
    g_rival_current_idx = 0;

    if ( !load_profile_if_needed() )
        return false;

    CURL *curl = _curl_easy_init();
    if(!curl)
        return false;

    struct curl_slist *list = NULL;
    char auth_header[128];
    sprintf(auth_header, "Authorization: Bearer %s", g_api_key);

    _curl_easy_setopt(curl, CURLOPT_URL, g_rivals_url);
    _curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

    list = _curl_slist_append(list, auth_header);
    _curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    _curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, parse_rivals);

    CURLcode res = _curl_easy_perform(curl);
    _curl_easy_cleanup(curl);

    if ( res != CURLE_OK )
        CURL_PRINT("[tachi] WARNING: get rivals request failed\n");

    CURL_PRINT("found %d rivals\n", g_rival_count);
    for (uint32_t j=0; j < g_rival_count; j++)
    {
        CURL_PRINT("%s\n", g_rivals[j]);
    }

    return g_rival_count;
}

extern bool (*popn22_is_normal_mode)();

void (*real_medal_commit)();
void hook_medal_commit()
{
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");

    __asm("push eax\n");
    __asm("call %0"::"a"(popn22_is_normal_mode));
    __asm("test al,al");
    __asm("pop eax\n");
    __asm("je skip_score_send");

    __asm("mov [_g_score_info], edi\n");
    __asm("mov [_g_song_info_zone], eax\n");
    __asm("call %P0" : : "i"(tachi_send_score));

    __asm("skip_score_send:");
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");

    real_medal_commit();
}

void (*real_end_of_credit)();
void hook_end_of_credit()
{
    __asm("mov %0, 0x01\n":"=m"(g_need_conf_load): :);
    real_end_of_credit();
}

void (*real_mode_select)();
void hook_mode_select_rival_inject()
{
    // if g_need_conf_load is 0 then it means we already injected rivals this credit, leave
    __asm("cmp dword ptr [_g_need_conf_load], 0\n");
    __asm("je leave_rival_inject\n");

    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("call %P0" : : "i"(tachi_get_rivals));
    __asm("cmp eax, 0\n");
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");
    __asm("je leave_rival_inject\n");

    /* fake login to prevent rival handling bypass */
    __asm("mov ecx, dword ptr [_g_playerdata_zone_addr]\n");
    __asm("mov edx, [ecx]\n");
    __asm("add edx, [_g_tachi_loggedin_offset]\n"); //offset where result screen is checking to decide if the favorite option should be displayed/handled
    __asm("mov ecx, [edx]\n");
    __asm("cmp ecx, 0\n");
    __asm("jne skip_fake_login_tachi\n");
    __asm("mov dword ptr [edx], 0xFF000001\n");
    __asm("sub edx, [_g_tachi_loggedin_offset]\n");
    __asm("add edx, 8\n"); //back to popn friendid offset
    __asm("mov dword ptr [edx], 0x61666564\n"); // "defa"
    __asm("add edx, 0x04\n");
    __asm("mov dword ptr [edx], 0x00746C75\n"); // "ult"
    __asm("skip_fake_login_tachi:\n");

    /* reset rival count in player data */
    __asm("mov eax, [_g_playerdata_zone_addr]\n");
    __asm("mov eax, [eax]\n");
    __asm("add eax, 0x44\n");
    __asm("mov byte ptr [eax], 0\n");

    /* INJECT RIVALS */
    __asm("push edi\n"); //edi is current rival
    __asm("xor edi, edi\n");

    __asm("inject_rival:\n");
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("call %P0" : : "i"(get_next_rival));
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");

    /* write rival_info_t data */
    __asm("call [_get_rivals_ptr]\n"); //retrieve &rivals[i]
    __asm("push ebx\n");
    __asm("push edx\n");
    __asm("mov edx, [_g_rival_entry_size]\n");
    __asm("mov ebx, edi\n");
    __asm("imul ebx, edx\n");
    __asm("lea eax, [ebx+eax+4]\n");
    __asm("pop edx\n");
    __asm("pop ebx\n");

    __asm("push ebx\n");
    __asm("xor ebx, ebx\n");
    __asm("write_rival_data:\n");
    __asm("lea edx, [_g_rival_data+ebx*4]\n");
    __asm("mov edx, [edx]\n");
    __asm("mov [eax], edx\n");
    __asm("add eax, 4\n");
    __asm("inc ebx\n");
    __asm("cmp ebx, 8\n");
    __asm("jb write_rival_data\n");
    __asm("pop ebx\n");

    /* increment rival count */
    __asm("mov eax, [_g_playerdata_zone_addr]\n");
    __asm("mov eax, [eax]\n");
    __asm("add eax, 0x44\n");
    __asm("inc byte ptr [eax]\n"); // increment rival count in playerdata
    __asm("call [_get_rivals_ptr]\n");
    __asm("add eax, [_g_rival_count_offset]\n");
    __asm("inc byte ptr [eax]\n"); // increment rival count in rival score zone

    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("call %P0" : : "i"(tachi_get_rival_scores_and_parse));
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");

    /* write score data */
    __asm("push esi\n");
    __asm("xor esi, esi\n");
    __asm("write_score_data:\n");
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("call %P0" : : "i"(get_next_score));
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");
    __asm("cmp dword ptr [_g_rival_score_current], 0\n"); // should not happen but we don't want the game to crash if it does
    __asm("je score_inject_next_rival\n");

    /* retrieve &rivals[i] */
    __asm("call [_get_rivals_ptr]\n"); //retrieve &rivals[i]
    __asm("push ebx\n");
    __asm("push edx\n");
    __asm("mov edx, [_g_rival_entry_size]\n");
    __asm("mov ebx, edi\n");
    __asm("imul ebx, edx\n");
    __asm("lea eax, [ebx+eax+4]\n");
    __asm("pop edx\n");
    __asm("pop ebx\n");

    __asm("mov edx, [_g_rival_score_current]\n");
    __asm("call [_write_rival_score]\n"); //will clobber eax
    __asm("inc esi\n");
    __asm("cmp esi, [_g_rival_score_count]\n");
    __asm("jb write_score_data\n");
    __asm("mov dword ptr [_g_rival_score_current], 0\n");
    __asm("score_inject_next_rival:\n");
    __asm("pop esi\n");

    __asm("inc edi\n");
    __asm("cmp edi, [_g_rival_count]\n");
    __asm("jb inject_rival\n");
    __asm("pop edi\n");

    /* fake not logged in return value to bypass real rivals retrieval */
    __asm("xor eax, eax\n");

    __asm("leave_rival_inject:\n");
    real_mode_select();
}

bool patch_tachi_rivals(const char *dllFilename, bool scorehook)
{
    DWORD dllSize = 0;
    char *data = getDllData(dllFilename, &dllSize);

#if DEBUG_CURL == 1
    if ( g_curl_log_fp == NULL )
        g_curl_log_fp = fopen("popnhax_curl.log", "w");
#endif

    if ( !load_libcurl())
    {
        LOG("popnhax: tachi rivals: cannot load libcurl\n");
        g_libcurl = NULL;
        return false;
    }

    /* retrieve get_rivals_ptr() */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x0F\xB6\x8E\x38\x02\x00\x00", 7, 0);
        if (pattern_offset == -1)
        {
            pattern_offset = _search(data, dllSize, "\x0F\xB6\x89\x38\x02\x00\x00", 7, 0); // usaneko/peace
            if (pattern_offset == -1) {
                LOG("popnhax: tachi rivals: cannot find get_rivals_ptr function\n");
                return false;
            }
        }

        int64_t fun_rel = *(int32_t *)(data + pattern_offset - 0x04 ); // function call is just before our pattern
        get_rivals_ptr = (uint32_t*(*)())((int64_t)data + pattern_offset + fun_rel);
    }

    /* retrieve rival_entry_size */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x8B\xE5\x5D\xC2\x08\x00\x69\xDB", 8, 0);
        if (pattern_offset == -1)
        {
            LOG("popnhax: tachi rivals: cannot find rival entry size\n");
            return false;
        }

        g_rival_entry_size = *(int32_t *)(data + pattern_offset + 0x08 );
    }

    /* retrieve offset where g_rival_count should be written */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x66\x83\xF8\xFF\x75\x07\x66\xFF\x87", 9, 0);
        if (pattern_offset == -1)
        {
            LOG("popnhax: tachi rivals: cannot find rival entry size\n");
            return false;
        }

        g_rival_count_offset = *(int32_t *)(data + pattern_offset + 0x09 );
    }

    /* retrieve write_rival_score */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x66\x8B\x0A\x50\x8A\x42\x04\xE8", 8, 0);
        if (pattern_offset == -1)
        {
            LOG("popnhax: tachi rivals: cannot find rival entry size\n");
            return false;
        }

        write_rival_score = (void(*)())(data + pattern_offset);
    }

    /* hook credit end to reset "need to load conf" marker, if scorehook didn't already install it */
    if ( !scorehook ) {
        { // same as in local favorites patch / score challenge / score hook
            int64_t pattern_offset = _search(data, dllSize, "\x8B\x01\x8B\x50\x14\xFF\xE2\xC3\xCC\xCC\xCC\xCC", 12, 0);
            if (pattern_offset == -1) {
                LOG("popnhax: tachi rivals: cannot find check if logged function\n");
                return false;
            }
            g_playerdata_zone_addr = (*(uint32_t *)(data + pattern_offset + 0x25));
        }

        {
            int64_t pattern_offset = _search(data, dllSize, "\x33\xC0\x89\x87\x18\x02\x00\x00\x89", 9, 0);
            if (pattern_offset == -1) {
                pattern_offset = _search(data, dllSize, "\x33\xC0\x89\x86\x18\x02\x00\x00\x89", 9, 0); // usaneko/peace
                if (pattern_offset == -1) {
                    LOG("popnhax: tachi rivals: cannot find end of credit check if logged function\n");
                    return false;
                }
            }
            uint64_t patch_addr = (int64_t)data + pattern_offset;
            _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_end_of_credit,
                         (void **)&real_end_of_credit);
        }

    }

    /* hook after mode select logged in check */

    //retrieve logged in status offset from playerdata
    {
        int64_t pattern_offset = _search(data, dllSize, "\xBF\x07\x00\x00\x00\xC6\x85", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: tachi rivals: cannot find result screen function\n");
            return false;
        }
            uint64_t function_call_addr = (int64_t)(data + pattern_offset + 0x1D);
            uint32_t function_offset = *((uint32_t*)(function_call_addr +0x01));
            uint64_t function_addr = function_call_addr+5+function_offset;
            g_tachi_loggedin_offset = *(uint32_t*)(function_addr+2);
            //LOG("LOGGED IN OFFSET IS %x\n",g_tachi_loggedin_offset); // 0x1A5 for popn27-, 0x22D for popn28
    }
    {
        int64_t pattern_offset = _search(data, dllSize, "\x8B\xE5\x5D\xC3\x8B\xC6\xE8", 7, 0);

        if (pattern_offset == -1)
        {
            LOG("popnhax: tachi rivals: cannot retrieve mode select logged in check\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 11;
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_mode_select_rival_inject,
                      (void **)&real_mode_select);
    }

    LOG("popnhax: tachi rival injection enabled\n");
    return true;
}

bool patch_tachi_scorehook(const char *dllFilename, bool pfree, bool hidden_is_offset, bool skip_omni)
{
    DWORD dllSize = 0;
    char *data = getDllData(dllFilename, &dllSize);

#if DEBUG_CURL == 1
    if ( g_curl_log_fp == NULL )
        g_curl_log_fp = fopen("popnhax_curl.log", "w");
#endif

    if ( !load_libcurl())
    {
        LOG("popnhax: tachi scorehook: cannot load libcurl\n");
        g_libcurl = NULL;
        return false;
    }

    /* do not send a "hidden" value if it really is an offset */
    if (hidden_is_offset)
        g_hidden_is_offset = 1;

    if (skip_omni)
        g_skip_omni = true;

    /* retrieve song struct size */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x8B\x74\x24\x14\x0F\xB6\xC0", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: tachi scorehook: cannot retrieve score zone offset computation function\n");
            return false;
        }

        g_stage_offset = *(uint16_t *) ((int64_t)data + pattern_offset + 12);
        //LOG("popnhax: tachi scorehook: stage offset size 0x%0x\n", g_stage_offset);
    }

    /* player data address (for friendid retrieval), same as local favorite patch */
    {
        //this is the same function used in score challenge patch, checking if we're logged in... but now we just directly retrieve the address
        int64_t pattern_offset = _search(data, dllSize, "\x8B\x01\x8B\x50\x14\xFF\xE2\xC3\xCC\xCC\xCC\xCC", 12, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: tachi scorehook: cannot find check if logged function\n");
            return false;
        }

        g_playerdata_zone_addr = (*(uint32_t *)(data + pattern_offset + 0x25));
    }
    /* hook credit end to reset "need to load conf" marker */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x33\xC0\x89\x87\x18\x02\x00\x00\x89", 9, 0);
        if (pattern_offset == -1) {
            pattern_offset = _search(data, dllSize, "\x33\xC0\x89\x86\x18\x02\x00\x00\x89", 9, 0); // usaneko/peace
            if (pattern_offset == -1) {
            LOG("popnhax: tachi scorehook: cannot find end of credit check if logged function\n");
            return false;
            }
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_end_of_credit,
                     (void **)&real_end_of_credit);
    }

    /* hook medal calculation */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x89\x84\x24\x68\x02\x00\x00\x8D\x44\x24\x20", 11, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: tachi hook: cannot retrieve medal handling function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x13;
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_medal_commit,
                      (void **)&real_medal_commit);
    }

    if (!pfree) //pfree already retrieves this info
    {
        int64_t pattern_offset = _search(data, dllSize, "\x83\xC4\x0C\x33\xC0\xC3\xCC\xCC\xCC\xCC\xE8", 11, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: tachi score hook: cannot find is_normal_mode function, fallback to best effort (active in all modes)\n");
        }
        else
        {
            popn22_is_normal_mode = (bool(*)()) (data + pattern_offset + 0x0A);
        }
    }

    LOG("popnhax: tachi score hook enabled%s\n", g_skip_omni ? " (omnimix charts excluded)" : "");
    return true;
}