#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "psx/types.h"

/* Half-unit height of a tile edge: base height plus the slope height scaled by
 * that side's two-bit slope factor. */
#define TILE_EDGE_HEIGHT(tile, shift)                                                                                  \
    (tile->height * 2 + tile->depth_half_height.half_height * ((tile->slope_type >> shift) & 3))

/* Checks whether the current target can be knocked back onto tile (x, y).
 *
 * direction is 0 south, 1 west, 2 north, 3 east and selects the facing edges
 * of the target's tile and the destination tile. A destination layer higher
 * than the target's edge is unusable; when only one layer is low enough, the
 * other layer's ceiling must leave at least four half-units of headroom. Of
 * two usable layers the higher one wins. On success the fall height goes to
 * g_action_current_ability.knockback_fall_height and the ability target ID to
 * g_action_current_ability.post_action_target_id, and the destination
 * layer is returned; -1 means off-map, blocked or occupied. */
s32 move_check_knockback_destination(s32 direction, s32 x, s32 y) {
    map_tile_t* tile;
    map_tile_t* lower;
    map_tile_t* upper;
    s32 source_shift;
    s32 destination_shift;
    s32 height;
    s32 lower_height;
    s32 upper_height;
    s32 level;
    s32 fall;
    s32 index;
    s32 upper_index;

    if (x < 0 || y < 0 || x >= g_map_max_x || y >= g_map_max_y) {
        return -1;
    }
    switch (direction) {
    case 3:
        source_shift = 0;
        destination_shift = 2;
        break;
    case 2:
        source_shift = 6;
        destination_shift = 4;
        break;
    case 1:
        source_shift = 2;
        destination_shift = 0;
        break;
    case 0:
        source_shift = 4;
        destination_shift = 6;
        break;
    }
    tile = &g_map_tile_data[map_calculate_location(g_action_target)];
    height = TILE_EDGE_HEIGHT(tile, source_shift);
    index = y * g_map_max_x + x;
    upper_index = index + MAP_TILES_PER_LAYER;
    lower = &g_map_tile_data[index];
    upper = &g_map_tile_data[upper_index];
    if (lower->flags.blocked) {
        lower_height = 0xff;
    } else {
        lower_height = TILE_EDGE_HEIGHT(lower, destination_shift);
    }
    upper_height = 0xff;
    if (!(upper->flags.blocked)) {
        upper_height = TILE_EDGE_HEIGHT(upper, destination_shift);
    }
    if (height < lower_height && height < upper_height) {
        return -1;
    }
    if (height < upper_height && height >= lower_height) {
        upper_height -= upper->ceiling_depth_and_marks.ceiling_depth;
        if (upper_height < height + 4) {
            return -1;
        }
        level = 0;
        fall = height - lower_height;
    } else if (height < lower_height && height >= upper_height) {
        lower_height -= lower->ceiling_depth_and_marks.ceiling_depth;
        if (lower_height < height + 4) {
            return -1;
        }
        level = 1;
        fall = height - upper_height;
    } else if (lower_height >= upper_height) {
        level = 0;
        fall = height - lower_height;
    } else {
        level = 1;
        fall = height - upper_height;
    }
    if (fall < 0) {
        return -1;
    }
    if (target_find_unit_at_tile(x, y, level) != -1) {
        return -1;
    }
    g_action_current_ability.knockback_fall_height = fall;
    g_action_current_ability.post_action_target_id = g_action_current_ability.target_id;
    return level;
}

/* One evade roll, made only when executing: evaded with numerator in
 * denominator odds. Records the miss type and the item that blocked; returns 1
 * when evaded. */
s32 formula_calculate_attack_evaded(u16 numerator, u16 denominator, u8 miss_type, u8 item_lost) {
    if (main_util_roll_pass_fail(denominator, numerator) != 0) {
        return 0;
    }
    if (g_action_state != ACTION_STATE_EXECUTE) {
        return 0;
    }
    g_action_target_data->hit = 0;
    g_action_target_data->miss_type = miss_type;
    g_action_target_data->item_lost = item_lost;
    return 1;
}

/* Scale the pending formula XA by the attacker/target zodiac compatibility.
 *
 * Serpentarius on either side leaves XA untouched.  The opposition entry
 * resolves by sex: monsters on either side fall back to the ordinary bad
 * result, otherwise same-sex pairs get the worst result and opposite-sex pairs
 * the best.
 *
 * Good and bad change XA by a quarter, best and worst by a half (XA * 25 / 100
 * or / 50, truncated); the result is at least 1.
 *
 * Casting the attacker sign to its stored 4-bit domain preserves the target's
 * distinct comparison copy without a compiler constraint.
 */
