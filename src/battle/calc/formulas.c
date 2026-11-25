/* BATTLE's ability formulas and strike resolution: accuracy, zodiac compatibility, elemental damage, stat
 * modifiers and base damage; shared result modifiers and direct ability effects; the ability-formula
 * catalog that s_formulas dispatches; and the resolution of each strike through reactions, knockback
 * and the final stat, equipment and reward changes. The original compiled all of it as one object. */

#include "fft/battle/action.h"
#include "fft/battle/ai.h"
#include "fft/battle/command.h"
#include "fft/battle/formula.h"
#include "fft/battle/move.h"
#include "fft/battle/reaction.h"
#include "fft/battle/reward.h"
#include "fft/battle/target.h"
#include "fft/battle/weather.h"
#include "fft/game/ability.h"
#include "fft/game/battle_unit.h"
#include "fft/game/character_identity.h"
#include "fft/game/item.h"
#include "fft/game/job.h"
#include "fft/game/skillset.h"
#include "fft/game/status.h"
#include "fft/gfx/zodiac.h"
#include "fft/map/map.h"
#include "fft/map/map_unit.h"
#include "fft/script/script_variable.h"
#include "fft/sys/math.h"
#include "fft/sys/memory.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Chances in 100 of a critical hit and of a weapon's added effect. */
enum { FORMULA_CRITICAL_HIT_CHANCE = 4, FORMULA_ADDED_EFFECT_CHANCE = 19 };

typedef enum formula_facing_modifier {
    FORMULA_FACING_FRONT = 0,
    FORMULA_FACING_SIDE = 1,
    FORMULA_FACING_BACK = 2
} formula_facing_modifier_e;

static s32 check_knockback_destination(s32 direction, s32 x, s32 y);
static void formula_evade_calculate_transparent(void);
static void formula_apply_status_to_action(void);
static void formula_calculate_physical_damage(void);
static s32 formula_apply_status_or_fail(void);
static void action_recalculate_target_result_as_attack(void);
static void store_ability_cancellations(void);
static void action_finalize_target_result(void);
static void formula_clear_pending_proc(void);
static void move_cancel_knockback(void);
static void clear_status_changes(action_data_t* action);
static s32 store_poached_item(void);
static s32 apply_level_change(void);
static s32 reaction_is_blocked(const battle_unit_t* unit);
static s32 check_pending_reactions(const battle_unit_t* unit);
static void dispatch_evasion_reactions(void);
static void dispatch_targeted_reactions(void);
static void dispatch_damage_sharing_reactions(void);
static void dispatch_after_damage_reactions(void);
static void try_attack_flag_reactions(u16 reaction_id, u32 mask);
static void try_hp_damage_reactions(s16 reaction_id);
static void try_critical_reactions(s16 reaction_id);
static void try_mp_cost_reactions(s16 reaction_id);
static void try_reflect(void);
static void try_blade_grasp(void);
static void try_arrow_guard(void);
static void try_mp_switch(void);
static void try_distribute(void);
static void try_damage_split(void);
static s32 apply_equipment_removal(void);
static s32 apply_stat_delta(s32 mod, u8* stat, u8 max, u8 min);
static void update_attacker_exp(s32 outcome);
static void set_target(battle_unit_t* unit);
static s32 is_out_of_action(const battle_unit_t* unit);
static void apply_jp_change(battle_unit_t* unit);
static void update_control_status(battle_unit_t* unit);
static void apply_removals(s32 unit_idx);
static void apply_inflictions(s32 unit_id, s32 removal_only);

/* accuracy and damage */

/* Zodiac compatibility by the absolute difference of the two signs, 12 entries:
 * 3 and 9 apart are bad, 4 and 8 good, 6 (the opposite sign) depends on sex,
 * the rest neutral. Serpentarius returns before the table is read. */
static u8 s_zodiac_compatibility_modifiers[12] = { ZODIAC_COMPATIBILITY_NEUTRAL, ZODIAC_COMPATIBILITY_NEUTRAL,
    ZODIAC_COMPATIBILITY_NEUTRAL, ZODIAC_COMPATIBILITY_BAD, ZODIAC_COMPATIBILITY_GOOD, ZODIAC_COMPATIBILITY_NEUTRAL,
    ZODIAC_COMPATIBILITY_GENDER_DEPENDENT, ZODIAC_COMPATIBILITY_NEUTRAL, ZODIAC_COMPATIBILITY_GOOD,
    ZODIAC_COMPATIBILITY_BAD, ZODIAC_COMPATIBILITY_NEUTRAL, ZODIAC_COMPATIBILITY_NEUTRAL };

/* The statuses a rider can be inflicted with, one mask per status byte. */
static u8 s_rider_infliction_mask[STATUS_BYTE_COUNT]
    = { STATUS_BYTE_MASK(STATUS_ID_UNNAMED_00) | STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD)
              | STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
              | STATUS_BYTE_MASK(STATUS_ID_DEFENDING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING),
          0,
          STATUS_BYTE_MASK(STATUS_ID_RERAISE) | STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT)
              | STATUS_BYTE_MASK(STATUS_ID_CRITICAL),
          STATUS_BYTE_MASK(STATUS_ID_REGEN) | STATUS_BYTE_MASK(STATUS_ID_PROTECT) | STATUS_BYTE_MASK(STATUS_ID_SHELL)
              | STATUS_BYTE_MASK(STATUS_ID_HASTE) | STATUS_BYTE_MASK(STATUS_ID_WALL),
          STATUS_BYTE_MASK(STATUS_ID_FAITH) | STATUS_BYTE_MASK(STATUS_ID_INNOCENT) | STATUS_BYTE_MASK(STATUS_ID_CHARM)
              | STATUS_BYTE_MASK(STATUS_ID_REFLECT) };

/* Knockback directions: quarter-turn facings in the UNIT_FACING_* order, which differs from
 * move_direction_e. */
enum {
    KNOCKBACK_DIRECTION_SOUTH = 0, /* -y */
    KNOCKBACK_DIRECTION_WEST = 1,  /* -x */
    KNOCKBACK_DIRECTION_NORTH = 2, /* +y */
    KNOCKBACK_DIRECTION_EAST = 3   /* +x */
};

/* Fail the action on this target outright: no hit, the forced-failure miss type and 0 accuracy.
 * Used when an ability cannot work on the target (nothing to steal, wrong target type, level
 * limit). */
static void formula_force_attack_miss(void) {
    g_action_target_data->hit = 0;
    g_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
    /* The accuracy field is cleared as a 16-bit value (sh). */
    g_action_target_data->attack_accuracy = 0;
}

/* Miss decided by the accuracy roll: no hit, the accuracy-miss type. */
static void formula_cause_accuracy_miss(void) {
    g_action_target_data->hit = 0;
    g_action_target_data->miss_type = ACTION_MISS_TYPE_ACCURACY_MISS;
}

/* Dance/Song hit check: the shown accuracy is scaled by X%, then, only while
 * executing, an X% roll decides the hit. Returns 1 after a miss, otherwise 0. */
static s32 formula_calculate_dance_song_hit(void) {
    action_data_t* action = g_action_target_data;
    u8 hit_percent = g_action_work.range_data.x;
    s32 action_state = g_action_state;

    action->attack_accuracy = action->attack_accuracy * hit_percent / 100;
    if (action_state != ACTION_STATE_EXECUTE || random_roll_pass_fail(100, hit_percent) == 0) {
        return 0;
    }
    formula_cause_accuracy_miss();
    return 1;
}

/* Pick the knockback destination for the current action's target.
 *
 * Runs only while g_action_work.target_count is 1. An immortal target, a target
 * of importance 3 (one whose fate decides the battle), the attacker itself and a mounted pair are
 * never pushed. The target moves one tile away from the attacker along the
 * dominant axis (the y axis on a tie), and the tile must be neither
 * untargetable, in water (any depth), steep (half height 3 or more), lava nor
 * an obstacle. On success g_action_work.knockback_flags records
 * MOVE_KNOCKBACK_PENDING with the knockback kind (NO_FALL_DAMAGE for a flier
 * that is neither a Frog nor a Chicken, otherwise FALL_DAMAGE), target_tile
 * holds the destination, and the target's
 * action data gets the knockback special effect.
 *
 * abs() is the builtin: its MIPS abssi2 pattern emits the target's
 * bgez/move/negu sequence. */
static void calculate_knockback(void) {
    battle_unit_t* target;
    battle_unit_t* attacker;
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

    if (g_action_work.target_count != 1) {
        return;
    }
    target = g_action_target;
    if (target->team_flags & UNIT_TEAM_FLAG_IMMORTAL) {
        return;
    }
    if ((target->team_flags & UNIT_TEAM_IMPORTANCE_MASK) == UNIT_TEAM_IMPORTANCE_MASK) {
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
            direction = KNOCKBACK_DIRECTION_EAST;
            x++;
        } else {
            direction = KNOCKBACK_DIRECTION_WEST;
            x--;
        }
    } else {
        if (dy > 0) {
            direction = KNOCKBACK_DIRECTION_NORTH;
            y++;
        } else {
            direction = KNOCKBACK_DIRECTION_SOUTH;
            y--;
        }
    }
    level = check_knockback_destination(direction, x, y);
    if (level < 0) {
        return;
    }
    tile = &g_map_tile_data[(level << 8) + y * g_map_terrain_width + x];
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
    if (!(move_get_effective_flags(g_action_target) & MOVE_EFFECTIVE_FLY)) {
        g_action_work.knockback_flags = MOVE_KNOCKBACK_PENDING | MOVE_KNOCKBACK_FALL_DAMAGE;
    } else if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
        & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG))) {
        g_action_work.knockback_flags = MOVE_KNOCKBACK_PENDING | MOVE_KNOCKBACK_FALL_DAMAGE;
    } else {
        g_action_work.knockback_flags = MOVE_KNOCKBACK_PENDING | MOVE_KNOCKBACK_NO_FALL_DAMAGE;
    }
    g_action_work.target_tile.x = x;
    g_action_work.target_tile.y = y;
    g_action_work.target_tile.layer = level;
    g_action_target_data->special_effect |= ACTION_SPECIAL_EFFECT_KNOCKBACK;
}

#define TILE_EDGE_HEIGHT(tile, shift)                                                                                  \
    (tile->height * 2 + tile->depth_half_height.half_height * ((tile->slope_type >> shift) & 3))

/* Check whether the current target can be knocked back onto tile (x, y).
 *
 * direction (a KNOCKBACK_DIRECTION_*) selects the facing edges of the
 * target's tile and the destination tile (two bits of slope_type per
 * edge scale the tile's half height). A destination layer higher
 * than the target's edge is unusable; when only one layer is low enough, the
 * other layer's ceiling must leave at least four half-units of headroom. Of
 * two usable layers the higher one wins. On success the fall height goes to
 * g_action_work.knockback_fall_height and the ability target ID to
 * g_action_work.post_action_target_id, and the destination
 * layer is returned; -1 means off-map, blocked or occupied. */
static s32 check_knockback_destination(s32 direction, s32 x, s32 y) {
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

    if (x < 0 || y < 0 || x >= g_map_terrain_width || y >= g_map_terrain_depth) {
        return -1;
    }
    switch (direction) {
    case KNOCKBACK_DIRECTION_EAST:
        source_shift = 0;
        destination_shift = 2;
        break;
    case KNOCKBACK_DIRECTION_NORTH:
        source_shift = 6;
        destination_shift = 4;
        break;
    case KNOCKBACK_DIRECTION_WEST:
        source_shift = 2;
        destination_shift = 0;
        break;
    case KNOCKBACK_DIRECTION_SOUTH:
        source_shift = 4;
        destination_shift = 6;
        break;
    }
    tile = &g_map_tile_data[battle_unit_get_tile_index(g_action_target)];
    height = TILE_EDGE_HEIGHT(tile, source_shift);
    index = y * g_map_terrain_width + x;
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
    g_action_work.knockback_fall_height = fall;
    g_action_work.post_action_target_id = g_action_work.target_id;
    return level;
}

/* Roll an evade with numerator in denominator odds (an evade value against the
 * base hit). The random roll is always
 * drawn, but only while executing does an evade clear the hit, record the miss
 * type and the item that blocked, and return 1; a preview or AI pass returns 0. */
static s32 roll_evade_source(u16 numerator, u16 denominator, u8 miss_type, u8 item_lost) {
    if (random_roll_pass_fail(denominator, numerator) != 0) {
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
 * or / 50, truncated). The product is kept in 16 bits, so an XA above 2621
 * would wrap. The result is then at least 1; the Serpentarius returns skip
 * that floor, so an XA already cut to 0 stays 0.
 */
static void formula_apply_zodiac_compatibility(void) {
    u16 attacker_zodiac;
    u16 target_zodiac;
    u16 difference;
    u8 compatibility;
    s16 xa;
    u16 product;

    attacker_zodiac = g_action_attacker->birthday.zodiac;
    target_zodiac = g_action_target->birthday.zodiac;
    if (attacker_zodiac == ZODIAC_SIGN_SERPENTARIUS || target_zodiac == ZODIAC_SIGN_SERPENTARIUS) {
        return;
    }

    if (target_zodiac < attacker_zodiac) {
        difference = attacker_zodiac - target_zodiac;
    } else {
        difference = target_zodiac - attacker_zodiac;
    }

    compatibility = s_zodiac_compatibility_modifiers[difference];
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

    xa = g_action_work.xa;
    product = xa * 25;
    switch (compatibility) {
    case ZODIAC_COMPATIBILITY_GOOD:
        g_action_work.xa = xa + product / 100;
        break;
    case ZODIAC_COMPATIBILITY_BEST:
        g_action_work.xa = xa + product / 50;
        break;
    case ZODIAC_COMPATIBILITY_BAD:
        g_action_work.xa = xa - product / 100;
        break;
    case ZODIAC_COMPATIBILITY_WORST:
        g_action_work.xa = xa - product / 50;
        break;
    }

    if (g_action_work.xa <= 0) {
        g_action_work.xa = 1;
    }
}

/* Filter the target's pending status inflictions and removals.
 *
 * A riding target's pending inflictions are masked by the rider whitelist,
 * even with removal_only. Unless removal_only is set, inflicting Frog on a frog
 * turns into its removal, and further masks use the current (sets 0-2) or innate
 * (sets 3-4) statuses, immunities, the immortal blacklist and each status's
 * can't-stack list, then every remaining infliction clears the statuses it
 * cancels. Removals always drop innate statuses and keep only statuses the
 * target has. Returns the sum of the remaining bytes, so 0 means nothing is
 * left to inflict or remove. */
s32 status_filter_action_changes(s32 removal_only) {
    u8 inflicted;
    s32 immortal;
    s32 total = 0;
    s32 i;
    s32 j;
    s32 byte;
    s32 mask;

    immortal = g_action_target->team_flags & UNIT_TEAM_FLAG_IMMORTAL;
    if (g_action_target->mount_info & UNIT_MOUNT_INFO_FLAG_RIDER) {
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_action_target_data->status_changes.infliction[i] &= s_rider_infliction_mask[i];
        }
    }
    inflicted = g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_FROG)];
    if ((inflicted & STATUS_BYTE_MASK(STATUS_ID_FROG))
        && (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG))
        && !removal_only) {
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
            = inflicted & ~STATUS_BYTE_MASK(STATUS_ID_FROG);
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
                    &= ~g_status_check_sets[STATUS_CHECK_SET_FORMATION_IMMUNITY][i];
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
                    if (g_action_target->status_sets.current[j] & g_status_effect_data[i].cant_stack[j]) {
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
                    g_action_target_data->status_changes.infliction[j] &= ~g_status_effect_data[i].cancels[j];
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
static void formula_nullify_action(void) {
    action_data_t* action;

    g_action_target_data->hit = 0;
    action = g_action_target_data;
    action->miss_type = ACTION_MISS_TYPE_NULLIFIED;
    action->hp_damage = 0;
    action->attack_accuracy = 0;
    action->special_effect = 0;
    /* The bytes reload the pointer for this last store. */
    g_action_target_data->attack_type = 0;
    formula_clear_pending_proc();
    move_cancel_knockback();
}

/* Target affinities on the stored HP damage: absorb flags the action, nullify
 * cancels it, half halves, weak doubles. Half and weak both apply. */
static void formula_element_modify_damage(s32 element) {
    u8 element_mask = element;

    if (g_action_target->elemental_affinity[ELEMENTAL_AFFINITY_ABSORB] & element_mask) {
        g_action_target_data->special_effect |= ACTION_SPECIAL_EFFECT_ELEMENTAL_ABSORPTION;
    }
    if (g_action_target->elemental_affinity[ELEMENTAL_AFFINITY_NULLIFY] & element_mask) {
        formula_nullify_action();
        return;
    }
    if (g_action_target->elemental_affinity[ELEMENTAL_AFFINITY_HALF] & element_mask) {
        g_action_target_data->hp_damage /= 2;
    }
    if (g_action_target->elemental_affinity[ELEMENTAL_AFFINITY_WEAK] & element_mask) {
        g_action_target_data->special_effect |= ACTION_SPECIAL_EFFECT_ELEMENTAL_WEAKNESS;
        g_action_target_data->hp_damage *= 2;
    }
}

/* Reset the base hit to 100 and clear the accessory and hand evades, then
 * collect the current target's physical evade sources for the ability being
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
static void formula_evade_store_physical(void) {
    s32 weapon_guard = 0;
    u8 weapon_flags;
    u8 shield_flags;
    u8 shield_secondary;
    item_data_t* shield_record;
    u8 weapon_secondary;

    if (g_action_target->ability_flags.reaction[3] & UNIT_REACTION_SET_4_WEAPON_GUARD) {
        weapon_guard = battle_unit_get_action_restriction(g_action_target) == UNIT_ACTION_RESTRICTION_NONE;
    }
    g_action_work.base_hit = 100;
    g_action_work.evades.accessory = 0;
    g_action_work.evades.right_hand = 0;
    g_action_work.evades.left_hand = 0;
    g_action_work.evades.job = g_action_target->equipment_stats[UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    if (g_action_target->unit_flags & UNIT_FLAG_MONSTER) {
        return;
    }

    /* weapon_flags first holds the accessory's type flags. */
    weapon_flags = g_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]].type_flags;
    if (weapon_flags & ITEM_TYPE_FLAG_ACCESSORY) {
        g_action_work.evades.accessory
            = g_item_accessory_data[g_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]]
                                        .secondary_data_id]
                  .physical_evade;
    }

    weapon_flags = g_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]].type_flags;
    weapon_secondary
        = g_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]].secondary_data_id;
    shield_flags = g_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD]].type_flags;
    shield_secondary
        = g_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD]].secondary_data_id;
    if ((weapon_flags & ITEM_TYPE_FLAG_WEAPON) && weapon_guard) {
        g_action_work.evades.right_hand = g_item_weapon_data[weapon_secondary].evade;
    } else if (shield_flags & ITEM_TYPE_FLAG_SHIELD) {
        g_action_work.evades.right_hand = g_item_shield_data[shield_secondary].physical_evade;
    }

    weapon_secondary = g_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]].type_flags;
    weapon_flags = weapon_secondary;
    weapon_secondary
        = g_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]].secondary_data_id;
    shield_record = &g_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD]];
    shield_flags = shield_record->type_flags;
    shield_secondary
        = g_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD]].secondary_data_id;
    if ((weapon_flags & ITEM_TYPE_FLAG_WEAPON) && weapon_guard) {
        g_action_work.evades.left_hand = g_item_weapon_data[weapon_secondary].evade;
    } else if (shield_flags & ITEM_TYPE_FLAG_SHIELD) {
        g_action_work.evades.left_hand = g_item_shield_data[shield_secondary].physical_evade;
    }
}

/* Reset the base hit to 100 and load the target's accessory and shield
 * magic evades; monsters get no equipment evade. */
static void formula_evade_store_magical(void) {
    battle_unit_t* target = g_action_target;
    item_data_t* items;
    item_data_t* item;
    s32 secondary;

    g_action_work.base_hit = 100;
    g_action_work.evades.accessory = 0;
    g_action_work.evades.right_hand = 0;
    g_action_work.evades.left_hand = 0;
    g_action_work.evades.job = 0;
    if (target->unit_flags & UNIT_FLAG_MONSTER) {
        return;
    }
    items = g_item_primary_data;
    item = &items[target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]];
    if (item->type_flags & ITEM_TYPE_FLAG_ACCESSORY) {
        g_action_work.evades.accessory = g_item_accessory_data[item->secondary_data_id].magic_evade;
    }
    item = &items[target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD]];
    secondary = item->secondary_data_id;
    if (item->type_flags & ITEM_TYPE_FLAG_SHIELD) {
        g_action_work.evades.right_hand = g_item_shield_data[secondary].magic_evade;
    }
    item = &items[target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD]];
    secondary = item->secondary_data_id;
    if (item->type_flags & ITEM_TYPE_FLAG_SHIELD) {
        g_action_work.evades.left_hand = g_item_shield_data[secondary].magic_evade;
    }
}

/* Concentrate on the attacker: the target's four evades (accessory, both hands
 * and class, stored together) become 0. */
static void formula_evade_calculate_concentrate(void) {
    if (g_action_attacker->ability_flags.support[1] & UNIT_SUPPORT_SET_2_CONCENTRATE) {
        mem_zero(&g_action_work.evades, sizeof(g_action_work.evades));
    }
}

/* A Dark or Confused attacker halves the base hit, which doubles every evade.
 * Then the Transparent step. */
static void formula_evade_apply_attacker_statuses(void) {
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DARKNESS)]
        & (STATUS_BYTE_MASK(STATUS_ID_DARKNESS) | STATUS_BYTE_MASK(STATUS_ID_CONFUSION))) {
        g_action_work.base_hit >>= 1;
    }
    formula_evade_calculate_transparent();
}

/* Abandon: a target free to act halves the base hit, which doubles each of its
 * evades. */
static void formula_evade_calculate_abandon(void) {
    if ((battle_unit_get_action_restriction(g_action_target) == UNIT_ACTION_RESTRICTION_NONE)
        && (g_action_target->ability_flags.reaction[3] & UNIT_REACTION_SET_4_ABANDON)) {
        g_action_work.base_hit >>= 1;
        g_action_target_data->reaction_id = ABILITY_ID_REACTION_ABANDON;
    }
}

/* Target statuses on the evades: Don't Act, Sleep, Stop, Confusion, Charging
 * or Performing clear all four; Defending halves the base hit, which doubles
 * them. */
static void formula_evade_apply_target_statuses(void) {
    battle_unit_t* target = g_action_target;
    u8 flag;

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
        mem_zero(&g_action_work.evades, sizeof(g_action_work.evades));
    }
    /* Defending is read through a fresh load of the target, as the bytes require. */
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEFENDING)]
        & STATUS_BYTE_MASK(STATUS_ID_DEFENDING)) {
        g_action_work.base_hit >>= 1;
    }
}

/* A Transparent attacker cannot be evaded: the four evades become 0. */
static void formula_evade_calculate_transparent(void) {
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
        & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT)) {
        mem_zero(&g_action_work.evades, sizeof(g_action_work.evades));
    }
}

/* Classify the attack direction and drop the evades the target cannot use.
 *
 * The dominant axis of the attacker-to-target offset is compared with the
 * target's facing (position bits 8-11); on a diagonal the target counts as
 * hit from the front when it faces either axis toward the attacker, and a
 * shared tile counts as the back. Side and back attacks clear class evade,
 * back attacks also both hand evades (a shield's, or a weapon's under Weapon
 * Guard). */
static void formula_evade_calculate_facing(void) {
    s32 dx = g_action_target->x - g_action_attacker->x;
    s32 dy;
    u8 distance_x = dx < 0 ? -dx : dx;
    u8 distance_y;
    s32 direction;
    s32 facing;

    dy = g_action_target->position.y - g_action_attacker->position.y;
    distance_y = dy < 0 ? -dy : dy;
    direction = FORMULA_FACING_SIDE;
    if (distance_y < distance_x) {
        if (dx > 0) {
            facing = g_action_target->position.facing << 8;
            if (facing == UNIT_POSITION_FACING_WEST) {
                direction = FORMULA_FACING_FRONT;
            } else if (facing == UNIT_POSITION_FACING_EAST) {
                direction = FORMULA_FACING_BACK;
            }
        }
        if (dx < 0) {
            facing = g_action_target->position.facing << 8;
            if (facing == UNIT_POSITION_FACING_EAST) {
                direction = FORMULA_FACING_FRONT;
            } else if (facing == UNIT_POSITION_FACING_WEST) {
                direction = FORMULA_FACING_BACK;
            }
        }
    } else if (distance_x < distance_y) {
        if (dy > 0) {
            facing = g_action_target->position.facing << 8;
            if (facing == UNIT_POSITION_FACING_SOUTH) {
                direction = FORMULA_FACING_FRONT;
            } else if (facing == UNIT_POSITION_FACING_NORTH) {
                direction = FORMULA_FACING_BACK;
            }
        }
        if (dy < 0) {
            facing = g_action_target->position.facing << 8;
            if (facing == UNIT_POSITION_FACING_NORTH) {
                direction = FORMULA_FACING_FRONT;
            } else if (facing == UNIT_POSITION_FACING_SOUTH) {
                direction = FORMULA_FACING_BACK;
            }
        }
    } else if (dx > 0 && (g_action_target->position.facing << 8) == UNIT_POSITION_FACING_WEST) {
        direction = FORMULA_FACING_FRONT;
    } else if (dx < 0 && (g_action_target->position.facing << 8) == UNIT_POSITION_FACING_EAST) {
        direction = FORMULA_FACING_FRONT;
    } else if (dy > 0 && (g_action_target->position.facing << 8) == UNIT_POSITION_FACING_SOUTH) {
        direction = FORMULA_FACING_FRONT;
    } else if (dy < 0 && (g_action_target->position.facing << 8) == UNIT_POSITION_FACING_NORTH) {
        direction = FORMULA_FACING_FRONT;
    } else if (dx == 0 && dy == 0) {
        direction = FORMULA_FACING_BACK;
    }
    g_action_work.facing_modifier = direction;
    if (g_action_work.facing_modifier != FORMULA_FACING_FRONT) {
        g_action_work.evades.job = 0;
    }
    if (g_action_work.facing_modifier == FORMULA_FACING_BACK) {
        g_action_work.evades.right_hand = 0;
        g_action_work.evades.left_hand = 0;
    }
}

/* Bows and crossbows lose a quarter of the base hit at night and another
 * quarter in a storm or a strong storm. */
