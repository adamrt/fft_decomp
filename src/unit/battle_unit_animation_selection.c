#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/sound.h"
#include "psx/types.h"

/* Copies the Float display status into BATTLE_MOTION_FLAG_FLOAT and, unless the
 * unit is in a Jump (flag set, being removed, or its height still active), puts
 * the unit back at its tile's height. */
void battle_unit_update_float_and_jump_height(battle_unit_t* unit) {
    if (unit->status_flags_5_6 & BATTLE_UNIT_STATUS_FLOAT) {
        unit->movement.floating = 1;
    } else {
        unit->movement.floating = 0;
    }
    if ((unit->status_flags_5_6 & (BATTLE_UNIT_STATUS_JUMP | BATTLE_UNIT_STATUS_JUMP_HEIGHT_ACTIVE)) == 0
        && (unit->statuses_to_remove_5_6 & BATTLE_UNIT_STATUS_JUMP) == 0) {
        unit->world_coords.vy = battle_gfx_calculate_world_height_from_world_coords(unit);
        battle_unit_set_real_coords_from_world_coords(unit);
    }
}

/* Per-weapon-type sound tables, 0x20 entries each: swing, hit, and the
 * guarded/nullified reaction sound. */

/* Play the weapon swing sound, or the hit sound for each current target.
 *
 * The weapon type comes from the unit's used weapon; Throwing and later types use
 * type 1. In hit mode each target's action miss type selects the table, and a
 * critical hit plays sound 0x72. */
void sound_play_weapon_sfx(battle_unit_t* unit, s32 hit) {
    battle_unit_t* target;
    s32 weapon_type;
    s32 i;

    weapon_type = main_item_get_data_pointer(unit->strike.used_weapon_id)->type;
    if (weapon_type >= ITEM_TYPE_SHURIKEN) {
        weapon_type = ITEM_TYPE_KNIFE;
    }
    switch (hit) {
    case 0:
        sound_play_movement_sfx(unit, g_sound_weapon_swing_sfx_ids[weapon_type]);
        break;
    case 1:
        for (i = 0; i < unit->strike.target_count; i++) {
            target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
            if (target == 0 || target->stats == 0) {
                continue;
            }
            switch (target->stats->action.miss_type) {
            case ACTION_MISS_TYPE_RIGHT_HAND_EVADE:
            case ACTION_MISS_TYPE_LEFT_HAND_EVADE:
            case ACTION_MISS_TYPE_GOLEM_GUARD:
                sound_play_movement_sfx(unit, g_sound_shield_deflect_sfx_ids[weapon_type]);
                break;
            case ACTION_MISS_TYPE_ACCESSORY_EVADE:
            case ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD:
            case ACTION_MISS_TYPE_FORCED_FAILURE:
            case ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD:
            case ACTION_MISS_TYPE_CATCH:
                sound_play_movement_sfx(unit, MAIN_SFX_ERROR_MESSAGE);
                break;
            case ACTION_MISS_TYPE_NULLIFIED:
            case ACTION_MISS_TYPE_ACCURACY_MISS:
            case ACTION_MISS_TYPE_CANCELED:
                break;
            default:
                if (target->stats->action.critical != 0) {
                    sound_play_movement_sfx(unit, MAIN_SFX_CRITICAL_HIT);
                } else {
                    sound_play_movement_sfx(unit, g_sound_weapon_hit_sfx_ids[weapon_type]);
                }
                break;
            }
        }
        break;
    }
}

/* The depth of water the unit stands in, which picks its animations: 0 on the surface (ON_WATER
 * movement, Float, a crystal), 1 for a chicken, frog or treasure and for IN_WATER movement,
 * otherwise the depth of its tile. */