void formula_apply_zodiac_compatibility(void) {
    unit_stats_t* attacker;
    unit_stats_t* target;
    u16 attacker_zodiac;
    u32 attacker_zodiac_compare;
    u16 target_zodiac;
    u32 serpentarius = ZODIAC_SIGN_SERPENTARIUS;
    u16 difference;
    u8 compatibility;
    s16 xa;
    u16 product;

    attacker = g_action_attacker;
    target = g_action_target;
    attacker_zodiac = attacker->birthday.zodiac;
    target_zodiac = target->birthday.zodiac;
    attacker_zodiac_compare = (u8)attacker_zodiac;
    if (serpentarius == attacker_zodiac_compare) {
        return;
    }
    if (target_zodiac == serpentarius) {
        return;
    }

    if (target_zodiac < attacker_zodiac_compare) {
        difference = attacker_zodiac - target_zodiac;
    } else {
        difference = target_zodiac - attacker_zodiac;
    }

    compatibility = g_battle_zodiac_compatibility_modifiers[difference];
    if (compatibility == ZODIAC_COMPATIBILITY_GENDER_DEPENDENT) {
        u8 attacker_flags = g_action_attacker->unit_flags;

        compatibility = ZODIAC_COMPATIBILITY_BAD;
        if (!(attacker_flags & UNIT_FLAG_MONSTER)) {
            u8 target_flags = g_action_target->unit_flags;

            if (!(target_flags & UNIT_FLAG_MONSTER)) {
                compatibility = ZODIAC_COMPATIBILITY_BEST;
                if ((attacker_flags & (UNIT_FLAG_MALE | UNIT_FLAG_FEMALE))
                    == (target_flags & (UNIT_FLAG_MALE | UNIT_FLAG_FEMALE))) {
                    compatibility = ZODIAC_COMPATIBILITY_WORST;
                }
            }
        }
    }

    xa = g_action_current_ability.xa;
    product = xa * 25;
    switch (compatibility) {
    case ZODIAC_COMPATIBILITY_GOOD:
        g_action_current_ability.xa = xa + product / 100;
        break;
    case ZODIAC_COMPATIBILITY_BEST:
        g_action_current_ability.xa = xa + product / 50;
        break;
    case ZODIAC_COMPATIBILITY_BAD:
        g_action_current_ability.xa = xa - product / 100;
        break;
    case ZODIAC_COMPATIBILITY_WORST:
        g_action_current_ability.xa = xa - product / 50;
        break;
    }

    if (g_action_current_ability.xa <= 0) {
        g_action_current_ability.xa = 1;
    }
}

/* Filters the target's pending status inflictions and removals.
 *
 * The rider whitelist always masks pending inflictions. Unless removal_only
 * is set, further masks use the current (sets 0-2) or innate (sets 3-4) statuses, immunities,
 * the immortal blacklist and each status's can't-stack list, then every
 * remaining infliction clears the statuses it cancels. Inflicting Frog on a
 * frog turns into its removal. Removals always drop innate statuses and keep
 * only statuses the target has. Returns the sum of the remaining bytes, so 0
 * means nothing is left to inflict or remove. */
s32 status_modify_inflictions(s32 removal_only) {
    action_data_t* action;
    u8 inflicted;
    s32 immortal;
    s32 total;
    s32 i;
    s32 j;
    s32 byte;
    s32 mask;

    total = 0;
    immortal = g_action_target->team_flags & BATTLE_TEAM_FLAG_IMMORTAL;
    if (g_action_target->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER) {
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_action_target_data->status_changes.infliction[i] &= g_battle_rider_status_infliction_mask[i];
        }
    }
    action = g_action_target_data;
    inflicted = action->status_changes.infliction[2];
    if ((inflicted & STATUS_BYTE_MASK(STATUS_ID_FROG))
        && (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG))
        && removal_only == 0) {
        action->status_changes.infliction[2] = inflicted & ~STATUS_BYTE_MASK(STATUS_ID_FROG);
        g_action_target_data->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
            |= STATUS_BYTE_MASK(STATUS_ID_FROG);
    }
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        if (removal_only == 0) {
            if (i < 3) {
                g_action_target_data->status_changes.infliction[i]
                    &= ~(g_action_target->status_sets.immunity[i] | g_action_target->status_sets.current[i]);
            } else {
                g_action_target_data->status_changes.infliction[i]
                    &= ~(g_action_target->status_sets.immunity[i] | g_action_target->status_sets.innate[i]);
            }
            if (immortal) {
                g_action_target_data->status_changes.infliction[i]
                    &= ~g_main_status_check_sets[MAIN_STATUS_CHECK_SET_FORMATION_IMMUNITY][i];
            }
        }
        g_action_target_data->status_changes.removal[i] = g_action_target_data->status_changes.removal[i]
            & ~g_action_target->status_sets.innate[i] & g_action_target->inflicted_status[i];
    }
    if (removal_only == 0) {
        for (i = 0; i < STATUS_COUNT; i++) {
            byte = i / 8;
            mask = 0x80 >> (i & 7);
            if (g_action_target_data->status_changes.infliction[byte] & mask) {
                for (j = 0; j < STATUS_BYTE_COUNT; j++) {
                    if (g_action_target->status_sets.current[j] & g_main_status_effect_data[i].cant_stack[j]) {
                        g_action_target_data->status_changes.infliction[byte] &= ~mask;
                    }
                }
            }
        }
        for (i = 0; i < STATUS_COUNT; i++) {
            byte = i / 8;
            mask = 0x80 >> (i & 7);
            if (g_action_target_data->status_changes.infliction[byte] & mask) {
                for (j = 0; j < STATUS_BYTE_COUNT; j++) {
                    g_action_target_data->status_changes.infliction[j] &= ~g_main_status_effect_data[i].cancels[j];
                }
            }
        }
    }
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        total = total + g_action_target_data->status_changes.infliction[i]
            + g_action_target_data->status_changes.removal[i];
    }
    return total;
}
