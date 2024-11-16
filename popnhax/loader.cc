#include <cstdio>
#include <cstring>
#include <io.h>
#include <windows.h>

#include "imports/avs.h"
#include "util/bst.h"
#include "util/patch.h"
#include "util/log.h"
#include "xmlhelper.h"
#include "custom_categs.h"

#include "tableinfo.h"
#include "loader.h"
#include <algorithm>
#include <map>
#include <vector>

#include "SearchFile.h"

#define RET_COMPARE_INDEX(ptr, val, idx)                                                           \
    if (strcmp((char *)ptr, val) == 0) {                                                           \
        return idx;                                                                                \
    }

struct property *load_prop_file(const char *filename);

bool file_exists(const char *filename);

uint8_t *find_string(uint8_t *input, size_t len);
uint8_t *add_string(uint8_t *input);

void add_chart_type_flag(uint32_t cur_idx, int8_t flag);
uint32_t get_lapis_shape_id(uint8_t *lapis_shape_ptr);
uint32_t get_lapis_color_id(uint8_t *lapis_color_ptr);
uint16_t get_chara_idx(const uint8_t *chara_id);
fontstyle_entry *get_fontstyle(int32_t cur_idx);
character_entry *get_chara(int32_t cur_idx);
music_entry *get_music(int32_t cur_idx);
chart_entry *get_chart(int32_t cur_idx);

uint32_t add_style(int32_t cur_idx, uint32_t fontface, uint32_t color, uint32_t height,
                   uint32_t width);

uint32_t add_flavor(int32_t cur_idx, uint8_t *phrase1, uint8_t *phrase2, uint8_t *phrase3,
                    uint8_t *phrase4, uint8_t *phrase5, uint8_t *phrase6, uint8_t *birthday,
                    uint8_t chara1_birth_month, uint8_t chara2_birth_month,
                    uint8_t chara3_birth_month, uint8_t chara1_birth_date,
                    uint8_t chara2_birth_date, uint8_t chara3_birth_date, uint16_t style1,
                    bool style2_flag, uint16_t style3, uint32_t fontstyle_fontface,
                    uint32_t fontstyle_color, uint32_t fontstyle_height, uint32_t fontstyle_width);

uint32_t add_chara(int32_t cur_idx, uint8_t *chara_id_ptr, uint32_t flags, uint8_t *folder_ptr,
                   uint8_t *gg_ptr, uint8_t *cs_ptr, uint8_t *icon1_ptr, uint8_t *icon2_ptr,
                   uint16_t chara_xw, uint16_t chara_yh, uint32_t display_flags, int16_t flavor_idx,
                   uint8_t chara_variation_num, uint8_t *sort_name_ptr, uint8_t *disp_name_ptr,
                   uint32_t file_type, uint32_t lapis_shape, uint8_t lapis_color, uint8_t *ha_ptr,
                   uint8_t *catchtext_ptr, int16_t win2_trigger, uint32_t game_version);

uint32_t add_music(int32_t cur_idx, uint8_t *fw_genre_ptr, uint8_t *fw_title_ptr,
                   uint8_t *fw_artist_ptr, uint8_t *genre_ptr, uint8_t *title_ptr,
                   uint8_t *artist_ptr, uint16_t chara1, uint16_t chara2, uint32_t mask,
                   uint32_t folder, uint32_t cs_version, uint32_t categories, uint8_t *diffs,
                   uint16_t *charts, uint8_t *ha_ptr, uint32_t chara_x, uint32_t chara_y,
                   uint16_t *unk1, uint16_t *display_bpm, uint8_t *hold_flags, bool allow_resize);

uint32_t add_chart(uint32_t cur_idx, uint8_t *folder, uint8_t *filename, int32_t audio_param1,
                   int32_t audio_param2, int32_t audio_param3, int32_t audio_param4,
                   uint32_t file_type, uint16_t used_keys, bool override_idx);

void parse_charadb(const char *input_filename, const char *target);
void parse_musicdb(const char *input_filename, const char *target, struct popnhax_config *config);

std::map<uint32_t, int8_t> chart_type_overrides;

uint32_t flavor_limit = 0;

uint8_t *filler_str = add_string((uint8_t *)"\x81\x5d\x00");

const uint32_t string_table_growth_size = 0x100000;
uint32_t string_table_size = 0;
uint32_t string_table_idx = 0;
uint8_t *string_table = NULL;

const uint32_t fontstyle_table_growth_size = 50;
uint32_t fontstyle_table_size = 0;
int32_t fontmax_style_id = -1;
uint8_t *fontstyle_table = NULL;

const uint32_t flavor_table_growth_size = 50;
uint32_t flavor_table_size = 0;
int32_t max_flavor_id = -1;
uint8_t *flavor_table = NULL;

std::map<uint32_t, bool> used_chara_id;
const uint32_t chara_table_growth_size = 50;
uint32_t chara_table_size = 0;
int32_t max_chara_id = -1;
uint8_t *chara_table = NULL;
static uint8_t chara_table_nullptr[1] = {};

std::map<uint32_t, bool> used_music_id;
const uint32_t music_table_growth_size = 50;
uint32_t music_table_size = 0;
int32_t max_music_id = -1;
uint8_t *music_table = NULL;

const uint32_t chart_table_growth_size = 50;
uint32_t chart_table_size = 0;
int32_t max_chart_id = -1;
uint8_t *chart_table = NULL;

bool file_exists(const char *filename) {
    FILE *test = fopen(filename, "rb");

    if (!test) {
        return false;
    }

    fclose(test);
    return true;
}

uint8_t *find_string(uint8_t *input, size_t len) {
    for (uint32_t i = 0; i < string_table_size - 1; i++) {
        if (memcmp(string_table + i, input, len) == 0 && string_table[i + len + 1] == 0) {
            return string_table + i;
        }
    }

    return nullptr;
}

uint8_t *add_string(uint8_t *input) {
    // Try to find string in string_table.
    // If it exists, return that address.
    // Otherwise add it to the table.
    // If the string can't fit into the table, grow the table before adding it.

    // Note for future self/other people:
    // The code doesn't really benefit from compressing the duplicate strings
    // in the string table and it only slows it down greatly.
    // The full chara and music database uses less than 1mb total so it's not an issue.
    if (input == NULL) {
        return NULL;
    }

    uint32_t len = strlen((char *)input);

    if (len == 0) {
        return NULL;
    }

    if (string_table == NULL || string_table_idx + len + 1 >= string_table_size) {
        // Realloc array to hold more strings
        uint32_t new_string_table_size = string_table_idx + len + 1 + string_table_growth_size;
        uint8_t *string_table_new = (uint8_t *)realloc(string_table, new_string_table_size + 1);

        if (string_table_new == NULL) {
            printf("Couldn't relloc array from %d bytes to %d bytes, exiting...\n",
                   string_table_size, string_table_size + string_table_growth_size);
            exit(1);
        }

        // Zero out new section
        memset(string_table_new + string_table_size, 0, new_string_table_size - string_table_size);

        string_table = string_table_new;
        string_table_size = new_string_table_size;
    }

    uint8_t *output = string_table + string_table_idx;
    memcpy(string_table + string_table_idx, input, len);
    string_table[string_table_idx + len + 1] = 0;
    string_table_idx += len + 1;

    return output;
}

void add_chart_type_flag(uint32_t cur_idx, int8_t flag) { chart_type_overrides[cur_idx] = flag; }

