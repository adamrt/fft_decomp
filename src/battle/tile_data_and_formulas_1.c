/* Tile data and formulas 1. Its start is a cut chosen by topic; its end is a cut chosen by topic. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/script.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Give the unit its move and its action back. Nothing in the game calls it. */
void action_activate_move_act(unit_stats_t* unit) {
    unit->movement_taken = 0;
    unit->action_taken = 0;
}

void map_copy_xy_coords_and_tile_data(u8* data) {
    g_map_max_x = *data++;
    g_map_max_y = *data++;
    memcpy(g_map_tile_data, data, 0x1000);
}

void map_load_move_find_item_data(map_move_find_item_data_t* map_data) {
    g_battle_current_map_move_find_item_data = *map_data;
}

/* The tile at (x, y) of a layer, or 0 outside the map or for a layer other than 0
 * and 1. */
map_tile_t* map_get_tile_data_pointer(s32 x, s32 y, u32 layer) {
    s32 max_x = g_map_max_x;
    s32 max_y = g_map_max_y;
    if (x < 0 || x >= max_x || y < 0 || y >= max_y || layer >= 2U) {
        return 0;
    }
    return &g_map_tile_data[(layer << 8) + y * max_x + x];
}

/* Returns one packed field of the tile at (x, y, layer), or 0xff for an
 * unknown selector. The tile address is not bounds-checked. */
s32 map_get_tile_data_value(map_tile_data_selector_e field, s32 x, s32 y, s32 layer) {
    map_tile_t* tile;

    tile = &g_map_tile_data[(layer << 8) + y * g_map_max_x + x];
    switch (field) {
    case MAP_TILE_DATA_SURFACE_TYPE:
        return tile->surface.type;
    case MAP_TILE_DATA_UNKNOWN_01:
        return tile->_unknown_01;
    case MAP_TILE_DATA_HEIGHT:
        return tile->height;
    case MAP_TILE_DATA_HALF_HEIGHT:
        return tile->depth_half_height.half_height;
    case MAP_TILE_DATA_DEPTH:
        return tile->depth_half_height.depth;
    case MAP_TILE_DATA_SLOPE_TYPE:
        return tile->slope_type;
    case MAP_TILE_DATA_CEILING_DEPTH:
        return tile->ceiling_depth_and_marks.ceiling_depth;
    case MAP_TILE_DATA_BLOCKED:
        return tile->flags.blocked;
    case MAP_TILE_DATA_UNTARGETABLE:
        return tile->flags.untargetable;
    case MAP_TILE_DATA_SHADING:
        return tile->flags.shading;
    case MAP_TILE_DATA_UNKNOWN_FLAGS_4_6:
        return tile->flags._unknown_00;
    case MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK:
        return tile->camera_block_masks & MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK;
    case MAP_TILE_DATA_MOVE_DESTINATION_MARK:
        return tile->ceiling_depth_and_marks.move_destination;
    case MAP_TILE_DATA_ABILITY_RANGE_MARK:
        return tile->ceiling_depth_and_marks.ability_range;
    }
    return 0xFF;
}

/* Store one packed field of the tile at (x, y, layer): the setter of
 * map_get_tile_data_value. Returns 0, or -2 for a selector with no case,
 * which includes the two mark selectors. The tile address is not bounds-checked.
 * The event script uses only the flat camera block selector.
 *
 * Selector 15 touches no tile: it calls map_tile_data_15_stub until that
 * returns 0, at most 1000 times, and raises battle load exception 0x80 when it
 * never does. The callee always returns 0, so neither the retry nor the exception
 * can run. */
s32 map_set_tile_data_value(map_tile_data_selector_e field, s32 x, s32 y, s32 layer, s32 value) {
    map_tile_t* tile;
    s32 attempts;
    s32 result;

    attempts = 0;
    result = 0;
    tile = &g_map_tile_data[(layer << 8) + y * g_map_max_x + x];
    switch (field) {
    case MAP_TILE_DATA_SURFACE_TYPE:
        tile->surface.type = value;
        break;
    case MAP_TILE_DATA_UNKNOWN_01:
        tile->_unknown_01 = value;
        break;
    case MAP_TILE_DATA_HEIGHT:
        tile->height = value;
        break;
    case MAP_TILE_DATA_HALF_HEIGHT:
        tile->depth_half_height.half_height = value;
        break;
    case MAP_TILE_DATA_DEPTH:
        /* A byte store keeps the mask before the shift and the shared switch tail. */
        value = (value & MAP_TILE_DEPTH_VALUE_MASK) << MAP_TILE_DEPTH_SHIFT;
        (*(u8*)&tile->depth_half_height) = ((*(u8*)&tile->depth_half_height) & MAP_TILE_HALF_HEIGHT_MASK) | value;
        break;
    case MAP_TILE_DATA_SLOPE_TYPE:
        tile->slope_type = value;
        break;
    case MAP_TILE_DATA_CEILING_DEPTH:
        tile->ceiling_depth_and_marks.ceiling_depth = value;
        break;
    case MAP_TILE_DATA_BLOCKED:
        tile->flags.blocked = value;
        break;
    case MAP_TILE_DATA_UNTARGETABLE:
        tile->flags.untargetable = value;
        break;
    case MAP_TILE_DATA_SHADING:
        tile->flags.shading = value;
        break;
    case MAP_TILE_DATA_UNKNOWN_FLAGS_4_6:
        tile->flags._unknown_00 = value;
        break;
    case MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK:
        tile->camera_block_masks = (tile->camera_block_masks & MAP_TILE_CAMERA_BLOCK_STEEP_MASK)
            | (value & MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK);
        break;
    case 15:
        while (attempts < 1000) {
            result = map_tile_data_15_stub();
            if (result == 0) {
                break;
            }
            attempts++;
        }
        if (attempts == 1000) {
            main_system_handle_battle_load_exception(0x80);
        }
        break;
    default:
        result = -2;
        break;
    }
    return result;
}