u8 move_get_water_depth(battle_unit_t* unit) {
    map_tile_t* tile;
    s32 status;
    u32 flags;

    tile = map_get_tile_data_pointer(
        unit->world_coords.vx / MAP_TILE_SIZE, unit->world_coords.vz / MAP_TILE_SIZE, unit->map_z);
    if (unit->stats != 0) {
        status = move_get_effective_flags(unit->stats);
        if (status & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER) {
            return 0;
        }
        flags = unit->status_flags_5_6;
        if (flags & BATTLE_UNIT_STATUS_FLOAT) {
            return 0;
        }
        if (flags & (BATTLE_UNIT_STATUS_CHICKEN | BATTLE_UNIT_STATUS_FROG | BATTLE_UNIT_STATUS_TREASURE)) {
            return 1;
        }
        if (status & BATTLE_EFFECTIVE_MOVEMENT_IN_WATER) {
            return 1;
        }
        if (flags & BATTLE_UNIT_STATUS_CRYSTAL) {
            return 0;
        }
    }
    return tile->depth_half_height.depth;
}

/* Choose the attacker's weapon attack animation from the target's height.
 *
 * g_battle_weapon_attack_animations has three animations for each item type:
 * at a higher target, a level one and a lower one. A bow indexes its row by
 * effect_get_bow_shot_angle. Other weapons compare the vertical
 * centers of the two sprites: a target 12 or more above the attacker takes
 * the first animation and one 12 or more below the third. A Golem or Blade
 * Grasp guard always takes the first, and the sheets from the crystal up
 * always play the unarmed level attack. */
void battle_unit_select_weapon_attack_animation(battle_unit_t* attacker, battle_unit_t* target) {
    s32 miss_type;
    s16 attacker_center;
    s16 target_center;
    s32 difference;

    if (attacker == 0) {
        return;
    }
    if (target != 0) {
        if (attacker->spritesheet_id >= BATTLE_SPRITESHEET_ID_CRYSTAL) {
            battle_unit_store_animation_facing_movement_data(
                g_battle_weapon_attack_animations[0][1], attacker->facing, attacker);
            return;
        }
        if (attacker->equipped_weapon_type == ITEM_TYPE_BOW) {
            battle_unit_store_animation_facing_movement_data(
                g_battle_weapon_attack_animations[ITEM_TYPE_BOW][effect_get_bow_shot_angle()], attacker->facing,
                attacker);
            return;
        }
        miss_type = target->stats->action.miss_type;
        if (miss_type != ACTION_MISS_TYPE_HIT) {
            if (miss_type >= ACTION_MISS_TYPE_REFLECTED) {
                if (miss_type < ACTION_MISS_TYPE_BUGGED_REFLECTION) {
                    if (miss_type >= ACTION_MISS_TYPE_GOLEM_GUARD) {
                        battle_unit_store_animation_facing_movement_data(
                            g_battle_weapon_attack_animations[attacker->equipped_weapon_type][0], attacker->facing,
                            attacker);
                        return;
                    }
                }
            }
        }
        /* The target loads the attacker's screen_z zero-extended (lhu). */
        attacker_center = battle_gfx_get_unit_spritesheet_height_by_runtime_id(attacker->runtime_id) / 2
            - attacker->world_coords.vy;
        target_center
            = battle_gfx_get_unit_spritesheet_height_by_runtime_id(target->runtime_id) / 2 - target->world_coords.vy;
        difference = attacker_center - target_center;
        if (difference < -11) {
            battle_unit_store_animation_facing_movement_data(
                g_battle_weapon_attack_animations[attacker->equipped_weapon_type][0], attacker->facing, attacker);
            return;
        }
        if (difference >= 12) {
            battle_unit_store_animation_facing_movement_data(
                g_battle_weapon_attack_animations[attacker->equipped_weapon_type][2], attacker->facing, attacker);
            return;
        }
    }
    battle_unit_store_animation_facing_movement_data(
        g_battle_weapon_attack_animations[attacker->equipped_weapon_type][1], attacker->facing, attacker);
}

/* Start the animation of an item ability: the item held up for a target on
 * the unit's own tile or beside it, thrown for one further away. */
