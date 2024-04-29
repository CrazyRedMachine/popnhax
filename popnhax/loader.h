#ifndef __LOADER_H__
#define __LOADER_H__

#include <stdint.h>
#include "popnhax/config.h"

int8_t get_chart_type_override(uint8_t *, uint32_t, uint32_t);

void musichax_core_init(struct popnhax_config *config,
                        char *target_datecode, char *base_data, uint64_t music_size,
                        uint64_t *new_music_size, char *orig_music_data, uint8_t **new_music_table,
                        uint64_t chart_size, uint64_t *new_chart_size, char *orig_chart_data,
                        uint8_t **new_chart_table, uint64_t style_size, uint64_t *new_style_size,
                        char *orig_style_data, uint8_t **new_style_table, uint64_t flavor_size,
                        uint64_t *new_flavor_size, char *orig_flavor_data,
                        uint8_t **new_flavor_table, uint64_t chara_size, uint64_t *new_chara_size,
                        char *orig_chara_data, uint8_t **new_chara_table);

#endif
