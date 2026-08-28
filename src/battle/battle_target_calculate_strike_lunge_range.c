#include "fft/battle.h"
#include "psx/types.h"

/*
 * Mark the special weapon-range shapes selected by weapon flags 0xc0: flag
 * 0x80 covers the four adjacent tiles within a narrow height band, otherwise
 * the two rings at distance 1 and 2 use a band that widens with distance.
 */
void battle_target_calculate_strike_lunge_range(battle_stats_t* unit, u8 flags) {
    u8 height;
    s32 x;
    s32 y;
    s32 i;
    s32 lo;
    s32 hi;

    height = battle_unit_get_effective_height(unit);
    x = unit->x;
    y = unit->position.bits.y;
    if (flags & 0x80) {
        lo = height - 6;
        hi = height + 5;
        battle_target_calculate_aoe_vertical_tolerance(x - 1, y, lo, hi);
        battle_target_calculate_aoe_vertical_tolerance(x + 1, y, lo, hi);
        battle_target_calculate_aoe_vertical_tolerance(x, y - 1, lo, hi);
        battle_target_calculate_aoe_vertical_tolerance(x, y + 1, lo, hi);
    } else {
        for (i = 1; i < 3; i++) {
            lo = height + (i - 5) * 2;
            hi = height + 7;
            battle_target_calculate_aoe_vertical_tolerance(x - i, y, lo, hi);
            battle_target_calculate_aoe_vertical_tolerance(x + i, y, lo, hi);
            battle_target_calculate_aoe_vertical_tolerance(x, y - i, lo, hi);
            battle_target_calculate_aoe_vertical_tolerance(x, y + i, lo, hi);
        }
    }
}
