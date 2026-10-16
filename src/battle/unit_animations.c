/* Unit animation selection by action, status and mount, unit frame and SEQ script execution, spritesheet unpacking and
 * VRAM loading, and the unit update loops. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/event_attack.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Per-weapon-type sound tables, 0x20 entries each: swing, hit, and the
 * guarded/nullified reaction sound. */

/* Play the weapon swing sound, or the hit sound for each current target.
 *
 * The weapon type comes from the unit's used weapon; Throwing and later types use
 * type 1. In hit mode each target's action miss type selects the table, and a
 * critical hit plays sound 0x72. */
void battle_sound_play_weapon_sfx(battle_unit_t* unit, s32 hit) {
    battle_unit_t* target;
    s32 weapon_type;
    s32 i;

    weapon_type = main_item_get_data_pointer(unit->strike.used_weapon_id)->type;
    if (weapon_type >= ITEM_TYPE_SHURIKEN) {
        weapon_type = ITEM_TYPE_KNIFE;
    }
    switch (hit) {
    case 0:
        battle_sound_play_movement_sfx(unit, g_sound_weapon_swing_sfx_ids[weapon_type]);
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
                battle_sound_play_movement_sfx(unit, g_sound_shield_deflect_sfx_ids[weapon_type]);
                break;
            case ACTION_MISS_TYPE_ACCESSORY_EVADE:
            case ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD:
            case ACTION_MISS_TYPE_FORCED_FAILURE:
            case ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD:
            case ACTION_MISS_TYPE_CATCH:
                battle_sound_play_movement_sfx(unit, MAIN_SFX_ERROR_MESSAGE);
                break;
            case ACTION_MISS_TYPE_NULLIFIED:
            case ACTION_MISS_TYPE_ACCURACY_MISS:
            case ACTION_MISS_TYPE_CANCELED:
                break;
            default:
                if (target->stats->action.critical != 0) {
                    battle_sound_play_movement_sfx(unit, MAIN_SFX_CRITICAL_HIT);
                } else {
                    battle_sound_play_movement_sfx(unit, g_sound_weapon_hit_sfx_ids[weapon_type]);
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
static u8 battle_move_get_water_depth(battle_unit_t* unit) {
    map_tile_t* tile;
    s32 status;
    u32 flags;

    tile = battle_map_get_tile_data_pointer(
        unit->world_coords.vx / MAP_TILE_SIZE, unit->world_coords.vz / MAP_TILE_SIZE, unit->map_z);
    if (unit->stats != 0) {
        status = battle_move_get_effective_flags(unit->stats);
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
                g_battle_weapon_attack_animations[ITEM_TYPE_BOW][battle_effect_get_bow_shot_angle()], attacker->facing,
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
static void battle_unit_set_animation_for_item_abilities(battle_unit_t* unit, battle_unit_t* target) {
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
        water_depth = battle_move_get_water_depth(unit);
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
    switch (battle_move_get_water_depth(unit)) {
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
        water_depth = battle_move_get_water_depth(unit) & 0xFF;
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
    switch (battle_move_get_water_depth(unit)) {
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

    water_depth = battle_move_get_water_depth(unit);
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
            battle_effect_set_secondary_death_smoke(unit);
        }
        if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
            stats = unit->stats;
            if (stats != 0) {
                if (stats->unit_flags & UNIT_FLAG_MALE) {
                    battle_sound_play_movement_sfx(unit, MAIN_SFX_DEATH_MALE);
                } else if (stats->unit_flags & UNIT_FLAG_FEMALE) {
                    battle_sound_play_movement_sfx(unit, MAIN_SFX_DEATH_FEMALE);
                } else if (stats->unit_flags & UNIT_FLAG_MONSTER) {
                    battle_sound_play_movement_sfx(unit, MAIN_SFX_DEATH_MONSTER);
                }
            }
        }
    }
    if (unit->statuses_to_add_5_6 & (BATTLE_UNIT_STATUS_CRYSTAL | BATTLE_UNIT_STATUS_TREASURE)) {
        battle_effect_set_secondary_zodiac_poof(unit);
    }
    if (g_battle_game_state == BATTLE_GAME_STATE_EVENT) {
        if (unit->statuses_to_add_5_6 & BATTLE_UNIT_STATUS_JUMP) {
            battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_JUMP_DEPART, unit->facing, unit);
        } else if (unit->statuses_to_remove_5_6 & BATTLE_UNIT_STATUS_JUMP) {
            battle_unit_draw_and_activate_by_runtime_id(unit->runtime_id);
            battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_JUMP_LAND, unit->facing, unit);
        }
        if ((battle_move_get_water_depth(unit) & 0xff) >= 2) {
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
    } else if ((battle_move_get_water_depth(unit) & 0xff) < 2) {
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
    tile = battle_map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
    battle_gfx_check_tile_status_palette_mod(unit, tile->flags.shading, 1);
    /* The target reads the u16 facing field signed (lh). */
    battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_IDLE, unit->facing, unit);
    battle_unit_transform_into_morbol(unit->stats);
    battle_ai_init_unit_abilities(unit->stats->battle_id);
}

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
    battle_map_load_palette_data(&g_battle_gfx_weapon_clut[g_battle_gfx_item_graphic_data[item_id].palettes >> 4],
        MAP_CLUT_ROW_ITEM_A, clut_index, 0);
    battle_map_load_palette_data(&g_battle_gfx_weapon_clut[g_battle_gfx_item_graphic_data[item_id].palettes & 0xF],
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
static void battle_unit_set_attacker_animation_for_shield_block(battle_unit_t* attacker, battle_unit_t* target) {
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
    water_depth = battle_move_get_water_depth(target);
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
                battle_move_transfer_target_coordinates(attacker, target);
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_POACH) {
                target->status_flags_5_6 |= BATTLE_UNIT_STATUS_POACHED;
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_MORBOL) {
                target->status_flags_5_6 |= BATTLE_UNIT_STATUS_MORBOL;
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT) {
                battle_ai_init_unit_abilities(target->stats->battle_id);
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
                battle_ai_init_unit_abilities(target->stats->battle_id);
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
static battle_unit_t* battle_unit_claim_slot(s16 requested_runtime_id) {
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

static void battle_gfx_load_unit_frame_parts(
    battle_unit_t* unit, battle_gfx_sprite_display_data_t* display, battle_gfx_source_frame_t* src, s32 tpage) {
    battle_gfx_source_part_t* part;
    u32 i;
    u32 count;
    u8 flags;
    u16 attr;
    u16 size;
    u32 tile;
    s32 u;
    s32 v;
    s32 width;
    s32 height;

    flags = src->flags;
    count = display->part_count;
    display->tpage = (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK) | tpage;
    display->clut = unit->clut;
    for (i = 0; i < count; i++) {
        part = &src->parts[i];
        attr = part->attributes;
        size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
        width = g_battle_gfx_part_sizes[size].width;
        height = g_battle_gfx_part_sizes[size].height;
        tile = attr & BATTLE_SHP_PART_TILE_MASK;
        u = (tile & 0x1f) * 8;
        v = (tile >> 5) * 8;
        battle_gfx_store_sprite_display_data(display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8,
            (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13));
    }
}

/* Return the frame tables that a unit frame id indexes and the frame itself:
 * a battle_gfx_source_frame_t pointer.
 *
 * A frame below BATTLE_SHP_FRAME_EVTCHR_FIRST is one of the unit's own SHP.
 * Its submerged table replaces the plain one for a rider that is not
 * jumping, and for a unit on a water tile whose surface is 12 or more above
 * the unit's feet; from 36 above, the unit shows submerged frame 0 whatever
 * the frame id. A unit that floats or moves on water, a crystal and a
 * treasure keep the plain table, and a chicken or a frog changes at 4.
 * Higher frame ids index the SHP of an EVTCHR slot: slot 0 for the sequences
 * below 600 and slot 1 for the rest, the submerged table on a water tile. */
static void battle_gfx_select_unit_shp_frame(battle_unit_t* unit, battle_unit_anim_state_t* state, u16 frame,
    u16 sequence_id, battle_gfx_unit_shp_frame_tables_t** out_table, battle_gfx_source_frame_t** out_frame) {
    map_tile_t* tile;
    battle_gfx_unit_shp_frame_tables_t* table;
    battle_gfx_source_frame_t* selected;
    s32 flags;
    s32 height;
    s32 z;

    tile = battle_map_get_tile_data_pointer(
        (unit->world_coords.vx / MAP_TILE_SIZE), (unit->world_coords.vz / MAP_TILE_SIZE), unit->map_z);
    if (frame < BATTLE_SHP_FRAME_EVTCHR_FIRST) {
        table = state->shp;
        if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
            if (unit->status_flags_5_6 & (BATTLE_UNIT_STATUS_JUMP | BATTLE_UNIT_STATUS_JUMP_HEIGHT_ACTIVE)) {
                selected = table->frames[frame];
            } else {
                selected = table->submerged_frames[frame];
            }
        } else if (tile->depth_half_height.depth) {
            if (unit->stats != 0) {
                flags = battle_move_get_effective_flags(unit->stats);
                /* The combined test keeps the table pointer's reference count
                 * low enough for the target's s0/s1 allocation. */
                if (unit->status_flags_5_6 & BATTLE_UNIT_STATUS_FLOAT) {
                    selected = table->frames[frame];
                } else if ((flags & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER) || (flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT)) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_CRYSTAL) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_TREASURE) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_CHICKEN
                    || unit->spritesheet_id == BATTLE_SPRITESHEET_ID_FROG) {
                    if ((tile->height + tile->depth_half_height.depth) * 12 >= -unit->world_coords.vy + 4) {
                        selected = table->submerged_frames[frame];
                    } else {
                        selected = table->frames[frame];
                    }
                } else {
                    height = (tile->height + tile->depth_half_height.depth) * 12;
                    z = -unit->world_coords.vy;
                    if (height >= z + 0x24) {
                        selected = table->submerged_frames[0];
                    } else if (height >= z + 0xc) {
                        selected = table->submerged_frames[frame];
                    } else {
                        selected = table->frames[frame];
                    }
                }
            } else {
                height = (tile->height + tile->depth_half_height.depth) * 12;
                z = -unit->world_coords.vy;
                if (height >= z + 0x24) {
                    selected = table->submerged_frames[0];
                } else if (height >= z + 0xc) {
                    selected = table->submerged_frames[frame];
                } else {
                    selected = table->frames[frame];
                }
            }
        } else {
            selected = table->frames[frame];
        }
    } else {
        table = &g_battle_gfx_vram_slots[1].shp.tables;
        if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_1) {
            table = &g_battle_gfx_vram_slots[0].shp.tables;
        }
        if (tile->depth_half_height.depth) {
            selected = table->submerged_frames[frame - BATTLE_SHP_FRAME_EVTCHR_FIRST];
        } else {
            selected = table->frames[frame - BATTLE_SHP_FRAME_EVTCHR_FIRST];
        }
    }
    *out_table = table;
    *out_frame = selected;
}

/* Expand one SHP frame into the parts of a unit's sprite display.
 *
 * Where the frame's pictures are decides the texture page: the OTHER page
 * for the crystal, chicken, frog and treasure sheets; the unit's spritesheet
 * slot for frames below the SHP's first attack frame; above that, the EVTCHR
 * slot that holds the unit's attack image (no slot, no parts). Frames from
 * BATTLE_SHP_FRAME_EVTCHR_FIRST up belong to an EVTCHR slot's own SHP and
 * the sequence id says which slot.
 *
 * A 48x48 part from a spritesheet slot ends the frame: it is stored as two
 * 48x24 halves, the upper one flagged to draw nearer the camera. The two
 * Altima sheets keep theirs whole. */
void battle_gfx_load_trap_and_unit_frame_parts(
    battle_unit_t* unit, battle_unit_anim_state_t* state, u16 frame, u16 sequence_id) {
    battle_gfx_unit_shp_frame_tables_t* frame_table;
    battle_gfx_source_frame_t* frame_data;
    battle_gfx_sprite_display_data_t* display;
    battle_gfx_source_part_t* part;
    u32 y_offset;
    u32 count;
    u16 i;
    u8 flags;
    u16 attr;
    u16 size;
    s32 u;
    u32 tile_index;
    s16 part_flags;
    u32 row;

    y_offset = unit->special_graphic_y_offset;
    /* The unpackers never store the -1 tested here, and three bits
     * of part count cannot pass 8. */
    battle_gfx_select_unit_shp_frame(unit, state, frame, sequence_id, &frame_table, &frame_data);
    if (frame_data == (battle_gfx_source_frame_t*)-1) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_SHP_FRAME);
    }
    display = state->display;
    count = display->part_count = (frame_data->part_count_and_rotation & 7) + 1;
    display->rotation = g_battle_gfx_sprite_y_rotations[frame_data->part_count_and_rotation >> 3];
    if (count > 8) {
        count = 8;
        main_system_handle_error(MAIN_SYSTEM_ERROR_FRAME_PART_COUNT);
    }
    flags = frame_data->flags;
    if (unit->spritesheet_id >= BATTLE_SPRITESHEET_ID_CRYSTAL) {
        display->tpage = (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK) | BATTLE_GFX_TPAGE_OTHER;
        display->clut = unit->clut;
        for (i = 0; i < count; i++) {
            part = &frame_data->parts[i];
            attr = part->attributes;
            size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
            {
                u32 tile;
                s32 part_u;
                s16 part_v;
                s32 width;
                s32 height;
                width = g_battle_gfx_part_sizes[size].width;
                height = g_battle_gfx_part_sizes[size].height;
                tile = attr & BATTLE_SHP_PART_TILE_MASK;
                part_u = (tile & 0x1f) * 8;
                part_v = y_offset + (tile >> 5) * 8;
                battle_gfx_store_sprite_display_data(display, i, part->x_shift, part->y_shift, part_u, part_v,
                    width * 8, height * 8,
                    (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13));
            }
        }
    } else if (frame < (u32)state->shp->first_attack_frame) {
        display->tpage = unit->tpage | (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK);
        display->clut = unit->clut;
        i = 0;
        if (count != 0) {
            do {
                part = &frame_data->parts[i];
                attr = part->attributes;
                size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
                if (size == BATTLE_SHP_PART_SIZE_48X48) {
                    if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_ALTIMA_SECOND_FORM
                        || unit->spritesheet_id == BATTLE_SPRITESHEET_ID_ALTIMA_FIRST_FORM) {
                        u32 tile;
                        s32 part_u;
                        s16 part_v;
                        s32 width;
                        s32 height;
                        tile = attr & BATTLE_SHP_PART_TILE_MASK;
                        part_u = (tile & 0x1f) * 8;
                        part_v = y_offset + (tile >> 5) * 8;
                        width = g_battle_gfx_part_sizes[size].width;
                        height = g_battle_gfx_part_sizes[size].height;
                        battle_gfx_store_sprite_display_data(display, i, part->x_shift, part->y_shift, part_u, part_v,
                            width * 8, height * 8,
                            (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                                | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13));
                    } else {
                        tile_index = attr & BATTLE_SHP_PART_TILE_MASK;
                        u = (tile_index & 0x1f) * 8;
                        row = tile_index >> 5;
                        part_flags
                            = (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                            | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13);
                        battle_gfx_store_sprite_display_data(
                            display, i, part->x_shift, part->y_shift + 0x18, u, row * 8 + 0x18, 0x30, 0x18, part_flags);
                        battle_gfx_store_sprite_display_data(display, i + 1, part->x_shift, part->y_shift, u, row * 8,
                            0x30, 0x18, part_flags | BATTLE_SPRITE_DISPLAY_NEARER);
                        display->part_count++;
                        return;
                    }
                } else {
                    u32 tile;
                    s32 part_u;
                    s16 part_v;
                    s32 width;
                    s32 height;
                    width = g_battle_gfx_part_sizes[size].width;
                    height = g_battle_gfx_part_sizes[size].height;
                    tile = attr & BATTLE_SHP_PART_TILE_MASK;
                    part_u = (tile & 0x1f) * 8;
                    part_v = y_offset + (tile >> 5) * 8;
                    battle_gfx_store_sprite_display_data(display, i, part->x_shift, part->y_shift, part_u, part_v,
                        width * 8, height * 8,
                        (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                            | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13));
                }
            } while (++i < count);
        }
    } else if (frame < BATTLE_SHP_FRAME_EVTCHR_FIRST) {
        if ((g_battle_gfx_vram_slots[0].owner & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK) == unit->runtime_id) {
            battle_gfx_load_unit_frame_parts(unit, display, frame_data, BATTLE_GFX_TPAGE_EVTCHR_SLOT_0);
        } else if ((g_battle_gfx_vram_slots[1].owner & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK) == unit->runtime_id) {
            battle_gfx_load_unit_frame_parts(unit, display, frame_data, BATTLE_GFX_TPAGE_EVTCHR_SLOT_1);
        }
    } else if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_1) {
        battle_gfx_load_unit_frame_parts(unit, display, frame_data, BATTLE_GFX_TPAGE_EVTCHR_SLOT_0);
    } else {
        battle_gfx_load_unit_frame_parts(unit, display, frame_data, BATTLE_GFX_TPAGE_EVTCHR_SLOT_1);
    }
}