int8_t get_chart_type_override(uint8_t *buffer, uint32_t music_idx, uint32_t chart_idx) {
    music_entry *m = (music_entry *)buffer;
    uint32_t idx = m[music_idx].charts[chart_idx];

    if (chart_type_overrides.find(idx) == chart_type_overrides.end()) {
        printf("Couldn't find chart\n");
        return -1;
    }

    printf("Found chart: %d\n", chart_type_overrides[idx]);
    if (chart_type_overrides[idx] == 1) {
        // Uses new chart type
        return 0;
    } else if (chart_type_overrides[idx] == 0) {
        // Does not use new chart
        return 1;
    }

    return -1;
}

uint32_t get_lapis_shape_id(uint8_t *lapis_shape_ptr) {
    if (!lapis_shape_ptr) {
        return 0;
    }

    RET_COMPARE_INDEX(lapis_shape_ptr, "dia", 1)
    RET_COMPARE_INDEX(lapis_shape_ptr, "tear", 2)
    RET_COMPARE_INDEX(lapis_shape_ptr, "heart", 3)
    RET_COMPARE_INDEX(lapis_shape_ptr, "squ", 4)

    return 0;
}

uint32_t get_lapis_color_id(uint8_t *lapis_color_ptr) {
    if (!lapis_color_ptr) {
        return 0;
    }

    RET_COMPARE_INDEX(lapis_color_ptr, "blue", 1)
    RET_COMPARE_INDEX(lapis_color_ptr, "pink", 2)
    RET_COMPARE_INDEX(lapis_color_ptr, "red", 3)
    RET_COMPARE_INDEX(lapis_color_ptr, "green", 4)
    RET_COMPARE_INDEX(lapis_color_ptr, "normal", 5)
    RET_COMPARE_INDEX(lapis_color_ptr, "yellow", 6)
    RET_COMPARE_INDEX(lapis_color_ptr, "purple", 7)
    RET_COMPARE_INDEX(lapis_color_ptr, "black", 8)

    return 0;
}

uint16_t get_chara_idx(const uint8_t *chara_id) {
    if (!chara_id || strlen((char *)chara_id) == 0) {
        return 0;
    }

    // This table is limited so keep it as small as possible
    for (int32_t i = 0; i <= max_chara_id; i++) {
        character_entry *cur = (character_entry *)chara_table + i;

        if (chara_id != NULL && cur->chara_id_ptr != NULL &&
            strlen((char *)cur->chara_id_ptr) == strlen((char *)chara_id) &&
            strcmp((char *)cur->chara_id_ptr, (char *)chara_id) == 0) {
            return i;
        }
    }

    return 0;
}

uint32_t chart_label_to_idx(uint8_t *chart_label) {
    if (!chart_label) {
        return 1;
    }

    RET_COMPARE_INDEX(chart_label, "ep", 0)
    RET_COMPARE_INDEX(chart_label, "np", 1)
    RET_COMPARE_INDEX(chart_label, "hp", 2)
    RET_COMPARE_INDEX(chart_label, "op", 3)
    RET_COMPARE_INDEX(chart_label, "bp_n", 4)
    RET_COMPARE_INDEX(chart_label, "bn", 4)
    RET_COMPARE_INDEX(chart_label, "bp_h", 5)
    RET_COMPARE_INDEX(chart_label, "bh", 5)

    RET_COMPARE_INDEX(chart_label, "0", 0)
    RET_COMPARE_INDEX(chart_label, "1", 1)
    RET_COMPARE_INDEX(chart_label, "2", 2)
    RET_COMPARE_INDEX(chart_label, "3", 3)
    RET_COMPARE_INDEX(chart_label, "4", 4)
    RET_COMPARE_INDEX(chart_label, "5", 5)

    return 1;
}

fontstyle_entry *get_fontstyle(int32_t cur_idx) {
    if (cur_idx < 11) {
        return NULL;
    }

    cur_idx -= 11;
    if (cur_idx > fontmax_style_id) {
        return NULL;
    }

    return (fontstyle_entry *)fontstyle_table + cur_idx;
}

character_entry *get_chara(int32_t cur_idx) {
    if (used_chara_id.find(cur_idx) == used_chara_id.end()) {
        return NULL;
    }

    return (character_entry *)chara_table + cur_idx;
}

music_entry *get_music(int32_t cur_idx) {
    if (used_music_id.find(cur_idx) == used_music_id.end()) {
        return NULL;
    }

    return (music_entry *)music_table + cur_idx;
}

chart_entry *get_chart(int32_t cur_idx) {
    if (cur_idx > max_chart_id) {
        return NULL;
    }

    return (chart_entry *)chart_table + cur_idx;
}

uint32_t add_style(int32_t cur_idx, uint32_t fontface, uint32_t color, uint32_t height,
                   uint32_t width) {
    if (fontstyle_table == NULL || cur_idx + 1 >= (int)fontstyle_table_size) {
        // Realloc array to hold more styles
        uint32_t new_style_table_size = cur_idx + 1 + fontstyle_table_growth_size;
        uint8_t *style_table_new = (uint8_t *)realloc(fontstyle_table, (new_style_table_size + 1) *
                                                                           sizeof(fontstyle_entry));

        if (style_table_new == NULL) {
            printf("Couldn't relloc array from %d bytes to %d bytes, exiting...\n",
                   fontstyle_table_size, fontstyle_table_size + fontstyle_table_growth_size);
            exit(1);
        }

        // Zero out new section
        memset(style_table_new + fontstyle_table_size * sizeof(fontstyle_entry), 0,
               (new_style_table_size - fontstyle_table_size) * sizeof(fontstyle_entry));

        fontstyle_table = style_table_new;
        fontstyle_table_size = new_style_table_size;
    }

    // This table is limited so keep it as small as possible
    for (int32_t i = 0; i <= fontmax_style_id; i++) {
        fontstyle_entry *cur = (fontstyle_entry *)fontstyle_table + i;

        if (cur->fontface == fontface && cur->color == color && cur->height == height &&
            cur->width == width) {
            return i;
        }
    }

    fontstyle_entry *ptr = (fontstyle_entry *)fontstyle_table + cur_idx;
    memset(ptr, 0, sizeof(fontstyle_entry));
    ptr->fontface = fontface;
    ptr->color = color;
    ptr->height = height;
    ptr->width = width;

    if (cur_idx > fontmax_style_id) {
        fontmax_style_id = cur_idx;
    }

    return cur_idx;
}