/* Selector 15 of map_set_tile_data_value retries this until it returns 0, which it does at
 * once. The 8-byte frame is one the retail build kept for an unreferenced local. */
s32 map_tile_data_15_stub(void) {
    char unused[8];

    return 0;
}

void formula_force_attack_miss(void) {
    g_action_target_data->hit = 0;
    g_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
    /* The accuracy field is cleared as a 16-bit value (sh). */
    g_action_target_data->attack_accuracy = 0;
}

/* Miss decided by the accuracy roll. */
void formula_cause_action_miss(void) {
    g_action_target_data->hit = 0;
    g_action_target_data->miss_type = ACTION_MISS_TYPE_ACCURACY_MISS;
}

/* Dance/Song hit check: accuracy scaled by X%, then an X% roll to hit. */
s32 formula_calculate_dance_song_hit(void) {
    action_data_t* action = g_action_target_data;
    u8 hit_percent = g_action_current_ability.range_data.x;
    s32 action_state = g_action_state;

    action->attack_accuracy = action->attack_accuracy * hit_percent / 100;
    if (action_state != ACTION_STATE_EXECUTE || main_util_roll_pass_fail(100, hit_percent) == 0) {
        return 0;
    }
    formula_cause_action_miss();
    return 1;
}

/* Picks the knockback destination for the current action's target.
 *
 * Runs only while g_action_current_ability.target_count is 1. An immortal target, a target
 * with both low team-flag bits set, the attacker itself and a mounted pair are
 * never pushed. The target moves one tile away from the attacker along the
 * dominant axis, and the tile must be neither untargetable, deep, steep, lava
 * nor an obstacle. On success g_action_current_ability.knockback_flags records the knockback
 * kind (0x82 for a flier that is neither frog nor chicken, else 0x81),
 * [0xa]..[0xc] the destination x, y and level, and the target's action data
 * gets the knockback special effect.
 *
 * abs() is the builtin: its MIPS abssi2 pattern emits the target's
 * bgez/move/negu sequence. The coordinate stores are array elements so their
 * aggregate aliasing keeps the special-effect load after them. */
void formula_calculate_knockback(void) {
    unit_stats_t* target;
    unit_stats_t* attacker;
    map_tile_t* tile;
    u8 surface;
    s32 dx;
    s32 dy;
    s32 adx;
    s32 ady;
    s32 direction;
    s32 x;
    s32 y;
    s32 level;

    if (g_action_current_ability.target_count != 1) {
        return;
    }
    target = g_action_target;
    if (target->team_flags & BATTLE_TEAM_FLAG_IMMORTAL) {
        return;
    }
    if ((target->team_flags & BATTLE_TEAM_IMPORTANCE_MASK) == BATTLE_TEAM_IMPORTANCE_MASK) {
        return;
    }
    attacker = g_action_attacker;
    if (target == attacker) {
        return;
    }
    if (target->mount_info != 0) {
        return;
    }
    dx = target->x - attacker->x;
    dy = target->position.y - attacker->position.y;
    adx = abs(dx);
    ady = abs(dy);
    if (adx + ady == 0) {
        return;
    }
    x = target->x;
    y = target->position.y;
    if (ady < adx) {
        if (dx > 0) {
            direction = 3;
            x++;
        } else {
            direction = 1;
            x--;
        }
    } else {
        if (dy > 0) {
            direction = 2;
            y++;
        } else {
            direction = 0;
            y--;
        }
    }
    level = move_check_knockback_destination(direction, x, y);
    if (level < 0) {
        return;
    }
    tile = &g_map_tile_data[(level << 8) + y * g_map_max_x + x];
    surface = tile->surface.type;
    if (tile->flags.untargetable) {
        return;
    }
    if (tile->depth_half_height.depth != 0) {
        return;
    }
    if (tile->depth_half_height.half_height >= 3) {
        return;
    }
    if (surface == MAP_SURFACE_LAVA || surface == MAP_SURFACE_OBSTACLE) {
        return;
    }
    if (!(move_get_effective_flags(g_action_target) & BATTLE_EFFECTIVE_MOVEMENT_FLY)) {
        g_action_current_ability.knockback_flags = BATTLE_KNOCKBACK_PENDING | BATTLE_KNOCKBACK_FALL_DAMAGE;
    } else if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
        & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG))) {
        g_action_current_ability.knockback_flags = BATTLE_KNOCKBACK_PENDING | BATTLE_KNOCKBACK_FALL_DAMAGE;
    } else {
        g_action_current_ability.knockback_flags = BATTLE_KNOCKBACK_PENDING | BATTLE_KNOCKBACK_NO_FALL_DAMAGE;
    }
    g_action_current_ability.target_new_x = x;
    g_action_current_ability.target_new_y = y;
    g_action_current_ability.target_new_elevation = level;
    g_action_target_data->special_effect |= ACTION_SPECIAL_EFFECT_KNOCKBACK;
}

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

/* Cancel the action on this target: no hit, damage, accuracy or special
 * effect, and a pending weapon proc and knockback are dropped. */
void formula_nullify_action(void) {
    action_data_t* target_data;
    action_data_t* target_data_reload;

    g_action_target_data->hit = 0;
    target_data = g_action_target_data;
    target_data->miss_type = ACTION_MISS_TYPE_NULLIFIED;
    target_data_reload = g_action_target_data;
    target_data->hp_damage = 0;
    /* 16-bit stores clear the byte pairs at 0x2a and 0x10. */
    target_data->attack_accuracy = 0;
    target_data->special_effect = 0;
    target_data_reload->attack_type = 0;
    formula_clear_nullify_flags();
    action_clear_knockback_flag();
}

/* Target affinities on the stored HP damage: absorb flags the action, nullify
 * cancels it, half halves, weak doubles. Half and weak both apply. */
