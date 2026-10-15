/* Status-driven animation and spritesheet changes of units. */

#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "psx/types.h"

/* Swap a unit's spritesheet for a transformation status, or restore it.
 *
 * Crystal, Treasure, Chicken, and Frog select their corresponding special
 * spritesheets; Crystal and Treasure also clear the other statuses and reset
 * the animation. The first swap away from a regular sheet saves its SHP/SEQ
 * pointers; with no transformation status the saved pointers and the VRAM
 * slot's sheet id are restored. */
void battle_gfx_apply_status_spritesheet_change(battle_unit_t* unit, s32 tile_effect_level) {
    u32 flags;
    u16 spritesheet_id;
    u8 original_id;

    flags = unit->status_flags_5_6;
    if (flags & BATTLE_UNIT_STATUS_TRANSFORMATION_MASK) {
        if (flags & BATTLE_UNIT_STATUS_CRYSTAL) {
            spritesheet_id = BATTLE_SPRITESHEET_ID_CRYSTAL;
            unit->special_graphic_y_offset = 0x60;
            unit->status_flags_5_6 = BATTLE_UNIT_STATUS_CRYSTAL;
            unit->status_flags_1_4 = 0;
            battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
        } else if (flags & BATTLE_UNIT_STATUS_TREASURE) {
            spritesheet_id = BATTLE_SPRITESHEET_ID_TREASURE;
            unit->special_graphic_y_offset = 0;
            unit->status_flags_5_6 = BATTLE_UNIT_STATUS_TREASURE;
            unit->status_flags_1_4 = 0;
            battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_TREASURE, unit->facing, unit);
        } else if (flags & BATTLE_UNIT_STATUS_CHICKEN) {
            spritesheet_id = BATTLE_SPRITESHEET_ID_CHICKEN;
            unit->special_graphic_y_offset = 0;
        } else if (flags & BATTLE_UNIT_STATUS_FROG) {
            spritesheet_id = BATTLE_SPRITESHEET_ID_FROG;
            unit->special_graphic_y_offset = 0x30;
        }
        if (unit->spritesheet_id != spritesheet_id) {
            if (unit->spritesheet_id < BATTLE_SPRITESHEET_ID_CRYSTAL) {
                unit->animation.saved_shp = unit->animation.shp;
                unit->animation.saved_seq = unit->animation.seq;
            }
            unit->animation.shp = battle_gfx_get_unit_shp_by_spritesheet_id(spritesheet_id);
            unit->animation.seq = battle_gfx_get_unit_seq_by_spritesheet_id(spritesheet_id);
            unit->spritesheet_id = spritesheet_id;
            battle_gfx_tint_unit_palette_for_weather_and_tile(unit, tile_effect_level, 1);
            battle_unit_set_animation_based_on_status(unit);
            battle_gfx_load_trap_and_unit_frame_parts(
                unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
        }
    } else if (unit->spritesheet_id != g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].spritesheet_id) {
        unit->special_graphic_y_offset = 0;
        original_id = g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].spritesheet_id;
        unit->animation.shp = unit->animation.saved_shp;
        unit->animation.seq = unit->animation.saved_seq;
        unit->spritesheet_id = original_id;
        battle_gfx_check_tile_status_palette_mod(unit, tile_effect_level, 1);
        battle_unit_set_animation_based_on_status(unit);
        battle_gfx_load_trap_and_unit_frame_parts(
            unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
    }
}

/* Make item `item_id` the one a unit's weapon and effect blocks draw.
 *
 * The item's two palettes of g_battle_gfx_item_graphic_data go to the unit's
 * CLUTs of the ITEM_A and ITEM_B rows, and its type selects the frame set of
 * the weapon SHP; a thrown weapon, any Throw ability and Throw Stone use
 * set 0x15. */
void battle_gfx_set_thrown_item_graphic_palette(s32 item_id, battle_unit_t* unit) {
    item_data_t* item;
    u32 weapon_type;
    s32 clut_index;
    /* The last test re-reads the ability id rather than reusing the value the
     * range test loaded; assigning it through its own local stops cse folding
     * the two reads into one. */
    u16 ability_id;

    item = main_item_get_data_pointer(item_id);
    weapon_type = item->type;
    if ((weapon_type >= ITEM_TYPE_SHURIKEN)
        || ((u32)(unit->used_ability_id - ABILITY_ID_THROW_FIRST) < ABILITY_ID_JUMP_FIRST - ABILITY_ID_THROW_FIRST)
        || ((ability_id = unit->used_ability_id), ability_id == ABILITY_ID_BASIC_SKILL_THROW_STONE)) {
        weapon_type = 0x15;
    }
    clut_index = unit->runtime_id;
    map_load_palette_data(&g_battle_gfx_weapon_clut[g_battle_gfx_item_graphic_data[item_id].palettes >> 4],
        MAP_CLUT_ROW_ITEM_A, clut_index, 0);
    map_load_palette_data(&g_battle_gfx_weapon_clut[g_battle_gfx_item_graphic_data[item_id].palettes & 0xF],
        MAP_CLUT_ROW_ITEM_B, clut_index, 0);
    unit->used_item_or_weapon_id = item_id;
    unit->equipped_weapon_type = weapon_type;
}