uint32_t add_flavor(int32_t cur_idx, uint8_t *phrase1, uint8_t *phrase2, uint8_t *phrase3,
                    uint8_t *phrase4, uint8_t *phrase5, uint8_t *phrase6, uint8_t *birthday,
                    uint8_t chara1_birth_month, uint8_t chara2_birth_month,
                    uint8_t chara3_birth_month, uint8_t chara1_birth_date,
                    uint8_t chara2_birth_date, uint8_t chara3_birth_date, uint16_t style1,
                    bool style2_flag, uint16_t style3, uint32_t fontstyle_fontface,
                    uint32_t fontstyle_color, uint32_t fontstyle_height, uint32_t fontstyle_width) {
    if (flavor_table == NULL || cur_idx + 1 >= (int)flavor_table_size) {
        // Realloc array to hold more flavors
        uint32_t new_flavor_table_size = cur_idx + 1 + flavor_table_growth_size;
        uint8_t *flavor_table_new =
            (uint8_t *)realloc(flavor_table, (new_flavor_table_size + 1) * sizeof(flavor_entry));

        if (flavor_table_new == NULL) {
            printf("Couldn't relloc array from %d bytes to %d bytes, exiting...\n",
                   flavor_table_size, flavor_table_size + flavor_table_growth_size);
            exit(1);
        }

        // Zero out new section
        memset(flavor_table_new + flavor_table_size * sizeof(flavor_entry), 0,
               (new_flavor_table_size - flavor_table_size) * sizeof(flavor_entry));

        flavor_table = flavor_table_new;
        flavor_table_size = new_flavor_table_size;
    }

    // This table is limited so keep it as small as possible
    for (int32_t i = 0; i < cur_idx; i++) {
        flavor_entry *cur = (flavor_entry *)flavor_table + i;

        fontstyle_entry *fs = get_fontstyle(cur->style2);

        if (phrase1 != NULL && cur->phrase1 != NULL &&
            strlen((char *)cur->phrase1) == strlen((char *)phrase1) &&
            strcmp((char *)cur->phrase1, (char *)phrase1) == 0 && phrase2 != NULL &&
            cur->phrase2 != NULL && strlen((char *)cur->phrase2) == strlen((char *)phrase2) &&
            strcmp((char *)cur->phrase2, (char *)phrase2) == 0 && phrase3 != NULL &&
            cur->phrase3 != NULL && strlen((char *)cur->phrase3) == strlen((char *)phrase3) &&
            strcmp((char *)cur->phrase3, (char *)phrase3) == 0 && phrase4 != NULL &&
            cur->phrase4 != NULL && strlen((char *)cur->phrase4) == strlen((char *)phrase4) &&
            strcmp((char *)cur->phrase4, (char *)phrase4) == 0 && phrase5 != NULL &&
            cur->phrase5 != NULL && strlen((char *)cur->phrase5) == strlen((char *)phrase5) &&
            strcmp((char *)cur->phrase5, (char *)phrase5) == 0 && phrase6 != NULL &&
            cur->phrase6 != NULL && strlen((char *)cur->phrase6) == strlen((char *)phrase6) &&
            strcmp((char *)cur->phrase6, (char *)phrase6) == 0 && birthday != NULL &&
            cur->birthday_ptr != NULL &&
            strlen((char *)cur->birthday_ptr) == strlen((char *)birthday) &&
            strcmp((char *)cur->birthday_ptr, (char *)birthday) == 0 &&
            cur->chara1_birth_month == chara1_birth_month &&
            cur->chara2_birth_month == chara2_birth_month &&
            cur->chara3_birth_month == chara3_birth_month &&
            cur->chara1_birth_date == chara1_birth_date &&
            cur->chara2_birth_date == chara2_birth_date &&
            cur->chara3_birth_date == chara3_birth_date && cur->style1 == style1 &&
            cur->style3 == style3 &&
            (fs == NULL // TODO: Would a NULL fontstyle actually be an error?
             || (fs != NULL && fs->fontface == fontstyle_fontface && fs->color == fontstyle_color &&
                 fs->height == fontstyle_height && fs->width == fontstyle_width))) {
            return i;
        }
    }

    uint16_t style2 = add_style(fontmax_style_id + 1, fontstyle_fontface, fontstyle_color,
                                fontstyle_height, fontstyle_width) +
                      11;

    flavor_entry *ptr = (flavor_entry *)flavor_table + cur_idx;
    memset(ptr, 0, sizeof(flavor_entry));
    if (phrase1) {
        memcpy(ptr->phrase1, phrase1, sizeof(ptr->phrase1) - 1);
    }

    if (phrase2) {
        memcpy(ptr->phrase2, phrase2, sizeof(ptr->phrase2) - 1);
    }

    if (phrase3) {
        memcpy(ptr->phrase3, phrase3, sizeof(ptr->phrase3) - 1);
    }

    if (phrase4) {
        memcpy(ptr->phrase4, phrase4, sizeof(ptr->phrase4) - 1);
    }

    if (phrase5) {
        memcpy(ptr->phrase5, phrase5, sizeof(ptr->phrase5) - 1);
    }

    if (phrase6) {
        memcpy(ptr->phrase6, phrase6, sizeof(ptr->phrase6) - 1);
    }

    ptr->birthday_ptr = birthday;
    ptr->chara1_birth_month = chara1_birth_month;
    ptr->chara2_birth_month = chara2_birth_month;
    ptr->chara3_birth_month = chara3_birth_month;
    ptr->chara1_birth_date = chara1_birth_date;
    ptr->chara2_birth_date = chara2_birth_date;
    ptr->chara3_birth_date = chara3_birth_date;
    ptr->style1 = style1;
    ptr->style2 = style2;
    ptr->style3 = style3;

    if (cur_idx > max_flavor_id) {
        max_flavor_id = cur_idx;
    }

    return cur_idx;
}

uint32_t add_chara(int32_t cur_idx, uint8_t *chara_id_ptr, uint32_t flags, uint8_t *folder_ptr,
                   uint8_t *gg_ptr, uint8_t *cs_ptr, uint8_t *icon1_ptr, uint8_t *icon2_ptr,
                   uint16_t chara_xw, uint16_t chara_yh, uint32_t display_flags, int16_t flavor_idx,
                   uint8_t chara_variation_num, uint8_t *sort_name_ptr, uint8_t *disp_name_ptr,
                   uint32_t file_type, uint32_t lapis_shape, uint8_t lapis_color, uint8_t *ha_ptr,
                   uint8_t *catchtext_ptr, int16_t win2_trigger, uint32_t game_version) {
    if (chara_table == NULL || cur_idx + 1 >= (int32_t)chara_table_size) {
        // Realloc array to hold more charas
        uint32_t new_chara_table_size = cur_idx + 1 + chara_table_growth_size;
        uint8_t *chara_table_new =
            (uint8_t *)realloc(chara_table, (new_chara_table_size + 1) * sizeof(character_entry));

        if (chara_table_new == NULL) {
            printf("Couldn't relloc array from %d bytes to %d bytes, exiting...\n",
                   chara_table_size, chara_table_size + chara_table_growth_size);
            exit(1);
        }

        // Zero out new section
        memset(chara_table_new + chara_table_size * sizeof(character_entry), 0,
               (new_chara_table_size - chara_table_size) * sizeof(character_entry));

        // Initialize new entries to sane defaults
        for (uint32_t i = chara_table_size; i < new_chara_table_size; i++) {
            character_entry *chara_entry = (character_entry *)chara_table_new + i;
            memset(chara_entry, 0, sizeof(*chara_entry));

            chara_entry->chara_id_ptr = add_string((uint8_t *)"bamb_1a");
            chara_entry->flags = 0x31;
            chara_entry->folder_ptr = add_string((uint8_t *)"22");
            chara_entry->gg_ptr = add_string((uint8_t *)"gg_bamb_1a");
            chara_entry->cs_ptr = add_string((uint8_t *)"cs_bamb_1a");
            chara_entry->icon1_ptr = add_string((uint8_t *)"cs_a");
            chara_entry->icon2_ptr = add_string((uint8_t *)"cs_b");
            chara_entry->chara_xw = 240;
            chara_entry->chara_yh = 220;
            chara_entry->file_type = 53;
            chara_entry->lapis_shape = get_lapis_shape_id((uint8_t *)"dia");
            chara_entry->lapis_color = get_lapis_color_id((uint8_t *)"yellow");
            chara_entry->win2_trigger = -1;
        }

        chara_table = chara_table_new;
        chara_table_size = new_chara_table_size;
    }

    character_entry *ptr = (character_entry *)chara_table + cur_idx;
    ptr->chara_id_ptr = chara_id_ptr;
    ptr->flags = flags;
    ptr->folder_ptr = folder_ptr;
    ptr->gg_ptr = gg_ptr;
    ptr->cs_ptr = cs_ptr;
    ptr->icon1_ptr = icon1_ptr;
    ptr->icon2_ptr = icon2_ptr;
    ptr->chara_xw = chara_xw;
    ptr->chara_yh = chara_yh;
    ptr->display_flags = display_flags;
    ptr->flavor_idx = flavor_idx;
    ptr->chara_variation_num = chara_variation_num;
    ptr->sort_name_ptr = sort_name_ptr;
    ptr->disp_name_ptr = disp_name_ptr;
    ptr->file_type = file_type;
    ptr->lapis_shape = lapis_shape;
    ptr->lapis_color = lapis_color;
    ptr->ha_ptr = ha_ptr;
    ptr->catchtext_ptr = catchtext_ptr ? catchtext_ptr : chara_table_nullptr;
    ptr->win2_trigger = win2_trigger;
    ptr->game_version = game_version;

    if (cur_idx > max_chara_id) {
        max_chara_id = cur_idx;
    }

    used_chara_id[cur_idx] = true;

    return cur_idx;
}