/* SEQ script step: moves the sprite offset by delta across the unit's facing: +x
 * facing north, -x facing south, +z facing west, -z facing east. A diagonal
 * facing matches no case. */
static void battle_unit_shift_right_or_left(battle_unit_t* unit, s8 delta) {
    s16* vector;
    switch (unit->facing & (BATTLE_FACING_EAST | BATTLE_FACING_NORTH)) {
    case BATTLE_FACING_NORTH:
        vector = &unit->seq_offset.vx;
        *vector = *vector + delta;
        return;
    case BATTLE_FACING_SOUTH:
        vector = &unit->seq_offset.vx;
        *vector = *vector - delta;
        return;
    case BATTLE_FACING_WEST:
        vector = &unit->seq_offset.vz;
        *vector = *vector + delta;
        return;
    case BATTLE_FACING_EAST:
        vector = &unit->seq_offset.vz;
        *vector = *vector - delta;
        return;
    }
}

/* SEQ script step: moves the sprite offset by delta along the height axis;
 * positive is down. */
static void battle_unit_shift_down_or_up(battle_unit_t* unit, s8 delta) {
    unit->seq_offset.vy += delta;
}

/* SEQ script step: moves the sprite offset by delta along the unit's facing
 * (north is +z, east is +x). A diagonal facing matches no case. */
static void battle_unit_shift_forward_or_backward(battle_unit_t* unit, s8 delta) {
    switch (unit->facing & (BATTLE_FACING_EAST | BATTLE_FACING_NORTH)) {
    case BATTLE_FACING_NORTH:
        unit->seq_offset.vz += delta;
        break;
    case BATTLE_FACING_SOUTH:
        unit->seq_offset.vz -= delta;
        break;
    case BATTLE_FACING_WEST:
        unit->seq_offset.vx -= delta;
        break;
    case BATTLE_FACING_EAST:
        unit->seq_offset.vx += delta;
        break;
    }
}

/* Run a unit's SEQ sequence up to its next frame instruction, or to its end.
 *
 * `state->sequence_id` selects the sequence: one of the unit's own SEQ table,
 * or sequence n of an EVTCHR slot for 500 + n and 600 + n. The commands in
 * front of the frame instruction run at once (battle_seq_opcode_e); then the
 * frame's parts are expanded into the unit's display and the state keeps the
 * frame, its wait and the position after it. With `advance` clear the
 * position first steps back to the frame instruction read last, which shows
 * that frame again without restarting its wait.
 *
 * A sequence that ends leaves a unit on its SHP type's
 * g_battle_gfx_targeted_frame_front or _back frame with a wait of 0. The
 * unpackers resolve a missing sequence to the start of the sequence data,
 * never to -1, so the MAIN_SYSTEM_ERROR_SEQ_ANIMATION test cannot fire. */
static void battle_gfx_run_unit_seq_script(battle_unit_t* unit, battle_unit_anim_state_t* state, s32 advance) {
    battle_gfx_unit_seq_table_t* seq;
    battle_unit_sprite_block_t* block;
    battle_unit_t* target;
    u16 sequence_id;
    u16 index;
    u16 frame;
    u16 i;
    s16 pos;
    s16 attack_id;
    u16 value;
    s16 offset;
    u8 block_id;
    u8 block_sequence;
    u8 timer;

    sequence_id = state->sequence_id;
    pos = state->script_pos;
    index = sequence_id;
    if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_0) {
        seq = state->seq;
    } else if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_1) {
        index = sequence_id - BATTLE_SEQUENCE_EVTCHR_SLOT_0;
        seq = &g_battle_gfx_vram_slots[0].seq.header;
    } else {
        seq = &g_battle_gfx_vram_slots[1].seq.header;
        index = sequence_id - BATTLE_SEQUENCE_EVTCHR_SLOT_1;
    }
    if (!advance) {
        pos -= 2;
    }
    if (seq->sequences[index] == (u8*)-1) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_ANIMATION);
    }
    for (;;) {
        u8 op;

        if (seq->sequences[index][pos] == BATTLE_SEQ_OP_PREFIX) {
            pos++;
            op = seq->sequences[index][pos++];
            switch (op) {
            case BATTLE_SEQ_OP_FLIP_HORIZONTAL:
                state->flags ^= BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP;
                break;
            case BATTLE_SEQ_OP_FLIP_VERTICAL:
                state->flags ^= BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP;
                break;
            case BATTLE_SEQ_OP_SHOW_SHADOW:
                unit->shadow_graphic_trigger = 1;
                break;
            case BATTLE_SEQ_OP_HIDE_SHADOW:
                unit->shadow_graphic_trigger = 0;
                break;
            case BATTLE_SEQ_OP_END_ANIMATION:
            case BATTLE_SEQ_OP_PAUSE_ANIMATION:
                if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_0) {
                    if (sequence_id % 2) {
                        frame = g_battle_gfx_targeted_frame_back[g_battle_gfx_spritesheet_data[unit->spritesheet_id]
                                .shp_id];
                    } else {
                        frame = g_battle_gfx_targeted_frame_front[g_battle_gfx_spritesheet_data[unit->spritesheet_id]
                                .shp_id];
                    }
                    unit->seq_offset.vz = 0;
                    unit->seq_offset.vy = 0;
                    unit->seq_offset.vx = 0;
                    battle_gfx_load_trap_and_unit_frame_parts(unit, state, frame, sequence_id);
                }
                state->wait = 0;
                return;
            case BATTLE_SEQ_OP_RESTART:
                pos = 0;
                state->restart_count++;
                break;
            case BATTLE_SEQ_OP_CLEAR_ROTATION:
                state->display->rotation = 0;
                break;
            case BATTLE_SEQ_OP_STRIKE_TARGETS:
                if (g_battle_game_state == BATTLE_GAME_STATE_EVENT) {
                    break;
                }
                /* The abilities whose hit is the weapon swing itself: a plain
                 * attack (0), the frog's, the Battle Skill breaks, the Snipe
                 * shots, Jump and Charge. */
                attack_id = unit->strike.last_attack_id;
                if (attack_id == 0 || attack_id == ABILITY_ID_FROG_ATTACK
                    || (attack_id >= ABILITY_ID_BATTLE_SKILL_HEAD_BREAK
                        && attack_id <= ABILITY_ID_BATTLE_SKILL_MIND_BREAK)
                    || (attack_id >= ABILITY_ID_SNIPE_LEG_AIM && attack_id <= ABILITY_ID_SNIPE_SEAL_EVIL)
                    || (attack_id >= ABILITY_ID_JUMP_FIRST && attack_id < ABILITY_ID_MATH_FIRST)) {
                    if (unit->strike.target_count != 0) {
                        for (i = 0; i < unit->strike.target_count; i++) {
                            target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
                            if (target != 0) {
                                battle_unit_set_target_animation_from_attack_type(unit, target);
                                battle_unit_set_attacker_animation_for_shield_block(unit, target);
                                battle_gfx_prepare_post_action_display_by_runtime_id(target->runtime_id);
                                battle_effect_set_evade_type_data_and_weapon_element(unit, 0);
                            }
                        }
                    }
                }
                break;
            case BATTLE_SEQ_OP_LAUNCH_PROJECTILE:
                battle_effect_start_action_projectile_secondary(unit);
            case BATTLE_SEQ_OP_UNKNOWN_C7:
            case BATTLE_SEQ_OP_UNKNOWN_C8:
            case BATTLE_SEQ_OP_UNKNOWN_C9:
            case BATTLE_SEQ_OP_UNKNOWN_E6:
            case BATTLE_SEQ_OP_UNKNOWN_E7:
            case BATTLE_SEQ_OP_UNKNOWN_E8:
            case BATTLE_SEQ_OP_UNKNOWN_EA:
            case BATTLE_SEQ_OP_UNKNOWN_F1:
            case BATTLE_SEQ_OP_UNKNOWN_F8:
            case BATTLE_SEQ_OP_UNKNOWN_FB:
                pos += 2;
                break;
            case BATTLE_SEQ_OP_RETURN:
                sequence_id = state->saved_sequence_id;
                pos = state->saved_script_pos;
                index = sequence_id;
                if (sequence_id >= BATTLE_SEQUENCE_EVTCHR_SLOT_0) {
                    if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_1) {
                        index = sequence_id - BATTLE_SEQUENCE_EVTCHR_SLOT_0;
                    } else {
                        index = sequence_id - BATTLE_SEQUENCE_EVTCHR_SLOT_1;
                    }
                }
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_EFFECT:
                if (g_battle_state_animation_continue_check != 0) {
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_PLAY_SOUND:
                value = seq->sequences[index][pos++];
                battle_sound_play_movement_sfx(unit, value);
                break;
            case BATTLE_SEQ_OP_MOVE_UNIT_RL:
                battle_unit_shift_right_or_left(unit, seq->sequences[index][pos++]);
                break;
            case BATTLE_SEQ_OP_MOVE_UNIT_DU:
                battle_unit_shift_down_or_up(unit, seq->sequences[index][pos++]);
                break;
            case BATTLE_SEQ_OP_MOVE_UNIT_FB:
                battle_unit_shift_forward_or_backward(unit, seq->sequences[index][pos++]);
                break;
            case BATTLE_SEQ_OP_SET_WAIT_BIAS:
                state->wait_bias = (s8)seq->sequences[index][pos++];
                break;
            case BATTLE_SEQ_OP_SET_FRAME_BIAS:
                state->frame_bias = (s8)seq->sequences[index][pos++];
                break;
            case BATTLE_SEQ_OP_CALL_SEQUENCE:
                state->saved_sequence_id = sequence_id;
                sequence_id = seq->sequences[index][pos++];
                index = sequence_id;
                if (sequence_id >= BATTLE_SEQUENCE_EVTCHR_SLOT_0) {
                    if (sequence_id < BATTLE_SEQUENCE_EVTCHR_SLOT_1) {
                        index = sequence_id - BATTLE_SEQUENCE_EVTCHR_SLOT_0;
                    } else {
                        index = sequence_id - BATTLE_SEQUENCE_EVTCHR_SLOT_1;
                    }
                }
                state->saved_script_pos = pos;
                pos = 0;
                break;
            case BATTLE_SEQ_OP_SET_LAYER_PRIORITY:
                unit->layer_priority = seq->sequences[index][pos++];
                break;
            case BATTLE_SEQ_OP_START_BLOCK_SEQUENCE:
                block_id = seq->sequences[index][pos++];
                block_sequence = seq->sequences[index][pos++];
                if (block_id == 0) {
                    main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_OPCODE);
                }
                block = &unit->sprite_blocks[block_id - 1];
                block->wait = 1;
                block->sequence_id = block_sequence;
                block->script_pos = 0;
                block->wait_bias = 0;
                block->frame_bias = 0;
                block->repeat_count = 0;
                block->trigger = 1;
                break;
            case BATTLE_SEQ_OP_REPEAT:
                offset = (s8)seq->sequences[index][pos++];
                value = seq->sequences[index][pos++];
                if (state->repeat_count != 0) {
                    if (--state->repeat_count != 0) {
                        pos += offset - 4;
                    }
                } else {
                    pos += offset - 4;
                    state->repeat_count = value;
                }
                break;
            case BATTLE_SEQ_OP_MOVE_SCREEN_OFFSET:
                unit->screen_offset.vx += (s8)seq->sequences[index][pos++];
                unit->screen_offset.vy += (s8)seq->sequences[index][pos++];
                break;
            case BATTLE_SEQ_OP_SET_ROTATION:
                value = seq->sequences[index][pos++];
                value |= seq->sequences[index][pos++] << 8;
                state->display->rotation = value;

                break;
            case BATTLE_SEQ_OP_MOVE_UNIT:
                battle_unit_shift_right_or_left(unit, seq->sequences[index][pos++]);
                battle_unit_shift_down_or_up(unit, seq->sequences[index][pos++]);
                battle_unit_shift_forward_or_backward(unit, seq->sequences[index][pos++]);
                break;
            case BATTLE_SEQ_OP_UNKNOWN_F7:
                pos += 3;
                break;
            case BATTLE_SEQ_OP_MOVE_FORWARD_1:
                battle_unit_shift_forward_or_backward(unit, 1);
                break;
            case BATTLE_SEQ_OP_MOVE_FORWARD_2:
                battle_unit_shift_forward_or_backward(unit, 2);
                break;
            case BATTLE_SEQ_OP_MOVE_DOWN_1:
                battle_unit_shift_down_or_up(unit, 1);
                break;
            case BATTLE_SEQ_OP_MOVE_DOWN_2:
                battle_unit_shift_down_or_up(unit, 2);
                break;
            case BATTLE_SEQ_OP_MOVE_BACKWARD_1:
                battle_unit_shift_forward_or_backward(unit, -1);
                break;
            case BATTLE_SEQ_OP_MOVE_BACKWARD_2:
                battle_unit_shift_forward_or_backward(unit, -2);
                break;
            case BATTLE_SEQ_OP_MOVE_UP_1:
                battle_unit_shift_down_or_up(unit, -1);
                break;
            case BATTLE_SEQ_OP_MOVE_UP_2:
                battle_unit_shift_down_or_up(unit, -2);
                break;
            case BATTLE_SEQ_OP_SHOW_ITEM:
                unit->item_display_active = 1;
                break;
            case BATTLE_SEQ_OP_HIDE_ITEM:
                unit->item_display_active = 0;
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_INPUT:
                if (g_action_post_action == 0) {
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_PLAY_WEAPON_SOUND:
                battle_sound_play_weapon_sfx(unit, seq->sequences[index][pos++]);
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_EFFECT_LOAD:
                if (g_battle_state_animation_continue_check == 1) {
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_SET_ITEM_POSITION:
                unit->item_get_x_offset = seq->sequences[index][pos++];
                unit->item_get_y_offset = seq->sequences[index][pos++];
                break;
            case BATTLE_SEQ_OP_START_DISTORTION:
                unit->distortion_animation_id = seq->sequences[index][pos++] + 2;
                timer = seq->sequences[index][pos++];
                unit->distortion_phase = 0;
                unit->distortion_timer = timer;
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_DISTORTION:
                if (unit->distortion_animation_id != BATTLE_DISTORTION_ANIMATION_NONE) {
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_JUMP:
                offset = (s8)seq->sequences[index][pos];
                pos += offset - 2;
                break;
            case BATTLE_SEQ_OP_START_POSITION_COPIES:
                battle_gfx_init_position_vector_copies(unit);
                break;
            case BATTLE_SEQ_OP_STOP_POSITION_COPIES:
                unit->position_copies_active = 0;
                break;
            case BATTLE_SEQ_OP_NOP:
                break;
            case BATTLE_SEQ_OP_UNKNOWN_CA:
            case BATTLE_SEQ_OP_UNKNOWN_D7:
            case BATTLE_SEQ_OP_UNKNOWN_E3:
            case BATTLE_SEQ_OP_UNKNOWN_E4:
            case BATTLE_SEQ_OP_UNKNOWN_E9:
            case BATTLE_SEQ_OP_UNKNOWN_ED:
            case BATTLE_SEQ_OP_UNKNOWN_F3:
            case BATTLE_SEQ_OP_UNKNOWN_F4:
            case BATTLE_SEQ_OP_UNKNOWN_F5:
                pos++;
                break;
            default:
                main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_OPCODE);
                break;
            }
        } else {
            frame = state->frame_bias + seq->sequences[index][pos++];
            if (advance) {
                state->wait = seq->sequences[index][pos++] + state->wait_bias;
                /* A bias below the wait wraps the 16-bit value: wait one frame. */
                if (state->wait > 0x100) {
                    state->wait = 1;
                }
            } else {
                pos++;
            }
            battle_gfx_load_trap_and_unit_frame_parts(unit, state, frame, sequence_id);
            state->script_pos = pos;
            state->frame = frame;
            state->sequence_id = sequence_id;
            return;
        }
    }
}