void battle_unit_set_animation_for_item_abilities(battle_unit_t* unit, battle_unit_t* target) {
    s32 dx;
    s32 dy;

    battle_gfx_store_item_display_data(unit, unit->used_item_or_weapon_id);
    if (target != 0) {
        dx = target->map_x - unit->map_x;
        dy = target->map_y - unit->map_y;
    } else {
        dx = 0;
        dy = 0;
    }
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    /* facing is read signed (lh). */
    if ((dx <= 0 && dy <= 0) || (dx < 2 && dy == 0) || (dx == 0 && dy < 2)) {
        battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_HOLD_ITEM, unit->facing, unit);
    } else {
        unit->item_display_active = 0;
        battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_THROW_ITEM, unit->facing, unit);
    }
}

/* Start the first animation of the used ability's charge set, if it has one. */
void battle_unit_start_ability_charge_animation(battle_unit_t* unit) {
    u8 animation;

    animation = g_battle_ability_charge_animation_sets[g_battle_ability_animation_data[unit->used_ability_id]
            .charge_animation_set_id][0];
    if (animation != 0) {
        /* facing is passed sign-extended (lh) here. */
        battle_unit_store_animation_facing(animation, unit->facing, unit);
    }
}

/* Start the second animation of the used ability's charge set, the one held
 * while charging or performing; the submerged idle in water two or more deep. */
void battle_unit_start_ability_charge_hold_animation(battle_unit_t* unit) {
    u8 animation;
    u8 water_depth;
    s32 selected_animation;

    animation = g_battle_ability_charge_animation_sets[g_battle_ability_animation_data[unit->used_ability_id]
            .charge_animation_set_id][1];
    if (animation != 0) {
        water_depth = move_get_water_depth(unit);
        selected_animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
        if (water_depth < 2) {
            selected_animation = animation;
        }
        /* facing is read signed (lh) here. */
        battle_unit_store_animation_facing(selected_animation, unit->facing, unit);
    }
}

/* Start the attacker's animation for the ability it uses: the cast for the
 * RUKA, ARUTE and KANZEN sequences, else the ability table's attack
 * animation, which may ask for the weapon's attack or the item animation. */
void battle_unit_select_attack_animation_for_ability(battle_unit_t* attacker, battle_unit_t* target) {
    s32 animation;
    s32 seq_type;

    seq_type = g_battle_gfx_spritesheet_data[attacker->spritesheet_id].seq_id;
    if (seq_type < 8) {
        if (seq_type >= BATTLE_SEQ_RUKA) {
            /* facing is loaded signed (lh) here. */
            battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_CAST, attacker->facing, attacker);
            return;
        }
    }

    animation = g_battle_ability_animation_data[attacker->used_ability_id].attack_animation;
    if (animation == BATTLE_ATTACK_ANIMATION_USE_WEAPON) {
        battle_unit_select_weapon_attack_animation(attacker, target);
        return;
    }
    if (animation == BATTLE_ATTACK_ANIMATION_USE_ITEM) {
        battle_unit_set_animation_for_item_abilities(attacker, target);
        return;
    }
    battle_unit_store_animation_facing(animation, attacker->facing, attacker);
}

/* Start a unit's level-up animation: the jump on dry land, the hops for a
 * rider and in water one deep, the submerged idle in water two deep. */
void battle_unit_set_level_up_animation(battle_unit_t* unit) {
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_LEVEL_UP_HOP, unit->facing, unit);
        return;
    }
    switch (move_get_water_depth(unit)) {
    case 0:
        battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_LEVEL_UP, unit->facing, unit);
        break;
    case 1:
        battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_LEVEL_UP_HOP, unit->facing, unit);
        break;
    case 2:
        battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
        break;
    }
}

/* Give a rider the riding animation and any other unit the standing one, or
 * the submerged idle in water two or more deep; a mount on land also puts
 * its rider back in the riding animation. The depth is a byte, so its
 * negative test never holds. */