uint32_t add_music(int32_t cur_idx, uint8_t *fw_genre_ptr, uint8_t *fw_title_ptr,
                   uint8_t *fw_artist_ptr, uint8_t *genre_ptr, uint8_t *title_ptr,
                   uint8_t *artist_ptr, uint16_t chara1, uint16_t chara2, uint32_t mask,
                   uint32_t folder, uint32_t cs_version, uint32_t categories, uint8_t *diffs,
                   uint16_t *charts, uint8_t *ha_ptr, uint32_t chara_x, uint32_t chara_y,
                   uint16_t *unk1, uint16_t *display_bpm, uint8_t *hold_flags, bool allow_resize) {
    if (allow_resize && (music_table == NULL || cur_idx + 1 >= (int32_t)music_table_size)) {
        // Realloc array to hold more musics
        uint32_t new_music_table_size = cur_idx + 1 + music_table_growth_size;
        uint8_t *music_table_new =
            (uint8_t *)realloc(music_table, (new_music_table_size + 1) * sizeof(music_entry));

        if (music_table_new == NULL) {
            printf("Couldn't relloc array from %d bytes to %d bytes, exiting...\n",
                   music_table_size, music_table_size + music_table_growth_size);
            exit(1);
        }

        // Initialize new entries to sane defaults
        for (uint32_t i = music_table_size; i < new_music_table_size; i++) {
            music_entry *e = (music_entry *)music_table_new + i;
            memset(e, 0, sizeof(*e));

            e->fw_genre_ptr = filler_str;
            e->fw_title_ptr = filler_str;
            e->fw_artist_ptr = filler_str;
            e->genre_ptr = filler_str;
            e->title_ptr = filler_str;
            e->artist_ptr = filler_str;
            e->mask = 0x80000000;
            e->folder = 25;
            memset((char *)e->diffs, 1, sizeof(e->diffs));
        }

        music_table = music_table_new;
        music_table_size = new_music_table_size;
    }

    music_entry *ptr = (music_entry *)music_table + cur_idx;
    ptr->fw_genre_ptr = fw_genre_ptr;
    ptr->fw_title_ptr = fw_title_ptr;
    ptr->fw_artist_ptr = fw_artist_ptr;
    ptr->genre_ptr = genre_ptr;
    ptr->title_ptr = title_ptr;
    ptr->artist_ptr = artist_ptr;
    ptr->chara1 = chara1;
    ptr->chara2 = chara2;
    ptr->mask = mask;
    ptr->folder = folder;
    ptr->cs_version = cs_version;
    ptr->categories = categories;
    memcpy(ptr->diffs, (char *)diffs, sizeof(uint8_t) * 6);
    memcpy(ptr->charts, charts, sizeof(uint16_t) * 7);
    ptr->ha_ptr = ha_ptr;
    ptr->chara_x = chara_x;
    ptr->chara_y = chara_y;
    memcpy(ptr->unk1, unk1, sizeof(uint16_t) * 32);
    memcpy(ptr->display_bpm, display_bpm, sizeof(uint16_t) * 12);
    memcpy(ptr->hold_flags, hold_flags, sizeof(uint8_t) * 8);

    if (cur_idx > max_music_id) {
        max_music_id = cur_idx;
    }

    used_music_id[cur_idx] = true;

    return cur_idx;
}

uint32_t add_chart(int32_t cur_idx, uint8_t *folder, uint8_t *filename, int32_t audio_param1,
                   int32_t audio_param2, int32_t audio_param3, int32_t audio_param4,
                   uint32_t file_type, uint16_t used_keys, bool override_idx) {
    if (chart_table == NULL || cur_idx + 1 >= (int32_t)chart_table_size) {
        // Realloc array to hold more charts
        uint32_t new_chart_table_size = cur_idx + 1 + chart_table_growth_size;
        uint8_t *chart_table_new =
            (uint8_t *)realloc(chart_table, (new_chart_table_size + 1) * sizeof(chart_entry));

        if (chart_table_new == NULL) {
            printf("Couldn't relloc array from %d bytes to %d bytes, exiting...\n",
                   chart_table_size, chart_table_size + chart_table_growth_size);
            exit(1);
        }

        // Initialize new entries to sane defaults
        for (uint32_t i = chart_table_size; i < new_chart_table_size; i++) {
            chart_entry *e = (chart_entry *)chart_table_new + i;
            memset(e, 0, sizeof(*e));
        }

        chart_table = chart_table_new;
        chart_table_size = new_chart_table_size;
    }

    // This table is limited so keep it as small as possible
    for (int32_t i = 0; !override_idx && i <= max_chart_id; i++) {
        chart_entry *cur = (chart_entry *)chart_table + i;

        if (folder != NULL && cur->folder_ptr != NULL &&
            strlen((char *)cur->folder_ptr) == strlen((char *)folder) &&
            strcmp((char *)cur->folder_ptr, (char *)folder) == 0 && filename != NULL &&
            cur->filename_ptr != NULL &&
            strlen((char *)cur->filename_ptr) == strlen((char *)filename) &&
            strcmp((char *)cur->filename_ptr, (char *)filename) == 0 &&
            cur->audio_param1 == audio_param1 && cur->audio_param2 == audio_param2 &&
            cur->audio_param3 == audio_param3 && cur->audio_param4 == audio_param4 &&
            cur->file_type == file_type && cur->used_keys == used_keys) {
            return i;
        }
    }

    chart_entry *ptr = (chart_entry *)chart_table + cur_idx;
    ptr->folder_ptr = folder;
    ptr->filename_ptr = filename;
    ptr->audio_param1 = audio_param1;
    ptr->audio_param2 = audio_param2;
    ptr->audio_param3 = audio_param3;
    ptr->audio_param4 = audio_param4;
    ptr->file_type = file_type;
    ptr->used_keys = used_keys;

    if (cur_idx > max_chart_id) {
        max_chart_id = cur_idx;
    }

    return cur_idx;
}