/* Rebuilds the casting unit's sprite parts for its current animation and frame. */
static void battle_gfx_load_casting_unit_graphics(void) {
    battle_unit_t* unit;

    unit = battle_unit_get_casting();
    if (unit != 0) {
        /* The sprite data block starts at the graphic trigger; 0x1e0 is the
         * current frame. */
        battle_gfx_load_trap_and_unit_frame_parts(
            unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
    }
}

/* Run a weapon or effect sprite block's sequence up to its next frame
 * instruction, or to its end, as battle_gfx_run_unit_seq_script does for the
 * unit's own sprite. The unit-only commands are errors here, the ones that
 * move a unit only skip their operands, and BATTLE_SEQ_OP_STRIKE_TARGETS and
 * the two shadow commands do nothing.
 *
 * A frame id counts from the first frame of the unit's weapon type
 * (battle_gfx_weapon_shp_t.first_frame). The frame's parts, three at most,
 * are expanded into the block's display: a weapon block draws them from the
 * weapon sheet, at the row of the used item's graphic, and an effect block
 * from the effect sheet. `index` is the block's number; it selects the CLUT
 * row.
 *
 * Matching constraints: the size table is indexed by element (`>> 10`); the
 * shift that combine later folds into `>> 7` keeps the script loop just large
 * enough that loop.c does not hoist the constant 1. `part` and `attr` are
 * function-scope (global allocation) while the other part temporaries are
 * block-local and `mode` is single-set, which gives the target's register
 * ties and schedule. */
static void battle_gfx_run_wep_eff_seq_script(
    battle_unit_t* unit, battle_unit_sprite_block_t* sprite, s32 index, s32 advance) {
    battle_gfx_weapon_shp_t* shp;
    u8** seq;
    u8 op;
    u8* data;
    u16 sequence_id;
    s16 pos;
    u16 frame;
    u16 graphic_v;
    battle_gfx_source_frame_t* src;
    battle_gfx_sprite_display_data_t* display;
    battle_unit_sprite_block_t* block;
    battle_gfx_source_part_t* part;
    s16 offset;
    u16 value;
    u8 block_id;
    u8 block_sequence;
    u32 parts;
    u8 flags;
    u16 i;
    u16 attr;

    sequence_id = sprite->sequence_id;
    graphic_v = g_battle_gfx_item_graphic_data[unit->used_item_or_weapon_id].texture_row * 8;
    pos = sprite->script_pos;
    shp = sprite->shp;
    seq = sprite->seq;
    if (advance == 0) {
        pos -= 2;
    }
    if (sequence_id > 0x100) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_ANIMATION);
    }
    if (seq[sequence_id] == (u8*)-1) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_ANIMATION);
    }
    for (;;) {
        data = seq[sequence_id];
        if (data[pos] == BATTLE_SEQ_OP_PREFIX) {
            pos++;
            op = seq[sequence_id][pos++];
            switch (op) {
            case BATTLE_SEQ_OP_FLIP_HORIZONTAL:
                sprite->flags ^= BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP;
                break;
            case BATTLE_SEQ_OP_FLIP_VERTICAL:
                sprite->flags ^= BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP;
                break;
            case BATTLE_SEQ_OP_END_ANIMATION:
                sprite->trigger = 0;
            case BATTLE_SEQ_OP_PAUSE_ANIMATION:
                sprite->wait = 0;
                return;
            case BATTLE_SEQ_OP_RESTART:
                sprite->restart_count++;
                pos = 0;
                break;
            case BATTLE_SEQ_OP_CLEAR_ROTATION:
                sprite->display->rotation = 0;
                break;
            case BATTLE_SEQ_OP_LAUNCH_PROJECTILE:
                battle_effect_start_action_projectile_secondary(unit);
            case BATTLE_SEQ_OP_UNKNOWN_E6:
            case BATTLE_SEQ_OP_UNKNOWN_E7:
            case BATTLE_SEQ_OP_UNKNOWN_E8:
            case BATTLE_SEQ_OP_UNKNOWN_EA:
            case BATTLE_SEQ_OP_UNKNOWN_F1:
            case BATTLE_SEQ_OP_UNKNOWN_F8:
            case BATTLE_SEQ_OP_UNKNOWN_FB:
                pos += 2;
                break;
            case BATTLE_SEQ_OP_RETURN:
                pos = sprite->saved_script_pos;
                sequence_id = sprite->saved_sequence_id;
                break;
            case BATTLE_SEQ_OP_SET_LAYER_PRIORITY:
                unit->layer_priority = seq[sequence_id][pos++];
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_EFFECT:
                if (g_battle_state_animation_continue_check != 0) {
                    offset = (s8)seq[sequence_id][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_PLAY_SOUND:
                value = seq[sequence_id][pos++];
                battle_sound_play_movement_sfx(unit, value);
                break;
            case BATTLE_SEQ_OP_SET_WAIT_BIAS:
                sprite->wait_bias = (s8)seq[sequence_id][pos++];
                break;
            case BATTLE_SEQ_OP_SET_FRAME_BIAS:
                sprite->frame_bias = (s8)seq[sequence_id][pos++];
                break;
            case BATTLE_SEQ_OP_CALL_SEQUENCE:
                sprite->saved_sequence_id = sequence_id;
                sequence_id = seq[sequence_id][pos++];
                sprite->saved_script_pos = pos;
                pos = 0;
                break;
            case BATTLE_SEQ_OP_MOVE_SCREEN_OFFSET:
                unit->screen_offset.vx += (s8)seq[sequence_id][pos++];
                unit->screen_offset.vy += (s8)seq[sequence_id][pos++];
                break;
            case BATTLE_SEQ_OP_REPEAT:
                offset = (s8)seq[sequence_id][pos++];
                value = seq[sequence_id][pos++];
                if (sprite->repeat_count != 0) {
                    if (--sprite->repeat_count != 0) {
                        pos += offset - 4;
                    }
                } else {
                    pos += offset - 4;
                    sprite->repeat_count = value;
                }
                break;
            case BATTLE_SEQ_OP_START_BLOCK_SEQUENCE:
                block_id = seq[sequence_id][pos++];
                block_sequence = seq[sequence_id][pos++];
                if (block_id == 0) {
                    main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_OPCODE);
                }
                block = &unit->sprite_blocks[block_id - 1];
                block->sequence_id = block_sequence;
                block->script_pos = 0;
                block->wait = 1;
                block->wait_bias = 0;
                block->frame_bias = 0;
                block->repeat_count = 0;
                block->trigger = 1;
                break;
            case BATTLE_SEQ_OP_SET_ROTATION:
                value = seq[sequence_id][pos++];
                value |= seq[sequence_id][pos++] << 8;
                sprite->display->rotation = value;

                break;
            case BATTLE_SEQ_OP_UNKNOWN_F7:
            case BATTLE_SEQ_OP_MOVE_UNIT:
                pos += 3;
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_EFFECT_LOAD:
                if (g_battle_state_animation_continue_check == 1) {
                    offset = (s8)seq[sequence_id][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_JUMP:
                offset = (s8)seq[sequence_id][pos];
                pos += offset - 2;
                break;
            case BATTLE_SEQ_OP_UNKNOWN_D7:
            case BATTLE_SEQ_OP_UNKNOWN_E3:
            case BATTLE_SEQ_OP_UNKNOWN_E4:
            case BATTLE_SEQ_OP_UNKNOWN_E9:
            case BATTLE_SEQ_OP_UNKNOWN_ED:
            case BATTLE_SEQ_OP_MOVE_UNIT_FB:
            case BATTLE_SEQ_OP_MOVE_UNIT_DU:
            case BATTLE_SEQ_OP_MOVE_UNIT_RL:
            case BATTLE_SEQ_OP_UNKNOWN_F3:
            case BATTLE_SEQ_OP_UNKNOWN_F4:
            case BATTLE_SEQ_OP_UNKNOWN_F5:
                pos++;
                break;
            case BATTLE_SEQ_OP_STRIKE_TARGETS:
            case BATTLE_SEQ_OP_HIDE_SHADOW:
            case BATTLE_SEQ_OP_SHOW_SHADOW:
                break;
            default:
                main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_OPCODE);
                break;
            }
        } else {
            frame = sprite->frame_bias + (data[pos++] + shp->first_frame[unit->equipped_weapon_type]);
            if (advance != 0) {
                sprite->wait = data[pos++] + sprite->wait_bias;
            } else {
                pos++;
            }
            src = shp->frames[frame];
            if (frame > 0x200) {
                main_system_handle_error(MAIN_SYSTEM_ERROR_SHP_FRAME);
            }
            if (src == (battle_gfx_source_frame_t*)-1) {
                main_system_handle_error(MAIN_SYSTEM_ERROR_SHP_FRAME);
            }
            display = sprite->display;
            parts = display->part_count = (src->part_count_and_rotation & 7) + 1;
            display->rotation = g_battle_gfx_sprite_y_rotations[src->part_count_and_rotation >> 3];
            if (unit->sprite_display_flags & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) {
                display->rotation = -display->rotation;
            }
            if (parts > 3) {
                parts = 3;
            }
            flags = src->flags;
            switch (sprite->graphic_type) {
            case BATTLE_SPRITE_GRAPHIC_WEAPON:
                display->tpage = (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK) | BATTLE_GFX_TPAGE_WEAPON;
                display->clut = unit->clut + index * 64 + 0x80; /* MAP_CLUT_ROW_ITEM_A or _B */
                for (i = 0; i < parts; i++) {
                    u32 size;
                    u32 tile;
                    s32 width;
                    s32 height;
                    s32 u;
                    s32 v;
                    u32 mode;

                    part = &src->parts[i];
                    attr = part->attributes;
                    size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
                    width = g_battle_gfx_part_sizes[size].width;
                    height = g_battle_gfx_part_sizes[size].height;
                    tile = attr & BATTLE_SHP_PART_TILE_MASK;
                    u = (tile & 0x1f) * 8;
                    v = (tile >> 5) * 8 + graphic_v;
                    mode = (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13);
                    battle_gfx_store_sprite_display_data(
                        display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8, mode);
                }
                break;
            case BATTLE_SPRITE_GRAPHIC_EFFECT:
                display->tpage = (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK) | BATTLE_GFX_TPAGE_EFFECT;
                display->clut = unit->clut + index * 64 + 0x80; /* MAP_CLUT_ROW_ITEM_A or _B */
                for (i = 0; i < parts; i++) {
                    u32 size;
                    u32 tile;
                    s32 width;
                    s32 height;
                    s32 u;
                    s32 v;
                    u32 mode;

                    part = &src->parts[i];
                    attr = part->attributes;
                    size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
                    width = g_battle_gfx_part_sizes[size].width;
                    height = g_battle_gfx_part_sizes[size].height;
                    tile = attr & BATTLE_SHP_PART_TILE_MASK;
                    u = (tile & 0x1f) * 8;
                    v = (tile >> 5) * 8;
                    mode = (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13);
                    battle_gfx_store_sprite_display_data(
                        display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8, mode);
                }
                break;
            }
            sprite->script_pos = pos;
            sprite->frame = frame;
            sprite->sequence_id = sequence_id;
            return;
        }
    }
}

/* Capture a unit's graphics into an EVTCHR VRAM cache slot.
 *
 * The unit's compressed spritesheet slot is expanded into `image_slot`'s
 * image buffer and uploaded to `slot`'s rectangle; the unit's 64x32 VRAM
 * block (column slot >> 3, row slot & 7 of the 0x340,0x100 spritesheet area)
 * is copied 200 lines below it. `slot` is then owned by the unit (id | 0x80). */
static void battle_gfx_load_unit_into_evtchr_slot(battle_unit_t* unit, s32 slot, s32 image_slot) {
    RECT rect;

    battle_gfx_decompress_attack_spritesheet(
        g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].compressed_attack_image,
        g_battle_gfx_vram_slots[image_slot].image_data);
    LoadImage(&g_battle_gfx_vram_slots[slot].image_rect, (u32*)g_battle_gfx_vram_slots[image_slot].image_data);
    rect.x = (unit->spritesheet_vram_slot >> 3) * 64 + 0x340;
    rect.y = (unit->spritesheet_vram_slot & 7) * 32 + 0x100;
    rect.w = 0x40;
    rect.h = 0x20;
    MoveImage(&rect, g_battle_gfx_vram_slots[slot].image_rect.x, g_battle_gfx_vram_slots[slot].image_rect.y + 200);
    g_battle_gfx_vram_slots[slot].owner = unit->runtime_id | BATTLE_GFX_VRAM_SLOT_OWNER_ATTACK_IMAGE;
    g_battle_frame_pacing_suppressed = 1;
    g_battle_gfx_vram_slots[image_slot].image_buffer_state = BATTLE_GFX_VRAM_SLOT_UPLOAD_FRAMES;
    g_battle_gfx_vram_slots[slot].evtchr_load_state = 1;
}