void formula_modify_damage_for_element(s32 element) {
    u8 element_mask;
    unit_stats_t* unit;
    action_data_t* absorb_action;
    action_data_t* half_action;
    action_data_t* weak_action;

    element_mask = element;

    if (g_action_target->elemental_affinity[ELEMENTAL_AFFINITY_ABSORB] & element_mask) {
        absorb_action = g_action_target_data;
        /* The target updates the combined special-effect field as a 16-bit value. */
        absorb_action->special_effect |= ACTION_SPECIAL_EFFECT_ELEMENTAL_ABSORPTION;
    }
    unit = g_action_target;
    if (unit->elemental_affinity[ELEMENTAL_AFFINITY_NULLIFY] & element_mask) {
        formula_nullify_action();
        return;
    }
    if (unit->elemental_affinity[ELEMENTAL_AFFINITY_HALF] & element_mask) {
        half_action = g_action_target_data;
        /* hp_damage is read signed (lh) for the division. */
        half_action->hp_damage = half_action->hp_damage / 2;
    }
    if (g_action_target->elemental_affinity[ELEMENTAL_AFFINITY_WEAK] & element_mask) {
        weak_action = g_action_target_data;
        weak_action->special_effect |= ACTION_SPECIAL_EFFECT_ELEMENTAL_WEAKNESS;
        weak_action->hp_damage = weak_action->hp_damage * 2;
    }
}

/* Collects the current target's physical evade sources for the ability being
 * resolved: class evade, accessory evade and one value per hand. A hand
 * contributes its weapon's evade when Weapon Guard is active and the unit can
 * still evade, otherwise its shield's evade. Monsters contribute nothing past
 * the class value.
 *
 * Two spellings in the left-hand block are both needed for one local-allocation
 * tie that otherwise puts the weapon record's secondary_data_id in $a2 and the
 * shield record's in $a0, the reverse of the target:
 *
 *  - staging the left weapon's type_flags through `weapon_secondary` before
 *    copying it into `weapon_flags` makes that load reuse the pseudo
 *    local-alloc ties to $a0, which swaps the two secondary ids' registers;
 *  - reading the left shield's flags through an `item_data_t*` gives the
 *    shield record its own address pseudo instead of folding the address into
 *    the load, which keeps the swap from being undone.
 *
 * Spelling the weapon half as a record pointer as well loses three
 * instructions. */
void formula_store_physical_evade_values(void) {
    s32 weapon_guard;
    u8 weapon_flags;
    u8 shield_flags;
    u8 shield_secondary;
    item_data_t* shield_record;
    u8 weapon_secondary;

    weapon_guard = 0;
    if ((g_action_target->ability_flags.reaction[3] & BATTLE_REACTION_SET_4_WEAPON_GUARD) != 0) {
        weapon_guard = battle_unit_get_action_restriction(g_action_target) == BATTLE_UNIT_ACTION_RESTRICTION_NONE;
    }
    g_action_current_ability.base_hit = 100;
    g_action_current_ability.accessory_evade = 0;
    g_action_current_ability.right_hand_evade = 0;
    g_action_current_ability.left_hand_evade = 0;
    g_action_current_ability.class_evade
        = g_action_target->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    if ((g_action_target->unit_flags & UNIT_FLAG_MONSTER) != 0) {
        return;
    }

    weapon_flags = g_main_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]].type_flags;
    if ((weapon_flags & ITEM_TYPE_FLAG_ACCESSORY) != 0) {
        g_action_current_ability.accessory_evade
            = g_main_item_accessory_data
                  [g_main_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]]
                          .secondary_data_id]
                      .physical_evade;
    }

    weapon_flags
        = g_main_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]].type_flags;
    weapon_secondary
        = g_main_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]].secondary_data_id;
    shield_flags
        = g_main_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD]].type_flags;
    shield_secondary
        = g_main_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD]].secondary_data_id;
    if ((weapon_flags & ITEM_TYPE_FLAG_WEAPON) != 0 && weapon_guard != 0) {
        g_action_current_ability.right_hand_evade = g_main_item_weapon_data[weapon_secondary].evade;
    } else if ((shield_flags & ITEM_TYPE_FLAG_SHIELD) != 0) {
        g_action_current_ability.right_hand_evade = g_main_item_shield_data[shield_secondary].physical_evade;
    }

    weapon_secondary
        = g_main_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]].type_flags;
    weapon_flags = weapon_secondary;
    weapon_secondary
        = g_main_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]].secondary_data_id;
    shield_record = &g_main_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD]];
    shield_flags = shield_record->type_flags;
    shield_secondary
        = g_main_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD]].secondary_data_id;
    if ((weapon_flags & ITEM_TYPE_FLAG_WEAPON) != 0 && weapon_guard != 0) {
        g_action_current_ability.left_hand_evade = g_main_item_weapon_data[weapon_secondary].evade;
    } else if ((shield_flags & ITEM_TYPE_FLAG_SHIELD) != 0) {
        g_action_current_ability.left_hand_evade = g_main_item_shield_data[shield_secondary].physical_evade;
    }
}

/* Reset the base hit to 100 and load the target's accessory and shield
 * magic evades; monsters get no equipment evade. */
void formula_store_magical_evade_values(void) {
    unit_stats_t* target;
    item_data_t* items;
    item_data_t* item;
    s32 secondary;

    target = g_action_target;
    g_action_current_ability.base_hit = 100;
    g_action_current_ability.accessory_evade = 0;
    g_action_current_ability.right_hand_evade = 0;
    g_action_current_ability.left_hand_evade = 0;
    g_action_current_ability.class_evade = 0;
    if (target->unit_flags & UNIT_FLAG_MONSTER) {
        return;
    }
    items = g_main_item_primary_data;
    item = &items[target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]];
    if (item->type_flags & ITEM_TYPE_FLAG_ACCESSORY) {
        g_action_current_ability.accessory_evade = g_main_item_accessory_data[item->secondary_data_id].magic_evade;
    }
    item = &items[target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD]];
    secondary = item->secondary_data_id;
    if (item->type_flags & ITEM_TYPE_FLAG_SHIELD) {
        g_action_current_ability.right_hand_evade = g_main_item_shield_data[secondary].magic_evade;
    }
    item = &items[target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD]];
    secondary = item->secondary_data_id;
    if (item->type_flags & ITEM_TYPE_FLAG_SHIELD) {
        g_action_current_ability.left_hand_evade = g_main_item_shield_data[secondary].magic_evade;
    }
}