void battle_unit_set_animation_based_on_mounted_state(battle_unit_t* unit) {
    s32 anim;
    s32 selected;
    s32 water_depth;
    battle_unit_t* other;

    anim = BATTLE_UNIT_ANIMATION_RIDING;
    if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_RIDER) {
        water_depth = move_get_water_depth(unit) & 0xFF;
        if (water_depth < 0) {
            selected = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
        } else if (water_depth < 2) {
            selected = BATTLE_UNIT_ANIMATION_STAND;
            if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
                other = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
                if (other != 0) {
                    battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_RIDING, unit->facing, other);
                }
            }
        } else {
            selected = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
        }
        if (selected == 0) {
            return;
        }
        anim = selected;
    }
    battle_unit_store_animation_facing(anim, unit->facing, unit);
}

/*
 * Select the animation of a unit that is walking: riding for a rider, else
 * the walk of its walk speed (fast from 0x3000, slow up to 0x1400), or the
 * submerged idle of that speed in water two or more deep. A mount on land
 * also puts its rider in the riding animation.
 *
 * The height-mode result is deliberately truncated to its low byte, and
 * facing is read signed (`lh`) throughout.
 */
void battle_unit_set_idle_animation_for_movement(battle_unit_t* unit) {
    s32 anim;
    s32 walk_speed;
    battle_unit_t* mount;

    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_RIDING, unit->facing, unit);
        return;
    }
    switch (move_get_water_depth(unit)) {
    case 0:
    case 1:
        walk_speed = unit->walk_speed;
        if (walk_speed >= 0x3000) {
            anim = BATTLE_UNIT_ANIMATION_WALK_FAST;
        } else {
            anim = BATTLE_UNIT_ANIMATION_WALK;
            if (walk_speed < 0x1401) {
                anim = BATTLE_UNIT_ANIMATION_WALK_SLOW;
            }
        }
        if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
            mount = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
            if (mount != 0) {
                battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_RIDING, unit->facing, mount);
            }
        }
        break;
    case 2:
    default:
        walk_speed = unit->walk_speed;
        if (walk_speed >= 0x3000) {
            anim = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE_HASTE;
        } else {
            anim = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
            if (walk_speed < 0x1401) {
                anim = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE_SLOW;
            }
        }
        break;
    }
    if (anim != 0) {
        battle_unit_store_animation_facing(anim, unit->facing, unit);
    }
}

/*
 * Select a unit's resting animation from its renderer-side status mirror.
 *
 * In water less than two deep a crystal or a treasure shows its own
 * animation. Any other unit takes the animation of the first of: dead,
 * riding, critical (the kneel of sleep; not for a mount), stop, sleep,
 * petrify, confusion, performing or charging (the hold animation of the
 * ability's charge set), defending, slow, haste and cursed; with none of
 * them it idles. The RUKA, ARUTE and KANZEN sequences have no
 * riding or critical animation, and a sheet with the flying flag idles with
 * the three flying animations. In deeper water the choice is the dead, the
 * riding or one of the submerged idle animations.
 *
 * Animation 0x21 belongs to bit 0 of the 32-bit status mask, which no status maps
 * to (g_battle_unit_status_mask_by_handler_index): that arm never runs.
 */