/* Free the EVTCHR slots whose page holds the unit's attack image. One that
 * holds its SP2 image stays. */
static void battle_gfx_release_unit_vram_slot(battle_unit_t* unit) {
    s32 i;
    s32 slot_id;

    for (i = 0; i < 2; i++) {
        slot_id = g_battle_gfx_vram_slots[i].owner;
        if (slot_id != BATTLE_GFX_VRAM_SLOT_OWNER_EVTCHR && slot_id != BATTLE_GFX_VRAM_SLOT_OWNER_NONE
            && (slot_id & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK) == unit->runtime_id
            && (slot_id & BATTLE_GFX_VRAM_SLOT_OWNER_SP2) == 0) {
            g_battle_gfx_vram_slots[i].owner = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
            g_battle_gfx_vram_slots[i].evtchr_load_state = 0;
        }
    }
}

/* Update one unit's sprite animation for the frame.
 *
 * The unit's facing relative to the camera yaw gives the quadrant and the
 * sixteenth of a turn it is seen from. A request for
 * BATTLE_UNIT_ANIMATION_STAND shows the standing sequence of that sixteenth.
 * Any other request becomes sequence 2 * animation, + 1 seen from behind
 * (a request from 500 up is the sequence id itself). Animations below 11
 * start only on the unit's frame in four unless forced, and a request for
 * the sequence already playing is dropped. A sequence of the SEQ file's attack
 * set needs the unit's attack image in an EVTCHR slot first: a free slot and
 * a free image buffer or, failing that, slot 1 taken from the unit that
 * holds it, which is left on its rest frame. Without a request, a change of
 * sixteenth turns the standing pose or swaps the running sequence for its
 * other side; otherwise the sequence and the three weapon/effect blocks
 * count their waits down by g_main_system_animation_speed.
 *
 * unit is initialized in its declaration so its copy precedes the table
 * copies, and the unread unused_10 sizes the target's frame. */
static void battle_gfx_update_and_animate_unit_wep_eff(battle_unit_t* unit_data) {
    battle_unit_t* unit = unit_data;
    s16 unused_10[4]; /* Unread; occupies sp+0x10 like the target. */
    /* The flip tables hold BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP for the views drawn mirrored. */
    s16 flip_by_quadrant[4] = { 0, 0, 2, 2 };
    s16 back_by_quadrant[4] = { 0, 1, 1, 0 };
    s16 flip_by_direction[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 0 };
    s16 stand_by_direction[16] = { 1, 2, 2, 3, 3, 4, 4, 5, 5, 4, 4, 3, 3, 2, 2, 1 };
    battle_unit_t* other;
    battle_gfx_unit_seq_table_t* seq;
    s32 direction;
    s16 quadrant;
    s16 sixteenth;
    s32 animation;
    s32 index;
    s32 ok;
    u16 sequence_id;
    s32 i;
    s32 j;

    if (unit->draw_enabled == 0) {
        return;
    }
    if (unit->position_copies_active != 0) {
        battle_gfx_shift_and_fill_position_copies(unit);
    }
    direction = (g_camera_render_state.vy + unit->facing) & 0xfff;
    quadrant = direction / 1024;
    sixteenth = direction / 256;
    if (unit->requested_animation != 0) {
        if (unit->requested_animation - 1 == BATTLE_UNIT_ANIMATION_STAND) {
            for (i = 0; i < 3; i++) {
                unit->sprite_blocks[i].trigger = 0;
            }
            unit->animation.sequence_id = stand_by_direction[sixteenth];
            unit->sprite_display_flags
                = (unit->sprite_display_flags
                      & ~(BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP | BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP))
                | flip_by_direction[sixteenth];
            unit->camera_facing_quadrant = quadrant;
            unit->camera_facing_sixteenth = sixteenth;
            unit->animation.script_pos = 0;
            unit->animation.wait = 0;
            unit->animation.restart_count = 0;
            unit->seq_offset.vz = 0;
            unit->seq_offset.vy = 0;
            unit->seq_offset.vx = 0;
            unit->screen_offset.vz = 0;
            unit->screen_offset.vy = 0;
            unit->screen_offset.vx = 0;
            unit->animation.wait_bias = 0;
            unit->animation.frame_bias = 0;
            unit->animation.repeat_count = 0;
            unit->animation.flags = 0;
            battle_gfx_run_unit_seq_script(unit, &unit->animation, 1);
            unit->requested_animation = 0;
            battle_gfx_release_unit_vram_slot(unit);
        } else if (unit->requested_animation >= 12 || (unit->runtime_id & 3) == g_frame_counter % 4
            || (unit->movement.force_animation_update)) {
            /* An animation below 11 waits for the unit's frame in four. */
            ok = 1;
            if ((unit->requested_animation - 1) * 2 + back_by_quadrant[quadrant] != unit->animation.sequence_id
                || unit->animation.wait == 0 || (unit->movement.force_animation_update)) {
                animation = unit->requested_animation - 1;
                if (animation < BATTLE_SEQUENCE_EVTCHR_SLOT_0) {
                    seq = unit->animation.seq;
                    index = animation * 2 + back_by_quadrant[quadrant];
                    if (index < (u32)seq->first_sp2_sequence) {
                        if (index >= (u32)seq->first_attack_sequence) {
                            for (i = 0; i < 2; i++) {
                                if ((g_battle_gfx_vram_slots[i].owner & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK)
                                    == unit->runtime_id) {
                                    break;
                                }
                            }
                            if (i == 2) {
                                for (i = 0; i < 2; i++) {
                                    if (g_battle_gfx_vram_slots[i].owner == BATTLE_GFX_VRAM_SLOT_OWNER_NONE) {
                                        for (j = 0; j < 2; j++) {
                                            if (g_battle_gfx_vram_slots[j].image_buffer_state
                                                == BATTLE_GFX_VRAM_SLOT_BUFFER_FREE) {
                                                battle_gfx_load_unit_into_evtchr_slot(unit, i, j);
                                                break;
                                            }
                                        }
                                        break;
                                    }
                                }
                                if (i == 2) {
                                    ok = 0;
                                }
                                if (j == 2) {
                                    ok = 0;
                                }
                                if (ok == 0) {
                                    if (g_battle_gfx_vram_slots[1].owner - BATTLE_GFX_VRAM_SLOT_OWNER_EVTCHR >= 2) {
                                        other = battle_unit_get_by_runtime_id(
                                            g_battle_gfx_vram_slots[1].owner & BATTLE_GFX_VRAM_SLOT_OWNER_ID_MASK);
                                        if (other != 0) {
                                            if (other->animation.sequence_id & 1) {
                                                other->animation.frame = g_battle_gfx_targeted_frame_back
                                                    [g_battle_gfx_spritesheet_data[other->spritesheet_id].shp_id];
                                            } else {
                                                other->animation.frame = g_battle_gfx_targeted_frame_front
                                                    [g_battle_gfx_spritesheet_data[other->spritesheet_id].shp_id];
                                            }
                                            other->seq_offset.vz = 0;
                                            other->seq_offset.vy = 0;
                                            other->seq_offset.vx = 0;
                                            battle_gfx_load_trap_and_unit_frame_parts(other, &other->animation,
                                                other->animation.frame, other->animation.sequence_id);
                                            ok = 1;
                                            unit->animation.wait = 0;
                                            battle_gfx_load_unit_into_evtchr_slot(unit, 1, 1);
                                        }
                                    }
                                }
                            }
                        } else {
                            battle_gfx_release_unit_vram_slot(unit);
                        }
                    }
                }
                if (ok != 0) {
                    for (i = 0; i < 3; i++) {
                        unit->sprite_blocks[i].trigger = 0;
                    }
                    animation = unit->requested_animation - 1;
                    if (animation >= BATTLE_SEQUENCE_EVTCHR_SLOT_0) {
                        unit->animation.sequence_id = unit->requested_animation - 1;
                    } else {
                        unit->animation.sequence_id = animation * 2 + back_by_quadrant[quadrant];
                    }
                    unit->sprite_display_flags
                        = (unit->sprite_display_flags
                              & ~(BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP | BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP))
                        | flip_by_quadrant[quadrant];
                    unit->camera_facing_quadrant = quadrant;
                    unit->camera_facing_sixteenth = sixteenth;
                    unit->animation.script_pos = 0;
                    unit->animation.wait = 0;
                    unit->animation.restart_count = 0;
                    unit->seq_offset.vz = 0;
                    unit->seq_offset.vy = 0;
                    unit->seq_offset.vx = 0;
                    unit->screen_offset.vz = 0;
                    unit->screen_offset.vy = 0;
                    unit->screen_offset.vx = 0;
                    unit->animation.wait_bias = 0;
                    unit->animation.frame_bias = 0;
                    unit->animation.repeat_count = 0;
                    unit->animation.flags = 0;
                    battle_gfx_run_unit_seq_script(unit, &unit->animation, 1);
                    unit->movement.force_animation_update = 0;
                    unit->requested_animation = 0;
                    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
                        other = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
                        if (other != 0) {
                            other->animation.script_pos = 0;
                            other->animation.wait = 0;
                            battle_gfx_run_unit_seq_script(other, &other->animation, 1);
                        }
                    }
                } else if (unit->animation.wait != 0) {
                    for (i = 0; i < g_main_system_animation_speed; i++) {
                        if (unit->animation.wait != 0) {
                            unit->animation.wait--;
                            if (unit->animation.wait == 0) {
                                battle_gfx_run_unit_seq_script(unit, &unit->animation, 1);
                            }
                        }
                    }
                }
            } else {
                unit->requested_animation = 0;
            }
        }
    } else if (!(unit->movement.suppress_facing_update)) {
        if (sixteenth != unit->camera_facing_sixteenth) {
            sequence_id = unit->animation.sequence_id;
            if (sequence_id < (BATTLE_UNIT_ANIMATION_IDLE << 1)) {
                unit->animation.sequence_id = stand_by_direction[sixteenth];
                unit->sprite_display_flags
                    = (unit->sprite_display_flags
                          & ~(BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP | BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP))
                    | flip_by_direction[sixteenth];
            } else {
                unit->animation.sequence_id = (sequence_id & 0xfffe) + back_by_quadrant[quadrant];
                unit->sprite_display_flags
                    = (unit->sprite_display_flags
                          & ~(BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP | BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP))
                    | flip_by_quadrant[quadrant];
            }
            unit->camera_facing_quadrant = quadrant;
            unit->camera_facing_sixteenth = sixteenth;
            battle_gfx_run_unit_seq_script(unit, &unit->animation, 0);
        } else if (unit->animation.wait != 0) {
            for (i = 0; i < g_main_system_animation_speed; i++) {
                if (unit->animation.wait != 0) {
                    unit->animation.wait--;
                    if (unit->animation.wait == 0) {
                        battle_gfx_run_unit_seq_script(unit, &unit->animation, 1);
                    }
                }
                battle_unit_dispatch_distortion_animation(unit);
            }
        }
    } else if (unit->animation.wait != 0) {
        for (i = 0; i < g_main_system_animation_speed; i++) {
            if (unit->animation.wait != 0) {
                unit->animation.wait--;
                if (unit->animation.wait == 0) {
                    battle_gfx_run_unit_seq_script(unit, &unit->animation, 1);
                }
            }
            battle_unit_dispatch_distortion_animation(unit);
        }
    }
    for (i = 0; i < 3; i++) {
        if (unit->sprite_blocks[i].trigger != 0 && unit->sprite_blocks[i].wait != 0) {
            for (j = 0; j < g_main_system_animation_speed; j++) {
                if (unit->sprite_blocks[i].wait != 0) {
                    unit->sprite_blocks[i].wait--;
                    if (unit->sprite_blocks[i].wait == 0) {
                        battle_gfx_run_wep_eff_seq_script(unit, &unit->sprite_blocks[i], i, 1);
                    }
                }
            }
        }
    }
}