/* Concentrate on the attacker: the target's four evades become 0. */
void formula_calculate_concentrate(void) {
    if (g_action_attacker->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_CONCENTRATE) {
        mem_zero(&g_action_current_ability.accessory_evade, 4);
    }
}

/* A Dark or Confused attacker halves the base hit, which doubles every evade.
 * Then the Transparent step. */
void formula_calculate_dark_confuse(void) {
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DARKNESS)]
        & (STATUS_BYTE_MASK(STATUS_ID_DARKNESS) | STATUS_BYTE_MASK(STATUS_ID_CONFUSION))) {
        u8* base_hit = &g_action_current_ability.base_hit;
        *base_hit = *base_hit >> 1;
    }
    formula_calculate_transparent();
}

/* Abandon: a target free to act halves the base hit, which doubles each of its
 * evades. */
void formula_calculate_abandon(void) {
    u8* base_hit;
    if ((battle_unit_get_action_restriction(g_action_target) == BATTLE_UNIT_ACTION_RESTRICTION_NONE)
        && (g_action_target->ability_flags.reaction[3] & BATTLE_REACTION_SET_4_ABANDON)) {
        base_hit = &g_action_current_ability.base_hit;
        *base_hit = *base_hit >> 1;
        g_action_target_data->reaction_id = ABILITY_ID_REACTION_ABANDON;
    }
}

/* Target statuses on the evades: Don't Act, Sleep, Stop, Confusion, Charging
 * or Performing clear all four; Defending halves the base hit, which doubles
 * them. */
void formula_apply_evasion_changes_due_to_statuses(void) {
    unit_stats_t* target;
    u8 flag;
    u8* base_hit;

    target = g_action_target;
    flag = (target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)]
               & (STATUS_BYTE_MASK(STATUS_ID_SLEEP) | STATUS_BYTE_MASK(STATUS_ID_DONT_ACT)))
        != 0;
    if (target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_STOP)] & STATUS_BYTE_MASK(STATUS_ID_STOP)) {
        flag = 1;
    }
    if (target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)] & STATUS_BYTE_MASK(STATUS_ID_CONFUSION)) {
        flag = 1;
    }
    if (target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
        & (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING))) {
        flag = 1;
    }
    if (flag) {
        mem_zero(&g_action_current_ability.accessory_evade, 4);
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEFENDING)]
        & STATUS_BYTE_MASK(STATUS_ID_DEFENDING)) {
        base_hit = &g_action_current_ability.base_hit;
        *base_hit >>= 1;
    }
}

/* A Transparent attacker cannot be evaded: the four evades become 0. */
void formula_calculate_transparent(void) {
    /* Status 3 bit 0x10: Transparent. */
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
        & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT)) {
        mem_zero(&g_action_current_ability.accessory_evade, 4);
    }
}

/* Classifies the attack direction and drops evades the target cannot use.
 *
 * The dominant axis of the attacker-to-target offset is compared with the
 * target's facing (position bits 8-11); on a diagonal the target counts as
 * hit from the front when it faces either axis toward the attacker, and a
 * shared tile counts as the back. Side and back attacks clear class evade,
 * back attacks also both shield evades. */
void formula_calculate_facing_evade(void) {
    s32 dx;
    s32 dy;
    u8 distance_x;
    u8 distance_y;
    s32 direction;
    s32 facing;
    u8* result;

    dx = g_action_target->x - g_action_attacker->x;
    distance_x = dx < 0 ? -dx : dx;
    dy = g_action_target->position.y - g_action_attacker->position.y;
    distance_y = dy < 0 ? -dy : dy;
    direction = BATTLE_ABILITY_FACING_SIDE;
    if (distance_y < distance_x) {
        if (dx > 0) {
            facing = g_action_target->position.facing << 8;
            if (facing == BATTLE_UNIT_POSITION_FACING_WEST) {
                direction = BATTLE_ABILITY_FACING_FRONT;
            } else if (facing == BATTLE_UNIT_POSITION_FACING_EAST) {
                direction = BATTLE_ABILITY_FACING_BACK;
            }
        }
        if (dx < 0) {
            facing = g_action_target->position.facing << 8;
            if (facing == BATTLE_UNIT_POSITION_FACING_EAST) {
                direction = BATTLE_ABILITY_FACING_FRONT;
            } else if (facing == BATTLE_UNIT_POSITION_FACING_WEST) {
                direction = BATTLE_ABILITY_FACING_BACK;
            }
        }
    } else if (distance_x < distance_y) {
        if (dy > 0) {
            facing = g_action_target->position.facing << 8;
            if (facing == BATTLE_UNIT_POSITION_FACING_SOUTH) {
                direction = BATTLE_ABILITY_FACING_FRONT;
            } else if (facing == BATTLE_UNIT_POSITION_FACING_NORTH) {
                direction = BATTLE_ABILITY_FACING_BACK;
            }
        }
        if (dy < 0) {
            facing = g_action_target->position.facing << 8;
            if (facing == BATTLE_UNIT_POSITION_FACING_NORTH) {
                direction = BATTLE_ABILITY_FACING_FRONT;
            } else if (facing == BATTLE_UNIT_POSITION_FACING_SOUTH) {
                direction = BATTLE_ABILITY_FACING_BACK;
            }
        }
    } else if (dx > 0 && (g_action_target->position.facing << 8) == BATTLE_UNIT_POSITION_FACING_WEST) {
        direction = BATTLE_ABILITY_FACING_FRONT;
    } else if (dx < 0 && (g_action_target->position.facing << 8) == BATTLE_UNIT_POSITION_FACING_EAST) {
        direction = BATTLE_ABILITY_FACING_FRONT;
    } else if (dy > 0 && (g_action_target->position.facing << 8) == BATTLE_UNIT_POSITION_FACING_SOUTH) {
        direction = BATTLE_ABILITY_FACING_FRONT;
    } else if (dy < 0 && (g_action_target->position.facing << 8) == BATTLE_UNIT_POSITION_FACING_NORTH) {
        direction = BATTLE_ABILITY_FACING_FRONT;
    } else if (dx == 0 && dy == 0) {
        direction = BATTLE_ABILITY_FACING_BACK;
    }
    result = &g_action_current_ability.facing_modifier;
    *result = direction;
    if (*result != BATTLE_ABILITY_FACING_FRONT) {
        g_action_current_ability.class_evade = 0;
    }
    if (*result == BATTLE_ABILITY_FACING_BACK) {
        g_action_current_ability.right_hand_evade = 0;
        g_action_current_ability.left_hand_evade = 0;
    }
}