void parse_charadb(const char *input_filename, const char *target) {
    if (!file_exists(input_filename)) {
        printf("Couldn't find %s, skipping...\n", input_filename);
        return;
    }

    property *config_xml = load_prop_file(input_filename);

    if (target && strlen(target) > 0) {
        char beforeTarget[64] = {};
        property_node_refer(config_xml, property_search(config_xml, NULL, "/database"), "before@",
                            PROPERTY_TYPE_ATTR, beforeTarget, 64);

        if (strlen(beforeTarget) > 0 && strcmp(target, beforeTarget) >= 0) {
            printf("Currently loading %s, found database that is only valid before %s, skipping %s...\n", target, beforeTarget, input_filename);
            return;
        }

        char afterTarget[64] = {};
        property_node_refer(config_xml, property_search(config_xml, NULL, "/database"), "after@",
                            PROPERTY_TYPE_ATTR, afterTarget, 64);
        if (strlen(afterTarget) > 0 && strcmp(target, afterTarget) < 0) {
            printf("Currently loading %s, found database that is only valid after %s, skipping %s...\n", target, afterTarget, input_filename);
            return;
        }
    }

    property_node *prop = NULL;
    if ((prop = property_search(config_xml, NULL, "/database/chara"))) {
        // Iterate over all charas in /database
        for (; prop != NULL; prop = property_node_traversal(prop, TRAVERSE_NEXT_SEARCH_RESULT)) {
            char idxStr[256] = {};
            property_node_refer(config_xml, prop, "id@", PROPERTY_TYPE_ATTR, idxStr,
                                sizeof(idxStr));
            uint32_t idx = atoi(idxStr);

            // Get an existing music entry in memory
            // If it exists, return the existing entry
            // If it doesn't exist, create a new entry in memory
            // Update the data in-place and make all parameters optional
            character_entry *c = get_chara(idx);
            bool is_fresh = c == NULL;

            if (is_fresh) {
                // Default character entry
                // TODO: Is there a better way I can make it force a new entry without having to
                // define everything like this?
                add_chara(idx, add_string((uint8_t *)"bamb_1a"), 0x31, add_string((uint8_t *)"22"),
                          add_string((uint8_t *)"gg_bamb_1a"), add_string((uint8_t *)"cs_bamb_1a"),
                          add_string((uint8_t *)"cs_a"), add_string((uint8_t *)"cs_b"), 240, 220, 0,
                          0, 0, NULL, NULL, 53, get_lapis_shape_id((uint8_t *)"dia"),
                          get_lapis_color_id((uint8_t *)"yellow"), NULL, NULL, -1, 0);

                c = get_chara(idx);
            }

            DWORD old_prot;
            VirtualProtect((LPVOID)c, sizeof(character_entry), PAGE_EXECUTE_READWRITE, &old_prot);

            // Save to character table
            READ_STR_OPT(config_xml, prop, "chara_id", c->chara_id_ptr)
            READ_U32_OPT(config_xml, prop, "flags", c->flags)
            READ_STR_OPT(config_xml, prop, "folder", c->folder_ptr)
            READ_STR_OPT(config_xml, prop, "gg", c->gg_ptr)
            READ_STR_OPT(config_xml, prop, "cs", c->cs_ptr)
            READ_STR_OPT(config_xml, prop, "icon1", c->icon1_ptr)
            READ_STR_OPT(config_xml, prop, "icon2", c->icon2_ptr)
            READ_U16_OPT(config_xml, prop, "chara_xw", c->chara_xw)
            READ_U16_OPT(config_xml, prop, "chara_yh", c->chara_yh)
            READ_U32_OPT(config_xml, prop, "display_flags", c->display_flags)
            READ_U8_OPT(config_xml, prop, "chara_variation_num", c->chara_variation_num)
            READ_STR_OPT(config_xml, prop, "sort_name", c->sort_name_ptr)
            READ_STR_OPT(config_xml, prop, "disp_name", c->disp_name_ptr)
            READ_U32_OPT(config_xml, prop, "file_type", c->file_type)
            READ_LAPIS_SHAPE_OPT(config_xml, prop, "lapis_shape", c->lapis_shape)
            READ_LAPIS_COLOR_OPT(config_xml, prop, "lapis_color", c->lapis_color)
            READ_STR_OPT(config_xml, prop, "ha", c->ha_ptr)
            READ_STR_OPT(config_xml, prop, "catchtext", c->catchtext_ptr)
            READ_S16_OPT(config_xml, prop, "win2_trigger", c->win2_trigger)
            READ_U32_OPT(config_xml, prop, "game_version", c->game_version)

            property_node *prop_flavor = NULL;
            int32_t flavor_idx = -1;
            if ((prop_flavor = property_search(config_xml, prop, "/flavor"))) {
                // Save to flavor table
                READ_STR(config_xml, prop_flavor, "phrase1", phrase1, phrase1_ptr)
                READ_STR(config_xml, prop_flavor, "phrase2", phrase2, phrase2_ptr)
                READ_STR(config_xml, prop_flavor, "phrase3", phrase3, phrase3_ptr)
                READ_STR(config_xml, prop_flavor, "phrase4", phrase4, phrase4_ptr)
                READ_STR(config_xml, prop_flavor, "phrase5", phrase5, phrase5_ptr)
                READ_STR(config_xml, prop_flavor, "phrase6", phrase6, phrase6_ptr)
                READ_STR(config_xml, prop_flavor, "birthday", birthday, birthday_ptr)
                READ_U8(config_xml, prop_flavor, "chara1_birth_month", chara1_birth_month)
                READ_U8(config_xml, prop_flavor, "chara2_birth_month", chara2_birth_month)
                READ_U8(config_xml, prop_flavor, "chara3_birth_month", chara3_birth_month)
                READ_U8(config_xml, prop_flavor, "chara1_birth_date", chara1_birth_date)
                READ_U8(config_xml, prop_flavor, "chara2_birth_date", chara2_birth_date)
                READ_U8(config_xml, prop_flavor, "chara3_birth_date", chara3_birth_date)
                READ_U16(config_xml, prop_flavor, "style1", style1)
                READ_U16(config_xml, prop_flavor, "style3", style3)

                property_node *prop_flavor_style = NULL;
                bool style2_flag = false;
                uint32_t fontface = 0, color = 0, height = 0, width = 0;
                if ((prop_flavor_style = property_search(config_xml, prop_flavor, "/style2"))) {
                    // Save to style table
                    READ_U32_OPT(config_xml, prop_flavor_style, "fontface", fontface)
                    READ_U32_OPT(config_xml, prop_flavor_style, "color", color)
                    READ_U32_OPT(config_xml, prop_flavor_style, "height", height)
                    READ_U32_OPT(config_xml, prop_flavor_style, "width", width)

                    style2_flag = true;

                    // printf("style[%d] idx[%d]\n", style_table_ptr, style2_idx);
                }

                flavor_idx = add_flavor(
                    max_flavor_id + 1, phrase1_ptr, phrase2_ptr, phrase3_ptr, phrase4_ptr,
                    phrase5_ptr, phrase6_ptr, birthday_ptr, chara1_birth_month, chara2_birth_month,
                    chara3_birth_month, chara1_birth_date, chara2_birth_date, chara3_birth_date,
                    style1, style2_flag, style3, fontface, color, height, width);

                // printf("flavor[%d] idx[%d]\n", max_flavor_id, flavor_idx);

                if (flavor_idx > (int32_t)flavor_limit) {
                    // This is a hack because I don't feel like finding the proper buffers to patch
                    // for this
                    printf("WARNING: Overflowed flavor text table limit of %d, defaulting to index "
                           "0 instead of %d\n",
                           flavor_limit, flavor_idx);
                    flavor_idx = -1;
                }

                c->flavor_idx = flavor_idx;
            }

            if ((c->flavor_idx == 0 || c->flavor_idx == -1)) {
                c->flavor_idx = 1;
                printf("Setting default flavor for chara id %d\n", idx);
            }

            VirtualProtect((LPVOID)c, sizeof(character_entry), old_prot, &old_prot);
        }
    }

    free(config_xml);
}

static char *get_subcateg_title(const char* path) {
    char *categ_name = NULL;
    char filename[64];

    //try to open "folderpath/_name.txt"
    size_t len = (size_t)(strchr(path+10, '\\')-(path));
    strncpy(filename, path, len);
    sprintf(filename+len, "\\_name.txt");

    FILE *file = fopen(filename, "r");
    if ( file != NULL ) {
        //if it has a custom title, use it
        char line[64];
        if (fgets(line, sizeof(line), file)) {
            //handle UTF-8 BOM since that could be common
            categ_name = (memcmp(line, "\xEF\xBB\xBF", 3) == 0) ? strdup(line+3) : strdup(line);
            LOG("%s sets subcategory name to %s\n", filename, categ_name);
        }
        fclose(file);
    } else { // or just keep folder name by itself (cut "data_mods")
        len = (size_t)(strchr(path+10, '\\')-(path+10));
        categ_name = (char*) malloc(len+1);
        strncpy(categ_name, path+10, len);
        categ_name[len] = '\0';
    }

    return categ_name;
}

