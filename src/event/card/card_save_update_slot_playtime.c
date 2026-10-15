#include "fft/event_card.h"
#include "psx/types.h"

/* Store save slot `slot`'s play time as hours (capped at 99), minutes and
 * seconds from the save buffer's elapsed seconds, or zeros for a slot that
 * is not occupied. */
void card_save_update_slot_playtime(s32 slot) {
    s32 component;
    s32 unused_04;
    s32 unused_08;
    s32 elapsed_seconds;
    s32 unused_14;

    if (g_main_card_save_slot_file_states[slot] != CARD_SAVE_SLOT_OCCUPIED) {
        g_main_card_save_slot_playtimes[slot].hours = 0;
        g_main_card_save_slot_playtimes[slot].minutes = 0;
        g_main_card_save_slot_playtimes[slot].seconds = 0;
    } else {
        elapsed_seconds = g_card_save_buffer_pointer->elapsed_seconds;

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