struct battle_unit;

void battle_unit_call_set_animation_based_on_status(struct battle_unit* unit) {
    battle_unit_set_animation_based_on_status(unit);
}

/* When the target blocked with a hand or a golem took the blow, replace the
 * attacker's unarmed, swing or thrust animation with its recoil at the same
 * height. Only the TYPE1 and TYPE2 sheets have the recoils. */
void battle_unit_set_attacker_animation_for_shield_block(battle_unit_t* attacker, battle_unit_t* target) {
    unit_stats_t* stats;
    s32 kind;
    s32 anim;

    stats = target->stats;
    if (stats == 0) {
        return;
    }
    if (g_battle_gfx_spritesheet_data[attacker->spritesheet_id].shp_id >= BATTLE_SHP_CYOKO) {
        return;
    }
    kind = stats->action.miss_type;
    anim = 0;
    if (kind >= ACTION_MISS_TYPE_RIGHT_HAND_EVADE
        && (kind < ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD || kind == ACTION_MISS_TYPE_GOLEM_GUARD)) {
        switch (attacker->animation.sequence_id >> 1) {
        case BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_HIGH:
            anim = BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_BLOCKED_HIGH;
            break;
        case BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_LEVEL:
            anim = BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_BLOCKED_LEVEL;
            break;
        case BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_LOW:
            anim = BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_BLOCKED_LOW;
            break;
        case BATTLE_UNIT_ANIMATION_SWING_HIGH:
            anim = BATTLE_UNIT_ANIMATION_SWING_BLOCKED_HIGH;
            break;
        case BATTLE_UNIT_ANIMATION_SWING_LEVEL:
            anim = BATTLE_UNIT_ANIMATION_SWING_BLOCKED_LEVEL;
            break;
        case BATTLE_UNIT_ANIMATION_SWING_LOW:
            anim = BATTLE_UNIT_ANIMATION_SWING_BLOCKED_LOW;
            break;
        case BATTLE_UNIT_ANIMATION_THRUST_HIGH:
            anim = BATTLE_UNIT_ANIMATION_THRUST_BLOCKED_HIGH;
            break;
        case BATTLE_UNIT_ANIMATION_THRUST_LEVEL:
            anim = BATTLE_UNIT_ANIMATION_THRUST_BLOCKED_LEVEL;
            break;
        case BATTLE_UNIT_ANIMATION_THRUST_LOW:
            anim = BATTLE_UNIT_ANIMATION_THRUST_BLOCKED_LOW;
            break;
        }
    }
    if (anim != 0) {
        /* The target reads the u16 facing field signed (lh). */
        battle_unit_store_animation_facing_movement_data(anim, attacker->facing, attacker);
    }
}

/*
 * Pick and start the reaction animation a target plays after an action
 * resolves, then apply the action's special-effect side effects.
 */