/* Draws the unit's status bubble when the unit is on screen and has one.
 *
 * Nothing is drawn unless the camera-relative position lies within x 0x61..0x19f
 * and y -0x1f..0x10f, the ordering-table depth within 1..0x17f,
 * sprite_graphic_trigger is set, the unit is not a mount carrying its rider and
 * status_bubble_active is set. */
static void battle_gfx_update_status_bubble_display(battle_unit_t* unit) {
    DVECTOR screen_coords;

    /* The camera-relative position is read unsigned (lhu) here. */
    screen_coords.vx = unit->camera_relative_x;
    screen_coords.vy = unit->camera_relative_y;
    if (screen_coords.vx < 0x61 || screen_coords.vx > 0x19f) {
        return;
    }
    if (screen_coords.vy < -0x1f || screen_coords.vy > 0x10f) {
        return;
    }
    if ((unit->otag_depth_index - 1) >= 0x17f) {
        return;
    }
    if (unit->animation.trigger == 0) {
        return;
    }
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
        return;
    }
    if (unit->status_bubble_active == 0) {
        return;
    }
    battle_gfx_draw_status_bubble(unit, &screen_coords);
}

/* Queue a unit's sprite layers into the ordering table at its depth.
 *
 * g_battle_gfx_animation_layer_priorities[layer_priority] lists four layers in draw order: 0 is the unit's own
 * sprite display, n > 0 is weapon/effect sprite block n - 1 (skipped while
 * its trigger is clear). A rider (mount state 1) is skipped here; a mount
 * (state 2) draws its first three body parts, then its rider's layers, then
 * the rest of its body. Unmounted units mirror their body sprite by the
 * animation state's flags. The shadow and the position copies follow.
 *
 * The rider loop issues one call per arm; cross-jumping merges them after
 * reload, but the duplicated RTL makes the mount loop large enough that
 * loop.c leaves `row * 16 + g_battle_gfx_animation_layer_priorities` inside it, which is why the target
 * recomputes the table address each iteration and spills `row`. */
static void battle_gfx_draw_unit_sprite_layers(battle_unit_t* unit) {
    SVECTOR position;
    SVECTOR rider_position;
    SVECTOR zoom = { ONE, ONE, ONE };
    battle_unit_t* rider;
    battle_gfx_sprite_display_data_t* display;
    battle_gfx_sprite_display_data_t* rider_display;
    s32 row;
    s32 rider_row;
    s32 i;
    s32 j;
    s32 entry;
    s16 angle;
    s16 flags;
    s16 rider_flags;

    position.vx = unit->camera_relative_x;
    position.vy = unit->camera_relative_y;
    SetGeomScreen(0x200);
    SetGeomOffset(0, 0);
    angle = 0;
    if (unit->otag_depth_index - 1 >= 0x17f) {
        return;
    }
    if (unit->numeric_display.active) {
        battle_gfx_animate_post_action_text(unit, &position);
    }
    if (!((position.vx >= 0x61 && position.vx <= 0x19f) && (position.vy >= -0x1f && position.vy <= 0x10f))
        && unit->spritesheet_id != BATTLE_SPRITESHEET_ID_ALTIMA_SECOND_FORM) {
        return;
    }
    copyVector(&zoom, &g_camera_zoom);
    flags = unit->horizontal_flip_flag ^ unit->sprite_display_flags;
    row = unit->layer_priority;
    if (unit->animation.trigger == 0) {
        return;
    }
    if (unit->item_display_active) {
        battle_gfx_init_item_get_rendering(unit);
    }
    if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
        if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        } else if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
            for (i = 0; i < 4; i++) {
                entry = g_battle_gfx_animation_layer_priorities[row][i];
                if (entry != 0) {
                    if (unit->sprite_blocks[entry - 1].trigger == 0) {
                        continue;
                    }
                    display = unit->sprite_blocks[entry - 1].display;
                    battle_gfx_construct_polygon_data_for_units(display, display->part_count, 0, &position, angle,
                        flags, &zoom, main_gfx_get_otag() + unit->otag_depth_index);
                } else {
                    display = unit->animation.display;
                    battle_gfx_construct_polygon_data_for_units(
                        display, 3, 0, &position, angle, flags, &zoom, main_gfx_get_otag() + unit->otag_depth_index);
                    rider = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
                    if (rider != 0 && rider->draw_enabled != 0) {
                        rider_position.vx = rider->camera_relative_x;
                        rider_position.vy = rider->camera_relative_y;
                        rider_flags = rider->sprite_display_flags;
                        rider_row = rider->layer_priority;
                        for (j = 0; j < 4; j++) {
                            entry = g_battle_gfx_animation_layer_priorities[rider_row][j];
                            if (entry != 0) {
                                if (rider->sprite_blocks[entry - 1].trigger == 0) {
                                    continue;
                                }
                                rider_display = rider->sprite_blocks[entry - 1].display;
                                battle_gfx_construct_polygon_data_for_units(rider_display, rider_display->part_count, 0,
                                    &rider_position, angle, rider_flags, &zoom,
                                    main_gfx_get_otag() + unit->otag_depth_index);
                            } else {
                                rider_display = rider->animation.display;
                                battle_gfx_construct_polygon_data_for_units(rider_display, rider_display->part_count, 0,
                                    &rider_position, angle, rider_flags, &zoom,
                                    main_gfx_get_otag() + unit->otag_depth_index);
                            }
                        }
                    }
                    battle_gfx_construct_polygon_data_for_units(display, display->part_count, 3, &position, angle,
                        flags, &zoom, main_gfx_get_otag() + unit->otag_depth_index);
                }
            }
            if (unit->shadow_graphic_trigger) {
                battle_gfx_draw_unit_shadow(unit, main_gfx_get_otag() + unit->otag_depth_index);
            }
        }
    } else {
        for (i = 0; i < 4; i++) {
            entry = g_battle_gfx_animation_layer_priorities[row][i];
            if (entry != 0) {
                if (unit->sprite_blocks[entry - 1].trigger == 0) {
                    continue;
                }
                display = unit->sprite_blocks[entry - 1].display;
                battle_gfx_construct_polygon_data_for_units(display, display->part_count, 0, &position, angle, flags,
                    &zoom, main_gfx_get_otag() + unit->otag_depth_index);
            } else {
                display = unit->animation.display;
                battle_gfx_construct_polygon_data_for_units(display, display->part_count, 0, &position, angle,
                    flags ^ unit->animation.flags, &zoom, main_gfx_get_otag() + unit->otag_depth_index);
            }
        }
        if (unit->shadow_graphic_trigger) {
            battle_gfx_draw_unit_shadow(unit, main_gfx_get_otag() + unit->otag_depth_index);
        }
    }
    if (unit->position_copies_active) {
        battle_gfx_draw_unit_sprite_position_copies(unit);
    }
}

/* Projects every display unit into camera space.
 *
 * Refreshes each item-effect origin and ordering-table depth. Terrain depth
 * lowers grounded units, while In Water, Float, and Walk on Water select
 * their original exceptions. */
void battle_gfx_update_all_unit_rotation_and_vectors(void) {
    VECTOR transformed;
    SVECTOR screen;
    SVECTOR adjusted;
    long gte_flag;
    battle_unit_t* unit;
    map_tile_t* tile;
    move_effective_flags_e movement_flags;

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        battle_unit_copy_draw_coords(&screen, unit);
        RotTrans(&screen, &transformed, &unit->gte_flag);

        if ((unit->sprite_display_flags & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) != 0) {
            unit->camera_relative_x = transformed.vx - unit->screen_offset.vx;
        } else {
            unit->camera_relative_x = transformed.vx + unit->screen_offset.vx;
        }
        if ((unit->sprite_display_flags & BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP) != 0) {
            unit->camera_relative_y = transformed.vy - unit->screen_offset.vy;
        } else {
            unit->camera_relative_y = transformed.vy + unit->screen_offset.vy;
        }

        tile = battle_map_get_tile_data_pointer(
            unit->world_coords.vx / MAP_TILE_SIZE, unit->world_coords.vz / MAP_TILE_SIZE, unit->map_z);
        if (unit->step_phase != MOVE_STEP_PHASE_NONE) {
            if ((tile->surface.depth_bias) != 0) {
                battle_camera_calculate_relative_offset_by_5(&screen, &adjusted);
            } else {
                battle_camera_calculate_relative_offset_by_1(&screen, &adjusted);
            }
        } else if ((tile->surface.depth_bias) != 0) {
            battle_camera_convert_screen_coords_modify_by_5(&screen, &adjusted);
        } else {
            battle_camera_convert_screen_coords_modify_by_1(&screen, &adjusted);
        }

        if (unit->stats != 0) {
            movement_flags = battle_move_get_effective_flags(unit->stats);
            if ((movement_flags & BATTLE_EFFECTIVE_MOVEMENT_IN_WATER) != 0) {
                if (tile->depth_half_height.depth != 0) {
                    adjusted.vy -= 12;
                }
            } else if ((movement_flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT) == 0) {
                if ((movement_flags & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER) == 0) {
                    adjusted.vy -= tile->depth_half_height.depth * 12;
                }
            }
        }

        RotTrans(&adjusted, &transformed, &gte_flag);
        unit->otag_depth_index = transformed.vz / 4;
        unit = unit->previous;
    }
}

/*
 * Per-frame status-bubble and unit-graphics update.
 *
 * In the listed game states, bubbles are held off for 0x3c frames whenever the
 * camera zooms, tilts, rotates or moves, and processed for every runtime unit
 * Data record once the countdown has run out. Every state then reloads the
 * casting unit's graphics and runs battle_gfx_draw_unit_sprite_layers on every record.
 */
void battle_gfx_update_status_bubbles_and_graphics(void) {
    battle_unit_t* unit;

    switch (g_battle_game_state) {
    case BATTLE_GAME_STATE_FREE_CURSOR:
    case BATTLE_GAME_STATE_HIGHLIGHT_UNITS:
    case BATTLE_GAME_STATE_OPEN_ACTION_MENUS:
    case BATTLE_GAME_STATE_IDLING_ACTION_MENUS:
    case BATTLE_GAME_STATE_MENU_TO_TARGETING:
    case BATTLE_GAME_STATE_DISPLAY_MOVE_AREA:
    case BATTLE_GAME_STATE_UNIT_MOVE:
    case BATTLE_GAME_STATE_CLOSE_MOVE_HELP:
    case BATTLE_GAME_STATE_ACTION_EXECUTE_SETUP:
    case BATTLE_GAME_STATE_TARGETING_RANGE:
    case BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING:
    case BATTLE_GAME_STATE_CONFIRM_ACTION:
        if (g_camera_zoom_action == CAMERA_STEP_NONE && g_camera_tilt_action == CAMERA_STEP_NONE
            && g_camera_rotation_action == CAMERA_ROTATION_NONE
            && !(g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz)) {
            if (g_battle_gfx_status_bubble_delay != 0) {
                g_battle_gfx_status_bubble_delay -= g_main_system_animation_speed;
                if (g_battle_gfx_status_bubble_delay <= 0) {
                    g_battle_gfx_status_bubble_delay = 0;
                }
            } else {
                for (unit = g_battle_unit_list_head; unit != 0; unit = unit->previous) {
                    battle_gfx_update_status_bubble_display(unit);
                }
            }
        } else {
            g_battle_gfx_status_bubble_delay = 0x3c;
        }
        break;
    }
    battle_gfx_load_casting_unit_graphics();
    for (unit = g_battle_unit_list_head; unit != 0; unit = unit->previous) {
        battle_gfx_draw_unit_sprite_layers(unit);
    }
}

void battle_unit_remove(battle_unit_t* unit) {
    battle_unit_t* it;
    s32 shared;

    shared = 0;
    battle_gfx_release_unit_sprite_display_stub(unit->animation.display);
    g_battle_unit_slot_states[unit->runtime_id].in_use = 0;
    /* The 32-bit list-head pointer doubles as a sentinel record (previous is at 0x000);
     * reading the first link through that view keeps the target's addressing. */
    for (it = ((battle_unit_t*)&g_battle_unit_list_head)->previous; it != 0; it = it->previous) {
        if (it->spritesheet_vram_slot == unit->spritesheet_vram_slot
            && g_battle_unit_slot_states[it->runtime_id].in_use != 0) {
            shared = 1;
        }
    }
    if (shared == 0) {
        g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].in_use = 0;
    }
    for (it = (battle_unit_t*)&g_battle_unit_list_head; it != 0; it = it->previous) {
        if (it->previous == unit) {
            it->previous = unit->previous;
            return;
        }
    }
}

void battle_unit_clear_all(void) {
    battle_unit_t* unit;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        battle_unit_remove(unit);
        unit = unit->previous;
    }
}

/* Steps a unit's fade-out removal; each phase waits for the unit's palette
 * animation to end.
 *
 * FLASH starts the fade to black and moves to DARKEN. DARKEN then disables the
 * battle unit slot, removes the runtime record and returns 1; for the source unit it
 * only resets the graphic trigger. Every other path returns 0. */