/* Bows and crossbows lose a quarter of the base hit at night and another
 * quarter when the weather variable is 3 or 4 (a storm, or a snowstorm on a
 * snow map). The variable is read as it is, not through
 * map_get_effective_weather. */
void formula_apply_weather_effects_on_bows(void) {
    u8* base_hit;
    s32 weather;
    s32 time_of_day;

    if (g_main_item_primary_data[g_action_current_ability.weapon_id].type == ITEM_TYPE_CROSSBOW
        || g_main_item_primary_data[g_action_current_ability.weapon_id].type == ITEM_TYPE_BOW) {
        weather = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        time_of_day = script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY);
        if (time_of_day == 1) { /* night */
            g_action_current_ability.base_hit = g_action_current_ability.base_hit * 3 / 4;
        }
        /* The raw weather variable, not map_get_effective_weather: a
         * storm strength counts on a snow map and on a map that ignores weather. */
        if (weather == BATTLE_WEATHER_STORM || weather == BATTLE_WEATHER_STRONG_STORM) {
            base_hit = &g_action_current_ability.base_hit;
            *base_hit = *base_hit * 3 / 4;
        }
    }
}

/* Scale the action accuracy by each evade source, then roll them in turn.
 *
 * Applies to the basic attack (ability id 0) or abilities flagged evadeable
 * (flags4 bit 0x02). Any evade at or above the base hit forces a miss;
 * otherwise the accuracy becomes the product of the four (hit - evade)
 * percentages. The rolls then run accessory, right hand, left hand and
 * class evade, reporting the item that blocked. */
void formula_calculate_final_hit_percent(void) {
    u8 hit;
    u8 accessory;
    u8 right;
    u8 left;
    u8 evade;
    u8 item;

    if ((g_action_current_ability.ability_id != 0)
        && !(g_action_current_ability.range_data.flags_4 & ABILITY_SECONDARY_FLAG_4_EVADEABLE)) {
        return;
    }
    hit = g_action_current_ability.base_hit;
    accessory = g_action_current_ability.accessory_evade;
    if ((accessory >= hit) || ((right = g_action_current_ability.right_hand_evade) >= hit)
        || ((left = g_action_current_ability.left_hand_evade) >= hit)
        || ((evade = g_action_current_ability.class_evade) >= hit)) {
        g_action_target_data->hit = 0;
        g_action_target_data->attack_accuracy = 0;
    } else {
        g_action_target_data->attack_accuracy
            = ((hit - accessory) * 100 / hit * ((hit - right) * 100 / hit) * ((hit - left) * 100 / hit)
                  * ((hit - evade) * 100 / hit) / 1000000)
            * g_action_target_data->attack_accuracy / 100;
    }
    if (formula_calculate_attack_evaded(g_action_current_ability.accessory_evade, hit, ACTION_MISS_TYPE_ACCESSORY_EVADE,
            g_action_target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY])
        != 0) {
        return;
    }
    item = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    if (item == ITEM_ID_NONE) {
        item = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
    }
    if (formula_calculate_attack_evaded(
            g_action_current_ability.right_hand_evade, hit, ACTION_MISS_TYPE_RIGHT_HAND_EVADE, item)
        != 0) {
        return;
    }
    item = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
    if (item == ITEM_ID_NONE) {
        item = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
    }
    if (formula_calculate_attack_evaded(
            g_action_current_ability.left_hand_evade, hit, ACTION_MISS_TYPE_LEFT_HAND_EVADE, item)
        != 0) {
        return;
    }
    formula_calculate_attack_evaded(
        g_action_current_ability.class_evade, hit, ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD, ITEM_ID_NONE);
}

/* Set XA and YA from the current weapon's item type.
 *
 * Bare hands give
 * XA = PA and YA = PA * Brave / 100 (at least 1); otherwise YA is the weapon
 * power and XA depends on the type (PA; (PA + Speed) / 2; PA * Brave / 100;
 * MA; random PA; (PA + MA) / 2), leaving XA unchanged for other types.
 * The attacker's stats must be u8 locals for the (PA + MA) zero-extension.
 */