static void formula_evade_apply_weather_on_bows(void) {
    s32 weather;
    s32 time_of_day;

    if (g_item_primary_data[g_action_work.weapon_id].type == ITEM_TYPE_CROSSBOW
        || g_item_primary_data[g_action_work.weapon_id].type == ITEM_TYPE_BOW) {
        weather = script_variable_get(SCRIPT_VAR_WEATHER);
        time_of_day = script_variable_get(SCRIPT_VAR_TIME_OF_DAY);
        if (time_of_day == TIME_OF_DAY_NIGHT) {
            g_action_work.base_hit = g_action_work.base_hit * 3 / 4;
        }
        /* The raw weather variable, not weather_get_effective: a
         * storm strength counts on a snow map and on a map that ignores weather. */
        if (weather == WEATHER_STORM || weather == WEATHER_STRONG_STORM) {
            g_action_work.base_hit = g_action_work.base_hit * 3 / 4;
        }
    }
}

/* Scale the action accuracy by each evade source, then roll them in turn.
 *
 * Applies to the basic attack (ability id 0) or abilities flagged
 * ABILITY_SECONDARY_FLAG_4_EVADEABLE. Any evade at or above the base hit forces a miss;
 * otherwise the accuracy becomes the product of the four (hit - evade)
 * percentages. The rolls then run accessory, right hand, left hand and
 * class evade, reporting the item that blocked. Each roll evades with evade in
 * base-hit odds and they are independent, so together they hit as often as the
 * shown accuracy, up to its truncation. The earlier base-hit halvings (Dark or
 * Confused attacker, Abandon, Defending) stack. */
static void formula_evade_roll_sources(void) {
    u8 hit;
    u8 accessory;
    u8 right;
    u8 left;
    u8 evade;
    u8 item;

    if ((g_action_work.ability_id != 0) && !(g_action_work.range_data.flags_4 & ABILITY_SECONDARY_FLAG_4_EVADEABLE)) {
        return;
    }
    hit = g_action_work.base_hit;
    accessory = g_action_work.evades.accessory;
    /* Each evade is loaded only after the one before it passes, as the bytes require. */
    if (accessory >= hit || (right = g_action_work.evades.right_hand) >= hit
        || (left = g_action_work.evades.left_hand) >= hit || (evade = g_action_work.evades.job) >= hit) {
        g_action_target_data->hit = 0;
        g_action_target_data->attack_accuracy = 0;
    } else {
        g_action_target_data->attack_accuracy
            = ((hit - accessory) * 100 / hit * ((hit - right) * 100 / hit) * ((hit - left) * 100 / hit)
                  * ((hit - evade) * 100 / hit) / 1000000)
            * g_action_target_data->attack_accuracy / 100;
    }
    if (roll_evade_source(g_action_work.evades.accessory, hit, ACTION_MISS_TYPE_ACCESSORY_EVADE,
            g_action_target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY])
        != 0) {
        return;
    }
    item = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    if (item == ITEM_ID_NONE) {
        item = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
    }
    if (roll_evade_source(g_action_work.evades.right_hand, hit, ACTION_MISS_TYPE_RIGHT_HAND_EVADE, item) != 0) {
        return;
    }
    item = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
    if (item == ITEM_ID_NONE) {
        item = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
    }
    if (roll_evade_source(g_action_work.evades.left_hand, hit, ACTION_MISS_TYPE_LEFT_HAND_EVADE, item) != 0) {
        return;
    }
    roll_evade_source(g_action_work.evades.job, hit, ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD, ITEM_ID_NONE);
}

/* Set XA and YA from the current weapon's item type.
 *
 * Bare hands give XA = PA and YA = PA * Brave / 100 (at least 1); otherwise YA
 * is the weapon power and XA depends on the type: PA for swords, crossbows,
 * rods and spears; (PA + Speed) / 2 for knives, ninja swords, bows and
 * shuriken; PA * Brave / 100 (at least 1) for knight swords and katanas; MA
 * for staves and sticks; 1..PA at random for axes, hammers and bags; (PA + MA)
 * / 2 for instruments, dictionaries and cloths. Other types leave XA unchanged.
 * The attacker's stats must be u8 locals for the (PA + MA) zero-extension.
 */
static void formula_store_weapon_xa_ya(void) {
    u8 type;
    u8 pa;
    u8 ma;
    s16 value;
    u8 brave;

    type = g_item_primary_data[g_action_work.weapon_id].type;
    pa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    ma = g_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    brave = g_action_attacker->brave;
    if (type == ITEM_TYPE_UNARMED) {
        g_action_work.xa = pa;
        value = pa * brave / 100;
        g_action_work.ya = value;
        if (value == 0) {
            g_action_work.ya = value + 1;
        }
        return;
    }
    g_action_work.ya = g_action_work.weapon_data.power;
    if (type == ITEM_TYPE_SWORD || type == ITEM_TYPE_CROSSBOW || type == ITEM_TYPE_ROD || type == ITEM_TYPE_SPEAR) {
        g_action_work.xa = pa;
    } else if (type == ITEM_TYPE_KNIFE || type == ITEM_TYPE_NINJA_SWORD || type == ITEM_TYPE_BOW
        || type == ITEM_TYPE_SHURIKEN) {
        g_action_work.xa = (pa + g_action_attacker->attributes[UNIT_ATTRIBUTE_SPEED]) >> 1;
    } else if (type == ITEM_TYPE_KNIGHT_SWORD || type == ITEM_TYPE_KATANA) {
        value = pa * brave / 100;
        g_action_work.xa = value;
        if (value == 0) {
            g_action_work.xa = value + 1;
        }
    } else if (type == ITEM_TYPE_STAFF || type == ITEM_TYPE_STICK) {
        g_action_work.xa = ma;
    } else if (type == ITEM_TYPE_AXE || type == ITEM_TYPE_HAMMER || type == ITEM_TYPE_BAG) {
        g_action_work.xa = pa * formula_get_random() / 0x8000 + 1;
    } else if (type == ITEM_TYPE_INSTRUMENT || type == ITEM_TYPE_DICTIONARY || type == ITEM_TYPE_CLOTH) {
        g_action_work.xa = (pa + ma) >> 1;
    }
}

/* XA = MA, YA = the ability's Y. */
static void formula_store_ma_and_y(void) {
    g_action_work.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    g_action_work.ya = g_action_work.range_data.y;
}

/* XA = MA, YA = (MA + Y) / 2. */
static void formula_store_ma_and_ma_plus_y_divided_by_two(void) {
    g_action_work.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    g_action_work.ya = (g_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK] + g_action_work.range_data.y) >> 1;
}

/* XA = PA, YA = (PA + Y) / 2. */
static void formula_store_pa_and_pa_plus_y_divided_by_two(void) {
    g_action_work.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    g_action_work.ya
        = (g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK] + g_action_work.range_data.y) >> 1;
}

/* XA = MA, YA = (PA + Y) / 2. */
static void formula_store_ma_and_pa_plus_y_divided_by_two(void) {
    g_action_work.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    g_action_work.ya
        = (g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK] + g_action_work.range_data.y) >> 1;
}

/* XA = MA, YA = the ability's X. */
static void formula_store_ma_and_x(void) {
    g_action_work.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    g_action_work.ya = g_action_work.range_data.x;
}

/* XA = PA, YA = the ability's X. */
static void formula_store_pa_and_x(void) {
    g_action_work.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    g_action_work.ya = g_action_work.range_data.x;
}

/* XA = PA, YA = the weapon's power. */
static void formula_store_pa_and_weapon_power(void) {
    g_action_work.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    g_action_work.ya = g_action_work.weapon_data.power;
}

/* XA = PA, YA = the ability's Y. */
static void formula_store_pa_and_y(void) {
    g_action_work.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    g_action_work.ya = g_action_work.range_data.y;
}

/* XA = Speed, YA = the ability's X. */
static void formula_store_speed_and_x(void) {
    g_action_work.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_SPEED];
    g_action_work.ya = g_action_work.range_data.x;
}

/* XA = PA, YA = the weapon's power + Y. */
static void formula_store_pa_and_weapon_power_plus_y(void) {
    g_action_work.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    g_action_work.ya = g_action_work.weapon_data.power + g_action_work.range_data.y;
}

/* XA and YA of formula 0x64 (Jump): XA = PA, or PA * 3/2 with a spear; YA =
 * weapon power, or bare-handed XA * Brave / 100 (at least 1). */
static void formula_store_jump_xa_ya(void) {
    u8 weapon_type = g_item_primary_data[g_action_work.weapon_id].type;

    if (weapon_type == ITEM_TYPE_SPEAR) {
        g_action_work.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK] * 3 / 2;
    } else {
        g_action_work.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    }

    if (weapon_type == ITEM_TYPE_UNARMED) {
        g_action_work.ya = g_action_work.xa * g_action_attacker->brave / 100;
        if (g_action_work.ya == 0) {
            g_action_work.ya++;
        }
    } else {
        g_action_work.ya = g_action_work.weapon_data.power;
    }
}

/* Add the Charge ability's power to XA (0 for every other action). */
static void formula_apply_charge(void) {
    g_action_work.xa += g_action_work.charge_power;
}

/* Strengthen step for weapon attacks: XA * 5/4 when the attacker strengthens
 * one of the weapon's elements. */
static void formula_element_apply_weapon_strengthen(void) {
    if (g_action_attacker->elemental_affinity[ELEMENTAL_AFFINITY_STRENGTHEN] & g_action_work.weapon_data.element) {
        g_action_work.xa = g_action_work.xa * 5 / 4;
    }
}

/* Strengthen step: XA * 5/4 when the attacker strengthens one of the ability's
 * elements. */
static void formula_element_apply_strengthen(void) {
    if (g_action_attacker->elemental_affinity[ELEMENTAL_AFFINITY_STRENGTHEN] & g_action_work.range_data.element) {
        g_action_work.xa = g_action_work.xa * 5 / 4;
    }
}

/* Attacker supports on XA, in order: Two Hands * 2 (active, on a weapon that
 * allows it and is not two-handed already), Attack Up * 4/3, Martial Arts *
 * 3/2 with no weapon (weapon id ITEM_ID_NOTHING). */
static void formula_apply_physical_attack_supports(void) {
    u8 weapon_characteristics = g_action_work.weapon_data.flags;

    if (!(weapon_characteristics & WEAPON_FLAG_FORCED_TWO_HANDS)
        && (weapon_characteristics & WEAPON_FLAG_TWO_HANDS_COMPATIBLE) && g_action_work.two_hands_active) {
        g_action_work.xa = g_action_work.xa * 2;
    }

    if (g_action_attacker->ability_flags.support[1] & UNIT_SUPPORT_SET_2_ATTACK_UP) {
        g_action_work.xa = g_action_work.xa * 4 / 3;
    }

    if (g_action_work.weapon_id == ITEM_ID_NOTHING
        && (g_action_attacker->ability_flags.support[2] & UNIT_SUPPORT_SET_3_MARTIAL_ARTS)) {
        g_action_work.xa = g_action_work.xa * 3 / 2;
    }
}

/* Attacker supports on XA for ability formulas: Attack Up * 4/3, then Martial
 * Arts * 3/2 when the weapon's item type is unarmed. No Two Hands step. */
static void formula_apply_attack_up_and_martial_arts(void) {
    u8 weapon_type;
    u8 martial_arts;

    if (g_action_attacker->ability_flags.support[1] & UNIT_SUPPORT_SET_2_ATTACK_UP) {
        g_action_work.xa = g_action_work.xa * 4 / 3;
    }

    weapon_type = g_item_primary_data[g_action_work.weapon_id].type;
    martial_arts = g_action_attacker->ability_flags.support[2] & UNIT_SUPPORT_SET_3_MARTIAL_ARTS;
    if (martial_arts && weapon_type == ITEM_TYPE_UNARMED) {
        g_action_work.xa = g_action_work.xa * 3 / 2;
    }
}

/* Magic Attack Up on the attacker: XA * 4/3. */
static void formula_apply_magic_attack_up(void) {
    if (g_action_attacker->ability_flags.support[1] & UNIT_SUPPORT_SET_2_MAGIC_ATTACK_UP) {
        g_action_work.xa = g_action_work.xa * 4 / 3;
    }
}

/* Attacker statuses on XA: Berserk * 3/2; a Frog's XA is 1, so the supports
 * and Berserk before it and a critical after it add nothing. Action setup gives
 * a Frog's default-menu abilities (Frog Attack) no weapon, so YA there is the
 * bare-handed PA * Brave / 100. */
static void formula_apply_attacker_berserk_frog(void) {
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_BERSERK)]
        & STATUS_BYTE_MASK(STATUS_ID_BERSERK)) {
        g_action_work.xa = g_action_work.xa * 3 / 2;
    }
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)] & STATUS_BYTE_MASK(STATUS_ID_FROG)) {
        g_action_work.xa = 1;
    }
}

/* Defense Up on the target: XA * 2/3. */
static void formula_apply_defense_up(void) {
    if (g_action_target->ability_flags.support[1] & UNIT_SUPPORT_SET_2_DEFENSE_UP) {
        g_action_work.xa = g_action_work.xa * 2 / 3;
    }
}

/* Magic Defense Up on the target: XA * 2/3. */
static void formula_apply_magic_defense_up(void) {
    if (g_action_target->ability_flags.support[1] & UNIT_SUPPORT_SET_2_MAGIC_DEFENSE_UP) {
        g_action_work.xa = g_action_work.xa * 2 / 3;
    }
}

/* Target statuses on physical XA: Protect * 2/3; Sleep or Charging * 3/2;
 * Chicken or Frog * 3/2. Sleep and Charging share one step: inflicting Sleep
 * cancels Charging (and Jump, Defending and Performing). */
static void formula_apply_target_physical_statuses(void) {
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PROTECT)]
        & STATUS_BYTE_MASK(STATUS_ID_PROTECT)) {
        g_action_work.xa = g_action_work.xa * 2 / 3;
    }

    if ((g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)] & STATUS_BYTE_MASK(STATUS_ID_SLEEP))
        || (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
            & STATUS_BYTE_MASK(STATUS_ID_CHARGING))) {
        g_action_work.xa = g_action_work.xa * 3 / 2;
    }

    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHICKEN)]
        & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG))) {
        g_action_work.xa = g_action_work.xa * 3 / 2;
    }
}

/* Target statuses on magical XA: Shell * 2/3; Chicken or Frog * 3/2. */
static void formula_apply_target_magical_statuses(void) {
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SHELL)] & STATUS_BYTE_MASK(STATUS_ID_SHELL)) {
        g_action_work.xa = g_action_work.xa * 2 / 3;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHICKEN)]
        & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG))) {
        g_action_work.xa = g_action_work.xa * 3 / 2;
    }
}

/* Critical hit step: a FORMULA_CRITICAL_HIT_CHANCE in 100 roll (outside
 * execution the fixed midpoint roll never crits). XA gains rnd(0..XA-1) and the
 * target may be knocked back. */
static void formula_calculate_critical_hit(void) {
    s32 rnd;
    s16 base_xa;

    if (formula_roll_random_at_least(100, FORMULA_CRITICAL_HIT_CHANCE) == 0) {
        g_action_target_data->critical = 1;
        rnd = formula_get_random();
        base_xa = g_action_work.xa;
        g_action_work.xa = base_xa + rnd * base_xa / 32768;
        calculate_knockback();
    }
}

/* Set the target's result to HP damage XA * YA, kept in 16 bits. */
static void formula_store_xa_times_ya_damage(void) {
    s16 damage = g_action_work.xa * g_action_work.ya;
    action_data_t* action = g_action_target_data;

    action->attack_type = ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = damage;
}

/* Store XA + YA in hp_damage: the hit chance read by
 * formula_use_hp_damage_as_action_hit_percent, or formula 0x32's damage
 * base. YA gets zodiac compatibility here (the callers apply it to XA), except
 * for Golem. */
static void formula_store_xa_plus_ya(void) {
    u16 original_xa;

    if (g_action_work.ability_id != ABILITY_ID_SUMMON_MAGIC_GOLEM) {
        original_xa = g_action_work.xa;
        g_action_work.xa = g_action_work.ya;
        formula_apply_zodiac_compatibility();
        g_action_work.ya = g_action_work.xa;
        g_action_work.xa = original_xa;
    }
    g_action_target_data->hp_damage = g_action_work.xa + g_action_work.ya;
}

/* HP damage of Y% of the target's max HP, rounded up. */
static void formula_calculate_hp_percent_damage(void) {
    action_data_t* action;
    s32 damage = (g_action_target->max_hp * g_action_work.range_data.y + 99) / 100;

    action = g_action_target_data;
    action->attack_type |= ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = damage;
}

/* MP damage of Y% of the target's max MP, rounded up. */
static void formula_calculate_mp_percent_damage(void) {
    action_data_t* action;
    s32 damage = (g_action_target->max_mp * g_action_work.range_data.y + 99) / 100;

    action = g_action_target_data;
    action->attack_type = ACTION_TYPE_MP_DAMAGE;
    action->mp_damage = damage;
}

/* HP damage equal to the target's current HP - 1 (0 when it has none). */
static void formula_calculate_damage_leaving_one_hp(void) {
    if (g_action_target->hp != 0) {
        g_action_target_data->hp_damage = g_action_target->hp - 1;
    } else {
        g_action_target_data->hp_damage = 0;
    }
    g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
}

/* Gil stolen: attacker Level * Speed, capped at the war funds when the target
 * is on the player team. Nothing to take is a forced miss. The attacker's
 * action data records the gain and the target's the loss. */
static void formula_calculate_stolen_gil(void) {
    battle_unit_t* attacker = g_action_attacker;
    action_data_t* action;
    action_data_t* target_action;
    action_data_t* other;
    s32 amount = attacker->attributes[UNIT_ATTRIBUTE_SPEED] * attacker->level;
    s32 available;
    s16 stolen;

    if (!(g_action_target->team_flags & UNIT_TEAM_MASK)) {
        available = script_variable_get(SCRIPT_VAR_WAR_FUNDS);
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
    other = g_action_attacker_data; /* reloaded, as retail does */
    action->gil_change = amount;
    other->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    stolen = g_action_attacker_data->gil_change;
    target_action = g_action_target_data;
    target_action->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    target_action->gil_change = -stolen;
}

/*
 * Steal Exp.: take attacker Speed + Y EXP, at most 100 and at most the target's
 * EXP; nothing to take is a forced miss. The attacker gains the amount; the
 * target's exp_change carries it with ACTION_EXP_CHANGE_LOSS, the flag
 * reward_apply_exp_change tests.
 */
static void formula_set_exp_stolen(void) {
    u8 amount = g_action_attacker->attributes[UNIT_ATTRIBUTE_SPEED] + g_action_work.range_data.y;
    u8 available_exp;

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
    g_action_target_data->exp_change = amount + ACTION_EXP_CHANGE_LOSS;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x36: +PA (Y). */
static void formula_add_pa_y(void) {
    g_action_target_data->pa_change = g_action_work.range_data.y | ACTION_STAT_CHANGE_INCREASE;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Damage XA * rnd(1..Y). When executing, the target is knocked back if a roll
 * of 0..(its Brave - 1) comes out under a roll of 0..(attacker Brave - 1). */
static void formula_apply_damage_and_knockback(void) {
    action_data_t* action;
    s32 damage;
    s32 roll;

    damage = (formula_get_random() * g_action_work.range_data.y / 32768 + 1) * (u16)g_action_work.xa;
    action = g_action_target_data;
    action->attack_type = ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = damage;
    if (g_action_state != ACTION_STATE_EXECUTE) {
        return;
    }
    roll = (formula_get_random() * g_action_attacker->brave) / 32768;
    if (random_roll_pass_fail(g_action_target->brave, roll) == 0) {
        calculate_knockback();
    }
}

/* Effect of each song. XA + YA is MA + Y; the stat songs add 1. */
static void formula_apply_song_abilities(void) {
    switch (g_action_work.ability_id) {
    case ABILITY_ID_SONG_ANGEL_SONG:
        g_action_target_data->mp_healing = g_action_work.xa + g_action_work.ya;
        g_action_target_data->attack_type = ACTION_TYPE_MP_HEALING;
        return;
    case ABILITY_ID_SONG_LIFE_SONG:
        g_action_target_data->hp_healing = g_action_work.xa + g_action_work.ya;
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

/* Effect of each dance. XA + YA is the weapon's XA plus its power
 * (formula_store_weapon_xa_ya); the stat dances take 1. */
static void formula_apply_dance_abilities(void) {
    switch (g_action_work.ability_id) {
    case ABILITY_ID_DANCE_WITCH_HUNT:
        g_action_target_data->mp_damage = g_action_work.xa + g_action_work.ya;
        g_action_target_data->attack_type = ACTION_TYPE_MP_DAMAGE;
        return;
    case ABILITY_ID_DANCE_WIZNAIBUS:
        g_action_target_data->hp_damage = g_action_work.xa + g_action_work.ya;
        g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
        return;
    case ABILITY_ID_DANCE_SLOW_DANCE:
        g_action_target_data->sp_change = 1;
        break;
    case ABILITY_ID_DANCE_POLKA_POLKA:
        g_action_target_data->pa_change = 1;
        break;
    case ABILITY_ID_DANCE_DISILLUSION:
        g_action_target_data->ma_change = 1;
        break;
    case ABILITY_ID_DANCE_NAMELESS_DANCE:
        formula_apply_status_to_action();
        return;
    case ABILITY_ID_DANCE_LAST_DANCE:
        g_action_target_data->ct_change = ACTION_CT_CHANGE_ZERO;
        break;
    }
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* result modifiers */

/* Effect of each talk skill: Negotiate takes gil, Persuade zeroes CT, Praise
 * and Threaten change Brave by Y, Preach and Solution change Faith by Y; the
 * rest inflict their status. */
static void formula_apply_talk_skill(void) {
    switch (g_action_work.ability_id) {
    case ABILITY_ID_TALK_NEGOTIATE:
        formula_calculate_stolen_gil();
        return;
    case ABILITY_ID_TALK_PERSUADE:
        g_action_target_data->ct_change = ACTION_CT_CHANGE_ZERO;
        break;
    case ABILITY_ID_TALK_PRAISE:
        g_action_target_data->brave_change = g_action_work.range_data.y | ACTION_STAT_CHANGE_INCREASE;
        break;
    case ABILITY_ID_TALK_THREATEN:
        g_action_target_data->brave_change = g_action_work.range_data.y & ACTION_STAT_CHANGE_VALUE_MASK;
        break;
    case ABILITY_ID_TALK_PREACH:
        g_action_target_data->faith_change = g_action_work.range_data.y | ACTION_STAT_CHANGE_INCREASE;
        break;
    case ABILITY_ID_TALK_SOLUTION:
        g_action_target_data->faith_change = g_action_work.range_data.y & ACTION_STAT_CHANGE_VALUE_MASK;
        break;
    default:
        formula_apply_status_to_action();
        return;
    }
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x3A: +Brave (Y). */
static void formula_cheer_up(void) {
    g_action_target_data->brave_change = g_action_work.range_data.y | ACTION_STAT_CHANGE_INCREASE;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x39: +Speed (Y). */
static void formula_yell(void) {
    g_action_target_data->sp_change = g_action_work.range_data.y | ACTION_STAT_CHANGE_INCREASE;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x3B: +Brave (X), +PA/MA/Speed (Y). */
static void formula_add_brave_x_stats_y(void) {
    s32 y;

    g_action_target_data->brave_change = g_action_work.range_data.x | ACTION_STAT_CHANGE_INCREASE;
    y = g_action_work.range_data.y | ACTION_STAT_CHANGE_INCREASE;
    g_action_target_data->pa_change = y;
    g_action_target_data->ma_change = y;
    g_action_target_data->sp_change = y;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x3C: damage the caster for a fifth of its max HP and heal the
 * target for twice the damage stored (read back as a 16-bit value). */
static void formula_sacrifice_hp_to_heal(void) {
    u16 amount = g_action_attacker->max_hp / 5;
    s32 dealt;
    action_data_t* action = g_action_attacker_data;
    action_data_t* target;

    action->attack_type = ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = amount;
    action = g_action_attacker_data; /* reloaded, as retail does */
    action->hit = 1;
    target = g_action_target_data;
    dealt = g_action_attacker_data->hp_damage;
    target->attack_type = ACTION_TYPE_HP_HEALING;
    target->hp_healing = dealt * 2;
}

/* Formula 0x43: damage equal to the caster's missing HP (max HP - HP). */
static void formula_damage_caster_missing_hp(void) {
    battle_unit_t* unit = g_action_attacker;
    action_data_t* action = g_action_target_data;
    u16 max_hp = unit->max_hp;
    u16 hp = unit->hp;

    action->attack_type = ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = max_hp - hp;
}

/* Formula 0x44: HP damage equal to the target's current MP. */
static void formula_difference(void) {
    u16 mp = g_action_target->mp;
    action_data_t* action = g_action_target_data;

    action->attack_type = ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = mp;
}

/* Formula 0x45: damage equal to the target's missing HP (max HP - HP). */
static void formula_climhazzard(void) {
    g_action_target_data->hp_damage = g_action_target->max_hp - g_action_target->hp;
    g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
}

/* Brave loss of Y: the amount is stored without
 * ACTION_STAT_CHANGE_INCREASE. */
static void formula_set_brave_loss_y(void) {
    g_action_target_data->brave_change = g_action_work.range_data.y & ACTION_STAT_CHANGE_VALUE_MASK;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Fire loses and lightning gains a quarter of its HP damage in a storm or a
 * strong storm; ice gains a quarter in a snowstorm or a strong snowstorm.
 * Plain rain and snow have no effect. The weather comes through
 * weather_get_effective, so an indoor map has none, unlike the bow
 * penalty of formula_evade_apply_weather_on_bows. */
static void formula_element_apply_weather(void) {
    u8 element;
    s32 weather;

    element = g_action_work.range_data.element;
    weather = weather_get_effective();
    if (weather == WEATHER_STORM || weather == WEATHER_STRONG_STORM) {
        if (element & ELEMENT_FIRE) {
            g_action_target_data->hp_damage = g_action_target_data->hp_damage * 3 / 4;
        }
        if (element & ELEMENT_LIGHTNING) {
            g_action_target_data->hp_damage = g_action_target_data->hp_damage * 5 / 4;
        }
    }
    if (weather == WEATHER_SNOWSTORM || weather == WEATHER_STRONG_SNOWSTORM) {
        if (element & ELEMENT_ICE) {
            g_action_target_data->hp_damage = g_action_target_data->hp_damage * 5 / 4;
        }
    }
}

/* Target affinities to the weapon's elements. */
static void formula_element_modify_weapon_damage(void) {
    formula_element_modify_damage(g_action_work.weapon_data.element);
}

/* Element step, run after the HP damage is stored: Oil against fire, Float
 * against earth (a rider uses its mount's Float), then the target's affinities
 * to the ability's elements. */
static void formula_element_apply(void) {
    u8 element;
    battle_unit_t* unit;
    s32 mount_info;

    element = g_action_work.range_data.element;
    /* Fire removes Oil. XA is doubled, but every caller has already stored the
     * HP damage and nothing reads XA afterwards, so the damage does not change. */
    if ((g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_OIL)] & STATUS_BYTE_MASK(STATUS_ID_OIL))
        && (element & ELEMENT_FIRE)) {
        g_action_work.xa = g_action_work.xa * 2;
        g_action_target_data->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_OIL)]
            |= STATUS_BYTE_MASK(STATUS_ID_OIL);
        if (status_filter_action_changes(0)) {
            g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
    }
    unit = g_action_target;
    mount_info = unit->mount_info;
    if (mount_info & UNIT_MOUNT_INFO_FLAG_RIDER) {
        unit = &g_battle_units[mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK];
    }
    /* Float nullifies earth. */
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT))
        && (element & ELEMENT_EARTH)) {
        formula_nullify_action();
        g_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
    }
    formula_element_modify_damage(element);
}

/* Turn absorbed elemental HP damage into HP healing; otherwise mark the action as HP damage. */
static void formula_element_apply_absorption(void) {
    action_data_t* action = g_action_target_data;

    if (action->miss_type != ACTION_MISS_TYPE_NULLIFIED) {
        if (action->special_effect & ACTION_SPECIAL_EFFECT_ELEMENTAL_ABSORPTION) {
            u16 damage = action->hp_damage;
            action->hp_damage = 0;
            action->attack_type = ACTION_TYPE_HP_HEALING;
            action->hp_healing = damage;
        } else {
            action->attack_type = ACTION_TYPE_HP_DAMAGE;
        }
    }
}

/* Faith step: HP damage * caster Faith * target Faith / 10000. The Faith
 * status counts as 100 and Innocent as 0; Innocent wins when a unit has both.
 * The two Faith values come from attacker_faith and target_faith, which the
 * action setup loaded. */
static void formula_calculate_faith(void) {

    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FAITH)]
        & STATUS_BYTE_MASK(STATUS_ID_FAITH)) {
        g_action_work.attacker_faith = 100;
    }
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
        & STATUS_BYTE_MASK(STATUS_ID_INNOCENT)) {
        g_action_work.attacker_faith = 0;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FAITH)] & STATUS_BYTE_MASK(STATUS_ID_FAITH)) {
        g_action_work.target_faith = 100;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
        & STATUS_BYTE_MASK(STATUS_ID_INNOCENT)) {
        g_action_work.target_faith = 0;
    }

    g_action_target_data->hp_damage
        = g_action_target_data->hp_damage * g_action_work.target_faith * g_action_work.attacker_faith / 10000;
}