static s32 battle_unit_update_fade_out_removal(battle_unit_t* unit) {
    u8 phase;

    phase = unit->fade_out_phase;
    if (phase == BATTLE_UNIT_FADE_OUT_NONE) {
        return 0;
    }
    if (g_map_palette_state.clut_rows[MAP_CLUT_ROW_UNITS].animations[unit->runtime_id].active != 0) {
        return 0;
    }
    switch (phase) {
    case BATTLE_UNIT_FADE_OUT_FLASH:
        unit->fade_out_phase = BATTLE_UNIT_FADE_OUT_DARKEN;
        battle_gfx_apply_unit_palette_modulation_to_units(
            MAP_COLOR_MODE_BASE, 4, unit->runtime_id, -0x1f, -0x1f, -0x1f);
        return 0;
    case BATTLE_UNIT_FADE_OUT_DARKEN:
        if (unit->runtime_id != g_battle_unit_source_runtime_id) {
            unit->fade_out_phase = BATTLE_UNIT_FADE_OUT_REMOVED;
            if (unit->stats != 0) {
                battle_unit_disable_remove(unit->stats->battle_id);
            }
            battle_unit_remove(unit);
            return 1;
        }
        battle_gfx_reset_unit_graphic_trigger(unit->runtime_id);
        return 0;
    }
    return 0;
}

/* Per-frame update of every unit record: its animation with the weapon and effect
 * sprites, its status bubble and its fade-out removal. */
void battle_unit_update_and_animate_units(void) {
    battle_unit_t* unit;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        battle_gfx_update_and_animate_unit_wep_eff(unit);
        battle_gfx_init_status_bubble(unit);
        battle_unit_update_fade_out_removal(unit);
        unit = unit->previous;
    }
}

static void battle_gfx_unpack_unit_seq_data(
    battle_gfx_native_unit_seq_table_t* destination, const battle_gfx_packed_seq_record_t* source) {
    const battle_gfx_packed_seq_record_t* header;
    const u8(*entry)[4];
    u32 cursor;
    u32 index;
    u32 payload_size;
    s32 offset;

    header = source;
    cursor = (u32)g_battle_gfx_seq_data_cursor;
    destination->header.first_attack_sequence
        = source->first_attack_sequence[0] + (source->first_attack_sequence[1] << 8);
    destination->header.first_sp2_sequence = source->first_sp2_sequence[0] + (source->first_sp2_sequence[1] << 8);
    entry = source->sequence_offsets;
    for (index = 0; index < 0x100; index++) {
        offset = entry[index][0] + (entry[index][1] << 8) + (entry[index][2] << 16) + (entry[index][3] << 24);
        if (offset == -1) {
            offset = 0;
        }
        destination->sequences[index] = (u8*)(offset + cursor);
    }

    payload_size = header->sequence_data_size[0] + (header->sequence_data_size[1] << 8);
    for (index = 0; index < payload_size; index++) {
        const u8* payload = source->sequence_data;

        g_battle_gfx_seq_data_cursor[index] = payload[index];
    }
    g_battle_gfx_seq_data_cursor += index;
}

/* Unpack a weapon or effect SEQ file, which has no header fields: its 256
 * sequence offsets become pointers into the sequence data, which is appended
 * at g_battle_gfx_seq_data_cursor.
 *
 * An offset of -1 resolves to the start of that data instead of to a real
 * sequence.
 *
 * `header` and `unused_frame_pad` are matching constraints. The target keeps
 * two live copies of the record pointer (`move t2,a1` at +0x4, with `a1` still
 * used for the entry base and the payload address), which only a second
 * variable reproduces; and it reserves an 8-byte frame that no instruction
 * touches, which only a declared local aggregate reproduces. */
static void battle_gfx_resolve_packed_offset_table(u8** entries, const battle_gfx_packed_offset_table_t* record) {
    const u8(*entry)[4];
    u32 index;
    u32 payload_size;
    const u8* payload;
    s32 offset;
    u8 unused_frame_pad[8];
    const battle_gfx_packed_offset_table_t* header;

    header = record;
    entry = record->entry;
    index = 0;
    do {
        offset = entry[index][0] + (entry[index][1] << 8) + (entry[index][2] << 16) + (entry[index][3] << 24);
        if (offset == -1) {
            offset = 0;
        }
        entries[index] = g_battle_gfx_seq_data_cursor + offset;
        index += 1;
    } while (index < 0x100);

    payload_size = header->payload_size[0] + (header->payload_size[1] << 8);
    index = 0;
    if (payload_size != 0) {
        payload = record->payload;
        do {
            g_battle_gfx_seq_data_cursor[index] = *payload;
            index += 1;
            payload += 1;
        } while (index < payload_size);
    }
    g_battle_gfx_seq_data_cursor += index;
}

/* Unpack a unit SHP file into its two frame tables.
 *
 * The file's first 32-bit value is the offset of its second frame set, the submerged
 * frames; 8 means it has only the set that starts there and both tables get
 * its frames. A set is 0x100 frame offsets, of which the first 0xd0 become
 * pointers into the frame data (-1 maps to its start), then the frame data:
 * a 16-bit count and that many bytes, appended at
 * g_battle_gfx_shp_frame_data_cursor.
 */
static void battle_gfx_unpack_unit_shp_data(
    battle_gfx_unit_shp_frame_tables_t* dst, const battle_gfx_packed_unit_shp_record_t* src) {
    u32 i;
    u32 j;
    s32 offset;
    s32 value;
    u32 count;
    const u8* data;
    s32 header;

    /* Named table indexing removes the retail cursor and skip-loop instructions. */
    data = (const u8*)src;
    header = src->submerged_frame_set_offset[0] + (src->submerged_frame_set_offset[1] << 8)
        + (src->submerged_frame_set_offset[2] << 16) + (src->submerged_frame_set_offset[3] << 24);
    dst->first_attack_frame = src->first_attack_frame[0] + (src->first_attack_frame[1] << 8);
    dst->first_sp2_frame = src->first_sp2_frame[0] + (src->first_sp2_frame[1] << 8);
    offset = offsetof(battle_gfx_packed_unit_shp_record_t, primary_frame_set);
    for (i = 0; i < 0xd0; i++) {
        value = data[offset] + (data[offset + 1] << 8) + (data[offset + 2] << 16) + (data[offset + 3] << 24);
        if (value == -1) {
            value = 0;
        }
        dst->frames[i] = (battle_gfx_source_frame_t*)(g_battle_gfx_shp_frame_data_cursor + value);
        if (header == 8) {
            dst->submerged_frames[i] = (battle_gfx_source_frame_t*)(g_battle_gfx_shp_frame_data_cursor + value);
        }
        offset += 4;
    }
    for (i = 0xd0; i < 0x100; i++) {
        offset += 4;
    }
    count = data[offset] + (data[offset + 1] << 8);
    offset += 2;
    for (j = 0; j < count; j++) {
        g_battle_gfx_shp_frame_data_cursor[j] = ((const u8*)src)[offset++];
    }
    g_battle_gfx_shp_frame_data_cursor += j;
    if (header != 8) {
        offset = header;
        for (i = 0; i < 0xd0; i++) {
            value = data[offset] + (data[offset + 1] << 8) + (data[offset + 2] << 16) + (data[offset + 3] << 24);
            if (value == -1) {
                value = 0;
            }
            dst->submerged_frames[i] = (battle_gfx_source_frame_t*)(g_battle_gfx_shp_frame_data_cursor + value);
            offset += 4;
        }
        for (i = 0xd0; i < 0x100; i++) {
            offset += 4;
        }
        count = data[offset] + (data[offset + 1] << 8);
        offset += 2;
        for (j = 0; j < count; j++) {
            g_battle_gfx_shp_frame_data_cursor[j] = ((const u8*)src)[offset++];
        }
        g_battle_gfx_shp_frame_data_cursor += j;
    }
}

/*
 * Unpack a weapon or effect SHP file into dst: after four unread bytes, the
 * first frame of each of 32 weapon types, 0x200 frame offsets that become
 * pointers into the frame data (-1 maps to its start), and the frame data
 * itself, a 16-bit count and that many bytes, which is appended at
 * g_battle_gfx_shp_frame_data_cursor.
 *
 * The header loops read through `data` while the blob copy indexes `src`;
 * using one name for both loses the target's separate copy of the argument.
 */
static void battle_gfx_copy_shp_data_to_ram(
    battle_gfx_weapon_shp_t* dst, const battle_gfx_packed_weapon_shp_record_t* src) {
    u32 i;
    u32 j;
    s32 offset;
    s32 value;
    u32 count;
    const u8* word;
    const u8* data;

    /* Named field indexing removes the retail running-offset instructions. */
    data = (const u8*)src;
    offset = offsetof(battle_gfx_packed_weapon_shp_record_t, first_frame);
    for (i = 0; i < 32; i++) {
        word = data + offset;
        dst->first_frame[i] = word[0] | (word[1] << 8);
        offset += 2;
    }
    for (i = 0; i < 0x200; i++) {
        word = data + offset;
        value = word[0] + (word[1] << 8) + (word[2] << 16) + (word[3] << 24);
        if (value == -1) {
            value = 0;
        }
        dst->frames[i] = (battle_gfx_source_frame_t*)(g_battle_gfx_shp_frame_data_cursor + value);
        offset += 4;
    }
    word = data + offset;
    offset += 2;
    count = word[0] + (word[1] << 8);
    for (j = 0; j < count; j++) {
        g_battle_gfx_shp_frame_data_cursor[j] = ((const u8*)src)[offset++];
    }
    g_battle_gfx_shp_frame_data_cursor += j;
}

/* Upload both uncompressed images, copy the compressed attack image,
 * and enable STP on each palette's entries 1..15.
 * Indexed byte copies preserve the original loop scheduling. Three RECT
 * locals preserve the 0x50-byte frame and its otherwise unused x/y stores. */
static void battle_gfx_load_spritesheet_into_vram_slot(
    const battle_gfx_unit_sprite_file_t* source, u32 index, s32 unused, battle_unit_t* unit, s16 unused_palette) {
    const u8* data;
    u32 column;
    u8* clut_bytes;
    u8* compressed_attack_image;
    s32 byte_index;
    RECT unused_rect;
    RECT rect;
    RECT unused_rect2;

    /* Byte stores preserve the palette loop and shared image cursor. */
    data = (const u8*)source;
    column = index;
    clut_bytes = (u8*)g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].clut[0];
    compressed_attack_image = g_battle_gfx_spritesheet_slots[unit->spritesheet_vram_slot].compressed_attack_image;
    if (data != 0) {
        for (byte_index = 0; byte_index < 0x200; byte_index++) {
            if (byte_index % 2 != 0 && byte_index % 32 != 1) {
                clut_bytes[byte_index] = data[byte_index] | 0x80;
            } else {
                clut_bytes[byte_index] = data[byte_index];
            }
        }
        data += sizeof(source->clut);
        rect.x = column * 64 + 0x100;
        rect.y = 0x100;
        LoadTPage((u32*)data, 0, 0, rect.x, 0x100, 0x100, 0x100);
        rect.x = (column / 8) * 64 + 0x340;
        rect.y = (column % 8) * 32 + 0x100;
        LoadTPage((u32*)(data + sizeof(source->pixels)), 0, 0, rect.x, rect.y, 0x100, 0x20);
        for (byte_index = 0; byte_index < 0x30d4; byte_index++) {
            compressed_attack_image[byte_index]
                = data[byte_index + sizeof(source->pixels) + sizeof(source->portrait_and_body_pixels)];
        }
    }
}

/* Upload WEP.SPR's weapon, trail and particle images. Keep weapon and trail
 * CLUTs in their CPU tables; the particle CLUTs use the map palette manager. */
static void battle_gfx_load_wep_spr_data(battle_gfx_weapon_sprite_archive_t* archive) {
    s32 i;
    u8* cursor;

    cursor = (u8*)archive->weapon_clut;
    for (i = 0; i < (s32)sizeof(archive->weapon_clut); i++) {
        ((u8*)g_battle_gfx_weapon_clut)[i] = *cursor++;
    }
    LoadTPage((u32*)archive->weapon_pixels, 0, 0, 0x240, 0, 0x100, 0x100);
    for (i = 0, cursor = (u8*)archive->trail_clut; i < (s32)sizeof(archive->trail_clut); i++) {
        ((u8*)g_battle_gfx_trail_clut)[i] = *cursor++;
    }
    LoadTPage((u32*)archive->trail_pixels, 0, 0, 0x200, 0, 0x100, 0x100);
    battle_map_load_palette_data(archive->particle_clut, MAP_CLUT_ROW_WEAPONS, 0, 1);
    LoadTPage((u32*)archive->particle_pixels, 0, 0, 0x200, 0x70, 0x100, 0x90);
}

/* Copy the first 21 OTHER.SPR palettes, setting STP on all but color zero. */
static void battle_gfx_copy_other_spr_palette_data_to_ram(const battle_gfx_other_sprite_archive_t* archive) {
    s32 i = 0;
    s32 one = 1; /* held in a local: a literal changes the setup schedule */
    u8* dst = (u8*)&g_battle_gfx_other_palettes;
    /* Byte transfers preserve the palette loop and STP store sequence. */
    const u8* src = (const u8*)archive->palettes;
    do {
        u8 color_byte;
        if ((i & 1) && i % 32 != one) {
            color_byte = *src++ | 0x80;
        } else {
            color_byte = *src++;
        }
        *dst++ = color_byte;
        i++;
    } while (i < (s32)sizeof(g_battle_gfx_other_palettes));
}

