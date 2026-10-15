#include "fft/main.h"
#include "fft/memory.h"
#include "psx/types.h"

void main_unit_init_position_and_rewards(unit_stats_t* unit, entd_unit_t* entd) {
    unit->x = entd->x;
    unit->position.y = entd->position.y;
    unit->position.higher_elevation = entd->position.higher_elevation;
    unit->position.facing = entd->position.facing;
    unit->position.spell_quote_skillsets = entd->position.spell_quote_skillsets;

    unit->war_trophy = entd->war_trophy;
    unit->bonus_money_modifier = entd->bonus_money_modifier;
}

/*
 * Return the highest-order status changed by an action. Bits 0-6 encode the
 * one-based status index, 0x80 marks removal, and 0x100 reflects flags_2 bit 3.
 */
s32 main_status_find_action_highest_order_effect(const action_data_t* action) {
    s32 highest_order;
    s32 result;
    s32 status_index;
    u8 mask;
    s32 byte_index;
    u8 current_order;

    highest_order = -1;
    result = 0;
    for (status_index = 0; status_index < STATUS_COUNT; status_index++) {
        byte_index = status_index / 8;
        mask = 0x80 >> (status_index & 7);
        if (action->status_changes.removal[byte_index] & mask) {
            current_order = g_main_status_effect_data[status_index].order;
            if (current_order > highest_order) {
                highest_order = current_order;
                result = status_index + 1 + MAIN_ACTION_STATUS_RESULT_REMOVAL;
                if (g_main_status_effect_data[status_index].flags_2 & STATUS_EFFECT_FLAG_2_SCRIPT_FILTER_EXCLUDED) {
                    result = status_index + 1 + MAIN_ACTION_STATUS_RESULT_REMOVAL
                        + MAIN_ACTION_STATUS_RESULT_SCRIPT_FILTER_EXCLUDED;
                }
            }
        }
        if (action->status_changes.infliction[byte_index] & mask) {
            current_order = g_main_status_effect_data[status_index].order;
            if (current_order > highest_order) {
                highest_order = current_order;
                result = status_index + 1;
                if (g_main_status_effect_data[status_index].flags_2 & STATUS_EFFECT_FLAG_2_SCRIPT_FILTER_EXCLUDED) {
                    result = status_index + 1 + MAIN_ACTION_STATUS_RESULT_SCRIPT_FILTER_EXCLUDED;
                }
            }
        }
    }
    return result;
}

/* Return the zodiac sign for a day-of-year birthday.
 *
 * Counts the sign boundaries at or before the day, then rotates the count
 * into zodiac order with (count + 9) % 12. */
s32 main_unit_calculate_zodiac_symbol(u16 birthday) {
    s32 zodiac;
    s32 i;

    zodiac = 0;
    for (i = 0; i < ZODIAC_SIGN_ORDINARY_COUNT; i++) {
        zodiac += birthday >= g_main_zodiac_day_limits[i];
    }
    zodiac = (zodiac + 9) % ZODIAC_SIGN_ORDINARY_COUNT;
    return (u16)zodiac;
}

void mem_zero(void* destination, s32 count) {
    u8* dst = destination;
    s32 index = 0;

    while (index < count) {
        *dst++ = 0;
        index++;
    }
}