#define F_OK 0
static bool is_excluded_folder(const char *path)
{
    char filename[64];

    //try to open "folderpath/_exclude"
    size_t len = (size_t)(strchr(path+10, '\\')-(path));
    strncpy(filename, path, len);
    sprintf(filename+len, "\\_exclude");
    if (access(filename, F_OK) == 0)
    {
        LOG("%s causes folder to be excluded from customs (contents will be treated as regular songs)\n", filename);
        return true;
    }

    if ( path[strlen("data_mods/")] == '_' )
    {
        filename[strchr(path+10, '\\')-path] = '\0';
        LOG("%s starting with _ causes folder to be excluded from customs (contents will be treated as regular songs)\n", filename);
        return true;
    }

    return false;
}

void parse_musicdb(const char *input_filename, const char *target, struct popnhax_config *config) {
    if (!file_exists(input_filename)) {
        printf("Couldn't find %s, skipping...\n", input_filename);
        return;
    }

    bool excluded = (config->custom_categ && is_excluded_folder(input_filename));

    char *subcateg_title = NULL;
    subcategory_s *subcateg = NULL;
    if (config->custom_categ == 2 && !excluded)
    {
        subcateg_title = get_subcateg_title(input_filename);
        subcateg = get_subcateg(subcateg_title); //will return a new one if not found
    }

    property *config_xml = load_prop_file(input_filename);

    if (target && strlen(target) > 0) {
        char beforeTarget[64] = {};
        property_node_refer(config_xml, property_search(config_xml, NULL, "/database"), "before@",
                            PROPERTY_TYPE_ATTR, beforeTarget, 64);

        if (strlen(beforeTarget) > 0 && strcmp(target, beforeTarget) >= 0) {
            printf("Currently loading %s, found database that is only valid before %s, skipping %s...\n", target, beforeTarget, input_filename);
            return;
        }

        char afterTarget[64] = {};
        property_node_refer(config_xml, property_search(config_xml, NULL, "/database"), "after@",
                            PROPERTY_TYPE_ATTR, afterTarget, 64);
        if (strlen(afterTarget) > 0 && strcmp(target, afterTarget) < 0) {
            printf("Currently loading %s, found database that is only valid after %s, skipping %s...\n", target, afterTarget, input_filename);
            return;
        }
    }

    property_node *prop = NULL;
    if ((prop = property_search(config_xml, NULL, "/database/music"))) {
        // Iterate over all musics in /database
        for (; prop != NULL; prop = property_node_traversal(prop, TRAVERSE_NEXT_SEARCH_RESULT)) {
            char idxStr[256] = {};
            property_node_refer(config_xml, prop, "id@", PROPERTY_TYPE_ATTR, idxStr,
                                sizeof(idxStr));
            uint32_t idx = atoi(idxStr);

            // Get an existing music entry in memory
            // If it exists, return the existing entry
            // If it doesn't exist, create a new entry in memory
            // Update the data in-place and make all parameters optional
            music_entry *m = get_music(idx);
            bool is_fresh = m == NULL; // ie. not part of internal songdb
            bool is_gone = ( m != NULL && strcmp((const char*) m->title_ptr, "\x81\x5D") == 0); // removed entries all have this title (SJIS "-")

            // Update customs/omni songid list
            if ( is_fresh || is_gone || config->partial_entries )
            {
                if ( idx >= config->custom_categ_min_songid && !excluded && bst_search(g_customs_bst, idx) == NULL )
                {
                    g_customs_bst = bst_insert(g_customs_bst, idx);
                    //LOG("%d inserted into customs bst\n", idx);
                    if (config->custom_categ == 2)
                    {
                        add_song_to_subcateg(idx, subcateg);
                    }
                }
            }

            if (is_fresh) {
                // Default music entry
                static const uint8_t diffs[6] = {0};
                static const uint16_t charts[7] = {0};
                static const uint16_t unk1[32] = {0};
                static const uint16_t display_bpm[12] = {0};
                static const uint8_t hold_flags[8] = {0};

                // TODO: Is there a better way I can make it force a new entry without having to
                // define everything like this?
                add_music(idx, filler_str, filler_str, filler_str, filler_str, filler_str,
                          filler_str, 0, 0, 0x80000000, 25, 0, 0, (uint8_t *)&diffs[0],
                          (uint16_t *)&charts[0], NULL, 0, 0, (uint16_t *)&unk1[0],
                          (uint16_t *)&display_bpm[0], (uint8_t *)&hold_flags[0], true);

                m = get_music(idx);
            }

            DWORD old_prot;
            VirtualProtect((LPVOID)m, sizeof(music_entry), PAGE_EXECUTE_READWRITE, &old_prot);

            READ_STR_OPT(config_xml, prop, "fw_genre", m->fw_genre_ptr)
            READ_STR_OPT(config_xml, prop, "fw_title", m->fw_title_ptr)
            READ_STR_OPT(config_xml, prop, "fw_artist", m->fw_artist_ptr)
            READ_STR_OPT(config_xml, prop, "genre", m->genre_ptr)
            READ_STR_OPT(config_xml, prop, "title", m->title_ptr)
            READ_STR_OPT(config_xml, prop, "artist", m->artist_ptr)
            READ_CHARA_OPT(config_xml, prop, "chara1", m->chara1)
            READ_CHARA_OPT(config_xml, prop, "chara2", m->chara2)

            uint32_t orig_mask = m->mask;
            READ_U32_OPT(config_xml, prop, "mask", m->mask)

            READ_U32_OPT(config_xml, prop, "folder", m->folder)
            READ_U32_OPT(config_xml, prop, "cs_version", m->cs_version)
            READ_U32_OPT(config_xml, prop, "categories", m->categories)
            READ_STR_OPT(config_xml, prop, "ha", m->ha_ptr)
            READ_U32_OPT(config_xml, prop, "chara_x", m->chara_x)
            READ_U32_OPT(config_xml, prop, "chara_y", m->chara_y)
            READ_U16_ARR_OPT(config_xml, prop, "unk1", m->unk1, 32)
            READ_U16_ARR_OPT(config_xml, prop, "display_bpm", m->display_bpm, 16)

            property_node *prop_chart = NULL;
            int32_t default_chart_idx = -1;
            const uint32_t CHART_MASKS[] = {0x00080000, 0, 0x01000000,
                                            0x02000000, 0, 0x04000000};

            // Copy old chart flags from the previous mask
            for (size_t i = 0; i < 6; i++) {
                if ((orig_mask & CHART_MASKS[i]) != 0) {
                    m->mask |= CHART_MASKS[i];
                }
            }

            if ( config->custom_categ
              && config->custom_exclude_from_version
              && !excluded
              && idx >= config->custom_categ_min_songid 
              && ( is_fresh || config->exclude_omni ) )
            {
                m->cs_version = 0;
                m->folder = 0;
            }

            if ((prop_chart = property_search(config_xml, prop, "charts/chart"))) {
                for (; prop_chart != NULL; prop_chart = property_node_traversal(
                                               prop_chart, TRAVERSE_NEXT_SEARCH_RESULT)) {
                    char chartIdxStr[256] = {};
                    property_node_refer(config_xml, prop_chart, "idx@", PROPERTY_TYPE_ATTR,
                                        chartIdxStr, sizeof(chartIdxStr));
                    uint32_t chart_idx = chart_label_to_idx((uint8_t *)chartIdxStr);

                    chart_entry *c2 = get_chart(m->charts[chart_idx]);
                    if (is_fresh || c2 == NULL) {
                        m->charts[chart_idx] = add_chart(max_chart_id + 1, filler_str, filler_str,
                                                         0, 0, 0, 0, 0, 0, true);
                    } else {
                        m->charts[chart_idx] =
                            add_chart(max_chart_id + 1, c2->folder_ptr, c2->filename_ptr,
                                      c2->audio_param1, c2->audio_param2, c2->audio_param3,
                                      c2->audio_param4, c2->file_type, c2->used_keys, true);
                    }

                    chart_entry *c = get_chart(m->charts[chart_idx]);
                    if (!c) {
                        printf("Couldn't find chart entry!\n");
                        exit(1);
                    }

                    DWORD old_prot_chart;
                    VirtualProtect((LPVOID)c, sizeof(chart_entry), PAGE_EXECUTE_READWRITE, &old_prot_chart);

                    READ_STR_OPT(config_xml, prop_chart, "folder", c->folder_ptr)
                    READ_STR_OPT(config_xml, prop_chart, "filename", c->filename_ptr)
                    READ_S32_OPT(config_xml, prop_chart, "audio_param1", c->audio_param1)
                    READ_S32_OPT(config_xml, prop_chart, "audio_param2", c->audio_param2)
                    READ_S32_OPT(config_xml, prop_chart, "audio_param3", c->audio_param3)
                    READ_S32_OPT(config_xml, prop_chart, "audio_param4", c->audio_param4)
                    READ_U32_OPT(config_xml, prop_chart, "file_type", c->file_type)
                    READ_U16_OPT(config_xml, prop_chart, "used_keys", c->used_keys)

                    READ_U8_OPT(config_xml, prop_chart, "diff", m->diffs[chart_idx])
                    READ_U8_OPT(config_xml, prop_chart, "hold_flag", m->hold_flags[chart_idx])

                    if (property_search(config_xml, prop_chart, "force_new_chart_format")) {
                        READ_U32(config_xml, prop_chart, "force_new_chart_format",
                                 force_new_chart_format)
                        add_chart_type_flag(m->charts[chart_idx], force_new_chart_format);
                    } else {
                        add_chart_type_flag(m->charts[chart_idx], m->hold_flags[chart_idx] == 1);
                    }

                    m->mask |= CHART_MASKS[chart_idx];

                    if (chart_idx == 1) {
                        default_chart_idx = m->charts[chart_idx];
                    }

                    VirtualProtect((LPVOID)c, sizeof(chart_entry), old_prot_chart, &old_prot_chart);
                }
            }

            // Make sure that all entries in the charts array have a value.
            if (m->charts[1] == 0 && default_chart_idx == -1) {
                // A normal chart wasn't specified and one didn't originally exist.
                // This will probably lead to the preview music not being the right preview.
                for (int i = 0; i < 6; i++) {
                    if (m->charts[i] != 0) {
                        m->charts[1] = m->charts[i];
                        break;
                    }
                }
            }

            VirtualProtect((LPVOID)m, sizeof(music_entry), old_prot, &old_prot);
        }
    }

    free(config_xml);

    if (config->custom_categ == 2 && !excluded)
    {
        free(subcateg_title);
    }
}

