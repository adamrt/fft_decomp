#include "fft/world.h"
#include "psx/types.h"

/* WORLD twin of card_save_update_slot_playtime: splits the loaded save's
 * play time into hours/minutes/seconds for the memory card menu. */
void world_card_update_save_slot_playtime(s32 slot) {
    s32 component;
    s32 elapsed_seconds;

    if (g_main_card_save_slot_file_states[slot] != CARD_SAVE_SLOT_OCCUPIED) {
        g_main_card_save_slot_playtimes[slot].hours = 0;
        g_main_card_save_slot_playtimes[slot].minutes = 0;
        g_main_card_save_slot_playtimes[slot].seconds = 0;
    } else {
        elapsed_seconds = g_world_load_work_buffer->elapsed_seconds;

        component = elapsed_seconds / 3600;
        if (component >= 100) {
            component = 99;
        }
        g_main_card_save_slot_playtimes[slot].hours = component;

        elapsed_seconds %= 3600;
        component = elapsed_seconds / 60;
        g_main_card_save_slot_playtimes[slot].minutes = component;

        elapsed_seconds %= 60;
        g_main_card_save_slot_playtimes[slot].seconds = elapsed_seconds;
    }
}