static battle_unit_t* battle_unit_init(s32 map_x, s32 map_y, s32 map_level, s16 facing, s32 spritesheet_id, s16 palette,
    s16 requested_runtime_id, unit_stats_t* stats, u32 flags, const battle_gfx_unit_sprite_file_t* data) {
    battle_unit_t* unit;
    union {
        battle_unit_anim_state_t* animation;
        battle_unit_sprite_block_t* block;
    } cursor;
    map_tile_t* tile;
    s32 runtime_id;
    s32 slot;
    u16 id;
    s32 i;
    battle_unit_t** head;
    u8 tile_x;
    u8 tile_y;
    u8 tile_z;

    id = spritesheet_id;
    unit = battle_unit_claim_slot(requested_runtime_id);
    if (unit == 0) {
        return 0;
    }
    runtime_id = unit->runtime_id;
    unit->spritesheet_id = id;
    slot = unit->spritesheet_vram_slot = battle_gfx_claim_spritesheet_slot(spritesheet_id);
    if (slot == 0xffff) {
        g_battle_unit_slot_states[unit->runtime_id].in_use = 0;
        return 0;
    }
    if (id >= BATTLE_SPRITESHEET_ID_JOB_FIRST && id < BATTLE_SPRITESHEET_ID_CHOCOBO && stats != 0) {
        palette = stats->sprite_palette;
    }
    if (palette >= 6) {
        palette = 0;
    }
    unit->stored_palette = palette;
    unit->statuses_to_remove_5_6 = 0;
    unit->statuses_to_remove_1_4 = 0;
    unit->statuses_to_add_5_6 = 0;
    unit->statuses_to_add_1_4 = 0;
    unit->status_flags_5_6 = 0;
    unit->status_flags_1_4 = 0;
    battle_gfx_load_spritesheet_into_vram_slot(data, slot, runtime_id, unit, palette);
    tile = battle_map_get_tile_data_pointer(map_x, map_y, map_level);
    battle_gfx_check_tile_status_palette_mod(unit, tile->flags.shading, 1);
    battle_map_load_palette_data(&g_battle_gfx_shadow_palette, MAP_CLUT_ROW_UNIT_SHADOWS, runtime_id, 0);
    unit->clut = GetClut(runtime_id << 4, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_UNITS);
    unit->tpage = slot + BATTLE_GFX_TPAGE_SPRITESHEET_SLOT_0;
    if (flags == 0) {
        unit->draw_enabled = 1;
        unit->animation.trigger = 1;
    } else {
        unit->draw_enabled = 0;
        unit->animation.trigger = 0;
    }
    unit->standing_on_battle_id_plus_one = 0;
    unit->stats = stats;
    unit->requested_animation = 0;
    unit->facing = facing * 1024;
    unit->camera_facing_sixteenth = unit->camera_facing_quadrant = unit->attack_facing = -1;
    unit->mount_state = BATTLE_UNIT_MOUNT_STATE_NONE;
    unit->horizontal_flip_flag = 0;
    unit->depth_height_offset = 0;
    unit->float_bob_offset = 0;
    unit->float_bob_phase = 0;
    unit->step_phase = MOVE_STEP_PHASE_NONE;
    unit->path.count = 0;
    unit->sprite_display_flags = 0;
    unit->layer_priority = 0;
    unit->turn_action_flags = 0;
    unit->special_graphic_y_offset = 0;
    unit->distortion_animation_id = BATTLE_DISTORTION_ANIMATION_NONE;
    unit->distortion_phase = 0;
    tile_x = map_x;
    unit->map_x = tile_x;
    unit->movement.suppress_facing_update = 0;
    tile_y = map_y;
    unit->map_y = tile_y;
    unit->movement.suppress_palette_update = 0;
    tile_z = map_level;
    unit->map_z = tile_z;
    unit->movement.alternate_short_hop = 0;
    unit->movement.floating = 0;
    unit->movement.force_animation_update = 0;
    unit->movement.destination_x = tile_x;
    unit->movement.destination_y = tile_y;
    unit->movement.destination_z = tile_z;
    battle_unit_set_move_and_world_coords(unit);
    cursor.animation = &unit->animation;
    unit->velocity.vz = 0;
    unit->velocity.vy = 0;
    unit->velocity.vx = 0;
    unit->walk_speed = 0x2000;
    unit->seq_offset.vz = 0;
    unit->seq_offset.vy = 0;
    unit->seq_offset.vx = 0;
    unit->screen_offset.vz = 0;
    unit->screen_offset.vy = 0;
    unit->screen_offset.vx = 0;
    unit->event_offset.vz = 0;
    unit->event_offset.vy = 0;
    unit->event_offset.vx = 0;
    unit->fade_out_phase = BATTLE_UNIT_FADE_OUT_NONE;
    unit->shadow_graphic_trigger = 1;
    unit->shadow_dirty = 1;
    unit->shadow_counter = 0;
    unit->numeric_display.active = 0;
    unit->numeric_display.selector = 0;
    unit->numeric_display.progress = 0;
    unit->numeric_display.displays[0] = battle_gfx_init_numeric_sprite_display(runtime_id, 0);
    unit->numeric_display.displays[1] = battle_gfx_init_numeric_sprite_display(runtime_id, 1);
    unit->numeric_display.displays[2] = battle_gfx_init_numeric_sprite_display(runtime_id, 2);
    unit->numeric_display.displays[0]->scale_x = unit->numeric_display.displays[0]->scale_y
        = unit->numeric_display.displays[1]->scale_x = unit->numeric_display.displays[1]->scale_y
        = unit->numeric_display.displays[2]->scale_x = unit->numeric_display.displays[2]->scale_y = ONE;
    unit->numeric_display.displays[0]->tpage = unit->numeric_display.displays[1]->tpage
        = unit->numeric_display.displays[2]->tpage = BATTLE_GFX_TPAGE_FRAME;
    unit->numeric_display.displays[0]->clut = unit->numeric_display.displays[1]->clut
        = unit->numeric_display.displays[2]->clut
        = unit->clut + 0x100; /* the unit's CLUT of MAP_CLUT_ROW_ACTION_RESULTS */
    unit->numeric_display.displays[0]->part_count = unit->numeric_display.displays[1]->part_count
        = unit->numeric_display.displays[2]->part_count = 1;
    unit->status_bubble_active = 0;
    unit->status_bubble_id = 0;
    unit->status_bubble_timer = 0;
    unit->status_bubble_display = battle_gfx_init_status_bubble_sprite_display(runtime_id);
    unit->item_display_active = 0;
    unit->item_display = battle_gfx_init_item_sprite_display(runtime_id);
    unit->position_copies_active = 0;
    unit->animation.display = battle_gfx_init_unit_sprite_display(runtime_id);
    cursor.animation->graphic_type = BATTLE_SPRITE_GRAPHIC_UNIT;
    unit->animation.shp = unit->animation.saved_shp = battle_gfx_get_unit_shp_by_spritesheet_id(id);
    unit->animation.seq = unit->animation.saved_seq = battle_gfx_get_unit_seq_by_spritesheet_id(id);
    cursor.animation->wait = 1;
    cursor.animation->restart_count = 0;
    cursor.animation->script_pos = 0;
    cursor.animation->sequence_id = BATTLE_UNIT_ANIMATION_IDLE << 1;
    cursor.animation->frame = 0;
    cursor.animation->flags = 0;
    cursor.animation->wait_bias = 0;
    cursor.animation->frame_bias = 0;
    cursor.animation->repeat_count = 0;
    unit->animation.display->scale_x = ONE;
    unit->animation.display->scale_y = ONE;
    unit->animation.display->rotation = 0;
    battle_unit_set_animation_based_on_status(unit);
    battle_gfx_run_unit_seq_script(unit, cursor.animation, 1);
    for (i = 0; i < 3; i++) {
        cursor.block = &unit->sprite_blocks[i];
        cursor.block->display = battle_gfx_init_weapon_sprite_display(runtime_id, i);
        if (i == 0) {
            cursor.block->graphic_type = BATTLE_SPRITE_GRAPHIC_WEAPON;
            cursor.block->shp = battle_gfx_get_weapon_shp_by_spritesheet_id(id);
            cursor.block->seq = battle_gfx_get_weapon_seq_by_spritesheet_id(id);
        } else {
            cursor.block->graphic_type = BATTLE_SPRITE_GRAPHIC_EFFECT;
            cursor.block->shp = battle_gfx_get_effect_shp_data(id);
            cursor.block->seq = battle_gfx_get_effect_seq_data(id);
        }
        cursor.block->trigger = 0;
        cursor.block->wait = 1;
        cursor.block->script_pos = 0;
        cursor.block->sequence_id = 0;
        cursor.block->frame = 0;
        cursor.block->flags = 0;
        cursor.block->wait_bias = 0;
        cursor.block->frame_bias = 0;
        cursor.block->repeat_count = 0;
        cursor.block->display->scale_x = ONE;
        cursor.block->display->scale_y = ONE;
        cursor.block->display->rotation = 0;
        if ((s16)id < BATTLE_SPRITESHEET_ID_CHOCOBO && (s16)id < BATTLE_SPRITESHEET_ID_VELIUS
            && (s16)id >= BATTLE_SPRITESHEET_ID_BOY && (s16)id != BATTLE_SPRITESHEET_ID_DAMI) {
            battle_gfx_run_wep_eff_seq_script(unit, &unit->sprite_blocks[i], i, 1);
        }
    }
    head = &g_battle_unit_list_head;
    unit->previous = *head;
    *head = unit;
    battle_gfx_set_thrown_item_graphic_palette(unit->stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON], unit);
    battle_status_init_special_flag_enabling(unit->stats->battle_id);
    unit->movement.suppress_sfx_and_landing_effects = 1;
    battle_unit_update_display_by_runtime_id(unit->runtime_id);
    battle_unit_set_animation_based_on_status(unit);
    unit->movement.suppress_sfx_and_landing_effects = 0;
    return unit;
}

/* The two clearing loops walk their arrays by byte offset. A struct-subscript
 * walk leaves GCC with both the index and the scaled address as induction
 * variables, adding an `addiu`/`li` pair to each loop; the byte offset is what
 * reproduces the target's single `bgez` on the address offset. */
void battle_gfx_reset_unit_graphics_load_state(void) {
    s32 i;
    s32 offset;

    g_battle_unit_list_head = 0;
    for (offset = (BATTLE_UNIT_RUNTIME_SLOT_COUNT - 1) * sizeof(battle_unit_slot_state_t); offset >= 0;
        offset -= sizeof(battle_unit_slot_state_t)) {
        ((battle_unit_slot_state_t*)((u8*)g_battle_unit_slot_states + offset))->in_use = 0;
    }
    for (offset = 8 * sizeof(battle_gfx_spritesheet_slot_t); offset >= 0;
        offset -= sizeof(battle_gfx_spritesheet_slot_t)) {
        ((battle_gfx_spritesheet_slot_t*)((u8*)g_battle_gfx_spritesheet_slots + offset))->in_use = 0;
    }
    for (i = 0; i < 2; i++) {
        g_battle_gfx_vram_slots[i].owner = BATTLE_GFX_VRAM_SLOT_OWNER_NONE;
        g_battle_gfx_vram_slots[i].image_buffer_state = BATTLE_GFX_VRAM_SLOT_BUFFER_FREE;
        g_battle_gfx_vram_slots[i].evtchr_load_state = 0;
    }
    setRECT(&g_battle_gfx_vram_slots[0].image_rect, 0x100, 0, 0x40, 0xc8);
    setRECT(&g_battle_gfx_vram_slots[1].image_rect, 0x140, 0, 0x40, 0xc8);
    g_battle_gfx_seq_data_cursor = g_battle_gfx_unit_animation_data;
    g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
    g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
    g_battle_gfx_unit_graphics_load_state.index = 0;
    g_battle_gfx_unit_graphics_load_state.buffer = 0;
    g_battle_gfx_shp_frame_data_cursor = g_battle_gfx_shp_frame_data_buffer;
}

/* Advances the unit SHP/SEQ loader by one step and returns the file kind index.
 *
 * g_battle_gfx_unit_graphics_load_state.file_kind selects the file kind (sprite SHP/SEQ, WEP SHP/SEQ, EFF SHP/SEQ,
 * WEP.SPR, OTHER.SPR), g_battle_gfx_unit_graphics_load_state.phase the phase (0 allocate and start the read,
 * 1 wait, 2 copy into the RAM tables, 3 free and advance) and g_battle_gfx_unit_graphics_load_state.index the
 * file index within the kind.
 *
 * The target keeps several of these globals' addresses in registers; only a
 * single-assignment pointer local reproduces that (a plain `g++` folds %lo into
 * each access), and a block-scoped one per case also lets the scheduler hoist
 * the g_battle_gfx_unit_graphics_load_state.file_kind load above the counter store, as the target does. */
