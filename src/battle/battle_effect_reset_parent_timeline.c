#include "psx/types.h"

typedef struct temp_effect_data_2_t {
    u8 _pad00[0x28];
    /* Effect state layout, pattern 1. */
    s16 frame;           /* 0x28 */
    s16 spawn_countdown; /* 0x2a */
    s16 spawned_count;   /* 0x2c */
    s16 values_2e[26];
    s16 values_62[26];
    s16 values_96[26];
} temp_effect_data_2_t;

void battle_effect_reset_parent_timeline(temp_effect_data_2_t* data) {
    s32 index;

    index = 0;
    do {
        data->values_96[index] = 0;
        data->values_62[index] = 0;
        data->values_2e[index] = 0;
        index++;
    } while (index < 26);
    data->spawn_countdown = 0;
    data->frame = 0;
    data->spawned_count = 0;
}