/* Drain step: once the target's result is final, a hit heals the attacker by
 * the HP damage. Against an undead target it is reversed: the attacker takes
 * the damage and the target is healed by it. Each load of the two pointers
 * keeps its own local, as the bytes require. */
static void formula_apply_hp_absorption(void) {
    action_data_t* undead;
    action_data_t* hurt_attacker;
    action_data_t* healed_undead;
    action_data_t* drained;
    action_data_t* healed_attacker;
    u16 amount;

    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
        & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        action_finalize_target_result();
        undead = g_action_target_data;
        if (undead->hit) {
            hurt_attacker = g_action_attacker_data;
            hurt_attacker->hp_damage = undead->hp_damage;
            hurt_attacker->attack_type = ACTION_TYPE_HP_DAMAGE;
            g_action_attacker_data->hit = 1;
            healed_undead = g_action_target_data;
            amount = healed_undead->hp_damage;
            healed_undead->hp_damage = 0;
            healed_undead->attack_type = ACTION_TYPE_HP_HEALING;
            healed_undead->hp_healing = amount;
        }
        return;
    }
    action_finalize_target_result();
    drained = g_action_target_data;
    if (drained->hit) {
        healed_attacker = g_action_attacker_data;
        healed_attacker->hp_healing = drained->hp_damage;
        healed_attacker->attack_type = ACTION_TYPE_HP_HEALING;
        g_action_attacker_data->hit = 1;
        g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
    }
}

/* Healing step: the stored HP damage becomes healing. An undead target takes
 * it as damage instead, and the ability no longer counts as canceling the
 * statuses of the first status byte (Dead among them). */
static void formula_convert_hp_damage_to_healing(void) {
    action_data_t* action;
    action_data_t* action_again;
    u16 hp_damage;

    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
        & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
        g_action_canceled_statuses[0] = 0;
        return;
    }
    action = g_action_target_data;
    action->attack_type = action->attack_type & ~ACTION_TYPE_HP_DAMAGE;
    hp_damage = action->hp_damage;
    action_again = g_action_target_data;
    action->hp_damage = 0;
    action->hp_healing = hp_damage;
    action_again->attack_type = action_again->attack_type | ACTION_TYPE_HP_HEALING;
}

/* Result of formula 0x0E: an undead target is healed by the amount and
 * receives none of the statuses of the first status byte (Dead among them); a
 * target immune to Dead is missed; anyone else takes it as HP damage. */
static void formula_apply_undead_absorb_attack(void) {
    battle_unit_t* unit = g_action_target;

    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)] & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        u16 amount;
        action_data_t* action = g_action_target_data;
        action_data_t* next;

        action->attack_type = ACTION_TYPE_HP_HEALING;
        amount = action->hp_damage;
        next = g_action_target_data;
        action->hp_damage = 0;
        action->hp_healing = amount;
        next->status_changes.infliction[0] = 0;
    } else if (unit->status_sets.immunity[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)) {
        formula_force_attack_miss();
    } else {
        g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
    }
}

/* MP drain step: the stored HP damage becomes MP damage on the target; when it
 * lands, the attacker recovers the same amount of MP. */
static void formula_apply_mp_absorption(void) {
    action_data_t* action = g_action_target_data;
    u16 amount;

    amount = action->hp_damage;
    action->hp_damage = 0;
    action->attack_type = ACTION_TYPE_MP_DAMAGE;
    action->mp_damage = amount;
    action_finalize_target_result();
    if (g_action_target_data->hit) {
        g_action_attacker_data->mp_healing = g_action_target_data->mp_damage;
        g_action_attacker_data->attack_type = ACTION_TYPE_MP_HEALING;
        g_action_attacker_data->hit = 1;
    }
}

/* Turn the stored HP damage into MP healing on the target (formula 0x54 heals MA * Y MP this way). */
static void formula_convert_hp_damage_to_mp_recovery(void) {
    action_data_t* action = g_action_target_data;
    u16 hp_damage;

    hp_damage = action->hp_damage;
    action->hp_damage = 0;
    action->attack_type = ACTION_TYPE_MP_HEALING;
    action->mp_healing = hp_damage;
}

/* Use the percentage in hp_damage as the hit chance and clear it. 100 or more
 * always hits. Below that the shown accuracy is scaled by it and, when
 * executing, a roll of 0..99 at or above it misses; a chance of 0 misses in
 * every state. */
static void formula_use_hp_damage_as_action_hit_percent(void) {
    s32 percent;
    s32 scaled;
    s32 state;
    s32 accuracy;
    action_data_t* action = g_action_target_data;

    percent = action->hp_damage;
    if (percent >= 100) {
        action->hp_damage = 0;
        return;
    }
    accuracy = action->attack_accuracy;
    state = g_action_state;
    scaled = percent * accuracy / 100;
    action->hp_damage = 0;
    action->attack_accuracy = scaled;
    if (state != ACTION_STATE_EXECUTE && percent != 0) {
        return;
    }
    if (random_roll_pass_fail(100, percent) != 0) {
        formula_cause_accuracy_miss();
        g_action_target_data->hp_damage = 0;
    }
}

/* Miss unless the target's monster graphic is 0x0F or 0x10 (one unsigned
 * compare tests both): the dragon and hydra jobs and the Holy Dragon. */
static void formula_force_non_dragon_target_miss(void) {
    if ((g_action_target->monster_graphic - 0xF) >= 2U) {
        formula_force_attack_miss();
    }
}

/* Force a miss when the target is asleep. Talk skills, songs and dances run this first. */
static void formula_force_sleeping_target_miss(void) {
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)] & STATUS_BYTE_MASK(STATUS_ID_SLEEP)) {
        formula_force_attack_miss();
    }
}

/* Maintenance on the target turns a break or steal into a forced failure. */
static void formula_apply_maintenance(void) {
    if (g_action_target->ability_flags.support[2] & UNIT_SUPPORT_SET_3_MAINTENANCE) {
        g_action_target_data->hit = 0;
        g_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
        g_action_target_data->attack_accuracy = 0;
    }
}

/* When executing, a roll of 0..99 below FORMULA_ADDED_EFFECT_CHANCE (19)
 * sets ACTION_SPECIAL_EFFECT_ADDED_EFFECT. */
static void roll_added_effect(void) {
    if (g_action_state == ACTION_STATE_EXECUTE && random_roll_pass_fail(100, FORMULA_ADDED_EFFECT_CHANCE) == 0) {
        g_action_target_data->special_effect |= ACTION_SPECIAL_EFFECT_ADDED_EFFECT;
    }
}

/* Queue the weapon's proc ability (proc_id) to strike the current target after
 * the action, in place of a status, and cancel any pending knockback. */
static void formula_queue_weapon_proc(void) {
    g_action_work.weapon_proc_pending = 1;
    g_action_work.reaction_id = g_action_work.proc_id;
    g_action_work.post_action_target_id = g_action_work.target_id;
    move_cancel_knockback();
}

/* Magic gun: select the Fire line for a fire gun, else Bolt for lightning, else
 * Ice (also for any other element or none), and the tier from a roll of 0..99:
 * below 10 the third spell, below 40 the second, otherwise the first
 * (10/30/60%). The spell becomes the current ability, in the attacker's action
 * command too, and its ability data is installed. */
static void formula_select_magic_gun_ability(void) {
    u8 element;
    u8 pct;
    u16 id;
    s32 roll;

    element = g_action_work.weapon_data.element;
    roll = formula_get_random();
    pct = (roll * 100) / 0x8000;
    if (element & ELEMENT_FIRE) {
        if (pct < 10) {
            id = ABILITY_ID_BLACK_MAGIC_FIRE_3;
        } else if (pct < 40) {
            id = ABILITY_ID_BLACK_MAGIC_FIRE_2;
        } else {
            id = ABILITY_ID_BLACK_MAGIC_FIRE;
        }
    } else if (element & ELEMENT_LIGHTNING) {
        if (pct < 10) {
            id = ABILITY_ID_BLACK_MAGIC_BOLT_3;
        } else if (pct < 40) {
            id = ABILITY_ID_BLACK_MAGIC_BOLT_2;
        } else {
            id = ABILITY_ID_BLACK_MAGIC_BOLT;
        }
    } else {
        if (pct < 10) {
            id = ABILITY_ID_BLACK_MAGIC_ICE_3;
        } else if (pct < 40) {
            id = ABILITY_ID_BLACK_MAGIC_ICE_2;
        } else {
            id = ABILITY_ID_BLACK_MAGIC_ICE;
        }
    }
    g_action_work.ability_id = id;
    g_action_attacker->action_command.ability_id = id;
    mem_copy_byte_data_source_first(
        &g_ability_range_data[id], &g_action_work.range_data, sizeof(g_action_work.range_data));
}

/* Quick: fill the target's CT (ACTION_CT_CHANGE_QUICK) as a pseudo-status change. */
static void formula_apply_quick_effect(void) {
    g_action_target_data->ct_change = ACTION_CT_CHANGE_QUICK;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Lower PA (Power Break, Power Ruin), MA (Mind Break, Mind Ruin) or Speed
 * (Speed Break, Speed Ruin) by X; other abilities change nothing. */
static void formula_set_stat_reduction(void) {
    u16 ability = g_action_work.ability_id;

    if (ability == ABILITY_ID_BATTLE_SKILL_POWER_BREAK || ability == ABILITY_ID_DESTROY_SWORD_POWER_RUIN) {
        g_action_target_data->pa_change = g_action_work.range_data.x & ACTION_STAT_CHANGE_VALUE_MASK;
    } else if (ability == ABILITY_ID_BATTLE_SKILL_MIND_BREAK || ability == ABILITY_ID_DESTROY_SWORD_MIND_RUIN) {
        g_action_target_data->ma_change = g_action_work.range_data.x & ACTION_STAT_CHANGE_VALUE_MASK;
    } else if (ability == ABILITY_ID_BATTLE_SKILL_SPEED_BREAK || ability == ABILITY_ID_DESTROY_SWORD_SPEED_RUIN) {
        g_action_target_data->sp_change = g_action_work.range_data.x & ACTION_STAT_CHANGE_VALUE_MASK;
    } else {
        return;
    }
    g_action_target_data->attack_type |= ACTION_TYPE_PSEUDO_STATUS;
}

/* Draw Out: the used katana breaks when a roll of 0..99 is below its weapon
 * power. The roll is made only when executing; any other state records it as
 * broken. A Mime's katana is not touched. */
static void formula_calculate_katana_break(void) {
    if (g_action_attacker->job_id == JOB_ID_MIME) {
        return;
    }

    g_action_attacker_data->hit = 1;
    g_action_attacker_data->item_lost = g_action_attacker->action_command.item_id;
    g_action_attacker_data->miss_type = ACTION_MISS_TYPE_CANCELED;

    if (g_action_state == ACTION_STATE_EXECUTE && random_roll_pass_fail(100, g_action_work.weapon_data.power) != 0) {
        g_action_attacker_data->special_effect = ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_NOT_BROKEN;
    } else {
        g_action_attacker_data->special_effect = ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN;
    }
}

/* Pick the equipment slot a Steal, Break or Mighty Sword ability targets.
 *
 * Helmet, armor, shield (the right hand's, else the left's), accessory and
 * weapon (the hand whose item has the higher required level, right on ties)
 * abilities record the slot bit in equipment_destroyed and the item in
 * item_lost, returning 0. Monsters, empty slots and other abilities clear
 * equipment_destroyed and return -1. */
static s32 formula_select_targeted_equipment(void) {
    u16 ability;
    s32 right_level;
    s32 left_level;

    g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_ALL;
    if (!(g_action_target->unit_flags & UNIT_FLAG_MONSTER)) {
        ability = g_action_work.ability_id;
        if (ability == ABILITY_ID_BATTLE_SKILL_HEAD_BREAK || ability == ABILITY_ID_MIGHTY_SWORD_BLASTAR_PUNCH
            || ability == ABILITY_ID_STEAL_HELMET) {
            if (g_action_target->equipment[UNIT_EQUIPMENT_SLOT_HEAD] != ITEM_ID_NONE) {
                g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_HEAD;
                g_action_target_data->item_lost = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
                return 0;
            }
        } else if (ability == ABILITY_ID_BATTLE_SKILL_ARMOR_BREAK || ability == ABILITY_ID_MIGHTY_SWORD_SHELLBUST_STAB
            || ability == ABILITY_ID_STEAL_ARMOR) {
            if (g_action_target->equipment[UNIT_EQUIPMENT_SLOT_BODY] != ITEM_ID_NONE) {
                g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_BODY;
                g_action_target_data->item_lost = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_BODY];
                return 0;
            }
        } else if (ability == ABILITY_ID_BATTLE_SKILL_SHIELD_BREAK || ability == ABILITY_ID_STEAL_SHIELD) {
            if (g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] != ITEM_ID_NONE) {
                g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_RIGHT_SHIELD;
                g_action_target_data->item_lost = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
                return 0;
            }
            if (g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] != ITEM_ID_NONE) {
                g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_LEFT_SHIELD;
                g_action_target_data->item_lost = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
                return 0;
            }
        } else if (ability == ABILITY_ID_MIGHTY_SWORD_ICEWOLF_BITE || ability == ABILITY_ID_STEAL_ACCESSORY) {
            if (g_action_target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY] != ITEM_ID_NONE) {
                g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_ACCESSORY;
                g_action_target_data->item_lost = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
                return 0;
            }
        } else if (ability == ABILITY_ID_BATTLE_SKILL_WEAPON_BREAK || ability == ABILITY_ID_MIGHTY_SWORD_HELLCRY_PUNCH
            || ability == ABILITY_ID_STEAL_WEAPON) {
            left_level = -1;
            right_level = -1;
            if (g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE) {
                right_level = g_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]]
                                  .required_level;
            }
            if (g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE) {
                left_level = g_item_primary_data[g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]]
                                 .required_level;
            }
            if (right_level != -1 || left_level != right_level) {
                if (right_level >= left_level) {
                    g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_RIGHT_WEAPON;
                    g_action_target_data->item_lost = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
                    return 0;
                }
                g_action_target_data->equipment_destroyed = ACTION_EQUIPMENT_SLOT_LEFT_WEAPON;
                g_action_target_data->item_lost = g_action_target->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON];
                return 0;
            }
        }
    }
    g_action_target_data->equipment_destroyed = 0;
    return -1;
}

/* Resolve Secret Hunt and Train after an HP-damaging formula 01-06 hit. Its
 * only caller skips every later formula, so Throw (0x63) and Jump (0x64) never
 * poach or train.
 *
 * Only monsters (neither sex bit set) qualify, and not through a team Golem
 * while the ability can be blocked by it, nor when dead, petrified or
 * walled. A killing blow from a Secret Hunt attacker poaches a monster job
 * target; a hit leaving the target at or below 1/5 max HP lets a Train
 * attacker inflict Invite. */
static void formula_apply_poach_and_train(void) {
    action_data_t* action = g_action_target_data;
    battle_unit_t* target;
    s32 remaining;

    if (!(action->attack_type & ACTION_TYPE_HP_DAMAGE)) {
        return;
    }
    target = g_action_target;
    remaining = target->hp - action->hp_damage + action->hp_healing;
    if (target->unit_flags & (UNIT_FLAG_FEMALE | UNIT_FLAG_MALE)) {
        return;
    }
    if ((g_action_team_golem[(target->initial_team_flags & UNIT_TEAM_MASK) >> UNIT_TEAM_SHIFT] != 0)
        && (g_action_work.range_data.flags_3 & ABILITY_SECONDARY_FLAG_3_BLOCKED_BY_GOLEM)) {
        return;
    }
    if ((target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
        || (target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))) {
        return;
    }
    if (target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_WALL)] & STATUS_BYTE_MASK(STATUS_ID_WALL)) {
        return;
    }
    if (((remaining <= 0)
            || (action->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
                & STATUS_BYTE_MASK(STATUS_ID_DEAD)))
        && (g_action_attacker->ability_flags.support[2] & UNIT_SUPPORT_SET_3_SECRET_HUNT)
        && !(target->team_flags & UNIT_TEAM_FLAG_IMMORTAL)) {
        if (target->job_id < JOB_ID_MONSTER_FIRST || target->job_id > JOB_ID_MONSTER_LAST) {
            return;
        }
        move_cancel_knockback();
        formula_clear_pending_proc();
        g_action_target_data->special_effect |= ACTION_SPECIAL_EFFECT_POACH;
        g_action_work.post_action_message = ACTION_POST_MESSAGE_POACHED;
        return;
    }
    if ((remaining > 0) && ((g_action_target->max_hp / 5) >= remaining)
        && (g_action_attacker->ability_flags.support[2] & UNIT_SUPPORT_SET_3_TRAIN)) {
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_INVITE)]
            |= STATUS_BYTE_MASK(STATUS_ID_INVITE);
        if (status_filter_action_changes(0) != 0) {
            g_action_target_data->special_effect |= ACTION_SPECIAL_EFFECT_TEAM_CHANGE;
            g_action_target_data->attack_type |= ACTION_TYPE_STATUS_CHANGE;
            g_action_work.post_action_message = ACTION_POST_MESSAGE_TRAINED;
        }
    }
}

/* Add the ability's status as a side effect of a hit: the hit flag, accuracy
 * and miss type are kept when the status does not apply. */
static void formula_apply_status_keeping_hit(void) {
    u8 hit;
    u8 miss_type;
    u16 accuracy;

    miss_type = g_action_target_data->miss_type;
    accuracy = g_action_target_data->attack_accuracy;
    hit = g_action_target_data->hit;
    formula_apply_status_to_action();
    g_action_target_data->hit = hit;
    g_action_target_data->attack_accuracy = accuracy;
    g_action_target_data->miss_type = miss_type;
}

/* Apply the current ability's status set to the target action. Also the
 * handler of formula 0x38 (status, 100%).
 *
 * All-or-nothing copies the full set, random chooses one status, separate
 * rolls each status independently, and cancel writes the removal set. Separate
 * keeps a status when a roll of 0..99 is below 24, while the shown accuracy is
 * divided by four. Preview and AI evaluation force probabilistic modes through
 * the all-or-nothing path. A set that changes nothing on the target is a
 * forced miss. */
static void formula_apply_status_to_action(void) {
    s32 status_ids[STATUS_COUNT];
    s32 type;
    s32 count;
    s32 status;
    s32 selected_status;
    s32 byte;
    s32 mask;
    u8 any;

    type = g_action_work.status_infliction.type;
    if (type & STATUS_INFLICTION_TYPE_SEPARATE) {
        g_action_target_data->attack_accuracy /= 4;
    }
    if (type & (STATUS_INFLICTION_TYPE_SEPARATE | STATUS_INFLICTION_TYPE_RANDOM_ONE)) {
        if (g_action_state != ACTION_STATE_EXECUTE) {
            type = STATUS_INFLICTION_TYPE_ALL_OR_NOTHING;
        }
    }

    if (type & STATUS_INFLICTION_TYPE_ALL_OR_NOTHING) {
        for (status = 0; status < STATUS_BYTE_COUNT; status++) {
            g_action_target_data->status_changes.infliction[status] |= g_action_work.status_infliction.statuses[status];
        }
    } else if (type & STATUS_INFLICTION_TYPE_RANDOM_ONE) {
        count = 0;
        for (status = 0; status < STATUS_COUNT; status++) {
            if (g_action_work.status_infliction.statuses[status / 8] & (0x80 >> (status & 7))) {
                status_ids[count] = status;
                count++;
            }
        }

        selected_status = status_ids[(formula_get_random() * count) / 32768];
        g_action_target_data->status_changes.infliction[selected_status / 8] |= 0x80 >> (selected_status & 7);
    } else if (type & STATUS_INFLICTION_TYPE_SEPARATE) {
        for (status = 0; status < STATUS_COUNT; status++) {
            byte = status / 8;
            /* The mask is computed inside the test, as the bytes require. */
            if (g_action_work.status_infliction.statuses[byte] & (mask = 0x80 >> (status & 7))) {
                /* Drop the status from the set unless a 24 in 100 roll keeps
                 * it; the preview test cannot pass, as only EXECUTE gets here. */
                if (random_roll_pass_fail(100, 24) != 0 || g_action_state == ACTION_STATE_PREVIEW) {
                    g_action_work.status_infliction.statuses[byte] ^= mask;
                }
            }
        }

        for (status = 0; status < STATUS_BYTE_COUNT; status++) {
            g_action_target_data->status_changes.infliction[status] |= g_action_work.status_infliction.statuses[status];
        }
    } else if (type & STATUS_INFLICTION_TYPE_CANCEL) {
        for (status = 0; status < STATUS_BYTE_COUNT; status++) {
            g_action_target_data->status_changes.removal[status] |= g_action_work.status_infliction.statuses[status];
        }
    }

    any = 0;
    for (status = 0; status < STATUS_BYTE_COUNT; status++) {
        any |= g_action_target_data->status_changes.infliction[status]
            | g_action_target_data->status_changes.removal[status];
    }

    if (any == 0 || status_filter_action_changes(0) == 0) {
        formula_force_attack_miss();
        return;
    }

    g_action_target_data->attack_type |= ACTION_TYPE_STATUS_CHANGE;
    if (g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_INVITE)]
        & STATUS_BYTE_MASK(STATUS_ID_INVITE)) {
        g_action_target_data->special_effect |= ACTION_SPECIAL_EFFECT_TEAM_CHANGE;
    }
}

/* Formula 0x49 (Ether): heal (Z * 10) MP, Z from the used item's data. */
static void formula_ether(void) {
    s32 amount = g_item_secondary_data[g_action_work.used_item_id].z;
    action_data_t* action = g_action_target_data;

    action->attack_type = ACTION_TYPE_MP_HEALING;
    action->mp_healing = amount * 10;
}

/* Store max HP as HP damage and max MP as MP healing. Both callers then run
 * formula_convert_hp_damage_to_healing, which turns the HP part into healing
 * (damage for an undead target). */
static void formula_apply_full_hp_mp_heal(void) {
    battle_unit_t* unit = g_action_target;
    action_data_t* action = g_action_target_data;

    action->hp_damage = unit->max_hp;
    action->mp_healing = unit->max_mp;
    action->attack_type = ACTION_TYPE_HP_DAMAGE | ACTION_TYPE_MP_HEALING;
}

/* Cancel the action with Finger Guard when the target's Brave-based reaction
 * roll succeeds. The shown accuracy becomes 100 - Brave, and a Brave of 100
 * always guards, even outside execution. */
static void reaction_try_finger_guard(void) {
    battle_unit_t* unit;
    s32 chance;

    if (reaction_is_blocked(g_action_target)) {
        return;
    }
    unit = g_action_target;
    if (!(unit->ability_flags.reaction[3] & UNIT_REACTION_SET_4_FINGER_GUARD)) {
        return;
    }
    chance = 100 - unit->brave;
    g_action_target_data->attack_accuracy = chance;
    if (chance != 0) {
        if (reaction_fails_brave_roll(unit)) {
            return;
        }
        if (g_action_state != ACTION_STATE_EXECUTE) {
            return;
        }
    }
    g_action_target_data->hit = 0;
    g_action_target_data->reaction_id = ABILITY_ID_REACTION_FINGER_GUARD;
    g_action_target_data->miss_type = ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD;
}

/* Catch the thrown weapon when the target's Brave-based reaction roll succeeds,
 * as reaction_try_finger_guard decides it; the caught weapon goes in
 * last_received_attack. A Mime attacker's throw is never caught. */
static void reaction_try_catch(void) {
    battle_unit_t* unit;
    action_data_t* action;
    /* Loaded through a pointer so GCC keeps it between the struct stores
     * (a plain scalar load is assumed not to alias them and floats). */
    u8* weapon_id_ptr = &g_action_work.weapon_id;
    s32 weapon_id;
    s32 chance;

    if (reaction_is_blocked(g_action_target)) {
        return;
    }
    if (g_action_attacker->job_id == JOB_ID_MIME) {
        return;
    }
    unit = g_action_target;
    if (!(unit->ability_flags.reaction[3] & UNIT_REACTION_SET_4_CATCH)) {
        return;
    }
    chance = 100 - unit->brave;
    g_action_target_data->attack_accuracy = chance;
    if (chance != 0) {
        if (reaction_fails_brave_roll(unit)) {
            return;
        }
        if (g_action_state != ACTION_STATE_EXECUTE) {
            return;
        }
    }
    g_action_target_data->hit = 0;
    action = g_action_target_data;
    action->reaction_id = ABILITY_ID_REACTION_CATCH;
    weapon_id = *weapon_id_ptr;
    action->miss_type = ACTION_MISS_TYPE_CATCH;
    action->last_received_attack = weapon_id;
}