void battle_unit_set_target_animation_from_attack_type(battle_unit_t* attacker, battle_unit_t* target) {
    unit_stats_t* stats;
    action_data_t* action;
    s32 animation;
    s32 delta;
    s16 attacker_height;
    s16 target_height;
    u32 water_depth;

    if (attacker == target) {
        return;
    }
    if ((target->animation.sequence_id >> 1) == BATTLE_UNIT_ANIMATION_DEAD) {
        return;
    }
    if ((target->animation.sequence_id >> 1) == BATTLE_UNIT_ANIMATION_DEATH) {
        return;
    }
    water_depth = move_get_water_depth(target);
    animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
    if (water_depth < 2) {
        stats = target->stats;
        if (stats != 0) {
            action = &stats->action;
            animation = BATTLE_UNIT_ANIMATION_STAND;
            switch (action->miss_type) {
            case ACTION_MISS_TYPE_RIGHT_HAND_EVADE:
            case ACTION_MISS_TYPE_LEFT_HAND_EVADE:
                battle_unit_face_towards_action_target(target, attacker);
                battle_gfx_set_thrown_item_graphic_palette(action->item_lost, target);
                attacker_height = battle_gfx_get_unit_spritesheet_height_by_runtime_id(attacker->runtime_id) / 2
                    - attacker->world_coords.vy;
                target_height = battle_gfx_get_unit_spritesheet_height_by_runtime_id(target->runtime_id) / 2
                    - target->world_coords.vy;
                delta = attacker_height - target_height;
                if (delta < -0xb) {
                    animation = BATTLE_UNIT_ANIMATION_WEAPON_GUARD_LOW;
                } else {
                    animation = BATTLE_UNIT_ANIMATION_WEAPON_GUARD_LEVEL;
                    if (delta >= 0xc) {
                        animation = BATTLE_UNIT_ANIMATION_WEAPON_GUARD_HIGH;
                    }
                }
                break;
            case ACTION_MISS_TYPE_ACCESSORY_EVADE:
            case ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD:
            case ACTION_MISS_TYPE_NULLIFIED:
            case ACTION_MISS_TYPE_ACCURACY_MISS:
            case ACTION_MISS_TYPE_FORCED_FAILURE:
                if (action->miss_display_flags & ACTION_MISS_DISPLAY_DODGE_ANIMATION) {
                    battle_unit_face_towards_action_target(target, attacker);
                    animation = BATTLE_UNIT_ANIMATION_DODGE;
                }
                break;
            case ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD:
            case ACTION_MISS_TYPE_CATCH:
                battle_unit_face_towards_action_target(target, attacker);
                animation = BATTLE_UNIT_ANIMATION_RAISE_HANDS;
                break;
            case ACTION_MISS_TYPE_CANCELED:
            case ACTION_MISS_TYPE_REFLECTED:
            case ACTION_MISS_TYPE_GOLEM_GUARD:
                break;
            default:
                if (action->hit != 0) {
                    if ((action->hp_damage != 0) || (action->mp_damage != 0)) {
                        animation = BATTLE_UNIT_ANIMATION_HURT;
                    } else if ((action->hp_healing != 0) || (action->mp_healing != 0)) {
                        animation = BATTLE_UNIT_ANIMATION_RAISE_HANDS;
                    }
                }
                break;
            }

            if (animation != 0) {
                if (target->spritesheet_id < BATTLE_SPRITESHEET_ID_CRYSTAL) {
                    battle_unit_store_animation_facing_movement_data(animation, target->facing, target);
                } else if (animation == BATTLE_UNIT_ANIMATION_HURT) {
                    battle_unit_store_animation_facing_movement_data(
                        BATTLE_UNIT_ANIMATION_HURT, target->facing, target);
                }
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_KNOCKBACK) {
                move_transfer_target_coordinates(attacker, target);
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_POACH) {
                target->status_flags_5_6 |= BATTLE_UNIT_STATUS_POACHED;
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_MORBOL) {
                target->status_flags_5_6 |= BATTLE_UNIT_STATUS_MORBOL;
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT) {
                ai_init_unit_abilities(target->stats->battle_id);
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
                ai_init_unit_abilities(target->stats->battle_id);
            }
            return;
        }
        animation = BATTLE_UNIT_ANIMATION_HURT;
    }
    battle_unit_store_animation_facing(animation, target->facing, target);
}

void battle_unit_update_anim_display_for_all_targets(battle_unit_t* unit) {
    s32 counter;
    battle_unit_t* target;

    if (unit->strike.target_count != 0) {
        for (counter = 0; counter < unit->strike.target_count; counter++) {
            target = battle_unit_get_by_battle_id(unit->strike.target_list[counter]);
            if (target != 0) {
                battle_unit_update_display_by_runtime_id(target->runtime_id);
            }
        }
    }
}

/*
 * Claim a unit runtime record slot and stamp its runtime ID.
 *
 * Ids below 16 take their own slot, failing if another in-use slot already
 * carries that id or the slot is taken. Larger ids take the first free slot,
 * whose index becomes the stored runtime ID; exhausting all 16 raises animation error
 * 6. Returns 0 on failure.
 */
battle_unit_t* battle_unit_claim_slot(s16 requested_runtime_id) {
    s32 i;

    if (requested_runtime_id < BATTLE_UNIT_RUNTIME_SLOT_COUNT) {
        for (i = 0; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
            if (g_battle_unit_slot_states[i].in_use != 0 && g_battle_units[i].runtime_id == requested_runtime_id) {
                break;
            }
        }
        if (i < BATTLE_UNIT_RUNTIME_SLOT_COUNT) {
            return 0;
        }
        if (g_battle_unit_slot_states[requested_runtime_id].in_use != 0) {
            return 0;
        }
        g_battle_unit_slot_states[requested_runtime_id].in_use = 1;
        g_battle_units[requested_runtime_id].runtime_id = requested_runtime_id;
        return &g_battle_units[requested_runtime_id];
    }
    for (i = 0; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
        if (g_battle_unit_slot_states[i].in_use == 0) {
            g_battle_unit_slot_states[i].in_use = 1;
            g_battle_units[i].runtime_id = i;
            return &g_battle_units[i];
        }
    }
    main_system_handle_error(MAIN_SYSTEM_ERROR_NO_UNIT_RUNTIME_SLOT);
    return 0;
}
