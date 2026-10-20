/* Select unit animations for actions, charging, statuses and target responses. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/ai.h"
#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/battle_state.h"
#include "fft/effect_secondary.h"
#include "fft/item.h"
#include "fft/move.h"
#include "fft/projectile.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/types.h"

/* sound_sfx_e by the weapon's item_type_e (0x20 entries each): the swing, the
 * hit, and the hit a shield or Golem stops (action_play_weapon_sfx). */
extern u8 g_action_weapon_swing_sfx_ids[];
extern u8 g_action_weapon_hit_sfx_ids[];
extern u8 g_action_shield_deflect_sfx_ids[];
extern clut_t g_sprite_weapon_clut;

/* Two animation_e for each set: the one that starts a charge and
 * the one held while the unit charges or performs; 0 for none. */
extern u8 g_animation_ability_charge_sets[ANIMATION_CHARGE_SET_COUNT][ANIMATION_CHARGE_VARIANT_COUNT];

/* Three animation_e variants per item_type_e; animation_weapon_attack_select chooses the column. */
extern u8 g_animation_weapon_attacks[][3];

/* Play the weapon swing sound (`hit` 0) or the sound of each strike target's
 * outcome (`hit` 1).
 *
 * The sound tables cover the 0x20 weapon item types, so shuriken and the later
 * item types use the knife's sounds. A target's miss type picks its sound: a
 * hand or Golem guard plays the weapon's deflect sound; an accessory or class
 * evasion, a forced failure, Blade Grasp or a catch the error sound; a
 * nullified, missed or canceled action none; anything else (a hit or a
 * reflection) the weapon's hit sound or, when critical, SOUND_SFX_CRITICAL_HIT.
 * A target without battle stats is skipped. */