/* Roll the evades (formula_evade_roll_sources). Returns 1 when
 * the attack was evaded. */
static s32 roll_evade(void) {
    formula_evade_roll_sources();
    return g_action_target_data->hit == 0;
}

/* Added-effect roll of the damage formulas: returns 0 when the status or proc
 * applies. When executing that is a roll of 0..99 below
 * FORMULA_ADDED_EFFECT_CHANCE (19); a preview always shows it; the AI
 * simulation never counts it. */
static s32 formula_roll_conditional_status_proc(void) {
    roll_added_effect();
    if (g_action_state == ACTION_STATE_PREVIEW) {
        return 0;
    }
    if (g_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_ADDED_EFFECT) {
        return 0;
    }
    return 1;
}

/* Physical evade step: load the target's evades, then Concentrate, a Dark,
 * Confused or Transparent attacker, Abandon, the target's statuses, the bow
 * weather penalty and facing; then roll. Returns 1 when the attack was evaded. */
static s32 formula_evade_calculate_physical(void) {
    formula_evade_store_physical();
    formula_evade_calculate_concentrate();
    formula_evade_apply_attacker_statuses();
    formula_evade_calculate_abandon();
    formula_evade_apply_target_statuses();
    formula_evade_apply_weather_on_bows();
    formula_evade_calculate_facing();
    return roll_evade();
}

/* formula_evade_calculate_physical without the bow weather penalty. */
static s32 formula_evade_calculate_physical_without_weather(void) {
    formula_evade_store_physical();
    formula_evade_calculate_concentrate();
    formula_evade_apply_attacker_statuses();
    formula_evade_calculate_abandon();
    formula_evade_apply_target_statuses();
    formula_evade_calculate_facing();
    return roll_evade();
}

/* Magic evade step: load the target's magic evades, then a Transparent
 * attacker, Abandon and the target's statuses; then roll. Returns 1 when the
 * attack was evaded. */
static s32 formula_evade_calculate_magical(void) {
    formula_evade_store_magical();
    formula_evade_calculate_transparent();
    formula_evade_calculate_abandon();
    formula_evade_apply_target_statuses();
    return roll_evade();
}

/* Physical modifiers on XA, in order: attacker supports (Two Hands, Attack Up,
 * Martial Arts), Berserk or Frog, Defense Up, the target's statuses, zodiac
 * compatibility. */
static void formula_apply_physical_modifiers(void) {
    formula_apply_physical_attack_supports();
    formula_apply_attacker_berserk_frog();
    formula_apply_defense_up();
    formula_apply_target_physical_statuses();
    formula_apply_zodiac_compatibility();
}

/* Damage of formulas 0x01 and 0x02: XA and YA by weapon type, charge power,
 * weapon element strengthen, physical modifiers, critical roll, XA * YA, then
 * the target's affinity to the weapon's elements. Returns 0 when the added
 * effect applies (formula_roll_conditional_status_proc), 1 when it does
 * not or the element canceled the attack. */
static s32 formula_calculate_weapon_damage(void) {
    formula_store_weapon_xa_ya();
    formula_apply_charge();
    formula_element_apply_weapon_strengthen();
    formula_calculate_physical_damage();
    formula_element_modify_weapon_damage();
    if (g_action_target_data->hit == 0) {
        return 1;
    }
    formula_element_apply_absorption();
    return formula_roll_conditional_status_proc();
}

/* Physical damage: XA * YA after the physical modifiers and the critical roll. */
static void formula_calculate_physical_damage(void) {
    formula_apply_physical_modifiers();
    formula_calculate_critical_hit();
    formula_store_xa_times_ya_damage();
}

/* Hit chance (XA+YA)% with every physical modifier on XA, Two Hands included.
 * Returns 1 on a miss. */
static s32 formula_accuracy_calculate_physical(void) {
    formula_apply_physical_modifiers();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}

/* Absorption, then the added-effect roll. Returns the roll's result: 0 when
 * the caller should apply the status. */
static s32 formula_element_apply_absorption_and_roll_status(void) {
    formula_element_apply_absorption();
    return formula_roll_conditional_status_proc();
}

/* Magical modifiers on XA, in order: Magic Attack Up, Magic Defense Up, the
 * target's statuses (Shell, Frog or Chicken), zodiac compatibility. */
static void formula_apply_magical_modifiers(void) {
    formula_apply_magic_attack_up();
    formula_apply_magic_defense_up();
    formula_apply_target_magical_statuses();
    formula_apply_zodiac_compatibility();
}

/* Store XA * YA as HP damage, then the weather and element steps. Returns 1
 * when the element canceled the action. */
static s32 formula_calculate_elemental_xa_times_ya(void) {
    formula_store_xa_times_ya_damage();
    formula_element_apply_weather();
    formula_element_apply();
    return g_action_target_data->hit == 0;
}

/* Absorption, then the added-effect roll; when it succeeds the ability's
 * status is applied. Returns the roll's result (0 when applied). */
static s32 formula_element_apply_absorption_and_status(void) {
    s32 result;

    formula_element_apply_absorption();
    result = formula_roll_conditional_status_proc();
    if (result == 0) {
        formula_apply_status_keeping_hit();
        return 0;
    }
    return result;
}

/* formula catalog */

/* Hit chance F(XA+YA)% for magic on allies: Magic Attack Up and zodiac
 * compatibility, none of the target's defenses. Returns 1 on a miss. */
static s32 calculate_friendly_magic_accuracy(void) {
    formula_apply_magic_attack_up();
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya();
    formula_calculate_faith();
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}

/* XA * YA with Magic Attack Up and zodiac compatibility only: the healing
 * formulas ignore the target's defenses. */
static void calculate_friendly_xa_times_ya(void) {
    formula_apply_magic_attack_up();
    formula_apply_zodiac_compatibility();
    formula_store_xa_times_ya_damage();
}

/* Golem's hit chance: (MA with Magic Attack Up + X) * caster Faith / 100, the
 * Faith status counting as 100 and Innocent as 0. No zodiac compatibility and
 * no target Faith. Returns 1 on a miss. */
static s32 calculate_golem_accuracy(void) {
    formula_store_ma_and_x();
    formula_apply_magic_attack_up();
    formula_store_xa_plus_ya();
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FAITH)]
        & STATUS_BYTE_MASK(STATUS_ID_FAITH)) {
        g_action_work.attacker_faith = 100;
    }
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
        & STATUS_BYTE_MASK(STATUS_ID_INNOCENT)) {
        g_action_work.attacker_faith = 0;
    }
    g_action_target_data->hp_damage = g_action_target_data->hp_damage * g_action_work.attacker_faith / 100;
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}

/* Magical damage without Faith: elemental strengthen, magical modifiers, XA *
 * YA with weather and element, absorption, 19% status. */
static void calculate_magical_damage_without_faith(void) {
    formula_element_apply_strengthen();
    formula_apply_magical_modifiers();
    if (formula_calculate_elemental_xa_times_ya() == 0) {
        formula_element_apply_absorption_and_status();
    }
}

/* Damage MA * (MA + Y) / 2 without Faith: stores XA and YA, then runs the magical
 * damage chain of calculate_magical_damage_without_faith. */
static void calculate_truth_damage(void) {
    formula_store_ma_and_ma_plus_y_divided_by_two();
    calculate_magical_damage_without_faith();
}

/* calculate_magic_accuracy without the elemental strengthen. */
static s32 calculate_magic_accuracy_without_strengthen(void) {
    formula_store_ma_and_x();
    formula_apply_magical_modifiers();
    formula_store_xa_plus_ya();
    formula_calculate_faith();
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}

/* Hit chance F(MA+X)%: elemental strengthen and the magical modifiers on MA,
 * zodiac compatibility on both terms, then Faith. Returns 1 on a miss. */
static s32 calculate_magic_accuracy(void) {
    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    formula_apply_magical_modifiers();
    formula_store_xa_plus_ya();
    formula_calculate_faith();
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}

/* calculate_magic_accuracy without the Faith step: hit (MA+X)%. */
static s32 calculate_magic_accuracy_without_faith(void) {
    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    formula_apply_magical_modifiers();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}

/* The physical modifiers on XA without the attacker supports: Berserk or Frog,
 * Defense Up, the target's statuses, zodiac compatibility. */
static void apply_physical_modifiers_without_supports(void) {
    formula_apply_attacker_berserk_frog();
    formula_apply_defense_up();
    formula_apply_target_physical_statuses();
    formula_apply_zodiac_compatibility();
}

/* Hit chance (XA+YA)% with Attack Up, Martial Arts, Berserk or Frog, Defense
 * Up, the target's statuses and zodiac compatibility on XA. Returns 1 on a
 * miss. */
static s32 calculate_physical_accuracy_without_two_hands(void) {
    formula_apply_attack_up_and_martial_arts();
    apply_physical_modifiers_without_supports();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    return g_action_target_data->hit == 0;
}

/* Formula 0x01: weapon damage (XA * YA by weapon type) with the weapon's
 * element; physical evade; 19% status. */
static void formula_weapon_damage(void) {
    if (formula_evade_calculate_physical() == 0 && formula_calculate_weapon_damage() == 0) {
        formula_apply_status_keeping_hit();
    }
}

/* Formula 0x02: as 0x01, but the 19% roll queues the weapon's proc ability on
 * the target instead of a status. */
static void formula_weapon_damage_with_proc(void) {
    if (formula_evade_calculate_physical() == 0 && formula_calculate_weapon_damage() == 0) {
        formula_queue_weapon_proc();
    }
}

/* Formula 0x03: gun damage (WP * WP); no evade roll, element or status. */
static void formula_gun_damage(void) {
    u16 weapon_power = g_action_work.weapon_data.power;

    g_action_work.xa = weapon_power;
    g_action_work.ya = weapon_power;
    formula_apply_charge();
    formula_calculate_physical_damage();
}

/* Formula 0x04: magic gun. Casts the first, second or third spell of the gun's
 * element (60/30/10%); damage F(WP * that spell's Y) with its element; no
 * evade. */
static void formula_magic_gun(void) {
    formula_select_magic_gun_ability();
    g_action_work.xa = g_action_work.weapon_data.power;
    g_action_work.ya = g_action_work.range_data.y;
    formula_apply_charge();
    formula_element_apply_weapon_strengthen();
    formula_apply_magical_modifiers();
    if (formula_calculate_elemental_xa_times_ya() == 0) {
        formula_calculate_faith();
        formula_element_apply_absorption();
    }
}

/* Formula 0x05: weapon damage as 0x01 without the weapon's element and the bow
 * weather penalty; physical evade; 19% status. No retail weapon, ability or item
 * record uses it: Charge runs the weapon's own formula (its menu case falls
 * through to Attack's), so it keeps the element and the bow weather penalty. */
static void formula_weapon_damage_without_element_or_weather(void) {
    if (formula_evade_calculate_physical_without_weather() == 0) {
        formula_store_weapon_xa_ya();
        formula_apply_charge();
        formula_calculate_physical_damage();
        if (formula_roll_conditional_status_proc() == 0) {
            formula_apply_status_keeping_hit();
        }
    }
}

/* Formula 0x06: weapon damage (XA * YA), absorbed by the attacker as HP
 * (reversed against undead); physical evade; no element. */
static void formula_weapon_absorb_hp(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_weapon_xa_ya();
        formula_apply_charge();
        formula_calculate_physical_damage();
        formula_apply_hp_absorption();
    }
}

/* Formula 0x07 (Healing Staff): heal weapon (XA * YA) HP (damage to undead); no evade. Zodiac
 * compatibility is applied twice: here and again inside
 * formula_apply_physical_modifiers. */
static void formula_healing_staff(void) {
    formula_store_weapon_xa_ya();
    formula_apply_zodiac_compatibility();
    formula_apply_charge();
    formula_apply_physical_modifiers();
    formula_store_xa_times_ya_damage();
    formula_convert_hp_damage_to_healing();
}

/* Formula 0x08: damage F(MA * Y) with element; magic evade; 19% status. */
static void formula_magical_damage(void) {
    if (formula_evade_calculate_magical() == 0) {
        formula_store_ma_and_y();
        formula_element_apply_strengthen();
        formula_apply_magical_modifiers();
        if (formula_calculate_elemental_xa_times_ya() == 0) {
            formula_calculate_faith();
            if (formula_element_apply_absorption_and_roll_status() == 0) {
                formula_apply_status_keeping_hit();
            }
        }
    }
}

/* Formula 0x09: damage (Y)% of max HP with element, hit F(MA+X)%; magic evade;
 * 19% status. */
static void formula_magic_hp_percent_damage(void) {
    if (formula_evade_calculate_magical() == 0) {
        if (calculate_magic_accuracy() == 0) {
            formula_calculate_hp_percent_damage();
            formula_element_apply_weather();
            formula_element_apply();
            if (g_action_target_data->hit != 0) {
                if (formula_element_apply_absorption_and_roll_status() == 0) {
                    formula_apply_status_keeping_hit();
                }
            }
        }
    }
}

/* Formula 0x0A: status, hit F(MA+X)%; magic evade. */
static void formula_hit_faith_ma_x_percent(void) {
    if (formula_evade_calculate_magical() == 0 && calculate_magic_accuracy() == 0) {
        formula_apply_status_to_action();
    }
}

/* Formula 0x0B: status, hit F(MA+X)%; no evade, and none of the target's magic
 * defenses. */
static void formula_hit_faith_ma_x_percent_no_evade(void) {
    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    if (calculate_friendly_magic_accuracy() == 0) {
        formula_apply_status_to_action();
    }
}

/* Formula 0x0C: heal F(MA * Y) HP (damage to undead). */
static void formula_magical_heal(void) {
    formula_store_ma_and_y();
    formula_element_apply_strengthen();
    calculate_friendly_xa_times_ya();
    formula_calculate_faith();
    formula_convert_hp_damage_to_healing();
}

/* Formula 0x0D: heal (Y)% of max HP, hit F(MA+X)%. The ability's status must
 * change something (so Raise fails on a living target) unless the target is
 * undead, which takes the amount as damage. */
static void formula_heal_y_percent_faith(void) {
    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    if (calculate_friendly_magic_accuracy() != 0) {
        return;
    }
    if (formula_apply_status_or_fail() == 0) {
        return;
    }
    formula_calculate_hp_percent_damage();
    formula_convert_hp_damage_to_healing();
}

/* Formula 0x0E: damage (Y)% of max HP, hit F(MA+X)%; magic evade. The
 * ability's status must apply. An undead target skips the evade and its magic
 * defenses and is healed instead; a target immune to Dead is missed. */
static void formula_damage_hp_percent_hit_faith_ma_x_percent(void) {
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
        & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        g_action_work.target_is_undead = 1;
    } else {
        g_action_work.target_is_undead = 0;
        if (formula_evade_calculate_magical() != 0) {
            return;
        }
    }
    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    formula_apply_magic_attack_up();
    if (g_action_work.target_is_undead == 0) {
        formula_apply_magic_defense_up();
        formula_apply_target_magical_statuses();
    }
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya();
    formula_calculate_faith();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit == 0) {
        return;
    }
    if (formula_apply_status_or_fail() == 0) {
        return;
    }
    formula_calculate_hp_percent_damage();
    formula_apply_undead_absorb_attack();
}

/* Formula 0x0F: absorb (Y)% of max MP, hit F(MA+X)%; magic evade. */
static void formula_absorb_mp_y_percent(void) {
    if (formula_evade_calculate_magical() != 0 || calculate_magic_accuracy_without_strengthen() != 0) {
        return;
    }
    formula_calculate_mp_percent_damage();
    /* formula_apply_mp_absorption takes its amount from hp_damage. */
    g_action_target_data->hp_damage = g_action_target_data->mp_damage;
    formula_apply_mp_absorption();
}

/* Formula 0x10: absorb (Y)% of max HP, hit F(MA+X)%; magic evade. */
static void formula_magic_hp_absorb(void) {
    if (formula_evade_calculate_magical() == 0 && calculate_magic_accuracy_without_strengthen() == 0) {
        formula_calculate_hp_percent_damage();
        formula_apply_hp_absorption();
    }
}

/* Formula 0x11: an empty handler in g_formulas. */
static void formula_11_unused(void) {
}

/* Formula 0x12: Quick, hit F(MA+X)%; no evade. */
static void formula_set_quick(void) {
    formula_store_ma_and_x();
    if (calculate_friendly_magic_accuracy() == 0) {
        formula_apply_quick_effect();
    }
}

/* Formula 0x13: an empty handler in g_formulas. */
static void formula_13_unused(void) {
}

/* Formula 0x14: Golem, hit ((MA+X) * caster Faith / 100)%. */
static void formula_set_golem(void) {
    if (calculate_golem_accuracy() == 0) {
        /* The 16-bit store clears all other special-effect flags. */
        g_action_target_data->special_effect = ACTION_SPECIAL_EFFECT_SET_GOLEM;
        g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    }
}

/* Formula 0x15 (Return 2): CT to 0, hit F(MA+X)%; magic evade. */
static void formula_return_2(void) {
    if (formula_evade_calculate_magical() == 0 && calculate_magic_accuracy_without_strengthen() == 0) {
        g_action_target_data->ct_change = ACTION_CT_CHANGE_ZERO;
        g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    }
}

/* Formula 0x16 (Mute): MP damage equal to the target's current MP, hit F(MA+X)%;
 * magic evade. */
static void formula_mute(void) {
    action_data_t* action;
    u16 mp;

    if (formula_evade_calculate_magical() == 0 && calculate_magic_accuracy_without_strengthen() == 0) {
        action = g_action_target_data;
        mp = g_action_target->mp;
        action->attack_type = ACTION_TYPE_MP_DAMAGE;
        action->mp_damage = mp;
    }
}

/* Formula 0x17 (Gravi 2): damage equal to the target's current HP - 1, hit F(MA+X)%;
 * magic evade. */
static void formula_gravi_2(void) {
    if (formula_evade_calculate_magical() == 0 && calculate_magic_accuracy_without_strengthen() == 0) {
        formula_calculate_damage_leaving_one_hp();
    }
}

/* Formula 0x18: an empty handler in g_formulas. */
static void formula_18_unused(void) {
}

/* Formula 0x19: an empty handler in g_formulas. */
static void formula_19_unused(void) {
}

/* Formula 0x1A (Destroy Sword stat ruins): -PA, -MA or -Speed (X) by ability, hit F(MA+Y)%; magic evade. */
static void formula_destroy_sword_stat_ruin(void) {
    if (formula_evade_calculate_magical() == 0) {
        formula_store_ma_and_y();
        formula_element_apply_strengthen();
        formula_apply_magical_modifiers();
        formula_store_xa_plus_ya();
        formula_calculate_faith();
        formula_use_hp_damage_as_action_hit_percent();
        if (g_action_target_data->hit == 0) {
            /* The target clears the bit in the special_effect 16-bit value. */
            g_action_target_data->special_effect &= ~ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT;
        } else {
            formula_set_stat_reduction();
        }
    }
}

/* Formula 0x1B (Magic Ruin): MP damage (Y)% of max MP, hit F(MA+X)%; magic evade. */
static void formula_magic_ruin(void) {
    if (formula_evade_calculate_magical() == 0) {
        if (calculate_magic_accuracy() == 0) {
            formula_calculate_mp_percent_damage();
        }
    }
}

/* Formula 0x1C: song, hit X%: heal (MA + Y) MP or HP, +1 Speed, PA or MA,
 * status, or Quick by ability. Never hits a sleeping target. */
static void formula_song(void) {
    formula_force_sleeping_target_miss();
    if (g_action_target_data->hit != 0) {
        if (formula_calculate_dance_song_hit() == 0) {
            formula_store_ma_and_y();
            formula_apply_song_abilities();
        }
    }
}

/* Formula 0x1D: dance, hit X%: (weapon XA + YA) MP or HP damage, -1 Speed, PA
 * or MA, status, or CT to 0 by ability. Never hits a sleeping target. */
static void formula_dance(void) {
    formula_force_sleeping_target_miss();
    if (g_action_target_data->hit != 0) {
        if (formula_calculate_dance_song_hit() == 0) {
            formula_store_weapon_xa_ya();
            formula_apply_dance_abilities();
        }
    }
}

/* Formula 0x1E: damage ((MA + Y) / 2 * MA) with element; magic evade; 19%
 * status. Strikes rnd(1..X) times
 * (init_strike_data). */
static void formula_multihit_truth_magic(void) {
    if (formula_evade_calculate_magical() == 0) {
        calculate_truth_damage();
    }
}

/* Formula 0x1F (Un-truth): damage ((MA + Y) / 2 * MA) with element, scaled by (100 -
 * caster Faith) and (100 - target Faith) over 10000, the Faith status counting
 * as 0 and Innocent as 100, so Faith on either side keeps that side's full
 * factor and Innocent zeroes the damage; magic evade; 19% status. Strikes
 * rnd(1..X) times (init_strike_data). */
static void formula_un_truth(void) {

    if (formula_evade_calculate_magical() != 0) {
        return;
    }
    formula_store_ma_and_ma_plus_y_divided_by_two();
    formula_element_apply_strengthen();
    formula_apply_magical_modifiers();
    if (formula_calculate_elemental_xa_times_ya() != 0) {
        return;
    }
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FAITH)]
        & STATUS_BYTE_MASK(STATUS_ID_FAITH)) {
        g_action_work.attacker_faith = 0;
    }
    if (g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
        & STATUS_BYTE_MASK(STATUS_ID_INNOCENT)) {
        g_action_work.attacker_faith = 100;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FAITH)] & STATUS_BYTE_MASK(STATUS_ID_FAITH)) {
        g_action_work.target_faith = 0;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INNOCENT)]
        & STATUS_BYTE_MASK(STATUS_ID_INNOCENT)) {
        g_action_work.target_faith = 100;
    }
    g_action_target_data->hp_damage = g_action_target_data->hp_damage * (100 - g_action_work.target_faith)
        * (100 - g_action_work.attacker_faith) / 10000;
    if (formula_element_apply_absorption_and_roll_status() == 0) {
        formula_apply_status_keeping_hit();
    }
}

/* Formula 0x20: Draw Out damage (MA * Y) with element; no evade; 19% status;
 * katana break roll. */
static void formula_draw_out_damage(void) {
    formula_calculate_katana_break();
    formula_store_ma_and_y();
    calculate_magical_damage_without_faith();
}

/* Formula 0x21 (Bizen Boat): Draw Out MP damage (MA * Y); katana break roll. */
static void formula_bizen_boat(void) {
    s16 damage;
    action_data_t* action;

    formula_calculate_katana_break();
    formula_store_ma_and_y();
    formula_apply_magical_modifiers();
    damage = g_action_work.xa * g_action_work.ya;
    action = g_action_target_data;
    action->attack_type = ACTION_TYPE_MP_DAMAGE;
    action->mp_damage = damage;
}

/* Formula 0x22: Draw Out status, 100%; katana break roll. */
static void formula_draw_out_status(void) {
    formula_calculate_katana_break();
    formula_apply_status_to_action();
}

/* Formula 0x23 (Murasame): Draw Out heal (MA * Y) HP with no modifiers (damage to
 * undead); katana break roll. */
static void formula_murasame(void) {
    formula_calculate_katana_break();
    formula_store_ma_and_y();
    formula_store_xa_times_ya_damage();
    formula_convert_hp_damage_to_healing();
}

/* Formula 0x24 (Elemental): damage ((PA + Y) / 2 * MA) with element; magic evade; 19%
 * status. */
static void formula_elemental(void) {
    if (formula_evade_calculate_magical() == 0) {
        formula_store_ma_and_pa_plus_y_divided_by_two();
        calculate_magical_damage_without_faith();
    }
}

/* Formula 0x25 (Battle Skill equipment breaks): break the equipment the ability names, hit (PA+WP+Y)%;
 * physical evade; Maintenance blocks it. With nothing in that slot the action
 * becomes a plain attack. */
static void formula_battle_skill_equipment_break(void) {

    if (formula_select_targeted_equipment() != 0) {
        g_action_work.defaulted_to_attack = 1;
        action_recalculate_target_result_as_attack();
        return;
    }
    g_action_work.defaulted_to_attack = 0;
    if (formula_evade_calculate_physical() != 0) {
        return;
    }
    formula_store_pa_and_weapon_power_plus_y();
    if (formula_accuracy_calculate_physical() != 0) {
        g_action_target_data->special_effect = 0;
        return;
    }
    formula_apply_maintenance();
    if (g_action_target_data->hit) {
        g_action_target_data->special_effect = ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT;
    }
}

/* Formula 0x26: steal the equipment the ability names, hit (SP+X)%; physical
 * evade; Maintenance blocks it. An empty slot or a monster target is a forced
 * failure. */
static void formula_steal_equipment(void) {

    if (formula_select_targeted_equipment() != 0) {
        formula_nullify_action();
        g_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
        return;
    }
    if (formula_evade_calculate_physical() != 0) {
        return;
    }
    formula_store_speed_and_x();
    if (calculate_physical_accuracy_without_two_hands() != 0) {
        return;
    }
    formula_apply_maintenance();
    if (g_action_target_data->hit) {
        g_action_target_data->special_effect = ACTION_SPECIAL_EFFECT_STEAL_ITEM;
    }
}

/* Formula 0x27: steal (Level * SP) gil, hit (SP+X)%; physical evade. */
static void formula_steal_gil(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_speed_and_x();
        if (calculate_physical_accuracy_without_two_hands() == 0) {
            formula_calculate_stolen_gil();
        }
    }
}

/* Formula 0x28 (Steal Exp.): steal (SP + Y) EXP, at most 100 and at most what the target
 * has, hit (SP+X)%; physical evade. */
static void formula_steal_exp(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_speed_and_x();
        formula_apply_attack_up_and_martial_arts();
        formula_apply_attacker_berserk_frog();
        formula_apply_defense_up();
        formula_apply_target_physical_statuses();
        formula_apply_zodiac_compatibility();
        formula_store_xa_plus_ya();
        formula_use_hp_damage_as_action_hit_percent();
        /* Outside execution the amount is filled in even after a miss (a 0%
         * chance), so previews and the AI still see it. */
        if (g_action_target_data->hit != 0 || g_action_state != ACTION_STATE_EXECUTE) {
            formula_set_exp_stolen();
        }
    }
}

