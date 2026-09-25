#include "fft/main.h"
#include "psx/types.h"

/*
 * Return the highest-order status changed by an action. Bits 0-6 encode the
 * one-based status index, 0x80 marks removal, and 0x100 reflects flags_2 bit 3.
 */
s32 main_status_find_action_highest_order_effect(const u8* action) {
    s32 highest_order;
    s32 result;
    s32 status_index;
    s32 mask;
    s32 status_mask;
    s32 status_change;
    s32 byte_index;
    u8 current_order;

    highest_order = -1;
    result = 0;
    for (status_index = 0; status_index < BATTLE_STATUS_COUNT; status_index++) {
        byte_index = status_index / 8;
        mask = 0x80 >> (status_index & 7);
        /* Keeps the shift ahead of the status-byte load instead of in its delay slot. */
        __asm__("" : : "r"(mask));
        status_change = (action + byte_index)[0x20] & mask;
        status_mask = mask;
        if (status_change != 0) {
            current_order = g_main_status_effect_data[status_index].order;
            if (current_order > highest_order) {
                highest_order = current_order;
                result = status_index + 0x81;
                if (g_main_status_effect_data[status_index].flags_2 & STATUS_EFFECT_FLAG_2_PROVISIONAL_CHECK_SET_8) {
                    result = status_index + 0x181;
                }
            }
        }
        if (status_mask & (action + byte_index)[0x1b]) {
            current_order = g_main_status_effect_data[status_index].order;
            if (current_order > highest_order) {
                highest_order = current_order;
                result = status_index + 1;
                if (g_main_status_effect_data[status_index].flags_2 & STATUS_EFFECT_FLAG_2_PROVISIONAL_CHECK_SET_8) {
                    result = status_index + 0x101;
                }
            }
        }
    }
    return result;
}
