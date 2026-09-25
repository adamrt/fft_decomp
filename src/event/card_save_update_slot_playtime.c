#include "fft/event_card.h"
#include "psx/types.h"

void card_save_update_slot_playtime(s32 slot) {
    s32 component;
    s32 unused_04;
    s32 unused_08;
    s32 elapsed_seconds;
    s32 unused_10;
    s32 unused_14;

    if (g_card_save_slot_file_states[slot] != 0) {
        g_card_save_slot_playtimes[slot][0] = 0;
        g_card_save_slot_playtimes[slot][1] = 0;
        g_card_save_slot_playtimes[slot][2] = 0;
    } else {
        elapsed_seconds = g_card_save_buffer_pointer->elapsed_seconds;

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
