#include "fft/battle.h"

/*
 * Record considered coordinates unless they are already in the four-entry list.
 *
 * Current coordinates and selected-action defaults are updated even when the
 * list is full. Equality includes all four bytes; x == 0xff marks an empty slot.
 */
void battle_ai_record_considered_coords(s32 x, s32 y, s32 level) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 i;

    g_battle_ai_considered_coords.bytes.x = x;
    g_battle_ai_considered_coords.bytes.y = y;
    g_battle_ai_considered_coords.bytes.elevation = level;
    g_battle_ai_considered_coords.bytes.zero = 0;
    battle_ai_init_selected_action();
    for (i = 0; i < 4; i++) {
        if (ai->coords_181c[i].bytes.x == 0xff) {
            ai->coords_181c[i].word = ai->considered_unit_coords.word;
            break;
        }
        if (ai->coords_181c[i].word == ai->considered_unit_coords.word) {
            break;
        }
    }
}