void formula_calculate_base_xa(void) {
    u8 type;
    u8 pa;
    u8 ma;
    s16 value;
    u8 brave;

    type = g_main_item_primary_data[g_action_current_ability.weapon_id].type;
    pa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    ma = g_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    brave = g_action_attacker->brave;
    if (type == ITEM_TYPE_UNARMED) {
        g_action_current_ability.xa = pa;
        value = pa * brave / 100;
        g_action_current_ability.ya = value;
        if (value == 0) {
            g_action_current_ability.ya = value + 1;
        }
        return;
    }
    g_action_current_ability.ya = g_action_current_ability.weapon_data.power;
    if (type == ITEM_TYPE_SWORD || type == ITEM_TYPE_CROSSBOW || type == ITEM_TYPE_ROD || type == ITEM_TYPE_SPEAR) {
        g_action_current_ability.xa = pa;
    } else if (type == ITEM_TYPE_KNIFE || type == ITEM_TYPE_NINJA_SWORD || type == ITEM_TYPE_BOW
        || type == ITEM_TYPE_SHURIKEN) {
        g_action_current_ability.xa = (pa + g_action_attacker->attributes[UNIT_ATTRIBUTE_SPEED]) >> 1;
    } else if (type == ITEM_TYPE_KNIGHT_SWORD || type == ITEM_TYPE_KATANA) {
        value = pa * brave / 100;
        g_action_current_ability.xa = value;
        if (value == 0) {
            g_action_current_ability.xa = value + 1;
        }
    } else if (type == ITEM_TYPE_STAFF || type == ITEM_TYPE_STICK) {
        g_action_current_ability.xa = ma;
    } else if (type == ITEM_TYPE_AXE || type == ITEM_TYPE_HAMMER || type == ITEM_TYPE_BAG) {
        g_action_current_ability.xa = pa * formula_get_random_0_7fff() / 0x8000 + 1;
    } else if (type == ITEM_TYPE_INSTRUMENT || type == ITEM_TYPE_DICTIONARY || type == ITEM_TYPE_CLOTH) {
        g_action_current_ability.xa = (pa + ma) >> 1;
    }
}

/* XA = MA, YA = the ability's Y. */
void formula_store_ma_and_y(void) {
    s32 ma = g_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    s32 y = g_action_current_ability.range_data.y;

    g_action_current_ability.ya = y;
    g_action_current_ability.xa = ma;
}

/* XA = MA, YA = (MA + Y) / 2. */
void formula_store_ma_and_ma_plus_y_divided_by_two(void) {
    g_action_current_ability.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    g_action_current_ability.ya
        = (g_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK] + g_action_current_ability.range_data.y) >> 1;
}

/* XA = PA, YA = (PA + Y) / 2. */
void formula_store_pa_and_pa_plus_y_divided_by_two(void) {
    g_action_current_ability.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    g_action_current_ability.ya
        = (g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK] + g_action_current_ability.range_data.y) >> 1;
}

/* XA = MA, YA = (PA + Y) / 2. */
void formula_store_ma_and_pa_plus_y_divided_by_two(void) {
    g_action_current_ability.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    g_action_current_ability.ya
        = (g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK] + g_action_current_ability.range_data.y) >> 1;
}

/* XA = MA, YA = the ability's X. */
void formula_store_ma_and_x(void) {
    s32 ma = g_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    s32 x = g_action_current_ability.range_data.x;

    g_action_current_ability.ya = x;
    g_action_current_ability.xa = ma;
}

/* XA = PA, YA = the ability's X. */
void formula_store_pa_and_x(void) {
    s32 pa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    s32 x = g_action_current_ability.range_data.x;

    g_action_current_ability.ya = x;
    g_action_current_ability.xa = pa;
}

/* XA = PA, YA = the weapon's power. */
void formula_store_pa_and_weapon_power(void) {
    s32 pa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    s32 weapon_power = g_action_current_ability.weapon_data.power;

    g_action_current_ability.ya = weapon_power;
    g_action_current_ability.xa = pa;
}

/* XA = PA, YA = the ability's Y. */
void formula_store_pa_and_y(void) {
    s32 pa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    s32 y = g_action_current_ability.range_data.y;

    g_action_current_ability.ya = y;
    g_action_current_ability.xa = pa;
}

/* XA = Speed, YA = the ability's X. */
void formula_store_speed_and_x(void) {
    s32 speed = g_action_attacker->attributes[UNIT_ATTRIBUTE_SPEED];
    s32 x = g_action_current_ability.range_data.x;

    g_action_current_ability.ya = x;
    g_action_current_ability.xa = speed;
}

/* XA = PA, YA = the weapon's power + Y. */
void formula_store_pa_and_weapon_power_plus_y(void) {
    s32 pa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    s32 weapon_power = g_action_current_ability.weapon_data.power;
    s32 y = g_action_current_ability.range_data.y;

    g_action_current_ability.ya = weapon_power + y;
    g_action_current_ability.xa = pa;
}

/* XA and YA of formula 0x64 (Jump): XA = PA, or PA * 3/2 with a polearm; YA =
 * weapon power, or bare-handed XA * Brave / 100 (at least 1). */
void formula_init_weapon_xa_ya(void) {
    u8 weapon_type = g_main_item_primary_data[g_action_current_ability.weapon_id].type;

    if (weapon_type == ITEM_TYPE_SPEAR) {
        g_action_current_ability.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK] * 3 / 2;
    } else {
        g_action_current_ability.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    }

    if (weapon_type == ITEM_TYPE_UNARMED) {
        g_action_current_ability.ya = g_action_current_ability.xa * g_action_attacker->brave / 100;
        if (g_action_current_ability.ya == 0) {
            g_action_current_ability.ya++;
        }
    } else {
        g_action_current_ability.ya = g_action_current_ability.weapon_data.power;
    }
}

/* Add the Charge ability's power to XA (0 for every other action). */
void formula_apply_charge(void) {
    g_action_current_ability.xa += g_action_current_ability.charge_power;
}

/* Strengthen step for weapon attacks: XA * 5/4 when the attacker strengthens
 * one of the weapon's elements. */
void formula_apply_weapon_element_strengthen(void) {
    if (g_action_attacker->elemental_affinity[ELEMENTAL_AFFINITY_STRENGTHEN]
        & g_action_current_ability.weapon_data.element) {
        g_action_current_ability.xa = g_action_current_ability.xa * 5 / 4;
    }
}

/* Strengthen step: XA * 5/4 when the attacker strengthens one of the ability's
 * elements. */
void formula_apply_elemental_strengthen(void) {
    if (g_action_attacker->elemental_affinity[ELEMENTAL_AFFINITY_STRENGTHEN]
        & g_action_current_ability.range_data.element) {
        g_action_current_ability.xa = g_action_current_ability.xa * 5 / 4;
    }
}