/* Formula 0x29: Charm, hit (MA+X)%. A target whose sex and monster
 * flags all equal the caster's is a forced miss, so a monster target of a
 * human caster can be hit. */
static void formula_charm(void) {
    formula_store_ma_and_x();
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit != 0) {
        if ((g_action_target->unit_flags & (UNIT_FLAG_MONSTER | UNIT_FLAG_FEMALE | UNIT_FLAG_MALE))
            == (g_action_attacker->unit_flags & (UNIT_FLAG_MONSTER | UNIT_FLAG_FEMALE | UNIT_FLAG_MALE))) {
            formula_force_attack_miss();
        } else {
            formula_apply_status_to_action();
        }
    }
}

/* Formula 0x2A (Talk Skill), hit (MA+X)%: gil (Level * SP), CT to 0, +/-Brave
 * or Faith (Y), or status by ability (formula_apply_talk_skill). Misses
 * a sleeping target and, without Monster Talk, a monster; Finger Guard can
 * cancel it. */
static void formula_talk_skill(void) {
    formula_force_sleeping_target_miss();
    if ((g_action_target->unit_flags & UNIT_FLAG_MONSTER)
        && !(g_action_attacker->ability_flags.support[2] & UNIT_SUPPORT_SET_3_MONSTER_TALK)) {
        formula_force_attack_miss();
    }
    if (g_action_target_data->hit != 0) {
        reaction_try_finger_guard();
        if (g_action_target_data->hit != 0) {
            formula_store_ma_and_x();
            formula_apply_zodiac_compatibility();
            formula_store_xa_plus_ya();
            formula_use_hp_damage_as_action_hit_percent();
            if (g_action_target_data->hit != 0) {
                formula_apply_talk_skill();
            }
        }
    }
}

/* Formula 0x2B (Battle Skill stat breaks): -PA, -MA or -Speed (X) by ability, hit (PA+Y)%; physical
 * evade. */
static void formula_battle_skill_stat_break(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_pa_and_y();
        if (formula_accuracy_calculate_physical() == 0) {
            formula_set_stat_reduction();
        }
    }
}

/* Formula 0x2C (Magic Break): MP damage (Y)% of max MP, hit (PA+Y)%; physical evade. */
static void formula_magic_break(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_pa_and_y();
        if (formula_accuracy_calculate_physical() == 0) {
            formula_calculate_mp_percent_damage();
        }
    }
}

/* Formula 0x2D (Holy Sword): damage (PA * (WP + Y)) with the weapon's element, 100% status;
 * physical evade. */
static void formula_holy_sword(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_pa_and_weapon_power_plus_y();
        formula_element_apply_weapon_strengthen();
        formula_calculate_physical_damage();
        formula_element_modify_weapon_damage();
        if (g_action_target_data->hit != 0) {
            formula_element_apply_absorption();
            formula_apply_status_keeping_hit();
        }
    }
}

/* Formula 0x2E (Mighty Sword): break the equipment the ability names and damage (PA * WP)
 * with the weapon's element; physical evade. Fails with nothing in that slot
 * or against Maintenance. */
static void formula_mighty_sword(void) {

    if (formula_evade_calculate_physical() != 0) {
        return;
    }
    if (formula_select_targeted_equipment() != 0) {
        formula_nullify_action();
        g_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
        return;
    }
    formula_apply_maintenance();
    if (g_action_target_data->hit == 0) {
        return;
    }
    g_action_target_data->special_effect = ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT;
    formula_store_pa_and_weapon_power();
    formula_element_apply_weapon_strengthen();
    formula_calculate_physical_damage();
    formula_element_modify_weapon_damage();
    if (g_action_target_data->hit == 0) {
        return;
    }
    formula_element_apply_absorption();
}

/* Formula 0x2F (Dark Sword): absorb (PA * WP) MP; physical evade. */
static void formula_dark_sword(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_pa_and_weapon_power();
        formula_calculate_physical_damage();
        formula_apply_mp_absorption();
    }
}

/* Formula 0x30 (Night Sword): absorb (PA * WP) HP; physical evade. */
static void formula_night_sword(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_pa_and_weapon_power();
        formula_calculate_physical_damage();
        formula_apply_hp_absorption();
    }
}

/* Formula 0x31: damage ((PA + Y) / 2 * PA) with element; physical evade; 19%
 * status. */
static void formula_damage_pa_plus_y_half_times_pa(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_pa_and_pa_plus_y_divided_by_two();
        formula_element_apply_strengthen();
        formula_apply_attack_up_and_martial_arts();
        apply_physical_modifiers_without_supports();
        formula_calculate_critical_hit();
        formula_store_xa_times_ya_damage();
        formula_element_apply_weather();
        formula_element_apply();
        if (g_action_target_data->hit != 0) {
            formula_element_apply_absorption();
            if (formula_roll_conditional_status_proc() == 0) {
                formula_apply_status_keeping_hit();
            }
        }
    }
}

/* Formula 0x32 (Repeating Fist): damage ((PA + (PA + Y) / 2) * rnd(1..X)); physical evade. The
 * sum comes from formula_store_xa_plus_ya, so zodiac compatibility
 * scales only its (PA + Y) / 2 part. The random factor is kept in
 * random_damage_factor. */
static void formula_repeating_fist(void) {
    if (formula_evade_calculate_physical() != 0) {
        return;
    }
    formula_store_pa_and_pa_plus_y_divided_by_two();
    formula_apply_attack_up_and_martial_arts();
    apply_physical_modifiers_without_supports();
    formula_calculate_critical_hit();
    formula_store_xa_plus_ya();
    g_action_work.random_damage_factor = formula_get_random() * g_action_work.range_data.x / 32768 + 1;
    g_action_target_data->hp_damage = g_action_target_data->hp_damage * g_action_work.random_damage_factor;
    g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
}

/* Formula 0x33: hit (PA+X)%. */
static void formula_stigma_magic(void) {
    formula_store_pa_and_x();
    formula_apply_attack_up_and_martial_arts();
    formula_apply_attacker_berserk_frog();
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit != 0) {
        formula_apply_status_to_action();
    }
}

/* Formula 0x34 (Chakra): heal (PA*Y) HP and (PA*Y/2) MP. */
static void formula_chakra(void) {
    action_data_t* action;
    u16 amount;

    formula_store_pa_and_y();
    formula_apply_attack_up_and_martial_arts();
    formula_apply_zodiac_compatibility();
    formula_store_xa_times_ya_damage();
    action = g_action_target_data;
    amount = action->hp_damage;
    action->hp_damage = 0;
    action->hp_healing = amount;
    action->attack_type = ACTION_TYPE_HP_HEALING | ACTION_TYPE_MP_HEALING;
    /* hp_healing is re-read signed (lh) for the halving. */
    action->mp_healing = action->hp_healing / 2;
}

/* Formula 0x35: heal (Y)% of max HP, hit (PA+X)%. */
static void formula_heal_y_percent_hit_pa_x_percent(void) {
    formula_store_pa_and_x();
    formula_apply_attack_up_and_martial_arts();
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit != 0 && formula_apply_status_or_fail() != 0) {
        formula_calculate_hp_percent_damage();
        formula_convert_hp_damage_to_healing();
    }
}

/* Formula 0x37: damage (PA * rnd(1..Y)); physical evade. Knockback when a roll
 * below the target's Brave is under a roll below the attacker's Brave. */
static void formula_damage_random_y_times_pa(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_pa_and_y();
        formula_apply_attack_up_and_martial_arts();
        apply_physical_modifiers_without_supports();
        formula_apply_damage_and_knockback();
    }
}

/* Formula 0x3D: status, hit (MA+X)%; magic evade. */
static void formula_hit_ma_x_percent_magical_evade(void) {
    if (formula_evade_calculate_magical() == 0 && calculate_magic_accuracy_without_faith() == 0) {
        formula_apply_status_to_action();
    }
}

/* Formula 0x3E: damage equal to the target's current HP - 1 (0 when it has none). */
static void formula_damage_target_hp_minus_one_always_hit(void) {
    formula_calculate_damage_leaving_one_hp();
}

/* Formula 0x3F (Snipe aim abilities): hit (SP+X)%. */
static void formula_snipe_aim(void) {
    formula_store_speed_and_x();
    formula_apply_physical_modifiers();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit != 0) {
        formula_apply_status_to_action();
    }
}

/* Formula 0x40: undead targets only, hit (SP+X)%. */
static void formula_seal_evil(void) {
    formula_store_speed_and_x();
    formula_apply_physical_modifiers();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit != 0) {
        if (!(g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
                & STATUS_BYTE_MASK(STATUS_ID_UNDEAD))) {
            formula_force_attack_miss();
        } else {
            formula_apply_status_to_action();
        }
    }
}

/* Formula 0x41: hit (MA+X)%; forced miss when caster and target share a zodiac sign. */
static void formula_galaxy_stop(void) {
    if (calculate_magic_accuracy_without_faith() == 0) {
        if (g_action_target->birthday.zodiac == g_action_attacker->birthday.zodiac) {
            formula_force_attack_miss();
        } else {
            formula_apply_status_to_action();
        }
    }
}

/* Formula 0x42 (Work): damage (PA * Y); the caster takes (PA * Y / X), so the
 * caster's action data is marked hit too. No evade. */
static void formula_work(void) {
    s16 damage;
    action_data_t* action;

    formula_store_pa_and_y();
    formula_apply_zodiac_compatibility();
    damage = g_action_work.xa * g_action_work.ya;
    g_action_target_data->hp_damage = damage;
    damage = damage / g_action_work.range_data.x;
    /* One load of the attacker's record serves both stores; reading the global
     * for each store is three instructions longer. */
    action = g_action_attacker_data;
    action->hit = 1;
    action->hp_damage = damage;
    g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
    g_action_attacker_data->attack_type = ACTION_TYPE_HP_DAMAGE;
}

/* Formula 0x46: an empty handler in g_formulas. */
static void formula_46_unused(void) {
}

/* Formula 0x47 (Blood Suck): absorb (Y)% of max HP, 100% status. */
static void formula_blood_suck(void) {
    formula_calculate_hp_percent_damage();
    formula_apply_hp_absorption();
    formula_apply_status_keeping_hit();
}

/* Formula 0x48 (Potion): heal (Z * 10) HP, Z from the used item's data. The amount is
 * stored as damage; formula_convert_hp_damage_to_healing turns it into healing
 * unless the target is undead. */
static void formula_potion(void) {
    g_action_target_data->hp_damage = g_item_secondary_data[g_action_work.used_item_id].z * 10;
    formula_convert_hp_damage_to_healing();
}

/* A second entry to formula 0x49's handler. The formula table does not point
 * here and no retail code calls it. */
static void formula_49_unused(void) {
    formula_ether();
}

/* Formula 0x4A (Elixir): heal 100% HP and 100% MP. */
static void formula_elixir(void) {
    formula_apply_full_hp_mp_heal();
    formula_convert_hp_damage_to_healing();
}

/* Formula 0x4B (Phoenix Down): heal rnd(1..Z) HP once its status applies; an undead target
 * takes its current HP as damage instead. */
static void formula_phoenix_down(void) {
    battle_unit_t* unit;

    if (formula_apply_status_or_fail() == 0) {
        return;
    }
    unit = g_action_target;
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)] & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        g_action_target_data->hp_damage = unit->hp;
    } else {
        g_action_target_data->hp_damage
            = formula_get_random() * g_item_secondary_data[g_action_work.used_item_id].z / 32768 + 1;
    }
    formula_convert_hp_damage_to_healing();
}

/* Formula 0x4C: heal (MA * Y) HP. */
static void formula_heal_ma_times_y(void) {
    formula_store_ma_and_y();
    calculate_friendly_xa_times_ya();
    formula_convert_hp_damage_to_healing();
}

/* Formula 0x4D: absorb (Y)% of max HP, hit (MA+X)%; physical evade. */
static void formula_absorb_hp_y_percent_hit_ma_x_percent(void) {
    if (formula_evade_calculate_physical() != 0) {
        return;
    }
    formula_store_ma_and_x();
    if (calculate_physical_accuracy_without_two_hands() != 0) {
        return;
    }
    formula_calculate_hp_percent_damage();
    formula_apply_hp_absorption();
}

/* Formula 0x4E: damage (MA * Y) with element, 19% status; magic evade. */
static void formula_damage_ma_times_y(void) {
    if (formula_evade_calculate_magical() == 0) {
        formula_store_ma_and_y();
        calculate_magical_damage_without_faith();
    }
}

/* Formula 0x4F: damage equal to the caster's missing HP, hit (MA+X)%; physical
 * evade. */
static void formula_goblin_punch(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_ma_and_x();
        if (calculate_physical_accuracy_without_two_hands() == 0) {
            formula_damage_caster_missing_hp();
        }
    }
}

/* Formula 0x50: status, hit (MA+X)%; physical evade. */
static void formula_hit_ma_x_percent_physical_evade(void) {
    if (formula_evade_calculate_physical() == 0) {
        formula_store_ma_and_x();
        if (calculate_physical_accuracy_without_two_hands() == 0) {
            formula_apply_status_to_action();
        }
    }
}

/* Formula 0x51: hit (MA+X)%. */
static void formula_hit_ma_x_percent_no_evade(void) {
    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    formula_apply_magic_attack_up();
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit != 0) {
        formula_apply_status_to_action();
    }
}

/* Formula 0x52 (Self-Destruct): damage equal to the caster's missing HP (max HP - HP), with
 * 100% status; the caster itself, when caught in the area, takes its own
 * current HP as damage and no status. */
static void formula_self_destruct(void) {
    if (g_action_target != g_action_attacker) {
        g_action_target_data->hp_damage = g_action_attacker->max_hp - g_action_attacker->hp;
        g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
    } else {
        g_action_target_data->hp_damage = g_action_target->hp;
        g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
    }
    if (g_action_target != g_action_attacker) {
        formula_apply_status_keeping_hit();
    }
}

/* Formula 0x53: damage (Y)% of max HP with element, hit (MA+X)%, 19% status;
 * magic evade. */
static void formula_damage_y_percent_hit_ma_x_percent(void) {
    if (formula_evade_calculate_magical() != 0) {
        return;
    }
    if (calculate_magic_accuracy_without_faith() != 0) {
        return;
    }
    formula_calculate_hp_percent_damage();
    formula_element_apply_weather();
    formula_element_apply();
    if (g_action_target_data->hit != 0 && formula_element_apply_absorption_and_roll_status() == 0) {
        formula_apply_status_keeping_hit();
    }
}

/* Formula 0x54: heal (MA * Y) MP. */
static void formula_magic_spirit(void) {
    formula_store_ma_and_y();
    calculate_friendly_xa_times_ya();
    formula_convert_hp_damage_to_mp_recovery();
}

/* Formula 0x55: -PA (Y), hit (MA+X)%. */
static void formula_beaking(void) {
    if (formula_evade_calculate_magical() != 0 || calculate_magic_accuracy_without_faith() != 0) {
        return;
    }
    g_action_target_data->pa_change = g_action_work.range_data.y & ACTION_STAT_CHANGE_VALUE_MASK;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x56: -MA (Y), hit (MA+X)%. */
static void formula_circle(void) {
    if (formula_evade_calculate_magical() != 0 || calculate_magic_accuracy_without_faith() != 0) {
        return;
    }
    g_action_target_data->ma_change = g_action_work.range_data.y & ACTION_STAT_CHANGE_VALUE_MASK;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}

/* Formula 0x57 (Please Eat): +1 level (a miss at level 99) and the ability's status on the
 * caster. A full HP and MP heal is stored first, but its action type is then
 * overwritten by the level change's. */
static void formula_please_eat(void) {
    action_data_t* action;
    battle_unit_t* saved_unit;
    action_data_t* saved_action;
    battle_unit_t* attacker;
    action_data_t* attacker_action;

    formula_apply_full_hp_mp_heal();
    formula_convert_hp_damage_to_healing();
    if (g_action_target->level >= UNIT_LEVEL_MAX) {
        g_action_target_data->attack_type = 0;
        formula_force_attack_miss();
        return;
    }

    /* The status goes to the caster: the target globals point at the caster
     * for formula_apply_status_to_action, then are put back. */
    action = g_action_target_data;
    action->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    saved_unit = g_action_target;
    saved_action = g_action_target_data;
    attacker = g_action_attacker;
    attacker_action = g_action_attacker_data;
    /* The target writes the combined special-effect field as one 16-bit value. */
    action->special_effect = ACTION_SPECIAL_EFFECT_LEVEL_UP;
    g_action_target = attacker;
    g_action_target_data = attacker_action;
    attacker_action->hit = 1;
    formula_apply_status_to_action();
    g_action_target = saved_unit;
    g_action_target_data = saved_action;
}

/* Formula 0x58 (Moldball Virus): hit (MA+X)%; no evade. A generic human or
 * monster target becomes a Morbol; an immortal, mounted or unique target, or
 * one already of the Morbol family, is a forced miss. */
static void formula_moldball_virus(void) {
    action_data_t* action;
    battle_unit_t* target;

    formula_store_ma_and_x();
    formula_element_apply_strengthen();
    formula_apply_magic_attack_up();
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya();
    formula_use_hp_damage_as_action_hit_percent();
    action = g_action_target_data;
    if (action->hit == 0) {
        return;
    }
    target = g_action_target;
    if (!(target->team_flags & UNIT_TEAM_FLAG_IMMORTAL) && target->mount_info == 0
        && target->character_identity >= CHARACTER_IDENTITY_GENERIC_MALE
        && target->character_identity <= CHARACTER_IDENTITY_MONSTER
        && (target->job_id < JOB_ID_MORBOL || target->job_id > JOB_ID_MORBOL_FAMILY_LAST)) {
        /* The 16-bit store clears all other special-effect flags. */
        action->special_effect = ACTION_SPECIAL_EFFECT_MORBOL;
        action->attack_type = ACTION_TYPE_PSEUDO_STATUS;
        return;
    }
    formula_force_attack_miss();
}

/* Formula 0x59: -Level (1), hit (MA+X)%; magic evade. A level-1 target is a
 * forced miss that still carries the level-down effect. */
static void formula_level_blast(void) {
    if (formula_evade_calculate_magical() == 0 && calculate_magic_accuracy_without_faith() == 0) {
        if (g_action_target->level < 2) {
            formula_force_attack_miss();
        }
        g_action_target_data->special_effect = ACTION_SPECIAL_EFFECT_LEVEL_DOWN;
        g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    }
}

/* Formula 0x5A (Dragon Tame): status, 100%; dragons only (formula_force_non_dragon_target_miss). */
static void formula_dragon_tame(void) {
    formula_force_non_dragon_target_miss();
    if (g_action_target_data->hit != 0) {
        formula_apply_status_to_action();
    }
}

/* Formula 0x5B (Dragon Care): dragons only; the caster loses MaxHP/5 and the target gains
 * twice that (formula 0x3C), then 100% status. Y is not read. */
static void formula_dragon_care(void) {
    formula_force_non_dragon_target_miss();
    if (g_action_target_data->hit != 0) {
        formula_sacrifice_hp_to_heal();
        formula_apply_status_keeping_hit();
    }
}

/* Formula 0x5C (Dragon PowerUp): +Brave (X), +PA/MA/Speed (Y); dragons only. */
static void formula_dragon_power_up(void) {
    formula_force_non_dragon_target_miss();
    if (g_action_target_data->hit != 0) {
        formula_add_brave_x_stats_y();
    }
}

/* Formula 0x5D (Dragon LevelUp): Quick; dragons only. */
static void formula_dragon_level_up(void) {
    formula_force_non_dragon_target_miss();
    if (g_action_target_data->hit != 0) {
        formula_apply_quick_effect();
    }
}

/* Formula 0x5E: damage ((MA + Y) / 2 * MA) with element, 19% status; magic
 * evade. init_strike_data runs X+1 strikes; this handler calculates one. */
static void formula_fixed_multihit_magic(void) {
    if (formula_evade_calculate_magical() == 0) {
        calculate_truth_damage();
    }
}

/* Formula 0x5F: damage ((MA + Y) / 2 * MA) with element, 19% status; magic
 * evade. */
static void formula_nanoflare(void) {
    if (formula_evade_calculate_magical() == 0) {
        calculate_truth_damage();
    }
}

/* Formula 0x60: damage ((MA + Y) / 2 * MA) with element, 19% status; no evade. */
static void formula_damage_ma_plus_y_half_times_ma_no_evade(void) {
    calculate_truth_damage();
}

/* Formula 0x61: -Brave (Y), hit F(MA+X)%; magic evade. */
static void formula_lower_brave_y(void) {
    if (formula_evade_calculate_magical() == 0 && calculate_magic_accuracy() == 0) {
        formula_set_brave_loss_y();
    }
}

/* Formula 0x62: -Brave (Y), hit (MA+X)%; magic evade. */
static void formula_look_of_fright(void) {
    if (formula_evade_calculate_magical() == 0 && calculate_magic_accuracy_without_faith() == 0) {
        formula_set_brave_loss_y();
    }
}

/* Formula 0x63: thrown weapon damage (SP * WP) with the weapon's element;
 * Catch, then physical evade. */
static void formula_throw_damage(void) {
    reaction_try_catch();
    if (g_action_target_data->hit == 0) {
        return;
    }
    if (formula_evade_calculate_physical() != 0) {
        return;
    }
    g_action_work.xa = g_action_attacker->attributes[UNIT_ATTRIBUTE_SPEED];
    g_action_work.ya = g_action_work.weapon_data.power;
    apply_physical_modifiers_without_supports();
    formula_store_xa_times_ya_damage();
    formula_element_modify_weapon_damage();
    if (g_action_target_data->hit == 0) {
        return;
    }
    formula_element_apply_absorption();
}

/* Formula 0x64: damage (PA * WP); with a spear (PA * 3/2 * WP); bare-handed
 * (PA * (PA * Brave / 100)). No evade roll here. */
static void formula_jump_damage(void) {
    formula_store_jump_xa_ya();
    apply_physical_modifiers_without_supports();
    formula_store_xa_times_ya_damage();
}

/* strike resolution */

extern item_poach_t g_item_poach_table[JOB_ID_MONSTER_LAST - JOB_ID_MONSTER_FIRST + 1];
extern const u8 g_terrain_status_flags[64];

/* Formula IDs are one-based; zero and invalid IDs select the weapon formula. */
static void (*s_formulas[ABILITY_FORMULA_ID_LIMIT - 1])(void) = {
    formula_weapon_damage,
    formula_weapon_damage_with_proc,
    formula_gun_damage,
    formula_magic_gun,
    formula_weapon_damage_without_element_or_weather,
    formula_weapon_absorb_hp,
    formula_healing_staff,
    formula_magical_damage,
    formula_magic_hp_percent_damage,
    formula_hit_faith_ma_x_percent,
    formula_hit_faith_ma_x_percent_no_evade,
    formula_magical_heal,
    formula_heal_y_percent_faith,
    formula_damage_hp_percent_hit_faith_ma_x_percent,
    formula_absorb_mp_y_percent,
    formula_magic_hp_absorb,
    formula_11_unused,
    formula_set_quick,
    formula_13_unused,
    formula_set_golem,
    formula_return_2,
    formula_mute,
    formula_gravi_2,
    formula_18_unused,
    formula_19_unused,
    formula_destroy_sword_stat_ruin,
    formula_magic_ruin,
    formula_song,
    formula_dance,
    formula_multihit_truth_magic,
    formula_un_truth,
    formula_draw_out_damage,
    formula_bizen_boat,
    formula_draw_out_status,
    formula_murasame,
    formula_elemental,
    formula_battle_skill_equipment_break,
    formula_steal_equipment,
    formula_steal_gil,
    formula_steal_exp,
    formula_charm,
    formula_talk_skill,
    formula_battle_skill_stat_break,
    formula_magic_break,
    formula_holy_sword,
    formula_mighty_sword,
    formula_dark_sword,
    formula_night_sword,
    formula_damage_pa_plus_y_half_times_pa,
    formula_repeating_fist,
    formula_stigma_magic,
    formula_chakra,
    formula_heal_y_percent_hit_pa_x_percent,
    formula_add_pa_y,
    formula_damage_random_y_times_pa,
    formula_apply_status_to_action,
    formula_yell,
    formula_cheer_up,
    formula_add_brave_x_stats_y,
    formula_sacrifice_hp_to_heal,
    formula_hit_ma_x_percent_magical_evade,
    formula_damage_target_hp_minus_one_always_hit,
    formula_snipe_aim,
    formula_seal_evil,
    formula_galaxy_stop,
    formula_work,
    formula_damage_caster_missing_hp,
    formula_difference,
    formula_climhazzard,
    formula_46_unused,
    formula_blood_suck,
    formula_potion,
    formula_ether,
    formula_elixir,
    formula_phoenix_down,
    formula_heal_ma_times_y,
    formula_absorb_hp_y_percent_hit_ma_x_percent,
    formula_damage_ma_times_y,
    formula_goblin_punch,
    formula_hit_ma_x_percent_physical_evade,
    formula_hit_ma_x_percent_no_evade,
    formula_self_destruct,
    formula_damage_y_percent_hit_ma_x_percent,
    formula_magic_spirit,
    formula_beaking,
    formula_circle,
    formula_please_eat,
    formula_moldball_virus,
    formula_level_blast,
    formula_dragon_tame,
    formula_dragon_care,
    formula_dragon_power_up,
    formula_dragon_level_up,
    formula_fixed_multihit_magic,
    formula_nanoflare,
    formula_damage_ma_plus_y_half_times_ma_no_evade,
    formula_lower_brave_y,
    formula_look_of_fright,
    formula_throw_damage,
    formula_jump_damage,
};

/* When the unit acts with the Jump skillset while Transparent is inflicted on it, make
 * the attacker's own action result a hit that removes Transparent. Also points
 * g_action_attacker_data at that result. */
void status_prepare_jump_transparent_removal(battle_unit_t* unit) {
    action_data_t* action = &g_action_attacker_action_data;

    g_action_attacker_data = action;
    if (unit->action_command.skillset_id == SKILLSET_ID_JUMP
        && (unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
            & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT))) {
        action->hit = 1;
        action->attack_type |= ACTION_TYPE_STATUS_CHANGE;
        action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
            |= STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT);
    }
}

/* Apply the ability's status. Returns 1 when it changes something or the
 * target is undead; otherwise the action is canceled as a forced failure and
 * 0 returned. */
static s32 formula_apply_status_or_fail(void) {
    formula_apply_status_keeping_hit();
    if (g_action_target_data->attack_type & ACTION_TYPE_STATUS_CHANGE) {
        return 1;
    }
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
        & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        return 1;
    }
    formula_nullify_action();
    g_action_target_data->miss_type = ACTION_MISS_TYPE_FORCED_FAILURE;
    return 0;
}

