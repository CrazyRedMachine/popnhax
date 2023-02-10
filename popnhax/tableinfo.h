#ifndef __TABLEINFO_H__
#define __TABLEINFO_H__

#include <stdint.h>

typedef struct {
    uint8_t *folder_ptr;
    uint8_t *filename_ptr;
    int32_t audio_param1;
    int32_t audio_param2;
    int32_t audio_param3;
    int32_t audio_param4;
    uint32_t file_type;
    uint16_t used_keys;
    uint8_t pad[2];
} chart_entry;

typedef struct {
    uint32_t fontface;
    uint32_t color;
    uint32_t height;
    uint32_t width;
} fontstyle_entry;

typedef struct {
    uint8_t phrase1[13];
    uint8_t phrase2[13];
    uint8_t phrase3[13];
    uint8_t phrase4[13];
    uint8_t phrase5[13];
    uint8_t phrase6[13];
    uint8_t _pad1[2];
    uint8_t *birthday_ptr;
    uint8_t chara1_birth_month;
    uint8_t chara2_birth_month;
    uint8_t chara3_birth_month;
    uint8_t chara1_birth_date;
    uint8_t chara2_birth_date;
    uint8_t chara3_birth_date;
    uint16_t style1;
    uint16_t style2;
    uint16_t style3;
} flavor_entry;

typedef struct {
    uint8_t *chara_id_ptr;
    uint32_t flags;
    uint8_t *folder_ptr;
    uint8_t *gg_ptr;
    uint8_t *cs_ptr;
    uint8_t *icon1_ptr;
    uint8_t *icon2_ptr;
    uint16_t chara_xw;
    uint16_t chara_yh;
    uint32_t display_flags;
    int16_t flavor_idx;
    uint8_t chara_variation_num;
    uint8_t _pad1[1];
    uint8_t *sort_name_ptr;
    uint8_t *disp_name_ptr;
    uint32_t file_type;
    uint32_t lapis_shape;
    uint8_t lapis_color;
    uint8_t _pad2[3];
    uint8_t *ha_ptr;
    uint8_t *catchtext_ptr;
    int16_t win2_trigger;
    uint8_t _pad3[2];
    uint32_t game_version;
} character_entry;

typedef struct {
    uint8_t *fw_genre_ptr;
    uint8_t *fw_title_ptr;
    uint8_t *fw_artist_ptr;
    uint8_t *genre_ptr;
    uint8_t *title_ptr;
    uint8_t *artist_ptr;
    uint16_t chara1;
    uint16_t chara2;
    uint32_t mask;
    uint32_t folder;
    uint32_t cs_version;
    uint32_t categories;
    uint8_t diffs[6];
    uint16_t charts[7];
    uint8_t *ha_ptr;
    uint32_t chara_x;
    uint32_t chara_y;
    uint16_t unk1[32];
    uint16_t display_bpm[12];
    uint8_t hold_flags[8];
} music_entry;

#endif