/* Attacker supports on XA, in order: Two Hands * 2 (active, on a weapon that
 * allows it and is not two-handed already), Attack Up * 4/3, Martial Arts *
 * 3/2 with no weapon (weapon id ITEM_ID_NOTHING). */
void formula_apply_physical_attack_supports(void) {
    u8 weapon_characteristics = g_action_current_ability.weapon_data.flags;

    if (!(weapon_characteristics & WEAPON_FLAG_FORCED_TWO_HANDS)
        && (weapon_characteristics & WEAPON_FLAG_TWO_HANDS_COMPATIBLE) && g_action_current_ability.two_hands_active) {
        g_action_current_ability.xa = g_action_current_ability.xa * 2;
    }

    if (g_action_attacker->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_ATTACK_UP) {
        g_action_current_ability.xa = g_action_current_ability.xa * 4 / 3;
    }

    if (g_action_current_ability.weapon_id == ITEM_ID_NOTHING
        && (g_action_attacker->ability_flags.support[2] & BATTLE_SUPPORT_SET_3_MARTIAL_ARTS)) {
        g_action_current_ability.xa = g_action_current_ability.xa * 3 / 2;
    }
}

/* Attacker supports on XA for ability formulas: Attack Up * 4/3, then Martial
 * Arts * 3/2 when the weapon's item type is unarmed. No Two Hands step. */
void formula_apply_attack_up_and_martial_arts(void) {
    if (g_action_attacker->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_ATTACK_UP) {
        g_action_current_ability.xa = g_action_current_ability.xa * 4 / 3;
    }

    {
        u8 weapon_id = g_action_current_ability.weapon_id;
        unit_stats_t* attacker = g_action_attacker;
        u8 weapon_type = g_main_item_primary_data[weapon_id].type;
        u8 has_martial_arts = attacker->ability_flags.support[2] & BATTLE_SUPPORT_SET_3_MARTIAL_ARTS;

        if (has_martial_arts && weapon_type == ITEM_TYPE_UNARMED) {
            g_action_current_ability.xa = g_action_current_ability.xa * 3 / 2;
        }
    }
}

/* Magic Attack Up on the attacker: XA * 4/3. */
void formula_apply_magic_attack_up(void) {
    if (g_action_attacker->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_MAGIC_ATTACK_UP) {
        g_action_current_ability.xa = g_action_current_ability.xa * 4 / 3;
    }
}

/* Attacker statuses on XA: Berserk * 3/2; a Frog's XA is 1. */
void formula_apply_attacker_berserk_frog(void) {
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_BERSERK)]
        & STATUS_BYTE_MASK(STATUS_ID_BERSERK)) {
        g_action_current_ability.xa = g_action_current_ability.xa * 3 / 2;
    }
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
        g_action_current_ability.xa = 1;
    }
}

/* Defense Up on the target: XA * 2/3. */
void formula_apply_defense_up(void) {
    /* Battle Stats 0x90 bit 0x08 is Defense Up. */
    if (g_action_target->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_DEFENSE_UP) {
        g_action_current_ability.xa = g_action_current_ability.xa * 2 / 3;
    }
}

/* Magic Defense Up on the target: XA * 2/3. */
void formula_apply_magic_defense_up(void) {
    /* Battle Stats 0x90 bit 0x02 is Magic Defense Up. */
    if (g_action_target->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_MAGIC_DEFENSE_UP) {
        g_action_current_ability.xa = g_action_current_ability.xa * 2 / 3;
    }
}

/* Target statuses on physical XA: Protect * 2/3; Sleep or Charging * 3/2;
 * Chicken or Frog * 3/2. */
void formula_apply_target_xa_affecting_statuses(void) {
    unit_stats_t* target;

    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PROTECT)]
        & STATUS_BYTE_MASK(STATUS_ID_PROTECT)) {
        g_action_current_ability.xa = g_action_current_ability.xa * 2 / 3;
    }

    target = g_action_target;
    if ((target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)] & STATUS_BYTE_MASK(STATUS_ID_SLEEP))
        || (target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
            & STATUS_BYTE_MASK(STATUS_ID_CHARGING))) {
        g_action_current_ability.xa = g_action_current_ability.xa * 3 / 2;
    }

    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHICKEN)]
        & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG))) {
        g_action_current_ability.xa = g_action_current_ability.xa * 3 / 2;
    }
}

/* Target statuses on magical XA: Shell * 2/3; Chicken or Frog * 3/2. */
void formula_apply_target_magical_status_xa_modifiers(void) {
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SHELL)] & STATUS_BYTE_MASK(STATUS_ID_SHELL)) {
        g_action_current_ability.xa = g_action_current_ability.xa * 2 / 3;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHICKEN)]
        & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG))) {
        g_action_current_ability.xa = g_action_current_ability.xa * 3 / 2;
    }
}

/* Critical hit step: a 4 in 100 roll, never outside execution. XA gains
 * rnd(0..XA-1) and the target may be knocked back. */
void formula_calculate_critical_hit(void) {
    s32 rnd;
    s16 base_xa;
    if (formula_roll_random_at_least(0x64, 4) == 0) {
        g_action_target_data->critical = 1;
        rnd = formula_get_random_0_7fff();
        base_xa = g_action_current_ability.xa;
        g_action_current_ability.xa = base_xa + rnd * base_xa / 32768;
        formula_calculate_knockback();
    }
}

/* Sets the target's result to HP damage XA * YA. */
void formula_store_xa_times_ya_damage(void) {
    s16 damage;
    action_data_t* action;

    damage = g_action_current_ability.xa * g_action_current_ability.ya;
    action = g_action_target_data;
    action->attack_type = ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = damage;
}

/* Hit chance of the accuracy formulas: XA + YA, left in hp_damage for
 * formula_use_hp_damage_as_action_hit_percent. YA gets zodiac
 * compatibility here (the callers apply it to XA), except for Golem. */
