#include "fft/battle_ai.h"
#include "fft/unit_slots.h"

typedef struct battle_ai_wait_facing_work {
    battle_ai_coords_t target_coords;
    u8 _pad04[4];
    u8 viable_directions[4];
} battle_ai_wait_facing_work_t;

/*
 * Choose a facing hint for an AI-controlled unit's Wait command.
 *
 * Values other than 5 are returned directly. Otherwise prefer the nearest
 * imminent enemy, then any enemy, while rejecting blocked or steeply rising
 * adjacent tiles. The original does not bounds-check the direction lookup, so
 * coincident source and target coordinates can read one byte past the four-byte
 * viability array.
 */
s32 battle_ai_choose_wait_facing(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_ai_wait_facing_work_t work;
    u8* viability;
    battle_ai_data_t* reverse_ai;
    s32 offset;
    /* Pin: unpinned, GCC swaps $s0 and $s1 between `i` and `offset`. */
    register s32 i __asm__("$16");
    s32 fallback_direction;
    s32 target_id;
    s32 tried_any_enemy;
    s32 facing;
    s32 one;

    if (ai->selected_action.wait_facing_hint != BATTLE_AI_DIRECTION_CHOOSE_AT_WAIT)
        return ai->selected_action.wait_facing_hint;

    {
        /* Keeps the argument loads ahead of the loop-state initialization. */
        s32 acting_unit_id = g_battle_ai_data_base.acting_unit_id;
        battle_ai_coords_t* acting_coords = &ai->acting_unit_coords;

        __asm__("" : : "r"(acting_unit_id), "r"(acting_coords));
        i = 0;
        viability = work.viable_directions;
        offset = 0;
        battle_ai_transfer_unit_coordinates(acting_unit_id, acting_coords);
    }
    do {
        *(volatile u8*)&work.target_coords.bytes.x
            = ai->acting_unit_coords.bytes.x + g_battle_ai_facing_tile_offsets.bytes[offset];
        work.target_coords.bytes.y = ai->acting_unit_coords.bytes.y + g_battle_ai_facing_tile_offsets.bytes[offset + 1];
        work.target_coords.bytes.elevation = 0;
        if (battle_ai_check_facing_tile(&ai->acting_unit_coords, (battle_ai_coords_t*)&work.target_coords) != 0) {
            *viability = 0;
        } else {
            s32 valid = 1;

            fallback_direction = i;
            /* Prevent 1 from being hoisted into a saved register. */
            __asm__("" : "=r"(valid) : "0"(valid));
            *viability = valid;
        }
        viability++;
        i++;
        offset += 2;
    } while (i < 4);

    one = 1;
    if (*(u32*)work.viable_directions == 0)
        *(u32*)work.viable_directions = 0x01010101;

    offset = BATTLE_UNIT_SLOT_COUNT - 1;
    /* Indexed access changes the target's address-update order; this shifted
     * typed base preserves the exact reverse scan. */
    reverse_ai = (battle_ai_data_t*)((u8*)ai + (BATTLE_UNIT_SLOT_COUNT - 1));
    do {
        reverse_ai->targetability.live.unit_targetable[0] = one;
        offset--;
        reverse_ai = (battle_ai_data_t*)((u8*)reverse_ai - 1);
    } while (offset >= 0);

    tried_any_enemy = 0;
find_target:
    i = ai->acting_unit_id;
    target_id = battle_ai_find_nearest_target(BATTLE_AI_NEAREST_IMMINENT_ENEMY);
    if (i == target_id) {
        target_id = battle_ai_find_nearest_target(BATTLE_AI_NEAREST_ENEMY);
        tried_any_enemy = 1;
    }
    battle_ai_transfer_unit_coordinates(target_id, (battle_ai_coords_t*)&work.target_coords);
    i = battle_ai_find_direction_of_target((s8*)&work.target_coords, (s8*)&ai->acting_unit_coords);
    /* Copies the returned direction into s0 instead of indexing with v0. */
    __asm__("" : "=r"(i) : "0"(i));
    if (work.viable_directions[i] != 0)
        return i;
    if (tried_any_enemy == 0) {
        ai->targetability.live.unit_targetable[target_id] = 0;
        /* A goto retry: a loop construct lets GCC hoist invariants. */
        goto find_target;
    }
    facing = ai->acting_unit->position.bits.facing;
    if (work.viable_directions[facing] == 0)
        return fallback_direction;
    return facing;
}