void action_play_weapon_sfx(unit_runtime_t* unit, s32 hit) {
    unit_runtime_t* target;
    s32 weapon_type = item_data_get(unit->strike.used_weapon_id)->type;
    s32 i;

    if (weapon_type >= ITEM_TYPE_SHURIKEN) {
        weapon_type = ITEM_TYPE_KNIFE;
    }
    switch (hit) {
    case 0:
        unit_sfx_play(unit, g_action_weapon_swing_sfx_ids[weapon_type]);
        break;
    case 1:
        for (i = 0; i < unit->strike.target_count; i++) {
            target = unit_battle_get(unit->strike.target_list[i]);
            if (target == 0 || target->stats == 0) {
                continue;
            }
            switch (target->stats->action.miss_type) {
            case ACTION_MISS_TYPE_RIGHT_HAND_EVADE:
            case ACTION_MISS_TYPE_LEFT_HAND_EVADE:
            case ACTION_MISS_TYPE_GOLEM_GUARD:
                unit_sfx_play(unit, g_action_shield_deflect_sfx_ids[weapon_type]);
                break;
            case ACTION_MISS_TYPE_ACCESSORY_EVADE:
            case ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD:
            case ACTION_MISS_TYPE_FORCED_FAILURE:
            case ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD:
            case ACTION_MISS_TYPE_CATCH:
                unit_sfx_play(unit, SOUND_SFX_ERROR_MESSAGE);
                break;
            case ACTION_MISS_TYPE_NULLIFIED:
            case ACTION_MISS_TYPE_ACCURACY_MISS:
            case ACTION_MISS_TYPE_CANCELED:
                break;
            default:
                if (target->stats->action.critical != 0) {
                    unit_sfx_play(unit, SOUND_SFX_CRITICAL_HIT);
                } else {
                    unit_sfx_play(unit, g_action_weapon_hit_sfx_ids[weapon_type]);
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
static u8 water_mode_get(unit_runtime_t* unit) {
    battle_map_tile_t* tile;
    s32 movement;
    u32 statuses;

    tile = battle_map_tile_get_pointer(
        unit->world_coords.vx / BATTLE_MAP_TILE_SIZE, unit->world_coords.vz / BATTLE_MAP_TILE_SIZE, unit->map_layer);
    if (unit->stats != 0) {
        movement = move_get_effective_flags(unit->stats);
        if (movement & MOVE_EFFECTIVE_ON_WATER) {
            return 0;
        }
        statuses = unit->status_flags_5_6;
        if (statuses & UNIT_STATUS_FLOAT) {
            return 0;
        }
        if (statuses & (UNIT_STATUS_CHICKEN | UNIT_STATUS_FROG | UNIT_STATUS_TREASURE)) {
            return 1;
        }
        if (movement & MOVE_EFFECTIVE_IN_WATER) {
            return 1;
        }
        if (statuses & UNIT_STATUS_CRYSTAL) {
            return 0;
        }
    }
    return tile->depth_half_height.depth;
}

/* Choose the attacker's weapon attack animation.
 *
 * Each item type has three animation variants. Bows select their table
 * column from the last trajectory test. Other weapons compare the vertical
 * centers of the two sprites: a target 12 or more above the attacker takes
 * the first animation and one 12 or more below the third. A Golem or Blade
 * Grasp guard always takes the first, and the sheets from the crystal up
 * always play the unarmed level attack. */
void animation_weapon_attack_select(unit_runtime_t* attacker, unit_runtime_t* target) {
    s32 miss_type;
    s16 attacker_center;
    s16 target_center;
    s32 difference;

    if (attacker == 0) {
        return;
    }
    if (target != 0) {
        if (attacker->spritesheet_id >= SPRITE_SHEET_ID_CRYSTAL) {
            animation_facing_force(g_animation_weapon_attacks[0][1], attacker->facing, attacker);
            return;
        }
        if (attacker->equipped_weapon_type == ITEM_TYPE_BOW) {
            animation_facing_force(g_animation_weapon_attacks[ITEM_TYPE_BOW][projectile_bow_animation_column_get()],
                attacker->facing, attacker);
            return;
        }
        miss_type = target->stats->action.miss_type;
        if (miss_type != ACTION_MISS_TYPE_HIT) {
            if (miss_type >= ACTION_MISS_TYPE_REFLECTED) {
                if (miss_type < ACTION_MISS_TYPE_BUGGED_REFLECTION) {
                    if (miss_type >= ACTION_MISS_TYPE_GOLEM_GUARD) {
                        animation_facing_force(
                            g_animation_weapon_attacks[attacker->equipped_weapon_type][0], attacker->facing, attacker);
                        return;
                    }
                }
            }
        }
        /* The target loads the attacker's world_coords.vy zero-extended (lhu). */
        attacker_center = sprite_runtime_sheet_height_get(attacker->runtime_id) / 2 - attacker->world_coords.vy;
        target_center = sprite_runtime_sheet_height_get(target->runtime_id) / 2 - target->world_coords.vy;
        difference = attacker_center - target_center;
        if (difference < -11) {
            animation_facing_force(
                g_animation_weapon_attacks[attacker->equipped_weapon_type][0], attacker->facing, attacker);
            return;
        }
        if (difference >= 12) {
            animation_facing_force(
                g_animation_weapon_attacks[attacker->equipped_weapon_type][2], attacker->facing, attacker);
            return;
        }
    }
    animation_facing_force(g_animation_weapon_attacks[attacker->equipped_weapon_type][1], attacker->facing, attacker);
}

/* Start the animation of an item ability: the item held up for a target on
 * the unit's own tile or beside it, thrown for one further away. */
static void item_ability_animation_set(unit_runtime_t* unit, unit_runtime_t* target) {
    s32 dx;
    s32 dy;

    sprite_display_store_item(unit, unit->used_item_or_weapon_id);
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
        animation_facing_store(ANIMATION_HOLD_ITEM, unit->facing, unit);
    } else {
        unit->item_display_active = 0;
        animation_facing_force(ANIMATION_THROW_ITEM, unit->facing, unit);
    }
}

/* Start the first animation of the used ability's charge set, if it has one. */
void animation_ability_charge_start(unit_runtime_t* unit) {
    u8 set_id = g_ability_animation_data[unit->used_ability_id].charge_animation_set_id;
    u8 animation = g_animation_ability_charge_sets[set_id][0];

    if (animation != 0) {
        animation_facing_store(animation, unit->facing, unit);
    }
}

/* Start the second animation of the used ability's charge set, the one held
 * while charging or performing; the submerged idle in water two or more deep. */
void animation_ability_charge_hold_start(unit_runtime_t* unit) {
    u8 set_id = g_ability_animation_data[unit->used_ability_id].charge_animation_set_id;
    u8 animation = g_animation_ability_charge_sets[set_id][1];
    u8 water_depth;
    s32 selected_animation;

    if (animation != 0) {
        water_depth = water_mode_get(unit);
        selected_animation = ANIMATION_SUBMERGED_IDLE;
        if (water_depth < 2) {
            selected_animation = animation;
        }
        animation_facing_store(selected_animation, unit->facing, unit);
    }
}

/* Start the attacker's animation for the ability it uses: the cast for the
 * RUKA, ARUTE and KANZEN sequences, else the ability table's attack
 * animation, which may ask for the weapon's attack or the item animation. */
void animation_ability_attack_select(unit_runtime_t* attacker, unit_runtime_t* target) {
    s32 animation;
    s32 seq_type = g_sprite_sheet_data[attacker->spritesheet_id].seq_id;

    if (seq_type <= SPRITE_SEQ_KANZEN) {
        if (seq_type >= SPRITE_SEQ_RUKA) {
            animation_facing_store(ANIMATION_CAST, attacker->facing, attacker);
            return;
        }
    }

    animation = g_ability_animation_data[attacker->used_ability_id].attack_animation;
    if (animation == ANIMATION_ATTACK_USE_WEAPON) {
        animation_weapon_attack_select(attacker, target);
        return;
    }
    if (animation == ANIMATION_ATTACK_USE_ITEM) {
        item_ability_animation_set(attacker, target);
        return;
    }
    animation_facing_store(animation, attacker->facing, attacker);
}

/* Start a unit's level-up animation: the jump on dry land, the hops for a
 * rider and in water one deep, the submerged idle in water two deep. */
void animation_level_up_set(unit_runtime_t* unit) {
    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        animation_facing_store(ANIMATION_LEVEL_UP_HOP, unit->facing, unit);
        return;
    }
    switch (water_mode_get(unit)) {
    case 0:
        animation_facing_store(ANIMATION_LEVEL_UP, unit->facing, unit);
        break;
    case 1:
        animation_facing_store(ANIMATION_LEVEL_UP_HOP, unit->facing, unit);
        break;
    case 2:
        animation_facing_store(ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
        break;
    }
}

/* Give a rider the riding animation and any other unit the standing one, or
 * the submerged idle in water two or more deep; a mount on land also puts
 * its rider back in the riding animation. The depth is a byte, so its
 * negative test never holds. */
void animation_stand_set(unit_runtime_t* unit) {
    s32 anim = ANIMATION_RIDING;
    s32 selected;
    s32 water_depth;
    unit_runtime_t* other;

    if (unit->mount_state != UNIT_MOUNT_STATE_RIDER) {
        water_depth = water_mode_get(unit);
        if (water_depth < 0) {
            selected = ANIMATION_SUBMERGED_IDLE;
        } else if (water_depth < 2) {
            selected = ANIMATION_STAND;
            if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
                other = unit_runtime_get(unit->mount_partner_runtime_id);
                if (other != 0) {
                    animation_facing_store(ANIMATION_RIDING, unit->facing, other);
                }
            }
        } else {
            selected = ANIMATION_SUBMERGED_IDLE;
        }
        if (selected == 0) {
            return;
        }
        anim = selected;
    }
    animation_facing_store(anim, unit->facing, unit);
}

/*
 * Select the animation of a unit that is walking: riding for a rider, else
 * the walk of its walk speed (fast from 0x3000, slow up to 0x1400), or the
 * submerged idle of that speed in water two or more deep. A mount on land
 * also puts its rider in the riding animation.
 */
void animation_walk_set(unit_runtime_t* unit) {
    s32 anim;
    s32 walk_speed;
    unit_runtime_t* rider;

    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        animation_facing_store(ANIMATION_RIDING, unit->facing, unit);
        return;
    }
    switch (water_mode_get(unit)) {
    case 0:
    case 1:
        walk_speed = unit->walk_speed;
        if (walk_speed >= 0x3000) {
            anim = ANIMATION_WALK_FAST;
        } else {
            anim = ANIMATION_WALK;
            if (walk_speed < 0x1401) {
                anim = ANIMATION_WALK_SLOW;
            }
        }
        if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
            rider = unit_runtime_get(unit->mount_partner_runtime_id);
            if (rider != 0) {
                animation_facing_store(ANIMATION_RIDING, unit->facing, rider);
            }
        }
        break;
    case 2:
    default:
        walk_speed = unit->walk_speed;
        if (walk_speed >= 0x3000) {
            anim = ANIMATION_SUBMERGED_IDLE_HASTE;
        } else {
            anim = ANIMATION_SUBMERGED_IDLE;
            if (walk_speed < 0x1401) {
                anim = ANIMATION_SUBMERGED_IDLE_SLOW;
            }
        }
        break;
    }
    if (anim != 0) {
        animation_facing_store(anim, unit->facing, unit);
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
 * The two animation 0x21 branches test bit 0 of status_flags_1_4, which normal
 * status updates never set. Crystal's bit 0 belongs to status_flags_5_6.
 */
void animation_status_set(unit_runtime_t* unit) {
    s32 animation;
    u8 water_depth = water_mode_get(unit);
    u8 spritesheet_id;

    animation = 0;
    switch (water_depth) {
    case 0:
    case 1:
        if (unit->status_flags_5_6 & UNIT_STATUS_CRYSTAL) {
            animation = ANIMATION_SUBMERGED_IDLE;
        } else if (unit->status_flags_5_6 & UNIT_STATUS_TREASURE) {
            animation = ANIMATION_TREASURE;
        } else if ((unit->status_flags_1_4 & UNIT_STATUS_ANIMATION_SELECTION_MASK) == 0) {
            animation = ANIMATION_IDLE_FLYING;
            if (sprite_sheet_get_flying_flag(unit->spritesheet_id) == 0) {
                animation = ANIMATION_IDLE;
            }
        } else {
            spritesheet_id = unit->spritesheet_id;
            if (g_sprite_sheet_data[spritesheet_id].seq_id >= SPRITE_SEQ_RUKA) {
                if (unit->status_flags_1_4 & UNIT_STATUS_DEAD) {
                    animation = ANIMATION_DEAD;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_STOP) {
                    animation = ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_SLEEP) {
                    animation = ANIMATION_SLEEP;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_PETRIFY) {
                    animation = ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_CONFUSION) {
                    animation = ANIMATION_CONFUSION;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_PERFORMING) {
                    unit->used_ability_id = unit->stats->action_command.ability_id;
                    animation_ability_charge_hold_start(unit);
                } else if (unit->status_flags_1_4 & UNIT_STATUS_CHARGING) {
                    unit->used_ability_id = unit->stats->action_command.ability_id;
                    animation_ability_charge_hold_start(unit);
                } else if (unit->status_flags_1_4 & UNIT_STATUS_DEFENDING) {
                    animation = ANIMATION_DEFEND;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_SLOW) {
                    animation = ANIMATION_IDLE_FLYING_SLOW;
                    if (sprite_sheet_get_flying_flag(unit->spritesheet_id) == 0) {
                        animation = ANIMATION_IDLE_SLOW;
                    }
                } else if (unit->status_flags_1_4 & UNIT_STATUS_HASTE) {
                    animation = ANIMATION_IDLE_FLYING_HASTE;
                    if (sprite_sheet_get_flying_flag(unit->spritesheet_id) == 0) {
                        animation = ANIMATION_IDLE_HASTE;
                    }
                } else if (unit->status_flags_1_4 & UNIT_STATUS_UNNAMED_0E) {
                    animation = ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & 1) {
                    animation = 0x21;
                } else {
                    animation = ANIMATION_IDLE_FLYING;
                    if (sprite_sheet_get_flying_flag(unit->spritesheet_id) == 0) {
                        animation = ANIMATION_IDLE;
                    }
                }
            } else {
                if (unit->status_flags_1_4 & UNIT_STATUS_DEAD) {
                    animation = ANIMATION_DEAD;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_MOUNTED) {
                    animation = ANIMATION_RIDING;
                } else if ((unit->status_flags_1_4 & UNIT_STATUS_CRITICAL)
                    && unit->mount_state != UNIT_MOUNT_STATE_MOUNT) {
                    animation = ANIMATION_SLEEP;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_STOP) {
                    animation = ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_SLEEP) {
                    animation = ANIMATION_SLEEP;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_PETRIFY) {
                    animation = ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_CONFUSION) {
                    animation = ANIMATION_CONFUSION;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_PERFORMING) {
                    unit->used_ability_id = unit->stats->action_command.ability_id;
                    animation_ability_charge_hold_start(unit);
                } else if (unit->status_flags_1_4 & UNIT_STATUS_CHARGING) {
                    unit->used_ability_id = unit->stats->action_command.ability_id;
                    animation_ability_charge_hold_start(unit);
                } else if (unit->status_flags_1_4 & UNIT_STATUS_DEFENDING) {
                    animation = ANIMATION_DEFEND;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_SLOW) {
                    animation = ANIMATION_IDLE_FLYING_SLOW;
                    if (sprite_sheet_get_flying_flag(unit->spritesheet_id) == 0) {
                        animation = ANIMATION_IDLE_SLOW;
                    }
                } else if (unit->status_flags_1_4 & UNIT_STATUS_HASTE) {
                    animation = ANIMATION_IDLE_FLYING_HASTE;
                    if (sprite_sheet_get_flying_flag(unit->spritesheet_id) == 0) {
                        animation = ANIMATION_IDLE_HASTE;
                    }
                } else if (unit->status_flags_1_4 & UNIT_STATUS_UNNAMED_0E) {
                    animation = ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & 1) {
                    animation = 0x21;
                } else {
                    animation = ANIMATION_IDLE_FLYING;
                    if (sprite_sheet_get_flying_flag(unit->spritesheet_id) == 0) {
                        animation = ANIMATION_IDLE;
                    }
                }
            }
        }
        break;
    case 2:
    default:
        if ((unit->status_flags_5_6 & UNIT_STATUS_TRANSFORMATION_MASK) != 0) {
            if (unit->status_flags_5_6 & UNIT_STATUS_CRYSTAL) {
                animation = ANIMATION_SUBMERGED_IDLE;
            } else if (unit->status_flags_5_6 & UNIT_STATUS_TREASURE) {
                animation = ANIMATION_TREASURE;
            }
        } else if ((unit->status_flags_1_4 & UNIT_STATUS_ANIMATION_SELECTION_MASK) != 0) {
            if (unit->status_flags_1_4 & UNIT_STATUS_DEAD) {
                animation = ANIMATION_DEAD;
            } else if (unit->status_flags_1_4 & UNIT_STATUS_MOUNTED) {
                animation = ANIMATION_RIDING;
            } else if (unit->status_flags_1_4 & UNIT_STATUS_HASTE) {
                animation = ANIMATION_SUBMERGED_IDLE_HASTE;
            } else {
                animation = ANIMATION_SUBMERGED_IDLE;
                if (unit->status_flags_1_4 & UNIT_STATUS_SLOW) {
                    animation = ANIMATION_SUBMERGED_IDLE_SLOW;
                }
            }
        } else {
            animation = ANIMATION_SUBMERGED_IDLE;
        }
        break;
    }
    if (animation != 0) {
        animation_facing_store(animation, unit->facing, unit);
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
 * ANIMATION_DEATH or _REVIVE; in water two or more deep, in
 * events too, the unit is put straight into its dead or its submerged idle
 * sequence, with the wait cleared so that the sequence starts at once.
 */
void animation_status_changes_update(unit_runtime_t* unit) {
    unit_stats_t* stats;

    if (unit->statuses_to_add_5_6 & UNIT_STATUS_CHICKEN) {
        animation_facing_force(ANIMATION_IDLE, unit->facing, unit);
    } else if (unit->statuses_to_remove_5_6 & UNIT_STATUS_CHICKEN) {
        animation_status_set(unit);
    }
    if (unit->statuses_to_add_5_6 & UNIT_STATUS_FROG) {
        animation_facing_force(ANIMATION_IDLE, unit->facing, unit);
    } else if (unit->statuses_to_remove_5_6 & UNIT_STATUS_FROG) {
        animation_status_set(unit);
    }
    if (unit->statuses_to_add_1_4 & UNIT_STATUS_DEAD) {
        if (g_sprite_sheet_data[unit->spritesheet_id].seq_id == SPRITE_SEQ_CYOKO
            || g_sprite_sheet_data[unit->spritesheet_id].seq_id == SPRITE_SEQ_MON) {
            effect_secondary_death_smoke_start(unit);
        }
        if (g_battle_state != BATTLE_STATE_EVENT) {
            stats = unit->stats;
            if (stats != 0) {
                if (stats->unit_flags & UNIT_FLAG_MALE) {
                    unit_sfx_play(unit, SOUND_SFX_DEATH_MALE);
                } else if (stats->unit_flags & UNIT_FLAG_FEMALE) {
                    unit_sfx_play(unit, SOUND_SFX_DEATH_FEMALE);
                } else if (stats->unit_flags & UNIT_FLAG_MONSTER) {
                    unit_sfx_play(unit, SOUND_SFX_DEATH_MONSTER);
                }
            }
        }
    }
    if (unit->statuses_to_add_5_6 & (UNIT_STATUS_CRYSTAL | UNIT_STATUS_TREASURE)) {
        effect_secondary_zodiac_poof_start(unit);
    }
    if (g_battle_state == BATTLE_STATE_EVENT) {
        if (unit->statuses_to_add_5_6 & UNIT_STATUS_JUMP) {
            animation_facing_force(ANIMATION_JUMP_DEPART, unit->facing, unit);
        } else if (unit->statuses_to_remove_5_6 & UNIT_STATUS_JUMP) {
            unit_runtime_show(unit->runtime_id);
            animation_facing_force(ANIMATION_JUMP_LAND, unit->facing, unit);
        }
        if (water_mode_get(unit) >= 2) {
            if (unit->statuses_to_add_1_4 & UNIT_STATUS_DEAD) {
                animation_facing_force(ANIMATION_DEAD, unit->facing, unit);
                unit->animation.sequence_id = ANIMATION_DEAD << 1;
                unit->animation.wait = 0;
            } else if (unit->statuses_to_remove_1_4 & UNIT_STATUS_DEAD) {
                animation_facing_force(ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
                unit->animation.sequence_id = ANIMATION_SUBMERGED_IDLE << 1;
                unit->animation.wait = 0;
            }
        }
    } else if (water_mode_get(unit) < 2) {
        if (unit->statuses_to_add_1_4 & UNIT_STATUS_DEAD) {
            animation_facing_store(ANIMATION_DEATH, unit->facing, unit);
        } else if (unit->statuses_to_remove_1_4 & UNIT_STATUS_DEAD) {
            animation_facing_store(ANIMATION_REVIVE, unit->facing, unit);
        }
    } else if (unit->statuses_to_add_1_4 & UNIT_STATUS_DEAD) {
        animation_facing_force(ANIMATION_DEAD, unit->facing, unit);
        unit->animation.sequence_id = ANIMATION_DEAD << 1;
        unit->animation.wait = 0;
    } else if (unit->statuses_to_remove_1_4 & UNIT_STATUS_DEAD) {
        animation_facing_force(ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
        unit->animation.sequence_id = ANIMATION_SUBMERGED_IDLE << 1;
        unit->animation.wait = 0;
    }
}

/* Carries out a pending Poached or Morbol display flag (status_flags_5_6) and
 * clears it; Poached is tested first and only one is handled per call.
 *
 * Poached configures the unit's palette modulation
 * (unit_fade_out_start_red). Morbol swaps the sprite
 * for the Morbol spritesheet (VRAM slot, SHP, SEQ, texture page), refreshes the
 * tile palette and the idle animation, then rewrites the battle stats as a Morbol
 * and reinitializes the unit's AI ability data. */
void unit_apply_poach_morbol_transformation(unit_runtime_t* unit) {
    u32 flags = unit->status_flags_5_6;
    battle_map_tile_t* tile;

    if ((flags & (UNIT_STATUS_POACHED | UNIT_STATUS_MORBOL)) == 0) {
        return;
    }
    if (flags & UNIT_STATUS_POACHED) {
        unit->status_flags_5_6 = flags & ~UNIT_STATUS_POACHED;
        unit_fade_out_start_red(unit->runtime_id);
        return;
    }
    if ((flags & UNIT_STATUS_MORBOL) == 0) {
        return;
    }
    unit->status_flags_5_6 = flags & ~UNIT_STATUS_MORBOL;
    unit->special_graphic_y_offset = 0;
    unit->spritesheet_id = SPRITE_SHEET_ID_MORBOL;
    unit->spritesheet_vram_slot = sprite_sheet_claim_slot(SPRITE_SHEET_ID_MORBOL);
    unit->animation.shp = sprite_sheet_get_unit_shp(SPRITE_SHEET_ID_MORBOL);
    unit->animation.seq = sprite_sheet_get_unit_seq(SPRITE_SHEET_ID_MORBOL);
    unit->stored_palette = 0;
    unit->tpage = unit->spritesheet_vram_slot + SPRITE_TPAGE_SPRITESHEET_SLOT_0;
    tile = battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_layer);
    sprite_palette_check_tile_status_mod(unit, tile->flags.shading, 1);
    animation_facing_force(ANIMATION_IDLE, unit->facing, unit);
    unit_transform_into_morbol(unit->stats);
    ai_unit_abilities_init(unit->stats->battle_id);
}

/* Swap the unit's sheet for its first transformation status (Crystal, Treasure, Chicken, Frog),
 * saving its own SHP and SEQ unless it already shows one; Crystal and Treasure also clear every
 * other status. Without such a status, a swapped unit gets back the sheet of its VRAM slot. */
void sprite_sheet_apply_status_change(unit_runtime_t* unit, s32 tile_effect_level) {
    u32 flags = unit->status_flags_5_6;
    u16 spritesheet_id;
    u8 original_id;

    if (flags & UNIT_STATUS_TRANSFORMATION_MASK) {
        if (flags & UNIT_STATUS_CRYSTAL) {
            spritesheet_id = SPRITE_SHEET_ID_CRYSTAL;
            unit->special_graphic_y_offset = 0x60;
            unit->status_flags_5_6 = UNIT_STATUS_CRYSTAL;
            unit->status_flags_1_4 = 0;
            animation_facing_store(ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
        } else if (flags & UNIT_STATUS_TREASURE) {
            spritesheet_id = SPRITE_SHEET_ID_TREASURE;
            unit->special_graphic_y_offset = 0;
            unit->status_flags_5_6 = UNIT_STATUS_TREASURE;
            unit->status_flags_1_4 = 0;
            animation_facing_store(ANIMATION_TREASURE, unit->facing, unit);
        } else if (flags & UNIT_STATUS_CHICKEN) {
            spritesheet_id = SPRITE_SHEET_ID_CHICKEN;
            unit->special_graphic_y_offset = 0;
        } else if (flags & UNIT_STATUS_FROG) {
            spritesheet_id = SPRITE_SHEET_ID_FROG;
            unit->special_graphic_y_offset = 0x30;
        }
        if (unit->spritesheet_id != spritesheet_id) {
            if (unit->spritesheet_id < SPRITE_SHEET_ID_CRYSTAL) {
                unit->animation.saved_shp = unit->animation.shp;
                unit->animation.saved_seq = unit->animation.seq;
            }
            unit->animation.shp = sprite_sheet_get_unit_shp(spritesheet_id);
            unit->animation.seq = sprite_sheet_get_unit_seq(spritesheet_id);
            unit->spritesheet_id = spritesheet_id;
            sprite_palette_tint_unit(unit, tile_effect_level, 1);
            animation_status_set(unit);
            sprite_unit_frame_load(unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
        }
    } else if (unit->spritesheet_id != g_sprite_sheet_slots[unit->spritesheet_vram_slot].spritesheet_id) {
        unit->special_graphic_y_offset = 0;
        original_id = g_sprite_sheet_slots[unit->spritesheet_vram_slot].spritesheet_id;
        unit->animation.shp = unit->animation.saved_shp;
        unit->animation.seq = unit->animation.saved_seq;
        unit->spritesheet_id = original_id;
        sprite_palette_check_tile_status_mod(unit, tile_effect_level, 1);
        animation_status_set(unit);
        sprite_unit_frame_load(unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
    }
}

/* Make item `item_id` the one a unit's weapon and effect blocks draw.
 *
 * The item's two palettes of g_sprite_item_graphic_data go to the unit's
 * CLUTs of the ITEM_A and ITEM_B rows, and its type selects the frame set of
 * the weapon SHP; a thrown weapon, any Throw ability and Throw Stone use
 * set 0x15. */
void sprite_thrown_item_palette_set(s32 item_id, unit_runtime_t* unit) {
    const item_data_t* item;
    u32 weapon_type;
    s32 clut_index;
    /* The last test re-reads the ability id rather than reusing the value the
     * range test loaded; assigning it through its own local stops cse folding
     * the two reads into one. */
    u16 ability_id;

    item = item_data_get(item_id);
    weapon_type = item->type;
    if ((weapon_type >= ITEM_TYPE_SHURIKEN)
        || ((u32)(unit->used_ability_id - ABILITY_ID_THROW_FIRST) < ABILITY_ID_JUMP_FIRST - ABILITY_ID_THROW_FIRST)
        || (ability_id = unit->used_ability_id) == ABILITY_ID_BASIC_SKILL_THROW_STONE) {
        weapon_type = 0x15;
    }
    clut_index = unit->runtime_id;
    battle_map_palette_load_data(&g_sprite_weapon_clut[g_sprite_item_graphic_data[item_id].palettes >> 4],
        BATTLE_MAP_CLUT_ROW_ITEM_A, clut_index, 0);
    battle_map_palette_load_data(&g_sprite_weapon_clut[g_sprite_item_graphic_data[item_id].palettes & 0xF],
        BATTLE_MAP_CLUT_ROW_ITEM_B, clut_index, 0);
    unit->used_item_or_weapon_id = item_id;
    unit->equipped_weapon_type = weapon_type;
}

/* A plain call through to animation_status_set, used by the
 * battle state handlers. */
void animation_status_set_on_main_stack(unit_runtime_t* unit) {
    animation_status_set(unit);
}

/* When the target blocked with a hand or a golem took the blow, replace the
 * attacker's unarmed, swing or thrust animation with its recoil at the same
 * height. Only the TYPE1 and TYPE2 sheets have the recoils. */
void animation_shield_block_attacker_set(unit_runtime_t* attacker, unit_runtime_t* target) {
    unit_stats_t* stats;
    s32 miss_type;
    s32 anim;

    stats = target->stats;
    if (stats == 0) {
        return;
    }
    if (g_sprite_sheet_data[attacker->spritesheet_id].shp_id >= SPRITE_SHP_CYOKO) {
        return;
    }
    miss_type = stats->action.miss_type;
    anim = 0;
    if (miss_type >= ACTION_MISS_TYPE_RIGHT_HAND_EVADE
        && (miss_type < ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD || miss_type == ACTION_MISS_TYPE_GOLEM_GUARD)) {
        switch (attacker->animation.sequence_id >> 1) {
        case ANIMATION_UNARMED_ATTACK_HIGH:
            anim = ANIMATION_UNARMED_ATTACK_BLOCKED_HIGH;
            break;
        case ANIMATION_UNARMED_ATTACK_LEVEL:
            anim = ANIMATION_UNARMED_ATTACK_BLOCKED_LEVEL;
            break;
        case ANIMATION_UNARMED_ATTACK_LOW:
            anim = ANIMATION_UNARMED_ATTACK_BLOCKED_LOW;
            break;
        case ANIMATION_SWING_HIGH:
            anim = ANIMATION_SWING_BLOCKED_HIGH;
            break;
        case ANIMATION_SWING_LEVEL:
            anim = ANIMATION_SWING_BLOCKED_LEVEL;
            break;
        case ANIMATION_SWING_LOW:
            anim = ANIMATION_SWING_BLOCKED_LOW;
            break;
        case ANIMATION_THRUST_HIGH:
            anim = ANIMATION_THRUST_BLOCKED_HIGH;
            break;
        case ANIMATION_THRUST_LEVEL:
            anim = ANIMATION_THRUST_BLOCKED_LEVEL;
            break;
        case ANIMATION_THRUST_LOW:
            anim = ANIMATION_THRUST_BLOCKED_LOW;
            break;
        }
    }
    if (anim != 0) {
        animation_facing_force(anim, attacker->facing, attacker);
    }
}

/*
 * Pick and start the reaction animation a target plays after an action
 * resolves, then apply the action's special-effect side effects.
 *
 * A target already dead or dying is left alone. In water two or more deep it
 * takes the submerged idle, and a target without battle stats the hurt
 * animation. Otherwise the miss type decides: a hand guard turns to the
 * attacker and parries at the attacker's relative height (the same 12-unit
 * thresholds as animation_weapon_attack_select), an evasion with
 * the dodge display flag dodges, Blade Grasp and catches raise the hands, and
 * a hit hurts on damage or raises the hands on healing. The sheets from the
 * crystal up play only the hurt animation. Knockback, Poach and Morbol start
 * here, and a broken or stolen item rebuilds the target's AI ability data.
 */
void action_start_target_response(unit_runtime_t* attacker, unit_runtime_t* target) {
    unit_stats_t* stats;
    action_data_t* action;
    s32 animation;
    s32 difference;
    s16 attacker_center;
    s16 target_center;
    u32 water_depth;

    if (attacker == target) {
        return;
    }
    if ((target->animation.sequence_id >> 1) == ANIMATION_DEAD) {
        return;
    }
    if ((target->animation.sequence_id >> 1) == ANIMATION_DEATH) {
        return;
    }
    water_depth = water_mode_get(target);
    animation = ANIMATION_SUBMERGED_IDLE;
    if (water_depth < 2) {
        stats = target->stats;
        if (stats != 0) {
            action = &stats->action;
            animation = ANIMATION_STAND;
            switch (action->miss_type) {
            case ACTION_MISS_TYPE_RIGHT_HAND_EVADE:
            case ACTION_MISS_TYPE_LEFT_HAND_EVADE:
                unit_facing_turn_to_target(target, attacker);
                sprite_thrown_item_palette_set(action->item_lost, target);
                attacker_center = sprite_runtime_sheet_height_get(attacker->runtime_id) / 2 - attacker->world_coords.vy;
                target_center = sprite_runtime_sheet_height_get(target->runtime_id) / 2 - target->world_coords.vy;
                difference = attacker_center - target_center;
                if (difference < -11) {
                    animation = ANIMATION_WEAPON_GUARD_LOW;
                } else {
                    animation = ANIMATION_WEAPON_GUARD_LEVEL;
                    if (difference >= 12) {
                        animation = ANIMATION_WEAPON_GUARD_HIGH;
                    }
                }
                break;
            case ACTION_MISS_TYPE_ACCESSORY_EVADE:
            case ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD:
            case ACTION_MISS_TYPE_NULLIFIED:
            case ACTION_MISS_TYPE_ACCURACY_MISS:
            case ACTION_MISS_TYPE_FORCED_FAILURE:
                if (action->miss_display_flags & ACTION_MISS_DISPLAY_DODGE_ANIMATION) {
                    unit_facing_turn_to_target(target, attacker);
                    animation = ANIMATION_DODGE;
                }
                break;
            case ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD:
            case ACTION_MISS_TYPE_CATCH:
                unit_facing_turn_to_target(target, attacker);
                animation = ANIMATION_RAISE_HANDS;
                break;
            case ACTION_MISS_TYPE_CANCELED:
            case ACTION_MISS_TYPE_REFLECTED:
            case ACTION_MISS_TYPE_GOLEM_GUARD:
                break;
            default:
                if (action->hit != 0) {
                    if ((action->hp_damage != 0) || (action->mp_damage != 0)) {
                        animation = ANIMATION_HURT;
                    } else if ((action->hp_healing != 0) || (action->mp_healing != 0)) {
                        animation = ANIMATION_RAISE_HANDS;
                    }
                }
                break;
            }

            if (animation != 0) {
                if (target->spritesheet_id < SPRITE_SHEET_ID_CRYSTAL) {
                    animation_facing_force(animation, target->facing, target);
                } else if (animation == ANIMATION_HURT) {
                    animation_facing_force(ANIMATION_HURT, target->facing, target);
                }
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_KNOCKBACK) {
                move_knockback_start(attacker, target);
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_POACH) {
                target->status_flags_5_6 |= UNIT_STATUS_POACHED;
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_MORBOL) {
                target->status_flags_5_6 |= UNIT_STATUS_MORBOL;
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT) {
                ai_unit_abilities_init(target->stats->battle_id);
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
                ai_unit_abilities_init(target->stats->battle_id);
            }
            return;
        }
        animation = ANIMATION_HURT;
    }
    animation_facing_store(animation, target->facing, target);
}

/* Refresh the display of every unit in the strike's target list. */
void unit_target_displays_update(unit_runtime_t* unit) {
    s32 i;
    unit_runtime_t* target;

    if (unit->strike.target_count != 0) {
        for (i = 0; i < unit->strike.target_count; i++) {
            target = unit_battle_get(unit->strike.target_list[i]);
            if (target != 0) {
                unit_runtime_display_update(target->runtime_id);
            }
        }
    }
}