/* Recalculate the current target's result as a plain weapon
 * Attack, then restore the attacker's skillset and ability and the strike
 * counter. It ends with the current ability's weapon_proc_pending and reaction_id
 * cleared. */
static void action_recalculate_target_result_as_attack(void) {
    battle_unit_t* unit = g_action_attacker;
    battle_unit_t* attacker;
    battle_unit_t* attacker_after;
    u8 saved_strike_counter = g_action_work.strike_counter;
    u8 saved_skillset = unit->action_command.skillset_id;
    u16 saved_ability;

    /* g_action_attacker is read again after each store, as retail does. */
    unit->action_command.skillset_id = SKILLSET_ID_ATTACK;
    attacker = g_action_attacker;
    saved_ability = unit->action_command.ability_id;
    attacker->action_command.ability_id = 0;
    action_calculate_target_result(&attacker->action_command, g_action_work.target_id);
    attacker_after = g_action_attacker;
    g_action_work.strike_counter = saved_strike_counter;
    attacker_after->action_command.ability_id = saved_ability;
    attacker_after->action_command.skillset_id = saved_skillset;
    g_action_work.weapon_proc_pending = 0;
    g_action_work.reaction_id = 0;
}

/* Reset the result records of the current target and the current attacker; the
 * attacker's starts as not hit. */
static void reset_current_results(void) {
    action_reset_result(g_action_target_data);
    action_reset_result(g_action_attacker_data);
    g_action_attacker_data->hit = 0;
}

/* Resolve a pending knockback as the target's own action. A grounded target
 * that is not a crystal, dead or petrified takes (fall height / 2 - Jump) *
 * MaxHP / 10 fall damage when that is positive, at most 999; a flying one
 * takes none. */
static void apply_knockback(void) {
    battle_unit_t* unit;
    s32 amount;
    s32 diff;

    reset_current_results();
    g_action_work.knockback_flags &= MOVE_KNOCKBACK_KIND_MASK;
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    unit = g_action_target;
    amount = 0;
    if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))) {
        if (g_action_work.knockback_flags == MOVE_KNOCKBACK_FALL_DAMAGE) {
            diff = (g_action_work.knockback_fall_height >> 1) - unit->jump;
            if (diff > 0) {
                amount = (diff * unit->max_hp) / 10;
                if (amount > ACTION_VALUE_MAX) {
                    amount = ACTION_VALUE_MAX;
                }
            }
        }
        g_action_target_data->hp_damage = amount;
        if (amount != 0) {
            g_action_target_data->attack_type |= ACTION_TYPE_HP_DAMAGE;
        }
    }
}

/* Apply the pending reaction ability's effect to the current target action
 * record and finish the strike. Returns 0 for a reaction whose behavior flags
 * have TRIGGER_ABILITY without ABILITY (one that answers with an ability, such
 * as Counter), which the caller resolves as an ordinary strike; -1 for an
 * ability with no effect entry; and 1 once the action has been finalized.
 * Reflect strikes on behalf of the original acting unit.
 *
 * The saves add 1 to PA, MA or Speed, Brave Up and Faith Up add 3, HP and MP
 * Restore fill the gauge, Distribute heals each listed unit its share of the
 * excess healing (rounded up) and Damage Split moves half the damage onto the
 * attacker while the reactor heals it.
 *
 * The Damage Split case chains the damage store so the value reaches `healing` through
 * the target record; storing it there first is what orders the three global
 * pointer loads the way the target emits them. */
static s32 apply_reaction_effect(void) {
    s16 ability_id;
    u8 attack_type = ACTION_TYPE_PSEUDO_STATUS;
    s32 healing;
    s32 count;

    ability_id = g_reaction_ability_id;
    if (ability_id == ABILITY_ID_REACTION_REFLECT) {
        g_action_attacker = &g_battle_units[g_action_actor_id];
    }
    if ((g_ability_reaction_behavior_flags[ability_id - ABILITY_ID_REACTION_FIRST]
            & (REACTION_BEHAVIOR_FLAG_TRIGGER_ABILITY | REACTION_BEHAVIOR_FLAG_ABILITY))
        == 1) {
        return 0;
    }
    reset_current_results();
    switch (g_reaction_ability_id) {
    case ABILITY_ID_REACTION_PA_SAVE:
        g_action_target_data->pa_change = ACTION_STAT_CHANGE_INCREASE | 1;
        break;
    case ABILITY_ID_REACTION_MA_SAVE:
        g_action_target_data->ma_change = ACTION_STAT_CHANGE_INCREASE | 1;
        break;
    case ABILITY_ID_REACTION_SPEED_SAVE:
        g_action_target_data->sp_change = ACTION_STAT_CHANGE_INCREASE | 1;
        break;
    case ABILITY_ID_REACTION_SUNKEN_STATE:
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
            = STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT);
        if (status_filter_action_changes(0) != 0) {
            attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case ABILITY_ID_REACTION_CAUTION:
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_DEFENDING)]
            = STATUS_BYTE_MASK(STATUS_ID_DEFENDING);
        if (status_filter_action_changes(0) != 0) {
            attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case ABILITY_ID_REACTION_DRAGON_SPIRIT:
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_RERAISE)]
            = STATUS_BYTE_MASK(STATUS_ID_RERAISE);
        if (status_filter_action_changes(0) != 0) {
            attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case ABILITY_ID_REACTION_REGENERATOR:
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_REGEN)]
            = STATUS_BYTE_MASK(STATUS_ID_REGEN);
        if (status_filter_action_changes(0) != 0) {
            attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
        break;
    case ABILITY_ID_REACTION_BRAVE_UP:
        g_action_target_data->brave_change = ACTION_STAT_CHANGE_INCREASE | 3;
        break;
    case ABILITY_ID_REACTION_FAITH_UP:
        g_action_target_data->faith_change = ACTION_STAT_CHANGE_INCREASE | 3;
        break;
    case ABILITY_ID_REACTION_HP_RESTORE:
        attack_type = ACTION_TYPE_HP_HEALING;
        g_action_target_data->hp_healing = g_action_target->max_hp - g_action_target->hp;
        break;
    case ABILITY_ID_REACTION_MP_RESTORE:
        attack_type = ACTION_TYPE_MP_HEALING;
        g_action_target_data->mp_healing = g_action_target->max_mp - g_action_target->mp;
        break;
    case ABILITY_ID_REACTION_CRITICAL_QUICK:
        g_action_target_data->ct_change = ACTION_CT_CHANGE_QUICK;
        break;
    case ABILITY_ID_REACTION_MEATBONE_SLASH:
        attack_type = ACTION_TYPE_HP_DAMAGE;
        g_action_target_data->hp_damage = g_action_attacker->max_hp;
        break;
    case ABILITY_ID_REACTION_ABSORB_USED_MP:
        attack_type = ACTION_TYPE_MP_HEALING;
        g_action_target_data->mp_healing = g_action_target_data->last_received_attack;
        break;
    case ABILITY_ID_REACTION_GILGAME_HEART:
        g_action_target_data->gil_change = g_action_target_data->last_received_attack;
        break;
    case ABILITY_ID_REACTION_DISTRIBUTE:
        count = g_reaction_distribute_target_count;
        if (count != 0) {
            healing = (g_action_attacker->action.last_received_attack + count - 1) / count;
        } else {
            healing = 0;
        }
        attack_type = ACTION_TYPE_HP_HEALING;
        g_action_target_data->hp_healing = healing;
        break;
    case ABILITY_ID_REACTION_DAMAGE_SPLIT:
        healing = g_action_target_data->hp_damage = g_action_attacker->action.last_received_attack;
        attack_type = ACTION_TYPE_HP_DAMAGE;
        g_action_attacker_data->hp_healing = healing;
        g_action_attacker_data->attack_type = ACTION_TYPE_HP_HEALING;
        g_action_attacker_data->hit = 1;
        break;
    default:
        g_action_target_data->attack_type = 0;
        return -1;
    }
    g_action_target_data->attack_type = attack_type;
    action_finalize_target_result();
    if (g_action_target_data->special_effect != 0) {
        g_action_target_data->attack_type |= ACTION_TYPE_PSEUDO_STATUS;
    }
    g_action_work.reaction_id = 0;
    return 1;
}

/* Record the weapon the current strike uses: the primary hand on the first strike and the
 * secondary after it; none for an ordinary ability that neither uses weapon range nor strikes with
 * the weapon, and none for Jump; the chosen item for the item and katana menus. An empty hand
 * (0xFF) is stored as none. */
void action_store_used_weapon(action_command_t* action) {
    u8 weapon;
    s16 ability_id;

    if (g_action_work.strike_counter == 0) {
        weapon = g_action_work.primary_weapon_id;
    } else {
        weapon = g_action_work.secondary_weapon_id;
    }
    switch (g_command_menu_types_by_skillset[action->skillset_id]) {
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_MATH_SKILL:
    case ACTION_MENU_TYPE_ELEMENTS:
    case ACTION_MENU_TYPE_MONSTER:
        ability_id = action->ability_id;
        if (ability_id < ABILITY_ID_ITEM_FIRST) {
            if (!(g_ability_range_data[ability_id].flags_1
                    & (ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE | ABILITY_SECONDARY_FLAG_1_WEAPON_STRIKE))) {
                weapon = ITEM_ID_NOTHING;
            }
        }
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        weapon = action->item_id;
        break;
    case ACTION_MENU_TYPE_JUMP:
    case ACTION_MENU_TYPE_UNKNOWN_0F:
        weapon = ITEM_ID_NOTHING;
        break;
    }
    if (weapon == ITEM_ID_NONE) {
        weapon = ITEM_ID_NOTHING;
    }
    g_action_work.weapon_id = weapon;
}

/* g_action_work bound a second time. Spelling every field through one
 * symbol lets GCC share a base register between the two access groups below,
 * which changes this function's code. */
static action_work_t s_action_work_view;

/* Calculate one target's action result: load the current ability, weapon and
 * item data for the attacker/target pair, run the evasion reactions and the
 * formula handler, and finalize the result.
 *
 * Returns 1 when the target was already missed before the formula (reflect,
 * Blade Grasp, Arrow Guard) and 0 otherwise, including the reaction and
 * knockback early exits. id holds a weapon or item id, then a status-infliction
 * index. */
s32 action_calculate_target_result(const action_command_t* source, u8 target_id) {
    action_command_t action;
    weapon_data_t* weapon;
    ability_secondary_data_t* ability;
    item_secondary_data_t* item;
    ability_data_t* data;
    u8 menu_type;
    u8 skillset;
    u8 formula;
    u8 id;
    u16 ability_id;
    s32 location;

    action_command_copy(source, &action);
    g_action_work.target_id = target_id;
    g_action_target = &g_battle_units[target_id];
    g_action_target_data = &g_battle_units[target_id].action;
    g_action_work.post_action_message = ACTION_POST_MESSAGE_NONE;
    s_action_work_view.attacker_id = action.actor_id;
    g_action_attacker_data = &g_action_attacker_action_data;
    g_action_attacker = &g_battle_units[s_action_work_view.attacker_id];

    if (g_action_context != ACTION_CONTEXT_PRIMARY && apply_reaction_effect() != 0) {
        action_store_used_weapon(&action);
        return 0;
    }
    if (g_action_work.knockback_flags & MOVE_KNOCKBACK_PENDING) {
        apply_knockback();
        return 0;
    }

    location = battle_unit_get_tile_index(g_action_target);
    s_action_work_view.terrain = g_map_tile_data[location].surface.type;
    if (g_action_work.strike_counter == 0) {
        id = g_action_work.primary_weapon_id;
    } else {
        id = g_action_work.secondary_weapon_id;
    }
    if (action.skillset_id == SKILLSET_ID_DRAW_OUT) {
        id = action.item_id;
    }
    /* Weapon data covers only item ids below 0x80. */
    if (id >= 0x80) {
        id = ITEM_ID_NOTHING;
    }

    s_action_work_view.weapon_id = id;
    s_action_work_view.ability_id = action.ability_id;
    ability_id = action.ability_id;
    g_action_work.two_hands_active = 0;
    if (s_action_work_view.ability_id > ABILITY_ID_ITEM_FIRST) {
        ability_id = 0;
    }

    /* Start from the weapon's data and formula; the menu switch below overrides them. */
    weapon = &g_item_weapon_data[id];
    mem_copy_byte_data_source_first(
        &g_ability_range_data[ability_id], &s_action_work_view.range_data, sizeof(ability_secondary_data_t));
    mem_copy_byte_data_source_first(weapon, &s_action_work_view.weapon_data, sizeof(weapon_data_t));
    g_action_work.proc_id = weapon->proc_id;
    g_action_work.charge_power = 0;
    g_action_work.formula = ABILITY_FORMULA_WEAPON_DAMAGE;
    g_action_work.attacker_faith = g_action_attacker->faith;
    g_action_work.target_faith = g_action_target->faith;

    reset_current_results();
    skillset = action.skillset_id;
    g_action_work.skillset = skillset;
    menu_type = g_command_menu_types_by_skillset[skillset];
    /* A pending weapon proc resolves its spell, held in reaction_id, as a default-menu ability. */
    if (s_action_work_view.weapon_proc_pending != 0) {
        action.ability_id = s_action_work_view.reaction_id;
        s_action_work_view.ability_id = action.ability_id;
        menu_type = ACTION_MENU_TYPE_DEFAULT;
    } else {
        s_action_work_view.reaction_id = 0;
    }

    data = &g_ability_data[action.ability_id];
    g_action_target_data->miss_display_flags = data->ai_flags.flags_1 & ACTION_MISS_DISPLAY_TEAM_MASK;
    if (data->ai_flags.flags_4 & AI_ABILITY_FLAG_4_EVADE_WITH_MOTION) {
        g_action_target_data->miss_display_flags += ACTION_MISS_DISPLAY_DODGE_ANIMATION;
    }

    if (menu_type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
        id = action.item_id;
        action.ability_id = id + (ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE);
    }

    switch (menu_type) {
    case ACTION_MENU_TYPE_ELEMENTS:
        action.ability_id = ability_get_elemental_id(g_action_attacker);
        /* fall through */
    case ACTION_MENU_TYPE_DEFAULT:
    case ACTION_MENU_TYPE_MATH_SKILL:
    case ACTION_MENU_TYPE_MONSTER:
    case ACTION_MENU_TYPE_KATANA_INVENTORY:
        formula = ABILITY_FORMULA_WEAPON_DAMAGE;
        if (action.ability_id <= ABILITY_ID_ITEM_FIRST) {
            /* A frog strikes without its weapon. */
            if ((g_action_attacker->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FROG)]
                    & STATUS_BYTE_MASK(STATUS_ID_FROG))
                && s_action_work_view.weapon_proc_pending == 0) {
                s_action_work_view.weapon_id = ITEM_ID_NOTHING;
                mem_copy_byte_data_source_first(
                    g_item_weapon_data, &s_action_work_view.weapon_data, sizeof(weapon_data_t));
            }
            ability = &g_ability_range_data[action.ability_id];
            mem_copy_byte_data_source_first(ability, &s_action_work_view.range_data, sizeof(ability_secondary_data_t));
            g_action_work.proc_id = ability->status_infliction_id;
            formula = ability->formula;
            /* An ability that does not use the weapon drops it, except Draw Out. */
            if (!(g_action_work.range_data.flags_1
                    & (ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE | ABILITY_SECONDARY_FLAG_1_WEAPON_STRIKE))
                && menu_type != ACTION_MENU_TYPE_KATANA_INVENTORY) {
                s_action_work_view.weapon_id = ITEM_ID_NOTHING;
            }
        }
        break;
    case ACTION_MENU_TYPE_ITEM_INVENTORY:
        formula = ABILITY_FORMULA_WEAPON_DAMAGE;
        id = action.item_id;
        if (id >= ITEM_ID_CONSUMABLE_FIRST) {
            s_action_work_view.weapon_id = id;
            id += ITEM_ID_COUNT - ITEM_ID_CONSUMABLE_FIRST;
            item = &g_item_secondary_data[id];
            g_action_work.used_item_id = id;
            g_action_work.proc_id = item->status_infliction_id;
            mem_zero(&s_action_work_view.range_data, sizeof(ability_secondary_data_t));
            formula = item->formula;
        }
        break;
    case ACTION_MENU_TYPE_WEAPON_INVENTORY:
        formula = ABILITY_FORMULA_THROW_DAMAGE;
        break;
    case ACTION_MENU_TYPE_JUMP:
        formula = ABILITY_FORMULA_JUMP_DAMAGE;
        break;
    case ACTION_MENU_TYPE_CHARGE:
        ability_id = action.ability_id;
        if (ability_id < ABILITY_ID_CHARGE_FIRST || ability_id >= ABILITY_ID_MATH_FIRST) {
            ability_id = ABILITY_ID_CHARGE_FIRST;
        }
        g_action_work.charge_power = (g_ability_charge_data - ABILITY_ID_CHARGE_FIRST)[ability_id].power;
        /* fall through */
    case ACTION_MENU_TYPE_ATTACK:
        formula = weapon->formula;
        if ((g_action_work.weapon_data.flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE)
            && (g_action_attacker->ability_flags.support[2] & UNIT_SUPPORT_SET_3_TWO_HANDS)) {
            if (g_action_attacker->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] == ITEM_ID_NONE
                && g_action_attacker->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] == ITEM_ID_NONE) {
                g_action_work.two_hands_active = 1;
            }
            if (g_action_attacker->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] == ITEM_ID_NONE
                && g_action_attacker->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] == ITEM_ID_NONE) {
                g_action_work.two_hands_active = 1;
            }
        }
        break;
    default:
        formula = ABILITY_FORMULA_WEAPON_DAMAGE;
        break;
    }

    s_action_work_view.weapon_proc_pending = 0;
    g_action_work.knockback_flags = 0;
    if (formula == 0 || formula >= ABILITY_FORMULA_ID_LIMIT) {
        formula = ABILITY_FORMULA_WEAPON_DAMAGE;
    }

    /* id now indexes the status infliction table; guns never inflict their proc (entry 0). */
    id = s_action_work_view.proc_id;
    if (formula == ABILITY_FORMULA_GUN_DAMAGE) {
        id = 0;
    }
    mem_copy_byte_data_source_first(
        &g_status_infliction_data[id], &s_action_work_view.status_infliction, sizeof(status_infliction_data_t));
    store_ability_cancellations();
    g_action_work.formula = formula;

    /* Evasion reactions run first; a miss skips the formula. */
    dispatch_evasion_reactions();
    if (g_action_target_data->hit == 0) {
        return 1;
    }

    s_formulas[formula - 1]();
    /* Only the weapon formulas before Weapon Heal (1..6) go on to Poach and Train. */
    if (formula < ABILITY_FORMULA_WEAPON_HEAL) {
        formula_apply_poach_and_train();
    }
    if (formula == ABILITY_FORMULA_JUMP_DAMAGE) {
        s_action_work_view.weapon_id = ITEM_ID_NOTHING;
    }

    action_finalize_target_result();
    if (g_action_target_data->special_effect != 0) {
        g_action_target_data->attack_type |= ACTION_TYPE_PSEUDO_STATUS;
    }
    return 0;
}

/* For an ability whose status infliction type is cancel, copy its status bytes
 * to g_action_canceled_statuses. Run while calculating a target result. */
static void store_ability_cancellations(void) {
    s32 i;

    if (g_action_work.status_infliction.type & STATUS_INFLICTION_TYPE_CANCEL) {
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_action_canceled_statuses[i] = g_action_work.status_infliction.statuses[i];
        }
    }
}

/* Validate and clamp the target's current action result before it applies.
 *
 * A Dead target (unless the ability cancels Dead and the target is not
 * undead), or a petrified or walled one the ability does not cancel,
 * turns the action into a miss. Otherwise zero accuracy clears the hit flag,
 * HP/MP changes are capped at 999, a lethal HP hit drops knockback, and a
 * hit absorbed by the target team's Golem becomes a guarded miss. */
static void action_finalize_target_result(void) {
    action_data_t* action;
    u8 status = g_action_target->status_sets.current[0];
    s32 invalid = 0;
    s32 team;

    if ((status & STATUS_BYTE_MASK(STATUS_ID_DEAD))
        && (!(g_action_canceled_statuses[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
            || (status & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)))) {
        invalid = 1;
    }
    if ((g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
            & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))
        && !(g_action_canceled_statuses[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)] & STATUS_BYTE_MASK(STATUS_ID_PETRIFY))) {
        invalid = 1;
    }
    if ((g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_WALL)] & STATUS_BYTE_MASK(STATUS_ID_WALL))
        && !(g_action_canceled_statuses[STATUS_BYTE_INDEX(STATUS_ID_WALL)] & STATUS_BYTE_MASK(STATUS_ID_WALL))) {
        invalid = 1;
    }
    if (invalid != 0) {
        action_remove_ability_target(g_action_target->battle_unit_id);
        reset_current_results();
        formula_clear_pending_proc();
        move_cancel_knockback();
        g_action_work.post_action_message = ACTION_POST_MESSAGE_NONE;
        formula_force_attack_miss();
        g_action_target_data->miss_type = ACTION_MISS_TYPE_CANCELED;
        return;
    }
    if (g_action_target_data->attack_accuracy == 0) {
        g_action_target_data->hit = 0;
    }
    if (g_action_target_data->hp_damage > ACTION_VALUE_MAX) {
        g_action_target_data->hp_damage = ACTION_VALUE_MAX;
    }
    if (g_action_target_data->hp_healing > ACTION_VALUE_MAX) {
        g_action_target_data->hp_healing = ACTION_VALUE_MAX;
    }
    if (g_action_target_data->mp_damage > ACTION_VALUE_MAX) {
        g_action_target_data->mp_damage = ACTION_VALUE_MAX;
    }
    if (g_action_target_data->mp_healing > ACTION_VALUE_MAX) {
        g_action_target_data->mp_healing = ACTION_VALUE_MAX;
    }
    if (g_action_target_data->hp_damage >= g_action_target->hp) {
        move_cancel_knockback();
        formula_clear_pending_proc();
    }
    team = (g_action_target->initial_team_flags & UNIT_TEAM_MASK) >> UNIT_TEAM_SHIFT;
    action = g_action_target_data;
    if (action->attack_type != 0 && action->hit && g_action_context == ACTION_CONTEXT_PRIMARY
        && g_action_team_golem[team] != 0
        && (g_action_work.range_data.flags_3 & ABILITY_SECONDARY_FLAG_3_BLOCKED_BY_GOLEM)) {
        action->miss_type = ACTION_MISS_TYPE_GOLEM_GUARD;
        action->special_effect = ACTION_SPECIAL_EFFECT_GOLEM_GUARD;
        clear_status_changes(g_action_target_data);
        move_cancel_knockback();
    }
}

/* Drop a pending weapon proc together with the target's
 * ACTION_SPECIAL_EFFECT_ADDED_EFFECT flag. */
static void formula_clear_pending_proc(void) {
    u16* proc_pending = &g_action_work.weapon_proc_pending;
    action_data_t* action;
    u16* special_effect;

    if (*proc_pending != 0) {
        action = g_action_target_data;
        g_action_work.reaction_id = 0;
        *proc_pending = 0;
        /* Clear the added-effect bit. Reading the field through a
         * second pointer keeps the lhu below the two scalar global stores;
         * a direct action->special_effect load is hoisted above them
         * (MEM_IN_STRUCT_P) and the function comes out 4 bytes short. */
        special_effect = &action->special_effect;
        *special_effect &= ~ACTION_SPECIAL_EFFECT_ADDED_EFFECT;
    }
}

/* Cancel a pending knockback: clear the current ability's knockback flags and the
 * target's KNOCKBACK special effect. */
static void move_cancel_knockback(void) {
    action_data_t* target_data;

    if (g_action_work.knockback_flags & MOVE_KNOCKBACK_PENDING) {
        target_data = g_action_target_data;
        g_action_work.knockback_flags = 0;
        target_data->special_effect &= ~ACTION_SPECIAL_EFFECT_KNOCKBACK;
    }
}

/* Reset an action result record to "hit, 100% accuracy, no effect".
 *
 * Bytes 0x00-0x0d, 0x10-0x25 and 0x28-0x29 are cleared; reaction_id (0x0e) and
 * last_received_attack (0x26) keep their values. The canceled-status list is
 * cleared with the record's status changes. */
void action_reset_result(action_data_t* action) {
    mem_zero(action, 0xE);
    mem_zero(&action->special_effect, 0x16);
    mem_zero(&action->exp_change, 2);
    /* The target stores the accuracy as a 16-bit value (0x2a..0x2b). */
    action->attack_accuracy = 100;
    action->attack_type = 0;
    action->hit = 1;
    clear_status_changes(action);
}

/* Clear the record's status inflictions and removals, and the shared list of
 * canceled statuses with them. */
static void clear_status_changes(action_data_t* action) {
    s32 i;

    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        g_action_canceled_statuses[i] = 0;
        action->status_changes.infliction[i] = 0;
        action->status_changes.removal[i] = 0;
    }
}

#define STATUS_MASK(id) STATUS_BYTE_MASK(STATUS_ID_##id)

/* Apply the target's pending action results and flag reactions.
 *
 * A dead unit turning into a crystal or treasure only resolves that change.
 * Otherwise, in order: the target's reactions are armed (a Catch takes the
 * item), a Golem guard takes the damage off the team's Golem HP instead, HP and
 * MP change (clamped), the stat changes apply, lost or stolen equipment goes,
 * an unbroken Draw Out katana comes back, gil, EXP and JP change, and a level
 * change, a poach and a Morbol transformation apply. A unit brought to 0 HP
 * only becomes Dead; otherwise Critical (HP at a fifth of max or less) and
 * Chicken (Brave below 10) are set or cleared, HP damage removes Sleep, Charm,
 * Confusion and Transparent, and a knockback cancels a Charge before the status
 * changes resolve. The outcome (0 nothing changed, 1 changed, 2 the unit died)
 * feeds the attacker's EXP.
 *
 * Returns -1 for an invalid or absent unit, or when a rider ends the action
 * dead or crystallized; otherwise 0.
 *
 * The HP/MP damage and healing fields are signed 16-bit values. The KO
 * test's three status-bit checks fold
 * into the target's single 16-bit load of current[0..1] & 0x160. */