void battle_unit_set_animation_based_on_status(battle_unit_t* unit) {
    s32 animation;
    u8 water_depth;
    u8 spritesheet_id;

    water_depth = move_get_water_depth(unit);
    animation = 0;
    switch (water_depth) {
    case 0:
    case 1:
        if (unit->status_flags_5_6 & BATTLE_UNIT_STATUS_CRYSTAL) {
            animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
        } else if (unit->status_flags_5_6 & BATTLE_UNIT_STATUS_TREASURE) {
            animation = BATTLE_UNIT_ANIMATION_TREASURE;
        } else if ((unit->status_flags_1_4 & BATTLE_UNIT_STATUS_ANIMATION_SELECTION_MASK) == 0) {
            animation = BATTLE_UNIT_ANIMATION_IDLE_FLYING;
            if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                animation = BATTLE_UNIT_ANIMATION_IDLE;
            }
        } else {
            spritesheet_id = unit->spritesheet_id;
            if (g_battle_gfx_spritesheet_data[spritesheet_id].seq_id >= BATTLE_SEQ_RUKA) {
                if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_DEAD) {
                    animation = BATTLE_UNIT_ANIMATION_DEAD;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_STOP) {
                    animation = BATTLE_UNIT_ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_SLEEP) {
                    animation = BATTLE_UNIT_ANIMATION_SLEEP;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_PETRIFY) {
                    animation = BATTLE_UNIT_ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_CONFUSION) {
                    animation = BATTLE_UNIT_ANIMATION_CONFUSION;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_PERFORMING) {
                    unit->used_ability_id = unit->stats->action_command.ability_id;
                    battle_unit_start_ability_charge_hold_animation(unit);
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_CHARGING) {
                    unit->used_ability_id = unit->stats->action_command.ability_id;
                    battle_unit_start_ability_charge_hold_animation(unit);
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_DEFENDING) {
                    animation = BATTLE_UNIT_ANIMATION_DEFEND;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_SLOW) {
                    animation = BATTLE_UNIT_ANIMATION_IDLE_FLYING_SLOW;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = BATTLE_UNIT_ANIMATION_IDLE_SLOW;
                    }
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_HASTE) {
                    animation = BATTLE_UNIT_ANIMATION_IDLE_FLYING_HASTE;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = BATTLE_UNIT_ANIMATION_IDLE_HASTE;
                    }
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_UNNAMED_0E) {
                    animation = BATTLE_UNIT_ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & 1) {
                    animation = 0x21;
                } else {
                    animation = BATTLE_UNIT_ANIMATION_IDLE_FLYING;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = BATTLE_UNIT_ANIMATION_IDLE;
                    }
                }
            } else {
                if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_DEAD) {
                    animation = BATTLE_UNIT_ANIMATION_DEAD;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_MOUNTED) {
                    animation = BATTLE_UNIT_ANIMATION_RIDING;
                } else if ((unit->status_flags_1_4 & BATTLE_UNIT_STATUS_CRITICAL)
                    && unit->mount_state != BATTLE_UNIT_MOUNT_STATE_MOUNT) {
                    animation = BATTLE_UNIT_ANIMATION_SLEEP;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_STOP) {
                    animation = BATTLE_UNIT_ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_SLEEP) {
                    animation = BATTLE_UNIT_ANIMATION_SLEEP;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_PETRIFY) {
                    animation = BATTLE_UNIT_ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_CONFUSION) {
                    animation = BATTLE_UNIT_ANIMATION_CONFUSION;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_PERFORMING) {
                    unit->used_ability_id = unit->stats->action_command.ability_id;
                    battle_unit_start_ability_charge_hold_animation(unit);
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_CHARGING) {
                    unit->used_ability_id = unit->stats->action_command.ability_id;
                    battle_unit_start_ability_charge_hold_animation(unit);
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_DEFENDING) {
                    animation = BATTLE_UNIT_ANIMATION_DEFEND;
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_SLOW) {
                    animation = BATTLE_UNIT_ANIMATION_IDLE_FLYING_SLOW;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = BATTLE_UNIT_ANIMATION_IDLE_SLOW;
                    }
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_HASTE) {
                    animation = BATTLE_UNIT_ANIMATION_IDLE_FLYING_HASTE;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = BATTLE_UNIT_ANIMATION_IDLE_HASTE;
                    }
                } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_UNNAMED_0E) {
                    animation = BATTLE_UNIT_ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & 1) {
                    animation = 0x21;
                } else {
                    animation = BATTLE_UNIT_ANIMATION_IDLE_FLYING;
                    if (battle_gfx_get_spritesheet_flying_flag(unit->spritesheet_id) == 0) {
                        animation = BATTLE_UNIT_ANIMATION_IDLE;
                    }
                }
            }
        }
        break;
    case 2:
    default:
        if ((unit->status_flags_5_6 & BATTLE_UNIT_STATUS_TRANSFORMATION_MASK) != 0) {
            if (unit->status_flags_5_6 & BATTLE_UNIT_STATUS_CRYSTAL) {
                animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
            } else if (unit->status_flags_5_6 & BATTLE_UNIT_STATUS_TREASURE) {
                animation = BATTLE_UNIT_ANIMATION_TREASURE;
            }
        } else if ((unit->status_flags_1_4 & BATTLE_UNIT_STATUS_ANIMATION_SELECTION_MASK) != 0) {
            if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_DEAD) {
                animation = BATTLE_UNIT_ANIMATION_DEAD;
            } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_MOUNTED) {
                animation = BATTLE_UNIT_ANIMATION_RIDING;
            } else if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_HASTE) {
                animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE_HASTE;
            } else {
                animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
                if (unit->status_flags_1_4 & BATTLE_UNIT_STATUS_SLOW) {
                    animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE_SLOW;
                }
            }
        } else {
            animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
        }
        break;
    }
    if (animation != 0) {
        battle_unit_store_animation_facing(animation, unit->facing, unit);
    }
}