void formula_store_xa_plus_ya_status_damage(void) {
    u16 original_xa;

    if (g_action_current_ability.ability_id != ABILITY_ID_SUMMON_MAGIC_GOLEM) {
        original_xa = g_action_current_ability.xa;
        g_action_current_ability.xa = g_action_current_ability.ya;
        formula_apply_zodiac_compatibility();
        g_action_current_ability.ya = g_action_current_ability.xa;
        g_action_current_ability.xa = original_xa;
    }
    g_action_target_data->hp_damage = g_action_current_ability.xa + g_action_current_ability.ya;
}

/* HP damage of Y% of the target's max HP, rounded up. */
void formula_calculate_hp_percent_damage(void) {
    action_data_t* action;
    s32 damage;

    damage = (g_action_target->max_hp * g_action_current_ability.range_data.y + 99) / 100;
    action = g_action_target_data;
    action->attack_type |= ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = damage;
}

/* MP damage of Y% of the target's max MP, rounded up. */
void formula_calculate_mp_percent_damage(void) {
    action_data_t* action;
    s32 damage;

    damage = (g_action_target->max_mp * g_action_current_ability.range_data.y + 99) / 100;
    action = g_action_target_data;
    action->attack_type = ACTION_TYPE_MP_DAMAGE;
    action->mp_damage = damage;
}

/* HP damage equal to the target's current HP - 1 (0 when it has none). */
void formula_calculate_gravi2_damage(void) {
    unit_stats_t* target;

    target = g_action_target;
    if (target->hp != 0) {
        g_action_target_data->hp_damage = g_action_target->hp - 1;
    } else {
        g_action_target_data->hp_damage = 0;
    }
    g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
}

/* Gil stolen: attacker Level * Speed, capped at the war funds when the target
 * is on team 0 (blue). Nothing to take is a miss. */
void formula_calculate_stolen_gil(void) {
    unit_stats_t* attacker;
    action_data_t* action;
    action_data_t* target_action;
    action_data_t* other;
    s32 amount;
    s32 available;
    s16 stolen;

    attacker = g_action_attacker;
    amount = attacker->attributes[UNIT_ATTRIBUTE_SPEED] * attacker->level;
    if (!(g_action_target->team_flags & BATTLE_TEAM_MASK)) {
        available = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
        if (available < amount) {
            amount = available;
        }
    }
    if (amount == 0) {
        formula_force_attack_miss();
        return;
    }
    action = g_action_attacker_data;
    action->hit = 1;
    other = g_action_attacker_data;
    action->gil_change = amount;
    other->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    stolen = g_action_attacker_data->gil_change;
    target_action = g_action_target_data;
    target_action->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    target_action->gil_change = -stolen;
}

/*
 * Record the stolen EXP on both action results.
 *
 * The target's loss is encoded by adding 0x80 to the amount.
 */
void formula_set_exp_stolen(void) {
    u8 amount;
    u8 available_exp;

    amount = g_action_attacker->attributes[UNIT_ATTRIBUTE_SPEED] + g_action_current_ability.range_data.y;
    if (amount > 100) {
        amount = 100;
    }
    available_exp = g_action_target->experience;
    if (available_exp < amount) {
        amount = available_exp;
    }
    if (amount == 0) {
        formula_force_attack_miss();
        return;
    }
    g_action_attacker_data->exp_change = amount;
    g_action_attacker_data->hit = 1;
    g_action_attacker_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    g_action_target_data->exp_change = amount + 0x80;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x36: +PA (Y). */
void formula_add_pa_y(void) {
    g_action_target_data->pa_change = g_action_current_ability.range_data.y | ACTION_STAT_CHANGE_INCREASE;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Damage XA * rnd(1..Y). When executing, the target is knocked back if a roll
 * of 0..(its Brave - 1) comes out under a roll of 0..(attacker Brave - 1). */
void formula_apply_damage_and_knockback(void) {
    action_data_t* action;
    s32 damage;
    s32 roll;
    s32 state;

    damage = (((formula_get_random_0_7fff() * g_action_current_ability.range_data.y) / 32768) + 1)
        * (u16)g_action_current_ability.xa;
    action = g_action_target_data;
    action->attack_type = ACTION_TYPE_HP_DAMAGE;
    state = g_action_state;
    action->hp_damage = damage;
    if (state != ACTION_STATE_EXECUTE) {
        return;
    }
    roll = (formula_get_random_0_7fff() * g_action_attacker->brave) / 32768;
    if (main_util_roll_pass_fail(g_action_target->brave, roll) == 0) {
        formula_calculate_knockback();
    }
}

/* Effect of each song. XA + YA is MA + Y; the stat songs add 1. */
void formula_apply_song_abilities(void) {
    switch (g_action_current_ability.ability_id) {
    case ABILITY_ID_SONG_ANGEL_SONG:
        g_action_target_data->mp_healing = g_action_current_ability.xa + g_action_current_ability.ya;
        g_action_target_data->attack_type = ACTION_TYPE_MP_HEALING;
        return;
    case ABILITY_ID_SONG_LIFE_SONG:
        g_action_target_data->hp_healing = g_action_current_ability.xa + g_action_current_ability.ya;
        g_action_target_data->attack_type = ACTION_TYPE_HP_HEALING;
        return;
    case ABILITY_ID_SONG_CHEER_SONG:
        g_action_target_data->sp_change = ACTION_STAT_CHANGE_INCREASE | 1;
        break;
    case ABILITY_ID_SONG_BATTLE_SONG:
        g_action_target_data->pa_change = ACTION_STAT_CHANGE_INCREASE | 1;
        break;
    case ABILITY_ID_SONG_MAGIC_SONG:
        g_action_target_data->ma_change = ACTION_STAT_CHANGE_INCREASE | 1;
        break;
    case ABILITY_ID_SONG_NAMELESS_SONG:
        formula_apply_status_to_action();
        return;
    case ABILITY_ID_SONG_LAST_SONG:
        g_action_target_data->ct_change = ACTION_CT_CHANGE_QUICK;
        break;
    }
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}