s32 action_apply_target_result(s32 battle_unit_id) {
    s32 outcome;
    s32 hp;
    s32 mp;
    s32 i;
    u16 old_hp;
    u16 old_mp;
    u8 old_status[STATUS_BYTE_COUNT];
    u8 old_status_ct[STATUS_TIMED_COUNT];

    outcome = 0;
    if (battle_unit_id >= BATTLE_UNIT_SLOT_COUNT) {
        return -1;
    }

    g_action_work.target_id = battle_unit_id;
    g_action_target = &g_battle_units[battle_unit_id];
    g_action_target_data = &g_battle_units[battle_unit_id].action;
    if (g_action_target->active_battle_unit_id == UNIT_ACTIVE_BATTLE_UNIT_ID_NONE) {
        return -1;
    }

    /* A dead unit turning into a crystal or treasure only resolves that change. */
    if ((g_action_target->status_sets.current[0] & STATUS_MASK(DEAD))
        && ((g_action_target_data->status_changes.infliction[0] & STATUS_MASK(CRYSTAL))
            || (g_action_target_data->status_changes.infliction[1] & STATUS_MASK(TREASURE)))) {
        status_resolve_unit_changes(battle_unit_id, 0);
        return 0;
    }

    if (g_action_context == ACTION_CONTEXT_PRIMARY) {
        g_action_target->ability_outcome = UNIT_ABILITY_OUTCOME_TARGETED;
    }
    dispatch_targeted_reactions();
    if (g_action_target_data->reaction_id == ABILITY_ID_REACTION_CATCH) {
        item_give(g_action_target, g_action_target_data->last_received_attack);
    }

    if (g_action_target_data->hit == 0) {
        return 0;
    }

    /* Golem takes the hit: only the team's Golem HP drops. */
    if (g_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_GOLEM_GUARD) {
        u16* golem;

        golem = &g_action_team_golem[(g_action_target->initial_team_flags & UNIT_TEAM_MASK) >> UNIT_TEAM_SHIFT];
        hp = *golem - g_action_target_data->hp_damage;
        if (hp < 0) {
            hp = 0;
        }
        *golem = hp;
        return 0;
    }

    if (g_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_SET_GOLEM) {
        outcome = 1;
        g_action_team_golem[(g_action_target->team_flags & UNIT_TEAM_MASK) >> UNIT_TEAM_SHIFT]
            = g_action_target->max_hp;
    }

    dispatch_damage_sharing_reactions();
    old_hp = g_action_target->hp;
    hp = old_hp - g_action_target_data->hp_damage + g_action_target_data->hp_healing;
    if (hp < 0) {
        hp = 0;
    }
    if (g_action_target->max_hp < hp) {
        hp = g_action_target->max_hp;
    }

    old_mp = g_action_target->mp;
    mp = old_mp - g_action_target_data->mp_damage + g_action_target_data->mp_healing;
    if (mp < 0) {
        mp = 0;
    }
    if (g_action_target->max_mp < mp) {
        mp = g_action_target->max_mp;
    }

    if (old_hp != hp || old_mp != mp) {
        outcome = 1;
    }
    g_action_target->hp = hp;
    g_action_target->mp = mp;

    /* The last argument is the stat's minimum: a rider's Brave stops at 10, so it never
     * turns Chicken while riding. */
    outcome |= apply_stat_delta(
        g_action_target_data->sp_change, &g_action_target->base_attributes[UNIT_ATTRIBUTE_SPEED], UNIT_SPEED_MAX, 1);
    outcome |= apply_stat_delta(g_action_target_data->ct_change, &g_action_target->ct, 255, 0);
    outcome |= apply_stat_delta(g_action_target_data->pa_change,
        &g_action_target->base_attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK], UNIT_ATTACK_STAT_MAX, 1);
    outcome |= apply_stat_delta(g_action_target_data->ma_change,
        &g_action_target->base_attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK], UNIT_ATTACK_STAT_MAX, 1);
    outcome |= apply_stat_delta(g_action_target_data->brave_change, &g_action_target->brave, UNIT_BRAVE_FAITH_MAX,
        (g_action_target->mount_info & UNIT_MOUNT_INFO_FLAG_RIDER) ? 10 : 0);
    outcome |= apply_stat_delta(g_action_target_data->faith_change, &g_action_target->faith, UNIT_BRAVE_FAITH_MAX, 0);

    outcome |= apply_equipment_removal();
    if (g_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_NOT_BROKEN) {
        item_give(g_action_target, g_action_target->action_command.item_id);
    }

    if (g_action_target_data->gil_change != 0 || g_action_target_data->exp_change != 0) {
        outcome |= 1;
    }
    reward_add_war_funds(g_action_target, g_action_target_data->gil_change, ITEM_ID_NOTHING);
    reward_apply_exp_change(g_action_target, g_action_target_data->exp_change);
    if (g_action_target_data->jp_change != 0) {
        apply_jp_change(g_action_target);
    }

    outcome |= apply_level_change();
    outcome |= store_poached_item();
    if (g_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_MORBOL) {
        outcome |= 1;
        if (g_action_state == ACTION_STATE_EXECUTE) {
            status_clear_all(g_action_target);
        }
    }

    /* At 0 HP, Dead replaces every other pending status change. */
    if (hp == 0 && !(g_action_target->status_sets.current[0] & STATUS_MASK(CRYSTAL))
        && !(g_action_target->status_sets.current[0] & STATUS_MASK(DEAD))
        && !(g_action_target->status_sets.current[1] & STATUS_MASK(TREASURE))) {
        clear_status_changes(g_action_target_data);
        g_action_target_data->status_changes.infliction[0] = STATUS_MASK(DEAD);
        status_resolve_unit_changes(battle_unit_id, 1);
        if (g_action_state == ACTION_STATE_EXECUTE) {
            g_action_target->ko_count++;
        }
        update_attacker_exp(2);
        /* -1 for a rider: bit 7 of mount_info is the rider flag. */
        return -(g_action_target->mount_info >> 7);
    }

    if (hp <= (g_action_target->max_hp / 5)) {
        g_action_target_data->status_changes.infliction[2] |= STATUS_MASK(CRITICAL);
    } else {
        g_action_target_data->status_changes.removal[2] |= STATUS_MASK(CRITICAL);
    }
    if (g_action_target->brave < 10) {
        g_action_target_data->status_changes.infliction[2] |= STATUS_MASK(CHICKEN);
    } else {
        g_action_target_data->status_changes.removal[2] |= STATUS_MASK(CHICKEN);
    }

    if (g_action_target_data->attack_type & ACTION_TYPE_HP_DAMAGE) {
        g_action_target_data->status_changes.removal[4] |= STATUS_MASK(CHARM) | STATUS_MASK(SLEEP);
        g_action_target_data->status_changes.removal[2] |= STATUS_MASK(TRANSPARENT);
        g_action_target_data->status_changes.removal[1] |= STATUS_MASK(CONFUSION);
    }

    if ((g_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_KNOCKBACK)
        && action_check_and_cancel_charge(g_action_target, 0) != 0) {
        g_action_target_data->status_changes.removal[0] |= STATUS_MASK(CHARGING);
        g_action_target->charged_ability_ct = 0xff;
    }

    status_filter_action_changes(0);
    /* Snapshot the statuses to see what resolving the changes alters. */
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        old_status[i] = g_action_target->status_sets.current[i];
    }
    for (i = 0; i < STATUS_TIMED_COUNT; i++) {
        old_status_ct[i] = g_action_target->status_ct[i];
    }

    status_resolve_unit_changes(battle_unit_id, 0);
    if ((g_action_target->status_sets.current[0] & STATUS_MASK(DEAD)) && !(old_status[0] & STATUS_MASK(DEAD))) {
        outcome = 2;
        if (g_action_state == ACTION_STATE_EXECUTE) {
            g_action_target->ko_count++;
        }
    } else {
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if (g_action_target->status_sets.current[i] != old_status[i]) {
                outcome = 1;
                break;
            }
        }
        for (i = 0; i < STATUS_TIMED_COUNT; i++) {
            if (g_action_target->status_ct[i] != old_status_ct[i]) {
                outcome = 1;
                break;
            }
        }
    }

    update_attacker_exp(outcome);
    if (outcome != 0) {
        g_action_target->ability_outcome = UNIT_ABILITY_OUTCOME_AFFECTED;
    }

    dispatch_after_damage_reactions();
    if (g_action_target->status_sets.current[0] & (STATUS_MASK(CRYSTAL) | STATUS_MASK(DEAD))) {
        if (g_action_target->mount_info & UNIT_MOUNT_INFO_FLAG_RIDER) {
            return -1;
        }
    }
    return 0;
}

/* Add a poached monster's item to the Fur Shop inventory: the rare one when a
 * roll of 0..255 is below 31 (31 in 256), otherwise the common one.
 *
 * Inventory quantities saturate at 0xff. */
static s32 store_poached_item(void) {
    battle_unit_t* unit;
    s32 monster_index;
    s32 item;
    s32 count;
    s32 result;

    if (g_action_state != ACTION_STATE_EXECUTE) {
        return 0;
    }
    if (!(g_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_POACH)) {
        return 0;
    }
    unit = g_action_target;
    if (unit->job_id < JOB_ID_CHOCOBO || unit->job_id > JOB_ID_TIAMAT) {
        return 0;
    }
    monster_index = unit->job_id - JOB_ID_CHOCOBO;
    if (random_roll_pass_fail(256, ITEM_POACH_RARE_ITEM_CHANCE) != 0) {
        item = g_item_poach_table[monster_index].common_item;
    } else {
        item = g_item_poach_table[monster_index].rare_item;
    }
    count = g_item_poached_quantities[item];
    /* The saturation test compares against the preloaded result, as the bytes require. */
    result = 0xff;
    if (count != result) {
        result = count + 1;
        g_item_poached_quantities[item] = result;
    }
    return result;
}

/* Transform a unit into a Morbol, keeping its name, unit id, formation index,
 * position and facing. UNIT_ENTD_INIT_TRANSFORM rebuilds the unit's stats
 * for its new job without resetting it or reading ENTD data; the generated-unit
 * formation index meanwhile lets the level growth run. */
void battle_unit_transform_into_morbol(battle_unit_t* unit) {
    u8 saved_name[0x10];
    u8 palette;
    u8* name;
    u8 unit_id;
    u8 formation_index;
    u8 x;
    u8 y;
    u8 higher_elevation;
    u8 facing;

    /* Preserve the unit's encoded name across the reset. */
    name = unit->name;
    unit_id = unit->unit_id;
    unit->job_id = JOB_ID_MORBOL;
    unit->character_identity = CHARACTER_IDENTITY_MONSTER;
    mem_copy_byte_data_source_first(name, saved_name, sizeof(saved_name));
    formation_index = unit->formation_index;
    x = unit->x;
    y = unit->position.y;
    higher_elevation = unit->position.higher_elevation;
    facing = unit->position.facing;
    unit->formation_index = UNIT_FORMATION_INDEX_GENERATED_UNIT;
    battle_unit_init_from_encounter(unit, 0, 0, UNIT_ENTD_INIT_TRANSFORM);
    unit->x = x;
    unit->position.y = y;
    unit->formation_index = formation_index;
    unit->position.higher_elevation = higher_elevation;
    unit->position.facing = facing;
    mem_copy_byte_data_source_first(saved_name, name, sizeof(saved_name));
    unit->unit_id = unit_id;
    battle_unit_set_sprite_identity(unit, &palette);
}

/* Apply a level-up or level-down special effect: level + 1 (99 at most) or
 * level - 1 (1 at least), then one level of growth or its inverse. The new
 * level is stored before the growth runs, so the growth divides by growth +
 * new level. The unit changes only when executing; returns 1 when the action
 * carries either effect. */
static s32 apply_level_change(void) {
    s32 level;
    s32 level_down;
    u16 flags = g_action_target_data->special_effect;

    if (flags & ACTION_SPECIAL_EFFECT_LEVEL_UP) {
        level = g_action_target->level + 1;
        level_down = 0;
        if (level > UNIT_LEVEL_MAX) {
            level = UNIT_LEVEL_MAX;
        }
    } else if (flags & ACTION_SPECIAL_EFFECT_LEVEL_DOWN) {
        level = g_action_target->level - 1;
        level_down = 1;
        if (level <= 0) {
            level = 1;
        }
    } else {
        return 0;
    }
    if (g_action_state == ACTION_STATE_EXECUTE) {
        g_action_target->level = level;
        battle_unit_apply_level_growth(g_action_target, level_down);
    }
    return 1;
}

/* Nonzero when the unit cannot react: a status or a non-primary action
 * prevents it (reaction_is_prevented), a reaction is already in
 * play (check_pending_reactions), or it cannot act or stands in deep
 * water (battle_unit_get_action_restriction). Returns the first nonzero
 * result, or 0 when the unit can react. */
static s32 reaction_is_blocked(const battle_unit_t* unit) {
    s32 result;

    if ((result = reaction_is_prevented(unit)) == 0 && (result = check_pending_reactions(unit)) == 0) {
        result = battle_unit_get_action_restriction(unit);
    }
    return result;
}

/* Nonzero when the unit cannot react: it has a status of the prevent-reaction
 * set, or the current action is a secondary application (anything but
 * the primary context), which never provokes one. */
s32 reaction_is_prevented(const battle_unit_t* unit) {
    if (g_action_context != ACTION_CONTEXT_PRIMARY) {
        return 1;
    }
    return status_has_any_in_check_set(unit, STATUS_CHECK_SET_PREVENT_REACTION);
}

/* Whether a reaction is already in play: 1 while a weapon's proc ability is
 * being resolved, 2 when the unit's result already carries a reaction, 0
 * otherwise. */
static s32 check_pending_reactions(const battle_unit_t* unit) {
    s32 result;

    /* Both globals are loaded unsigned (lhu) here. */
    if (g_action_work.reaction_id != 0 && g_action_work.weapon_proc_pending == 0) {
        return 1;
    }
    result = unit->action.reaction_id;
    if (result != 0) {
        result = 2;
    }
    return result;
}

/* Apply the target's Reflect status or its weapon-evasion reaction (Blade
 * Grasp, else Arrow Guard) before the attack resolves. Only a primary action
 * other than Weapon Heal is checked, and not while a weapon's proc resolves. */
static void dispatch_evasion_reactions(void) {
    u8 reaction_flags;

    if (g_action_context != ACTION_CONTEXT_PRIMARY) {
        return;
    }
    if (g_action_work.formula == ABILITY_FORMULA_WEAPON_HEAL) {
        return;
    }
    if (check_pending_reactions(g_action_target) == 1) {
        return;
    }
    if ((g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_REFLECT)]
            & STATUS_BYTE_MASK(STATUS_ID_REFLECT))
        && g_action_work.reaction_id == 0) {
        try_reflect();
    }
    /* reaction_id is loaded signed (lh) here. */
    if (g_action_target_data->reaction_id != 0) {
        return;
    }
    if (reaction_is_prevented(g_action_target) != 0) {
        return;
    }
    if (battle_unit_get_action_restriction(g_action_target) != UNIT_ACTION_RESTRICTION_NONE) {
        return;
    }
    reaction_flags = g_action_target->ability_flags.reaction[3];
    if (reaction_flags & UNIT_REACTION_SET_4_BLADE_GRASP) {
        try_blade_grasp();
        return;
    }
    if (reaction_flags & UNIT_REACTION_SET_4_ARROW_GUARD) {
        try_arrow_guard();
    }
}

/* Dispatch the target unit's first reaction ability whose bit is set in
 * ability_flags.reaction[0..2]. Weapon Heal (formula 7) never provokes a reaction.
 *
 * Counter Magic (byte 1) is tested last, after the byte-2 reactions, so
 * Absorb Used MP or Counter takes precedence over it. */
static void dispatch_targeted_reactions(void) {
    battle_unit_t* unit;

    if (g_action_work.formula == ABILITY_FORMULA_WEAPON_HEAL) {
        return;
    }
    if (reaction_is_blocked(g_action_target)) {
        return;
    }
    unit = g_action_target;
    if (unit->ability_flags.reaction[0] & UNIT_REACTION_SET_1_SUNKEN_STATE) {
        try_attack_flag_reactions(ABILITY_ID_REACTION_SUNKEN_STATE, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->ability_flags.reaction[0] & UNIT_REACTION_SET_1_CAUTION) {
        try_attack_flag_reactions(ABILITY_ID_REACTION_CAUTION, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->ability_flags.reaction[0] & UNIT_REACTION_SET_1_DRAGON_SPIRIT) {
        try_attack_flag_reactions(ABILITY_ID_REACTION_DRAGON_SPIRIT, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->ability_flags.reaction[0] & UNIT_REACTION_SET_1_BRAVE_UP) {
        try_attack_flag_reactions(ABILITY_ID_REACTION_BRAVE_UP, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->ability_flags.reaction[1] & UNIT_REACTION_SET_2_FAITH_UP) {
        try_mp_cost_reactions(ABILITY_ID_REACTION_FAITH_UP);
    } else if (unit->ability_flags.reaction[1] & UNIT_REACTION_SET_2_COUNTER_TACKLE) {
        try_attack_flag_reactions(ABILITY_ID_REACTION_COUNTER_TACKLE, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->ability_flags.reaction[1] & UNIT_REACTION_SET_2_COUNTER_FLOOD) {
        try_attack_flag_reactions(ABILITY_ID_REACTION_COUNTER_FLOOD, ABILITY_SECONDARY_FLAG_4_COUNTER_FLOOD);
    } else if (unit->ability_flags.reaction[2] & UNIT_REACTION_SET_3_ABSORB_USED_MP) {
        try_mp_cost_reactions(ABILITY_ID_REACTION_ABSORB_USED_MP);
    } else if (unit->ability_flags.reaction[2] & UNIT_REACTION_SET_3_COUNTER) {
        try_attack_flag_reactions(ABILITY_ID_REACTION_COUNTER, ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE);
    } else if (unit->ability_flags.reaction[1] & UNIT_REACTION_SET_2_COUNTER_MAGIC) {
        try_attack_flag_reactions(ABILITY_ID_REACTION_COUNTER_MAGIC, ABILITY_SECONDARY_FLAG_4_COUNTER_MAGIC);
    }
}

/* Check the target's first equipped damage-sharing reaction, in the order MP
 * Switch, Distribute, Damage Split; when its check fails no other is tried.
 * Nothing runs for the Weapon Heal formula or a target that cannot react. */
static void dispatch_damage_sharing_reactions(void) {
    battle_unit_t* unit;
    u8 flags;

    if (g_action_work.formula == ABILITY_FORMULA_WEAPON_HEAL) {
        return;
    }
    if (reaction_is_blocked(g_action_target)) {
        return;
    }
    unit = g_action_target;
    flags = unit->ability_flags.reaction[2];
    if (flags & UNIT_REACTION_SET_3_MP_SWITCH) {
        try_mp_switch();
        return;
    }
    if (flags & UNIT_REACTION_SET_3_DISTRIBUTE) {
        try_distribute();
        return;
    }
    if (unit->ability_flags.reaction[3] & UNIT_REACTION_SET_4_DAMAGE_SPLIT) {
        try_damage_split();
    }
}

/* Dispatch the first active stat-save or resource-restoring reaction. A weapon
 * that heals never triggers them. */
static void dispatch_after_damage_reactions(void) {
    battle_unit_t* target;
    u8 reaction_flags;

    if (g_action_work.formula == ABILITY_FORMULA_WEAPON_HEAL || reaction_is_blocked(g_action_target)) {
        return;
    }
    target = g_action_target;
    reaction_flags = target->ability_flags.reaction[0];
    if (reaction_flags & UNIT_REACTION_SET_1_PA_SAVE) {
        try_hp_damage_reactions(ABILITY_ID_REACTION_PA_SAVE);
    } else if (reaction_flags & UNIT_REACTION_SET_1_MA_SAVE) {
        try_hp_damage_reactions(ABILITY_ID_REACTION_MA_SAVE);
    } else if (reaction_flags & UNIT_REACTION_SET_1_SPEED_SAVE) {
        try_hp_damage_reactions(ABILITY_ID_REACTION_SPEED_SAVE);
    } else if (reaction_flags & UNIT_REACTION_SET_1_REGENERATOR) {
        try_hp_damage_reactions(ABILITY_ID_REACTION_REGENERATOR);
    } else {
        reaction_flags = target->ability_flags.reaction[1];
        if (reaction_flags & UNIT_REACTION_SET_2_HP_RESTORE) {
            try_critical_reactions(ABILITY_ID_REACTION_HP_RESTORE);
        } else if (reaction_flags & UNIT_REACTION_SET_2_MP_RESTORE) {
            try_critical_reactions(ABILITY_ID_REACTION_MP_RESTORE);
        } else if (reaction_flags & UNIT_REACTION_SET_2_CRITICAL_QUICK) {
            try_critical_reactions(ABILITY_ID_REACTION_CRITICAL_QUICK);
        } else if (reaction_flags & UNIT_REACTION_SET_2_MEATBONE_SLASH) {
            try_critical_reactions(ABILITY_ID_REACTION_MEATBONE_SLASH);
        } else {
            reaction_flags = target->ability_flags.reaction[2];
            if (reaction_flags & UNIT_REACTION_SET_3_GILGAME_HEART) {
                try_hp_damage_reactions(ABILITY_ID_REACTION_GILGAME_HEART);
            } else if (reaction_flags & UNIT_REACTION_SET_3_AUTO_POTION) {
                try_hp_damage_reactions(ABILITY_ID_REACTION_AUTO_POTION);
            }
        }
    }
}

/* Arm a counter-type reaction on the target: the attack must not come through
 * Math Skill, its flags_4 must have a bit of mask, and the target's Brave roll
 * must pass. Records the reaction and the ability it answers. */
static void try_attack_flag_reactions(u16 reaction_id, u32 mask) {
    action_data_t* action;

    if (g_action_work.skillset == SKILLSET_ID_MATH_SKILL) {
        return;
    }
    if (!(g_action_work.range_data.flags_4 & mask)) {
        return;
    }
    if (reaction_fails_brave_roll(g_action_target)) {
        return;
    }
    action = g_action_target_data;
    action->reaction_id = reaction_id;
    action->last_received_attack = g_action_work.ability_id;
}

/* Record reaction_id when the action deals HP damage and the target passes its
 * reaction roll; the HP damage is the reaction's amount. */
static void try_hp_damage_reactions(s16 reaction_id) {
    action_data_t* action;

    if ((g_action_target_data->attack_type & ACTION_TYPE_HP_DAMAGE)
        && reaction_fails_brave_roll(g_action_target) == 0) {
        action = g_action_target_data;
        action->reaction_id = reaction_id;
        action->last_received_attack = action->hp_damage;
    }
}

/* Record reaction_id for a target that is in Critical, is taking HP damage and
 * passes its reaction roll. */
static void try_critical_reactions(s16 reaction_id) {
    battle_unit_t* unit = g_action_target;

    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)] & STATUS_BYTE_MASK(STATUS_ID_CRITICAL))
        && (g_action_target_data->attack_type & ACTION_TYPE_HP_DAMAGE) && reaction_fails_brave_roll(unit) == 0) {
        g_action_target_data->reaction_id = reaction_id;
    }
}

/* Record reaction_id when the ability cost MP and the target passes its reaction
 * roll; the MP cost is the reaction's amount. */
static void try_mp_cost_reactions(s16 reaction_id) {
    action_data_t* action;

    if (g_action_work.mp_cost != 0 && reaction_fails_brave_roll(g_action_target) == 0) {
        action = g_action_target_data;
        action->reaction_id = reaction_id;
        action->last_received_attack = g_action_work.mp_cost;
    }
}

/* Reflect: a reflectable ability not cast through Math Skill misses the target
 * and is recorded on it for the bounce. */
static void try_reflect(void) {
    action_data_t* action;
    /* Loaded through a pointer so the reaction_id store stays above it. */
    u16* ability_ptr = &g_action_work.ability_id;
    u16 ability;

    if (g_action_work.skillset != SKILLSET_ID_MATH_SKILL
        && (g_action_work.range_data.flags_3 & ABILITY_SECONDARY_FLAG_3_REFLECTABLE)) {
        action = g_action_target_data;
        action->reaction_id = ABILITY_ID_REACTION_REFLECT;
        ability = *ability_ptr;
        action->attack_accuracy = 0;
        action->hit = 0;
        action->last_received_attack = ability;
        g_action_target_data->miss_type = ACTION_MISS_TYPE_REFLECTED;
    }
}

/* Blade Grasp. An ability flagged Blade Grasp eligible has its accuracy set to
 * 100 - Brave of the target; when the target passes its reaction roll during
 * execution the strike becomes a miss and the reaction is recorded. */
static void try_blade_grasp(void) {
    battle_unit_t* target;
    action_data_t* action;

    if (!(g_action_work.range_data.flags_4 & ABILITY_SECONDARY_FLAG_4_BLADE_GRASP_ELIGIBLE)) {
        return;
    }
    target = g_action_target;
    action = g_action_target_data;
    action->attack_accuracy = 100 - target->brave;
    if (reaction_fails_brave_roll(target)) {
        return;
    }
    if (g_action_state != ACTION_STATE_EXECUTE) {
        return;
    }
    g_action_target_data->hit = 0;
    g_action_target_data->miss_type = ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD;
    g_action_target_data->reaction_id = ABILITY_ID_REACTION_BLADE_GRASP;
}

/* Apply Arrow Guard to bow and crossbow attacks that use weapon range.
 *
 * The displayed accuracy becomes 100 - Brave; a successful Brave roll while
 * executing changes the target result to the shared class-evade/Arrow Guard
 * miss type. */
static void try_arrow_guard(void) {
    battle_unit_t* target;
    action_data_t* action;

    if (!(g_action_work.range_data.flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE)) {
        return;
    }
    if (g_item_primary_data[g_action_work.weapon_id].type != ITEM_TYPE_CROSSBOW
        && g_item_primary_data[g_action_work.weapon_id].type != ITEM_TYPE_BOW) {
        return;
    }
    target = g_action_target;
    action = g_action_target_data;
    action->attack_accuracy = 100 - target->brave;
    if (reaction_fails_brave_roll(g_action_target) == 0 && g_action_state == ACTION_STATE_EXECUTE) {
        g_action_target_data->hit = 0;
        g_action_target_data->miss_type = ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD;
        g_action_target_data->reaction_id = ABILITY_ID_REACTION_ARROW_GUARD;
    }
}

/* MP Switch: when the target would lose HP, still has MP and passes its reaction
 * roll, move the HP damage onto MP (capped at 999) and mark the reaction.
 *
 * Only an executing action (not a preview or simulation) is converted. */
static void try_mp_switch(void) {
    battle_unit_t* unit;
    action_data_t* action;
    action_data_t* action2;

    /* The target reads hp_damage / mp_damage signed (lh). */
    if (g_action_target_data->hp_damage == 0) {
        return;
    }
    unit = g_action_target;
    if (unit->mp == 0) {
        return;
    }
    if (reaction_fails_brave_roll(g_action_target) != 0) {
        return;
    }
    if (g_action_state != ACTION_STATE_EXECUTE) {
        return;
    }
    action = g_action_target_data;
    action->mp_damage = action->mp_damage + action->hp_damage;
    if (action->mp_damage > ACTION_VALUE_MAX) {
        action->mp_damage = ACTION_VALUE_MAX;
    }
    /* The record pointer is reloaded twice more, as retail does. */
    action2 = g_action_target_data;
    action2->attack_type &= ~ACTION_TYPE_HP_DAMAGE;
    action = g_action_target_data;
    action2->hp_damage = 0;
    action->attack_type |= ACTION_TYPE_MP_DAMAGE;
    g_action_target_data->reaction_id = ABILITY_ID_REACTION_MP_SWITCH;
}

/* Distribute: when the healing exceeds the target's missing HP and the reaction
 * roll passes, record the reaction with the excess as its amount. */
static void try_distribute(void) {
    battle_unit_t* target = g_action_target;
    /* hp_healing is read signed (lh) here. */
    s32 excess = g_action_target_data->hp_healing - (target->max_hp - target->hp);

    if (excess > 0 && reaction_fails_brave_roll(target) == 0) {
        g_action_target_data->last_received_attack = excess;
        g_action_target_data->reaction_id = ABILITY_ID_REACTION_DISTRIBUTE;
    }
}

/* Damage Split: when the target takes HP damage and passes its reaction roll,
 * record the reaction with half the damage, rounded up, as its amount. */
static void try_damage_split(void) {
    u16 hp = g_action_target_data->hp_damage;
    action_data_t* action;

    if (hp != 0 && reaction_fails_brave_roll(g_action_target) == 0) {
        hp++;
        hp = hp >> 1;
        action = g_action_target_data;
        action->last_received_attack = hp;
        action->reaction_id = ABILITY_ID_REACTION_DAMAGE_SPLIT;
    }
}

/* Reaction roll: returns 0 when the unit reacts, a Brave in 100 chance.
 * Outside execution it always returns 0. */
s32 reaction_fails_brave_roll(const battle_unit_t* unit) {
    if (g_action_state != ACTION_STATE_EXECUTE) {
        return 0;
    }
    return random_roll_pass_fail(100, unit->brave);
}

/* Take the equipment a Break or Steal hit off the target (unremovable items stay) and
 * give stolen items to the attacker's side; losing a weapon also cancels a Charge in
 * progress. Returns 1 when anything was removed.
 *
 * equipment_destroyed holds one bit per equipment slot, head first from
 * ACTION_EQUIPMENT_SLOT_HEAD. */
static s32 apply_equipment_removal(void) {
    s32 i;
    s32 count = 0;
    u8 item;
    battle_unit_t* unit;
    action_data_t* action;

    if (g_action_target_data->special_effect
        & (ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT | ACTION_SPECIAL_EFFECT_STEAL_ITEM)) {
        for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
            if (g_action_target_data->equipment_destroyed & (ACTION_EQUIPMENT_SLOT_HEAD >> i)) {
                item = g_action_target->equipment[i];
                if (item != ITEM_ID_NOTHING && item != ITEM_ID_NONE
                    && !(g_item_primary_data[item].type_flags & ITEM_TYPE_FLAG_UNREMOVABLE)) {
                    g_action_target->equipment[i] = ITEM_ID_NONE;
                    count++;
                    if (g_action_target_data->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
                        item_give(g_action_attacker, item);
                    }
                }
            }
        }
        if (count != 0) {
            if (g_action_state == ACTION_STATE_EXECUTE) {
                battle_unit_refresh_stats(g_action_target);
            }
            action = g_action_target_data;
            if (action->equipment_destroyed == ACTION_EQUIPMENT_SLOT_RIGHT_WEAPON
                || action->equipment_destroyed == ACTION_EQUIPMENT_SLOT_LEFT_WEAPON) {
                unit = g_action_target;
                if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                        & STATUS_BYTE_MASK(STATUS_ID_CHARGING))
                    && unit->charged_ability_ct != 0xFF && unit->action_command.skillset_id == SKILLSET_ID_CHARGE) {
                    action->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                        |= STATUS_BYTE_MASK(STATUS_ID_CHARGING);
                }
            }
            return 1;
        }
    }
    return 0;
}