s32 battle_gfx_step_load_unit_shp_seq_data(void) {
    s32* phase;
    s32* index;
    void** slot;
    void* buffer;

    phase = &g_battle_gfx_unit_graphics_load_state.phase;
    switch (*phase) {
    case BATTLE_GFX_UNIT_LOAD_PHASE_START_READ:
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SHP:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_sprite_shp_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_sprite_shp_files[*index].sector, g_battle_gfx_sprite_shp_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SEQ:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_sprite_seq_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_sprite_seq_files[*index].sector, g_battle_gfx_sprite_seq_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SHP:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_weapon_shp_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_weapon_shp_files[*index].sector, g_battle_gfx_weapon_shp_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SEQ:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_weapon_seq_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_weapon_seq_files[*index].sector, g_battle_gfx_weapon_seq_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SHP:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_effect_shp_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_effect_shp_files[*index].sector, g_battle_gfx_effect_shp_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SEQ:
            index = &g_battle_gfx_unit_graphics_load_state.index;
            buffer = main_heap_alloc(g_battle_gfx_effect_seq_files[*index].size);
            g_battle_gfx_unit_graphics_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_effect_seq_files[*index].sector, g_battle_gfx_effect_seq_files[*index].size, buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SPR:
            *(slot = &g_battle_gfx_unit_graphics_load_state.buffer) = main_heap_alloc(0x15000);
            if (*slot == 0) {
                break;
            }
            if (main_file_call_build_header(DISC_SECTOR_BATTLE_WEP_SPR, 0x15000, (void*)*slot) == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free((void*)*slot);
            }
            break;
        case BATTLE_GFX_SHARED_LOAD_OTHER_SPR:
            *(slot = &g_battle_gfx_unit_graphics_load_state.buffer) = main_heap_alloc(0x8800);
            if (*slot == 0) {
                break;
            }
            if (main_file_call_build_header(DISC_SECTOR_BATTLE_OTHER_SPR, 0x8800, (void*)*slot) == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free((void*)*slot);
            }
            break;
        }
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_WAIT:
        if (main_file_is_still_loading() != 0) {
            break;
        }
        (*phase)++;
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_INSTALL:
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SHP:
            battle_gfx_unpack_unit_shp_data(
                &g_battle_gfx_unit_shp_frame_tables[g_battle_gfx_unit_graphics_load_state.index],
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SEQ:
            battle_gfx_unpack_unit_seq_data(&g_battle_gfx_unit_seq_tables[g_battle_gfx_unit_graphics_load_state.index],
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SHP:
            battle_gfx_copy_shp_data_to_ram(
                &g_battle_gfx_weapon_graphics[g_battle_gfx_unit_graphics_load_state.index].shp,
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SEQ:
            battle_gfx_resolve_packed_offset_table(
                g_battle_gfx_weapon_graphics[g_battle_gfx_unit_graphics_load_state.index].seq,
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SHP:
            battle_gfx_copy_shp_data_to_ram(
                &g_battle_gfx_effect_graphics[g_battle_gfx_unit_graphics_load_state.index].shp,
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SEQ:
            battle_gfx_resolve_packed_offset_table(
                g_battle_gfx_effect_graphics[g_battle_gfx_unit_graphics_load_state.index].seq,
                g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SPR:
            battle_gfx_load_wep_spr_data(g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_SHARED_LOAD_OTHER_SPR:
            battle_gfx_copy_other_spr_palette_data_to_ram(g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        }
        {
            s32* next_phase = &g_battle_gfx_unit_graphics_load_state.phase;

            (*next_phase)++;
        }
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_FINISH:
        main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
        *phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SHP: {
            s32* counter = &g_battle_gfx_unit_graphics_load_state.index;

            if (++*counter >= 5) {
                *counter = 0;
                g_battle_gfx_unit_graphics_load_state.file_kind++;
            }
            break;
        }
        case BATTLE_GFX_SHARED_LOAD_SPRITE_SEQ: {
            s32* counter = &g_battle_gfx_unit_graphics_load_state.index;

            if (++*counter >= 5) {
                *counter = 0;
                g_battle_gfx_unit_graphics_load_state.file_kind++;
            }
            break;
        }
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SHP:
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SEQ: {
            s32* counter = &g_battle_gfx_unit_graphics_load_state.index;

            if (++*counter >= 2) {
                *counter = 0;
                g_battle_gfx_unit_graphics_load_state.file_kind++;
            }
            break;
        }
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SHP:
        case BATTLE_GFX_SHARED_LOAD_EFFECT_SEQ: {
            s32* counter = &g_battle_gfx_unit_graphics_load_state.index;

            if (++*counter >= 1) {
                *counter = 0;
                g_battle_gfx_unit_graphics_load_state.file_kind++;
            }
            break;
        }
        case BATTLE_GFX_SHARED_LOAD_WEAPON_SPR:
        case BATTLE_GFX_SHARED_LOAD_OTHER_SPR: {
            s32* kind = &g_battle_gfx_unit_graphics_load_state.file_kind;

            g_battle_gfx_unit_graphics_load_state.index = 0;
            (*kind)++;
            break;
        }
        }
        break;
    }
    return g_battle_gfx_unit_graphics_load_state.file_kind;
}

static void battle_unit_update_and_animate_units_three_times(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        battle_unit_update_and_animate_units();
    }
}

/* Rewinds the unit graphics loader (g_battle_gfx_unit_graphics_load_state) to its
 * first file kind, the start-read phase, index 0 and no buffer. The argument is
 * not read. */
void battle_gfx_clear_unit_graphics_load_state(s32 unused_map_id) {
    g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
    g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
    g_battle_gfx_unit_graphics_load_state.index = 0;
    g_battle_gfx_unit_graphics_load_state.buffer = 0;
}

/* Builds the deployed units' data when a map starts and rewinds the unit graphics
 * loader.
 *
 * A battle whose deployment screen is skipped places the units from
 * g_main_deployed_unit_map_coordinates; otherwise the coordinate slots of the
 * deployment screen are used, and in the debug battle style the red team is set
 * up from them first. */
void battle_gfx_init_deployment_and_reset_unit_graphics_state(void) {
    g_main_gfx_unit_graphics_load_queue.count = 0;
    if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) == 0) {
        if (g_battle_deployment_skipped != 0) {
            battle_unit_build_deployed_units_data(g_main_deployed_unit_map_coordinates, 0);
            g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
            g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
            g_battle_gfx_unit_graphics_load_state.index = 0;
            g_battle_gfx_unit_graphics_load_state.buffer = 0;
            return;
        }
    } else {
        battle_unit_init_deployed_units_data_for_debug_red_team(attack_deploy_get_coordinate_slots(), 0);
    }
    battle_unit_build_deployed_units_data(attack_deploy_get_coordinate_slots(), 0);
    g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_UNIT_LOAD_KIND_FIRST;
    g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
    g_battle_gfx_unit_graphics_load_state.index = 0;
    g_battle_gfx_unit_graphics_load_state.buffer = 0;
}

/* Steps the per-unit graphics loader for the queued load descriptors.
 *
 * g_battle_gfx_unit_graphics_load_state holds the file kind (0 SHP, 1 SEQ, 2 SPR), the
 * phase (0 read, 1 wait, 2 install, 3 finish), the descriptor index and the
 * heap buffer. SHP/SEQ ids below 5, or equal to the one already resident, and
 * an already loaded spritesheet skip the read. Returns 0 once every
 * descriptor is done, otherwise 1 inside a step and 2 after a step boundary. */
s32 battle_gfx_step_queued_unit_graphics_load(void) {
    s32 result;
    s32 id;

    result = 1;
    switch (g_battle_gfx_unit_graphics_load_state.phase) {
    case BATTLE_GFX_UNIT_LOAD_PHASE_START_READ:
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_QUEUED_LOAD_SHP:
            id = g_battle_gfx_spritesheet_data[g_main_gfx_unit_graphics_load_queue
                                                   .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                   .spritesheet_id]
                     .shp_id;
            if (id < 5 || id == g_battle_gfx_last_loaded_shp_id) {
                result = 2;
                g_battle_gfx_unit_graphics_load_state.buffer = 0;
                g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_FINISH;
                break;
            }
            g_battle_gfx_last_loaded_shp_id = id;
            g_battle_gfx_unit_graphics_load_state.buffer = main_heap_alloc(g_battle_gfx_sprite_shp_files[id].size);
            if (g_battle_gfx_unit_graphics_load_state.buffer == 0) {
                break;
            }
            if (main_file_call_build_header(g_battle_gfx_sprite_shp_files[id].sector,
                    g_battle_gfx_sprite_shp_files[id].size, g_battle_gfx_unit_graphics_load_state.buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_QUEUED_LOAD_SEQ:
            id = g_battle_gfx_spritesheet_data[g_main_gfx_unit_graphics_load_queue
                                                   .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                   .spritesheet_id]
                     .seq_id;
            if (id < BATTLE_SEQ_RUKA || id == g_battle_gfx_last_loaded_seq_id) {
                result = 2;
                g_battle_gfx_unit_graphics_load_state.buffer = 0;
                g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_FINISH;
                break;
            }
            g_battle_gfx_last_loaded_seq_id = id;
            g_battle_gfx_unit_graphics_load_state.buffer = main_heap_alloc(g_battle_gfx_sprite_seq_files[id].size);
            if (g_battle_gfx_unit_graphics_load_state.buffer == 0) {
                break;
            }
            if (main_file_call_build_header(g_battle_gfx_sprite_seq_files[id].sector,
                    g_battle_gfx_sprite_seq_files[id].size, g_battle_gfx_unit_graphics_load_state.buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        case BATTLE_GFX_QUEUED_LOAD_SPR:
            if (g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index]
                    .spritesheet_id
                >= 0xa0) {
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index]
                    .spritesheet_id = BATTLE_SPRITESHEET_ID_RAMZA_CHAPTER_1;
            }
            if (battle_gfx_is_spritesheet_id_loaded(
                    g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index]
                        .spritesheet_id)
                != 0) {
                g_battle_gfx_unit_graphics_load_state.buffer = 0;
                g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_INSTALL;
                result = 2;
                break;
            }
            g_battle_gfx_unit_graphics_load_state.buffer = main_heap_alloc(
                g_battle_gfx_spritesheet_files[g_main_gfx_unit_graphics_load_queue
                                                   .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                   .spritesheet_id]
                    .size);
            if (g_battle_gfx_unit_graphics_load_state.buffer == 0) {
                break;
            }
            if (main_file_call_build_header(
                    g_battle_gfx_spritesheet_files[g_main_gfx_unit_graphics_load_queue
                                                       .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                       .spritesheet_id]
                        .sector,
                    g_battle_gfx_spritesheet_files[g_main_gfx_unit_graphics_load_queue
                                                       .descriptors[g_battle_gfx_unit_graphics_load_state.index]
                                                       .spritesheet_id]
                        .size,
                    g_battle_gfx_unit_graphics_load_state.buffer)
                == 0) {
                g_battle_gfx_unit_graphics_load_state.phase++;
            } else {
                main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
            }
            break;
        }
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_WAIT:
        if (!main_file_is_still_loading()) {
            g_battle_gfx_unit_graphics_load_state.phase++;
        }
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_INSTALL:
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_QUEUED_LOAD_SHP:
            g_battle_gfx_shp_frame_data_cursor = g_battle_gfx_fallback_shp_frame_data;
            battle_gfx_unpack_unit_shp_data(
                &g_battle_gfx_boss_shp_frame_tables, g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_QUEUED_LOAD_SEQ:
            g_battle_gfx_seq_data_cursor = g_battle_gfx_fallback_seq_data;
            battle_gfx_unpack_unit_seq_data(
                &g_battle_gfx_boss_seq_tables, g_battle_gfx_unit_graphics_load_state.buffer);
            break;
        case BATTLE_GFX_QUEUED_LOAD_SPR:
            battle_unit_init(
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].map_x,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].map_y,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].map_level,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].facing,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index]
                    .spritesheet_id,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].palette_id,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].runtime_id,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].stats,
                g_main_gfx_unit_graphics_load_queue.descriptors[g_battle_gfx_unit_graphics_load_state.index].flags,
                g_battle_gfx_unit_graphics_load_state.buffer);
            battle_unit_update_and_animate_units_three_times();
            break;
        }
        g_battle_gfx_unit_graphics_load_state.phase++;
        break;
    case BATTLE_GFX_UNIT_LOAD_PHASE_FINISH:
        if (g_battle_gfx_unit_graphics_load_state.buffer != 0) {
            main_heap_call_free(g_battle_gfx_unit_graphics_load_state.buffer);
        }
        g_battle_gfx_unit_graphics_load_state.phase = BATTLE_GFX_UNIT_LOAD_PHASE_START_READ;
        switch (g_battle_gfx_unit_graphics_load_state.file_kind) {
        case BATTLE_GFX_QUEUED_LOAD_SHP:
        case BATTLE_GFX_QUEUED_LOAD_SEQ:
            g_battle_gfx_unit_graphics_load_state.file_kind++;
            break;
        case BATTLE_GFX_QUEUED_LOAD_SPR:
            g_battle_gfx_unit_graphics_load_state.file_kind = BATTLE_GFX_QUEUED_LOAD_SHP;
            g_battle_gfx_unit_graphics_load_state.index++;
            break;
        }
        result = 2;
        break;
    }
    return (g_battle_gfx_unit_graphics_load_state.index < g_main_gfx_unit_graphics_load_queue.count) ? result : 0;
}

/* Cycles the colors of the two deployed-unit palettes (sprite CLUTs 1 and 2).
 *
 * Every fourth animation tick the offset advances through 0..14 and the palettes
 * are uploaded again with colors 1..15 rotated down by that many places; color
 * 0 stays in place, and offset 0 uploads the palettes as stored. */
void battle_gfx_update_unit_palette_animation(void) {
    s32 dst;
    s32 src;
    palette_t* palettes;
    RECT rect; /* never used; retail reserves its 8 bytes */

    g_battle_unit_palette_animation_timer += g_main_system_animation_speed;
    if (g_battle_unit_palette_animation_timer >= 4) {
        g_battle_unit_palette_animation_timer = 0;
        g_battle_unit_palette_animation_offset++;
        if (g_battle_unit_palette_animation_offset >= 15) {
            g_battle_unit_palette_animation_offset = 0;
        }
        if (g_battle_unit_palette_animation_offset != 0) {
            /* Bytewise rotation retains the native stores and first-color 16-bit load. */
            /* dst before src: the reverse order changes the copy loop registers. */
            dst = 2;
            src = g_battle_unit_palette_animation_offset * 2 + 2;
            ((u8*)g_battle_gfx_cycled_deployed_palettes[0])[0] = ((u8*)g_battle_deployed_unit_palettes)[0];
            ((u8*)g_battle_gfx_cycled_deployed_palettes[0])[1] = ((u8*)g_battle_deployed_unit_palettes)[1];
            ((u8*)g_battle_gfx_cycled_deployed_palettes[1])[0] = *(u16*)&g_battle_deployed_unit_palettes[1][0];
            ((u8*)g_battle_gfx_cycled_deployed_palettes[1])[1] = ((u8*)g_battle_deployed_unit_palettes[1])[1];
            /* Colors offset + 1..15 move down to 1.., and colors 1..offset wrap to the top. */
            for (; src < 32; src++, dst++) {
                ((u8*)g_battle_gfx_cycled_deployed_palettes[0])[dst] = ((u8*)g_battle_deployed_unit_palettes)[src];
                ((u8*)g_battle_gfx_cycled_deployed_palettes[1])[dst] = ((u8*)g_battle_deployed_unit_palettes[1])[src];
            }
            for (src = 2, dst = 32 - g_battle_unit_palette_animation_offset * 2;
                src < g_battle_unit_palette_animation_offset * 2 + 2; src++, dst++) {
                ((u8*)g_battle_gfx_cycled_deployed_palettes[0])[dst] = ((u8*)g_battle_deployed_unit_palettes)[src];
                ((u8*)g_battle_gfx_cycled_deployed_palettes[1])[dst] = ((u8*)g_battle_deployed_unit_palettes[1])[src];
            }
            palettes = g_battle_gfx_cycled_deployed_palettes;
        } else {
            palettes = g_battle_deployed_unit_palettes;
        }
        battle_map_load_palette_data(palettes, MAP_CLUT_ROW_SPRITES, 1, 0);
        battle_map_load_palette_data(palettes + 1, MAP_CLUT_ROW_SPRITES, 2, 0);
    }
}