/*
 * Apply this frame's pending status changes to a unit's animation.
 *
 * A unit that becomes a chicken or a frog starts its idle animation, and one
 * that stops being either the animation its statuses select. A death raises
 * the death smoke for the chocobo and monster sequences and, outside events,
 * the cry of the unit's sex or kind; a new crystal or treasure gets its
 * puff. In an event, gaining and losing the Jump status play the jump's
 * departure and landing. Outside events a death or a revival on land plays
 * BATTLE_UNIT_ANIMATION_DEATH or _REVIVE; in water two or more deep, in
 * events too, the unit is put straight into its dead or its submerged idle
 * sequence, with the wait cleared so that the sequence starts at once.
 */
void battle_unit_update_animation_for_status_changes(battle_unit_t* unit) {
    unit_stats_t* stats;

    if (unit->statuses_to_add_5_6 & BATTLE_UNIT_STATUS_CHICKEN) {
        battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_IDLE, unit->facing, unit);
    } else if (unit->statuses_to_remove_5_6 & BATTLE_UNIT_STATUS_CHICKEN) {
        battle_unit_set_animation_based_on_status(unit);
    }
    if (unit->statuses_to_add_5_6 & BATTLE_UNIT_STATUS_FROG) {
        battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_IDLE, unit->facing, unit);
    } else if (unit->statuses_to_remove_5_6 & BATTLE_UNIT_STATUS_FROG) {
        battle_unit_set_animation_based_on_status(unit);
    }
    if (unit->statuses_to_add_1_4 & BATTLE_UNIT_STATUS_DEAD) {
        if (g_battle_gfx_spritesheet_data[unit->spritesheet_id].seq_id == BATTLE_SEQ_CYOKO
            || g_battle_gfx_spritesheet_data[unit->spritesheet_id].seq_id == BATTLE_SEQ_MON) {
            effect_set_secondary_death_smoke(unit);
        }
        if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
            stats = unit->stats;
            if (stats != 0) {
                if (stats->unit_flags & UNIT_FLAG_MALE) {
                    sound_play_movement_sfx(unit, MAIN_SFX_DEATH_MALE);
                } else if (stats->unit_flags & UNIT_FLAG_FEMALE) {
                    sound_play_movement_sfx(unit, MAIN_SFX_DEATH_FEMALE);
                } else if (stats->unit_flags & UNIT_FLAG_MONSTER) {
                    sound_play_movement_sfx(unit, MAIN_SFX_DEATH_MONSTER);
                }
            }
        }
    }
    if (unit->statuses_to_add_5_6 & (BATTLE_UNIT_STATUS_CRYSTAL | BATTLE_UNIT_STATUS_TREASURE)) {
        effect_set_secondary_zodiac_poof(unit);
    }
    if (g_battle_game_state == BATTLE_GAME_STATE_EVENT) {
        if (unit->statuses_to_add_5_6 & BATTLE_UNIT_STATUS_JUMP) {
            battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_JUMP_DEPART, unit->facing, unit);
        } else if (unit->statuses_to_remove_5_6 & BATTLE_UNIT_STATUS_JUMP) {
            battle_unit_draw_and_activate_by_runtime_id(unit->runtime_id);
            battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_JUMP_LAND, unit->facing, unit);
        }
        if ((move_get_water_depth(unit) & 0xff) >= 2) {
            if (unit->statuses_to_add_1_4 & BATTLE_UNIT_STATUS_DEAD) {
                battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_DEAD, unit->facing, unit);
                unit->animation.sequence_id = BATTLE_UNIT_ANIMATION_DEAD << 1;
                unit->animation.wait = 0;
            } else if (unit->statuses_to_remove_1_4 & BATTLE_UNIT_STATUS_DEAD) {
                battle_unit_store_animation_facing_movement_data(
                    BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
                unit->animation.sequence_id = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE << 1;
                unit->animation.wait = 0;
            }
        }
    } else if ((move_get_water_depth(unit) & 0xff) < 2) {
        if (unit->statuses_to_add_1_4 & BATTLE_UNIT_STATUS_DEAD) {
            battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_DEATH, unit->facing, unit);
        } else if (unit->statuses_to_remove_1_4 & BATTLE_UNIT_STATUS_DEAD) {
            battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_REVIVE, unit->facing, unit);
        }
    } else if (unit->statuses_to_add_1_4 & BATTLE_UNIT_STATUS_DEAD) {
        battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_DEAD, unit->facing, unit);
        unit->animation.sequence_id = BATTLE_UNIT_ANIMATION_DEAD << 1;
        unit->animation.wait = 0;
    } else if (unit->statuses_to_remove_1_4 & BATTLE_UNIT_STATUS_DEAD) {
        battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
        unit->animation.sequence_id = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE << 1;
        unit->animation.wait = 0;
    }
}