/* Apply one stat change of an action result: the low seven bits are the amount
 * and ACTION_STAT_CHANGE_INCREASE means increase. The result is clamped to min..max; returns 1
 * when the stat changed. With max 255 (CT) an amount of 0x7f stands for 255,
 * which empties or fills CT. With min 1 (Speed, PA, MA) stat is the base
 * value: the total six bytes on is rebuilt as base + equipment bonus, capped
 * at max.
 *
 * The caller's ranges: Speed 1..50, PA and MA 1..99, Brave 0..100 (10 at least
 * for a rider), Faith 0..100, CT 0..255. */
static s32 apply_stat_delta(s32 mod, u8* stat, u8 max, u8 min) {
    s32 amount;
    s32 value = stat[0];

    amount = mod & ACTION_STAT_CHANGE_VALUE_MASK;
    if (max == 0xFF && amount == 0x7F) {
        amount = 0xFF;
    }

    if (mod & ACTION_STAT_CHANGE_INCREASE) {
        value += amount;
    } else {
        value -= amount;
    }

    if (value < min) {
        value = min;
    }
    if (value > max) {
        value = max;
    }

    if (value == stat[0]) {
        return 0;
    }
    stat[0] = value;

    if (min == 1) {
        value = stat[0] + stat[3];
        if (value > max) {
            value = max;
        }
        stat[6] = value;
    }
    return 1;
}

/* Record the highest EXP award produced by the current action, for an outcome
 * of 1 (the target changed) or 2 (the target died).
 *
 * The base is 10 for a change, and for a kill 20 divided by the target's KO
 * count. The target's level less the attacker's is added (at least 1 in all),
 * Gained EXP Up doubles it and the award is capped at 99. Reactions and
 * simulations earn nothing. */
static void update_attacker_exp(s32 outcome) {
    s16 base;
    s32 signed_exp;
    s32 divisor;
    battle_unit_t* attacker;
    s16 exp;

    if (g_action_context == ACTION_CONTEXT_PRIMARY && outcome != 0) {
        if (outcome == 1) {
            base = REWARD_EXP_SUCCESS_BASE;
        } else {
            divisor = g_action_target->ko_count;
            base = REWARD_EXP_KILL_BASE;
            if (divisor != 0) {
                base = REWARD_EXP_KILL_BASE / divisor;
            }
        }
        attacker = g_action_attacker;
        exp = base + g_action_target->level - attacker->level;
        if (exp <= 0) {
            exp = 1;
        }
        if (attacker->ability_flags.support[1] & UNIT_SUPPORT_SET_2_GAINED_EXP_UP) {
            exp = exp * 2;
        }
        if (exp > REWARD_EXP_MAX) {
            exp = REWARD_EXP_MAX;
        }
        /* The award is compared through an explicit 16-bit sign extension, as the bytes require. */
        signed_exp = exp << 16;
        if (g_action_work.earned_experience < (signed_exp >> 16)) {
            g_action_work.earned_experience = exp;
        }
    }
}

/* Apply the attacker's own action result to its unit: make the unit the current
 * target, copy the result in g_action_attacker_action_data into its
 * action record and apply it with action_apply_target_result.
 *
 * Returns that routine's result, or -1 for an invalid or empty slot. A result
 * that is not a hit is marked as canceled. */
s32 action_apply_attacker_result(s32 unit_id) {
    battle_unit_t* battle_unit = &g_battle_units[unit_id];
    action_data_t* action;
    s32 result;

    g_action_target = battle_unit;
    if (unit_id >= BATTLE_UNIT_SLOT_COUNT) {
        return -1;
    }
    action = &g_battle_units[unit_id].action;
    g_action_work.target_id = unit_id;
    g_action_target_data = action;
    if (battle_unit->active_battle_unit_id == UNIT_ACTIVE_BATTLE_UNIT_ID_NONE) {
        return -1;
    }
    mem_copy_byte_data_source_first(&g_action_attacker_action_data, action, sizeof(action_data_t));
    result = action_apply_target_result(unit_id);
    if (g_action_target_data->hit == 0) {
        g_action_target_data->miss_type = ACTION_MISS_TYPE_CANCELED;
    }
    return result;
}

/* Set up a unit's Poison or Regen tick as an action against itself: Poison deals MaxHP/8,
 * otherwise Regen heals MaxHP/8. Returns the action type, 0 when neither applies or the unit is
 * out of action. */
s32 status_prepare_poison_and_regen(battle_unit_t* unit) {
    u8 status;

    if (is_out_of_action(unit)) {
        return 0;
    }
    set_target(unit);
    status = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_POISON)];
    if (status & STATUS_BYTE_MASK(STATUS_ID_POISON)) {
        g_action_target_data->hp_damage = unit->max_hp >> 3;
        g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
    } else if (status & STATUS_BYTE_MASK(STATUS_ID_REGEN)) {
        g_action_target_data->hp_healing = unit->max_hp >> 3;
        g_action_target_data->attack_type = ACTION_TYPE_HP_HEALING;
    }
    return g_action_target_data->attack_type;
}

/* Between-turn step: make a unit that has acted lose Transparent.
 *
 * The unit becomes the current target with a cleared action result. When it has
 * taken its action, carries Transparent as an inflicted status and has
 * transparent_removal_flag set, the result becomes a status change removing
 * Transparent. Returns the result's attack type, nonzero when there is a change
 * to show. Crystals, treasures, dead, petrified and jumping units return 0
 * untouched. */
s32 status_prepare_transparent_removal(battle_unit_t* unit) {
    battle_unit_t* target;

    if (is_out_of_action(unit)) {
        return 0;
    }
    set_target(unit);
    if (unit->action_taken) {
        target = g_action_target;
        if ((target->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
                & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT))
            && target->transparent_removal_flag) {
            g_action_target_data->status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
                = STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT);
            g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
        }
    }
    return g_action_target_data->attack_type;
}

/* Count down the unit's timed statuses, Poison through Reflect (Death Sentence
 * is not among them), and queue the removal of each that runs out. */
void status_update_expiring(s32 unit_id) {
    s32 i;
    s32 byte_index;
    s32 mask;
    battle_unit_t* target;
    action_data_t* action;
    u8 remaining_ct;

    set_target(&g_battle_units[unit_id]);

    /* Statuses 24..38 (bytes 3 and 4): tick the CT of each inflicted,
     * non-innate status and flag it for removal when it reaches zero. */
    for (i = 0; i < 15; i++) {
        byte_index = (i / 8) + 3;
        target = g_action_target;
        if (target->inflicted_status[byte_index] & (mask = 0x80 >> (i & 7))) {
            if (!(target->status_sets.innate[byte_index] & mask)) {
                remaining_ct = target->status_ct[i] - 1;
                target->status_ct[i] = remaining_ct;
                if (remaining_ct == 0) {
                    action = g_action_target_data;
                    action->status_changes.removal[byte_index] = mask | action->status_changes.removal[byte_index];
                }
            }
        }
    }

    if (status_filter_action_changes(0) != 0) {
        g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
    }
}

/* Make the unit the current target and reset its action result record. */
static void set_target(battle_unit_t* unit) {
    action_data_t* action = &unit->action;

    g_action_target = unit;
    g_action_target_data = action;
    g_action_work.target_id = unit->battle_unit_id;
    action_reset_result(action);
}

/* 1 for an empty slot or a unit that is a crystal, dead, in the air from Jump,
 * petrified or a treasure; otherwise 0. */
static s32 is_out_of_action(const battle_unit_t* unit) {
    s32 result;

    if (unit->active_battle_unit_id == UNIT_ACTIVE_BATTLE_UNIT_ID_NONE) {
        return 1;
    }
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        & (STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD) | STATUS_BYTE_MASK(STATUS_ID_JUMP))) {
        return 1;
    }
    result = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
        & (STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_TREASURE));
    if (result != 0) {
        result = 1;
    }
    return result;
}

/* Prepare the automatic Poison of a poisonous surface (such as a poisoned
 * marsh) for a grounded unit, as an action against itself. Returns 1 when
 * Poison would take hold, else 0.
 *
 * Mounts cannot receive the terrain effect. A rider uses the linked mount's
 * Float status because that unit determines whether the pair touches the
 * terrain. g_terrain_status_flags marks the surfaces that inflict Poison. */
s32 status_prepare_terrain_poison(battle_unit_t* unit) {
    s32 mount_info;
    u8 unit_id;
    action_data_t* action;

    mount_info = unit->mount_info;
    if (is_out_of_action(unit)) {
        return 0;
    }
    if (mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) {
        return 0;
    }
    if (mount_info & UNIT_MOUNT_INFO_FLAG_RIDER) {
        if (g_battle_units[mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK]
                .status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
            return 0;
        }
    } else if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
        return 0;
    }
    action = &unit->action;
    unit_id = unit->battle_unit_id;
    g_action_target = unit;
    g_action_target_data = action;
    g_action_work.target_id = unit_id;
    action_reset_result(action);
    if (g_terrain_status_flags[g_map_tile_data[battle_unit_get_tile_index(unit)].surface.type]
        & TERRAIN_STATUS_FLAG_POISON) {
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_POISON)]
            = STATUS_BYTE_MASK(STATUS_ID_POISON);
        if (status_filter_action_changes(0)) {
            g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
            return 1;
        }
    }
    return 0;
}

/* Set up the trap on a unit's tile as an action against it (against the rider when the unit is a
 * mount): Degenerator lowers a level, Deathtrap inflicts Death Sentence and Sleeping Gas inflicts
 * Sleep (a miss when they cannot take hold), Steel Needle deals MaxHP/5. Returns the trap id, or
 * MOVE_FIND_TRAP_NONE. */
s32 move_find_prepare_trap_result(battle_unit_t* unit) {
    u8 trap = move_find_get_result(unit->x, unit->position.y, unit->position.higher_elevation)->trap_id;

    if (unit->mount_info & UNIT_MOUNT_INFO_FLAG_MOUNT) {
        unit = &g_battle_units[unit->mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK];
    }
    g_action_work.target_id = unit->battle_unit_id;
    g_action_target = unit;
    g_action_target_data = &unit->action;
    action_reset_result(&unit->action);
    switch (trap) {
    case MOVE_FIND_TRAP_DEGENERATOR:
        g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
        g_action_target_data->special_effect = ACTION_SPECIAL_EFFECT_LEVEL_DOWN;
        break;
    case MOVE_FIND_TRAP_DEATHTRAP:
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_DEATH_SENTENCE)]
            = STATUS_BYTE_MASK(STATUS_ID_DEATH_SENTENCE);
        if (status_filter_action_changes(0) != 0) {
            g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
        } else {
            formula_force_attack_miss();
        }
        break;
    case MOVE_FIND_TRAP_SLEEPING_GAS:
        g_action_target_data->status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_SLEEP)]
            = STATUS_BYTE_MASK(STATUS_ID_SLEEP);
        if (status_filter_action_changes(0) != 0) {
            g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
        } else {
            formula_force_attack_miss();
        }
        break;
    case MOVE_FIND_TRAP_STEEL_NEEDLE:
        g_action_target_data->hp_damage = unit->max_hp / 5;
        g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
        break;
    default:
        trap = MOVE_FIND_TRAP_NONE;
        break;
    }
    return trap;
}

/* Prepare the action record for a movement ability's benefit.
 *
 * Initialize the record, then select HP, MP, EXP or JP in that order. Move-HP
 * Up and Move-MP Up restore a tenth of the maximum, rounded up; Move-Get EXP
 * and Move-Get JP give reward_calculate_move_get_amount's amount,
 * doubled by Gained EXP Up or Gained JP Up. Return -1 for a blocking status
 * and 0 otherwise, including when no movement benefit is present. The caller
 * applies and displays the result. */
s32 move_prepare_ability_benefit(battle_unit_t* unit) {
    s32 flags;
    u8 unit_id = unit->battle_unit_id;
    s32 amount;
    action_data_t* action;

    g_action_target = unit;
    g_action_target_data = &unit->action;
    g_action_work.target_id = unit_id;
    action_reset_result(&unit->action);
    if (status_has_any_in_check_set(unit, STATUS_CHECK_SET_PREVENT_REACTION)) {
        return -1;
    }
    flags = move_get_ability_events(unit);
    if (flags & MOVE_POST_EVENT_MOVE_HP_UP) {
        amount = (unit->max_hp + 9) / 10;
        action = g_action_target_data;
        action->attack_type = ACTION_TYPE_HP_HEALING;
        action->hp_healing = amount;
    } else if (flags & MOVE_POST_EVENT_MOVE_MP_UP) {
        amount = (unit->max_mp + 9) / 10;
        action = g_action_target_data;
        action->attack_type = ACTION_TYPE_MP_HEALING;
        action->mp_healing = amount;
    } else if (flags & MOVE_POST_EVENT_MOVE_GET_EXP) {
        g_action_target_data->exp_change = reward_calculate_move_get_amount(g_action_target);
        if (g_action_target->ability_flags.support[1] & UNIT_SUPPORT_SET_2_GAINED_EXP_UP) {
            g_action_target_data->exp_change *= 2;
        }
        g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    } else if (flags & MOVE_POST_EVENT_MOVE_GET_JP) {
        g_action_target_data->jp_change = reward_calculate_move_get_amount(g_action_target);
        if (g_action_target->ability_flags.support[1] & UNIT_SUPPORT_SET_2_GAINED_JP_UP) {
            g_action_target_data->jp_change *= 2;
        }
        g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
    }
    return 0;
}

/* Apply the unit's pending action result to itself in the secondary context
 * (as for a movement ability's benefit) and write the EXP and JP gained and any
 * new level and job level to rewards. Returns -1 for an empty unit slot,
 * otherwise action_apply_target_result's result. */
s32 reward_apply_result_and_collect(battle_unit_t* unit, action_reward_display_t* rewards) {
    s32 job_slot;
    s32 result;
    s32 unit_id;

    if (unit->active_battle_unit_id == UNIT_ACTIVE_BATTLE_UNIT_ID_NONE) {
        return -1;
    }
    unit_id = unit->battle_unit_id;
    g_action_work.level_gained_flag = 0;
    g_action_work.job_level_gained_flag = 0;
    g_action_context = ACTION_CONTEXT_SECONDARY;
    g_action_actor_id = unit_id;
    result = action_apply_target_result(unit->battle_unit_id);
    g_action_context = ACTION_CONTEXT_PRIMARY;
    rewards->earned_experience = unit->action.exp_change;
    rewards->earned_jp = unit->action.jp_change;
    rewards->level_for_display = 0;
    if (g_action_work.level_gained_flag != 0) {
        rewards->level_for_display = unit->level;
    }
    rewards->job_level_for_display = 0;
    if (g_action_work.job_level_gained_flag != 0) {
        rewards->job_level_for_display = reward_get_job_level(unit, unit->job_id, &job_slot);
    }
    return result;
}

/* Add the pending JP change to the unit's job and, if that changed the job
 * level, set the job-level-gained flag so the caller queues the level-up report. */
static void apply_jp_change(battle_unit_t* unit) {
    s32 job_slot;
    s32 initial_level;
    u8 job = unit->job_id;
    u8 battle_unit_id;

    battle_unit_id = unit->battle_unit_id;
    initial_level = reward_get_job_level(unit, job, &job_slot);
    reward_add_job_jp(battle_unit_id, unit->action.jp_change, unit->initial_team_flags & UNIT_TEAM_MASK, job_slot);
    if (reward_get_job_level(unit, job, &job_slot) != initial_level) {
        g_action_work.job_level_gained_flag = 1;
    }
}

/* Apply the current target record's status changes to unit unit_id, making it the current target:
 * filter the inflictions, apply the removals and inflictions with their CTs, zero HP under Dead,
 * drop the charged ability CT unless it is still Charging, Jumping or Performing, and refresh
 * player control (Berserk, Confusion, Charm and the like). The record's status-change bytes are
 * restored afterward. -1 for an empty slot. */
s32 status_resolve_unit_changes(s32 unit_id, s32 removal_only) {
    unit_action_status_changes_t saved;

    if (g_battle_units[unit_id].active_battle_unit_id == UNIT_ACTIVE_BATTLE_UNIT_ID_NONE) {
        return -1;
    }
    mem_copy_byte_data_source_first(&g_action_target_data->status_changes, &saved, sizeof(saved));
    g_action_work.target_id = unit_id;
    g_action_target = &g_battle_units[unit_id];
    g_action_target_data = &g_battle_units[unit_id].action;
    status_filter_action_changes(removal_only);
    apply_removals(unit_id);
    apply_inflictions(unit_id, removal_only);
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)) {
        g_action_target->hp = 0;
    }
    if (!(g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
            & (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
                | STATUS_BYTE_MASK(STATUS_ID_PERFORMING)))) {
        g_action_target->charged_ability_ct = 0xFF;
    }
    update_control_status(g_action_target);
    mem_copy_byte_data_source_first(&saved, &g_action_target_data->status_changes, sizeof(saved));
    return 0;
}

/* Confusion, Blood Suck, Berserk, Chicken and Charm take the unit out of
 * player control; without them the control bit comes back from the initial
 * team flags. Without Charm the team bits come back from the initial team as
 * well. */
static void update_control_status(battle_unit_t* unit) {
    if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CONFUSION)]
            & (STATUS_BYTE_MASK(STATUS_ID_CONFUSION) | STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK)))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_BERSERK)]
            & (STATUS_BYTE_MASK(STATUS_ID_BERSERK) | STATUS_BYTE_MASK(STATUS_ID_CHICKEN)))
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARM)] & STATUS_BYTE_MASK(STATUS_ID_CHARM))) {
        unit->team_flags &= ~UNIT_TEAM_FLAG_PLAYER_CONTROLLED;
    } else {
        unit->team_flags |= unit->initial_team_flags & UNIT_TEAM_FLAG_PLAYER_CONTROLLED;
    }
    if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARM)] & STATUS_BYTE_MASK(STATUS_ID_CHARM))) {
        unit->team_flags = (unit->team_flags & ~UNIT_TEAM_MASK) | (unit->initial_team_flags & UNIT_TEAM_MASK);
    }
}

/* Apply the removals of the unit's action result: each removed status leaves
 * inflicted_status, has its CT cleared and, when executing, its graphics flag
 * switched off; then the current set is rebuilt as innate | inflicted. */
static void apply_removals(s32 unit_idx) {
    battle_unit_t* unit = &g_battle_units[unit_idx];
    s32 i;
    s32 mask;
    s32 byte_index;

    for (i = 0; i < STATUS_COUNT; i++) {
        byte_index = i / 8;
        mask = 0x80 >> (i & 7);
        if (unit->action.status_changes.removal[byte_index] & mask) {
            unit->inflicted_status[byte_index] &= ~mask;
            if (status_set_ct(unit, i, 1) == 0) {
                status_queue_display_flag_change_if_executing(
                    STATUS_HANDLER_INDEX(i), STATUS_DISPLAY_CHANGE_REMOVE, unit_idx);
            }
        }
    }
    status_refresh_current(unit);
}

/* Apply the pending status inflictions of unit_id's current action.
 *
 * For each of the 40 statuses the action inflicts that none of the unit's
 * inflicted statuses blocks, the status's cancel list becomes the action's
 * removal set, the pending changes are filtered, the status is recorded as
 * inflicted and given its CT, and its special flags are enabled (mode 2 when
 * re-inflicting anything but Crystal or Oil). Transparent clears the removal
 * flag; Invite and Charm copy the attacker's team, Invite permanently. A unit
 * that started on the blue team and becomes Crystal or Dead sets
 * g_reward_player_unit_fallen, which voids the battle's level bonus money.
 * Ends by storing the unit's current statuses. */
static void apply_inflictions(s32 unit_id, s32 removal_only) {
    s32 i;
    s32 j;
    s32 byte;
    u8 mask;
    s32 blocked;
    u8 previous;

    g_action_work.target_id = unit_id;
    g_action_target = &g_battle_units[unit_id];
    g_action_target_data = &g_battle_units[unit_id].action;
    for (i = 0; i < STATUS_COUNT; i++) {
        byte = i / 8;
        /* Signed shift (srav); STATUS_BYTE_MASK shifts unsigned. */
        mask = 0x80 >> (i & 7);
        if (!(g_action_target_data->status_changes.infliction[byte] & mask)) {
            continue;
        }
        blocked = 0;
        for (j = 0; j < STATUS_BYTE_COUNT; j++) {
            if (g_action_target->inflicted_status[j] & g_status_effect_data[i].cant_stack[j]) {
                blocked = 1;
                break;
            }
        }
        if (blocked) {
            continue;
        }
        for (j = 0; j < STATUS_BYTE_COUNT; j++) {
            g_action_target_data->status_changes.removal[j] = g_status_effect_data[i].cancels[j];
        }
        status_filter_action_changes(removal_only);
        apply_removals(unit_id);
        previous = g_action_target->inflicted_status[byte];
        g_action_target->inflicted_status[byte] = previous | mask;
        if (status_set_ct(g_action_target, i, 0) != 0) {
            continue;
        }
        if ((previous & mask) && i != STATUS_ID_CRYSTAL && i != STATUS_ID_OIL) {
            status_queue_display_flag_change_if_executing(STATUS_HANDLER_INDEX(i), STATUS_DISPLAY_CHANGE_KEEP, unit_id);
        } else {
            status_queue_display_flag_change_if_executing(STATUS_HANDLER_INDEX(i), STATUS_DISPLAY_CHANGE_ADD, unit_id);
        }
        if (i == STATUS_ID_TRANSPARENT) {
            g_action_target->transparent_removal_flag = 0;
        }
        if (i == STATUS_ID_INVITE || i == STATUS_ID_CHARM) {
            previous
                = (g_action_target->team_flags & ~UNIT_TEAM_MASK) | (g_action_attacker->team_flags & UNIT_TEAM_MASK);
            g_action_target->team_flags = previous;
            if (i == STATUS_ID_INVITE) {
                previous = (g_action_target->initial_team_flags & ~UNIT_TEAM_OR_PLAYER_CONTROL_MASK)
                    | (g_action_attacker->team_flags & UNIT_TEAM_MASK);
                g_action_target_data->reaction_id = 0;
                g_action_target->initial_team_flags = previous;
                g_action_target->auto_battle_setting = AI_AUTOBATTLE_MANUAL;
                g_action_target->auto_battle_target = 0;
            }
        }
        if (!(g_action_target->initial_team_flags & UNIT_TEAM_MASK)
            && (i == STATUS_ID_CRYSTAL || i == STATUS_ID_DEAD)) {
            g_reward_player_unit_fallen = 1;
        }
    }
    status_refresh_current(g_action_target);
}