void load_databases(const char *target_datecode, struct popnhax_config *config) {

    SearchFile s;
    printf("XML db files search...\n");
    s.search("data_mods", "xml", true);
    auto result = s.getResult();
    
    // Character databases must be loaded before music databases because the music databases could reference modified/new characters
    for(uint16_t i=0;i<result.size();i++)
    {
        if ( strstr(result[i].c_str(), "charadb") == NULL )
            continue;
        printf("(charadb) Loading %s...\n", result[i].c_str());
        parse_charadb(result[i].c_str(), target_datecode);
    }

    if (config->custom_categ == 2)
        init_subcategories();

    for(uint16_t i=0;i<result.size();i++)
    {
        if ( strstr(result[i].c_str(), "musicdb") == NULL )
            continue;
        printf("(musicdb) Loading %s...\n", result[i].c_str());
        parse_musicdb(result[i].c_str(), target_datecode, config);
    }
}

void musichax_core_init(struct popnhax_config *config,
                        char *target_datecode,

                        char *base_data,

                        uint64_t music_size, uint64_t *new_music_size, char *orig_music_data,
                        uint8_t **new_music_table,

                        uint64_t chart_size, uint64_t *new_chart_size, char *orig_chart_data,
                        uint8_t **new_chart_table,

                        uint64_t style_size, uint64_t *new_style_size, char *orig_style_data,
                        uint8_t **new_style_table,

                        uint64_t flavor_size, uint64_t *new_flavor_size, char *orig_flavor_data,
                        uint8_t **new_flavor_table,

                        uint64_t chara_size, uint64_t *new_chara_size, char *orig_chara_data,
                        uint8_t **new_chara_table) {
    bool force_unlocks = config->force_unlocks;
    bool is_expansion_allowed = !config->disable_expansions;
    bool is_redirection_allowed = !config->disable_redirection;

    if (style_size > fontstyle_table_size) {
        fontstyle_table_size = style_size;
    }

    if (flavor_size > flavor_table_size) {
        flavor_table_size = flavor_size;
    }

    if (chart_size > chart_table_size) {
        chart_table_size = chart_size;
    }

    if (music_size > music_table_size) {
        music_table_size = music_size;
    }

    if (chara_size > chara_table_size) {
        chara_table_size = chara_size;
    }

    flavor_limit = flavor_size;

    fontstyle_table = (uint8_t *)calloc(fontstyle_table_size, sizeof(fontstyle_entry));
    flavor_table = (uint8_t *)calloc(flavor_table_size, sizeof(flavor_entry));
    chara_table = (uint8_t *)calloc(chara_table_size, sizeof(character_entry));
    music_table = (uint8_t *)calloc(music_table_size, sizeof(music_entry));
    chart_table = (uint8_t *)calloc(chart_table_size * 25, sizeof(chart_entry));

    // Copy style table for use in the flavor table
    printf("Copying style table\n");
    for (uint32_t idx = 0; idx < style_size; idx++) {
        fontstyle_entry *cur = (fontstyle_entry *)orig_style_data + idx;

        add_style(idx, cur->fontface, cur->color, cur->height, cur->width);
    }

    // Copy flavor table for use in the character database
    printf("Copying flavor table\n");
    for (uint32_t idx = 0; idx < flavor_size; idx++) {
        flavor_entry *cur = (flavor_entry *)orig_flavor_data + idx;

        fontstyle_entry *cur_style = (fontstyle_entry *)orig_style_data + (cur->style2 - 11);

        add_flavor(
            idx, cur->phrase1, cur->phrase2, cur->phrase3, cur->phrase4, cur->phrase5, cur->phrase6,
            cur->birthday_ptr != NULL
                ? add_string(cur->birthday_ptr)
                : NULL,
            cur->chara1_birth_month, cur->chara2_birth_month, cur->chara3_birth_month,
            cur->chara1_birth_date, cur->chara2_birth_date, cur->chara3_birth_date, cur->style1,
            true, cur->style3, cur_style->fontface, cur_style->color, cur_style->height,
            cur_style->width);
    }

    // Copy character database for use in the music database
    printf("Copying character database\n");
    for (uint32_t idx = 0; idx < chara_size; idx++) {
        character_entry *cur = (character_entry *)orig_chara_data + idx;
        add_chara(
            idx,
            cur->chara_id_ptr != NULL
                ? add_string(cur->chara_id_ptr)
                : NULL,
            cur->flags,
            cur->folder_ptr != NULL
                ? add_string(cur->folder_ptr)
                : NULL,
            cur->gg_ptr != NULL
                ? add_string(cur->gg_ptr)
                : NULL,
            cur->cs_ptr != NULL
                ? add_string(cur->cs_ptr)
                : NULL,
            cur->icon1_ptr != NULL
                ? add_string(cur->icon1_ptr)
                : NULL,
            cur->icon2_ptr != NULL
                ? add_string(cur->icon2_ptr)
                : NULL,
            cur->chara_xw, cur->chara_yh, cur->display_flags, cur->flavor_idx,
            cur->chara_variation_num,
            cur->sort_name_ptr != NULL
                ? add_string(cur->sort_name_ptr)
                : NULL,
            cur->disp_name_ptr != NULL
                ? add_string(cur->disp_name_ptr)
                : NULL,
            cur->file_type, cur->lapis_shape, cur->lapis_color,
            cur->ha_ptr != NULL
                ? add_string(cur->ha_ptr)
                : NULL,
            cur->catchtext_ptr != NULL
                ? add_string(cur->catchtext_ptr)
                : NULL,
            cur->win2_trigger, cur->game_version);
    }

    // Copy music database
    printf("Copying music database\n");
    for (uint32_t idx = 0; idx < music_size; idx++) {
        music_entry *cur = (music_entry *)orig_music_data + idx;

        uint16_t charts[7] = {0};
        for (int i = 0; i < 7; i++) {
            chart_entry *cur_chart = (chart_entry *)orig_chart_data + cur->charts[i];
            charts[i] = add_chart(cur->charts[i],
                                  cur_chart->folder_ptr != NULL
                                      ? add_string(cur_chart->folder_ptr)
                                      : NULL,
                                  cur_chart->filename_ptr != NULL
                                      ? add_string(cur_chart->filename_ptr)
                                      : NULL,
                                  cur_chart->audio_param1, cur_chart->audio_param2,
                                  cur_chart->audio_param3, cur_chart->audio_param4,
                                  cur_chart->file_type, cur_chart->used_keys, true);
        }

        add_music(
            idx,
            cur->fw_genre_ptr != NULL
                ? add_string(cur->fw_genre_ptr)
                : NULL,
            cur->fw_title_ptr != NULL
                ? add_string(cur->fw_title_ptr)
                : NULL,
            cur->fw_artist_ptr != NULL
                ? add_string(cur->fw_artist_ptr)
                : NULL,
            cur->genre_ptr != NULL
                ? add_string(cur->genre_ptr)
                : NULL,
            cur->title_ptr != NULL
                ? add_string(cur->title_ptr)
                : NULL,
            cur->artist_ptr != NULL
                ? add_string(cur->artist_ptr)
                : NULL,
            cur->chara1, cur->chara2, cur->mask, cur->folder, cur->cs_version, cur->categories,
            cur->diffs, charts,
            cur->ha_ptr != NULL
                ? add_string(cur->ha_ptr)
                : NULL,
            cur->chara_x, cur->chara_y, cur->unk1, cur->display_bpm, cur->hold_flags, true);
    }

    load_databases((const char *)target_datecode, config);

    // Add some filler charts to fix some bugs (hack)
    for (int i = 0; i < 10; i++) {
        add_chart(max_chart_id + i + 1, NULL, NULL, 0, 0, 0, 0, 0, 0, true);
    }

    // Add one extra song as padding
    {
        if (max_music_id > (int64_t)music_size) {
            static const uint8_t diffs[6] = {0};
            static const uint16_t charts[7] = {0};
            static const uint16_t unk1[32] = {0};
            static const uint16_t display_bpm[12] = {0};
            static const uint8_t hold_flags[8] = {0};

            add_music(max_music_id + 1, filler_str, filler_str, filler_str, filler_str, filler_str,
                        filler_str, 0, 0, 0x80000000, 25, 0, 0, (uint8_t *)&diffs[0],
                        (uint16_t *)&charts[0], NULL, 0, 0, (uint16_t *)&unk1[0],
                        (uint16_t *)&display_bpm[0], (uint8_t *)&hold_flags[0], true);
        }

        if (max_chara_id > (int64_t)chara_size) {
            add_chara(max_chara_id + 1, add_string((uint8_t *)"bamb_1a"), 0x31, add_string((uint8_t *)"22"),
                        add_string((uint8_t *)"gg_bamb_1a"), add_string((uint8_t *)"cs_bamb_1a"),
                        add_string((uint8_t *)"cs_a"), add_string((uint8_t *)"cs_b"), 240, 220, 0,
                        0, 0, NULL, NULL, 53, get_lapis_shape_id((uint8_t *)"dia"),
                        get_lapis_color_id((uint8_t *)"yellow"), NULL, NULL, -1, 0);
        }
    }

    if (!is_expansion_allowed) {
        max_music_id = music_size;
        max_chara_id = chara_size;
        fontmax_style_id = style_size;
        max_flavor_id = flavor_size;
        max_chart_id = chart_size;
    }

    if (is_redirection_allowed) {
        *new_music_table = music_table;
        *new_chara_table = chara_table;
        *new_style_table = fontstyle_table;
        *new_flavor_table = flavor_table;
        *new_chart_table = chart_table;
    } else {
        // Copy new buffer data over top original buffers
        // EXPERIMENTAL!
        // Not really a supported feature, but I added it just in case someone wants to replace data
        // in-place
        patch_memory((uint64_t)orig_music_data, (char *)music_table,
                     sizeof(music_entry) * music_size);
        patch_memory((uint64_t)orig_chart_data, (char *)chart_table,
                     sizeof(chart_entry) * chart_size);
        patch_memory((uint64_t)orig_style_data, (char *)fontstyle_table,
                     sizeof(fontstyle_entry) * style_size);
        patch_memory((uint64_t)orig_flavor_data, (char *)flavor_table,
                     sizeof(flavor_entry) * flavor_size);
        patch_memory((uint64_t)orig_chara_data, (char *)chara_table,
                     sizeof(character_entry) * chara_size);

        *new_music_table = (uint8_t *)orig_music_data;
        *new_chara_table = (uint8_t *)orig_chara_data;
        *new_style_table = (uint8_t *)orig_style_data;
        *new_flavor_table = (uint8_t *)orig_flavor_data;
        *new_chart_table = (uint8_t *)orig_chart_data;
    }

    *new_music_size = max_music_id;
    *new_chara_size = max_chara_id;
    *new_style_size = fontmax_style_id;
    *new_flavor_size = max_flavor_id;
    *new_chart_size = max_chart_id;

    if (force_unlocks) {
        music_entry *m = (music_entry *)*new_music_table;
        for (uint64_t i = 0; i < *new_music_size; i++) {
            uint32_t new_mask = m[i].mask & ~0x8020080;

            if (m[i].title_ptr != NULL && new_mask != m[i].mask) {
                printf("Unlocking [%04lld] %s... %08x -> %08x\n", i, m[i].title_ptr, m[i].mask,
                       new_mask);
                patch_memory((uint64_t)&m[i].mask, (char *)&new_mask, sizeof(uint32_t));
            }
        }

        character_entry *c = (character_entry *)*new_chara_table;
        for (uint64_t i = 0; i < *new_chara_size; i++) {
            uint32_t new_flags = c[i].flags & ~3;

            if (new_flags != c[i].flags && c[i].disp_name_ptr != NULL && strlen((char*)c[i].disp_name_ptr) > 0) {
                printf("Unlocking [%04lld] %s... %08x -> %08x\n", i, c[i].disp_name_ptr, c[i].flags, new_flags);
                patch_memory((uint64_t)&c[i].flags, (char *)&new_flags, sizeof(uint32_t));

                if ((c[i].flavor_idx == 0 || c[i].flavor_idx == -1)) {
                    int flavor_idx = 1;
                    patch_memory((uint64_t)&c[i].flavor_idx, (char *)&flavor_idx, sizeof(uint32_t));
                    printf("Setting default flavor for chara id %lld\n", i);
                }
            }
        }
    }
}
