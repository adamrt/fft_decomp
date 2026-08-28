#include "fft/card.h"
#include "fft/world.h"
#include "psx/types.h"

/* WORLD twin of card_save_update_slot_playtime: splits the loaded save's
 * play time into hours/minutes/seconds for the memory card menu. */
void world_card_update_save_slot_playtime(s32 slot) {
    s32 component;
    s32 elapsed_seconds;

    if (g_card_save_slot_file_states[slot] != 0) {
        g_card_save_slot_playtimes[slot][0] = 0;
        g_card_save_slot_playtimes[slot][1] = 0;
        g_card_save_slot_playtimes[slot][2] = 0;
    } else {
        elapsed_seconds = g_world_load_work_buffer->elapsed_seconds;

        component = elapsed_seconds / 3600;
        if (component >= 100) {
            component = 99;
        }
        g_card_save_slot_playtimes[slot][0] = component;

        elapsed_seconds %= 3600;
        component = elapsed_seconds / 60;
        g_card_save_slot_playtimes[slot][1] = component;

        elapsed_seconds %= 60;
        g_card_save_slot_playtimes[slot][2] = elapsed_seconds;
    }
}