/* Carries out a pending Poached or Morbol display flag (status_flags_5_6) and
 * clears it; Poached is tested first and only one is handled per call.
 *
 * Poached configures the unit's palette modulation
 * (battle_gfx_start_unit_fade_out_red). Morbol swaps the sprite
 * for the Morbol spritesheet (VRAM slot, SHP, SEQ, texture page), refreshes the
 * tile palette and the idle animation, then rewrites the battle stats as a Morbol
 * and reinitializes the unit's AI ability data. */
void battle_unit_apply_poach_morbol_transformation(battle_unit_t* unit) {
    u32 flags;
    map_tile_t* tile;

    flags = unit->status_flags_5_6;
    if ((flags & (BATTLE_UNIT_STATUS_POACHED | BATTLE_UNIT_STATUS_MORBOL)) == 0) {
        return;
    }
    if (flags & BATTLE_UNIT_STATUS_POACHED) {
        unit->status_flags_5_6 = flags & ~BATTLE_UNIT_STATUS_POACHED;
        battle_gfx_start_unit_fade_out_red(unit->runtime_id);
        return;
    }
    if ((flags & BATTLE_UNIT_STATUS_MORBOL) == 0) {
        return;
    }
    unit->status_flags_5_6 = flags & ~BATTLE_UNIT_STATUS_MORBOL;
    unit->special_graphic_y_offset = 0;
    unit->spritesheet_id = BATTLE_SPRITESHEET_ID_MORBOL;
    unit->spritesheet_vram_slot = battle_gfx_claim_spritesheet_slot(BATTLE_SPRITESHEET_ID_MORBOL);
    unit->animation.shp = battle_gfx_get_unit_shp_by_spritesheet_id(BATTLE_SPRITESHEET_ID_MORBOL);
    unit->animation.seq = battle_gfx_get_unit_seq_by_spritesheet_id(BATTLE_SPRITESHEET_ID_MORBOL);
    unit->stored_palette = 0;
    unit->tpage = unit->spritesheet_vram_slot + BATTLE_GFX_TPAGE_SPRITESHEET_SLOT_0;
    tile = map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
    battle_gfx_check_tile_status_palette_mod(unit, tile->flags.shading, 1);
    /* The target reads the u16 facing field signed (lh). */
    battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_IDLE, unit->facing, unit);
    battle_unit_transform_into_morbol(unit->stats);
    ai_init_unit_abilities(unit->stats->battle_id);
}
