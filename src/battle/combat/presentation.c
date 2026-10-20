/* Translate action results into live-unit presentation: secondary effects, projectile responses, status refresh and
 * mount changes. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/ai.h"
#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/cursor.h"
#include "fft/effect.h"
#include "fft/effect_secondary.h"
#include "fft/item.h"
#include "fft/result.h"
#include "fft/sprite.h"
#include "fft/state.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Projectile secondary animation of each weapon item type: BOW_ARROW for bows, CROSSBOW_BOLT for
 * crossbows, NONE for the rest. */
static u8 g_weapon_projectile_animations[32]
    = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

extern const u32 g_status_mask_by_handler_index[STATUS_COUNT + 1];

/* Fill an init record for a secondary effect that one unit both casts and
 * receives: the caster and the single target are the unit. Every other field of
 * the record is left as the caller's stack held it. */
void effect_secondary_init_build(unit_runtime_t* unit, effect_context_t* effect) {
    effect->targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect->targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = unit->runtime_id;
    effect->target_count = 1;
    effect->targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect->targets[0].data.unit.runtime_id = unit->runtime_id;
}

/* Fill a secondary-effect init record with the unit's current action: the
 * caster entry, the used weapon and one entry per strike target.
 *
 * Each target entry records the target's runtime id and a result animation
 * read from its action data: a miss, no effect, a Golem block, an equipment
 * break, a critical or a normal hit. With no targets the first entry instead
 * holds the cursor tile. */
static void action_secondary_init_build(unit_runtime_t* unit, effect_context_t* init) {
    unit_runtime_t* target;
    action_data_t* action;
    s32 i;

    init->target_count = unit->strike.target_count;
    init->targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    init->targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = unit->runtime_id;
    init->used_weapon_id = unit->strike.used_weapon_id;
    if (init->target_count != 0) {
        for (i = 0; i < init->target_count; i++) {
            target = unit_battle_get(unit->strike.target_list[i]);
            init->targets[i].target_type = EFFECT_TARGET_TYPE_UNIT;
            init->targets[i].data.unit.runtime_id = target->runtime_id;
            action = &target->stats->action;
            switch (action->miss_type) {
            case ACTION_MISS_TYPE_ACCESSORY_EVADE:
            case ACTION_MISS_TYPE_RIGHT_HAND_EVADE:
            case ACTION_MISS_TYPE_LEFT_HAND_EVADE:
            case ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD:
                init->targets[i].data.unit.result_animation = EFFECT_HIT_MISSED;
                break;
            case ACTION_MISS_TYPE_NULLIFIED:
            case ACTION_MISS_TYPE_ACCURACY_MISS:
            case ACTION_MISS_TYPE_FORCED_FAILURE:
            case ACTION_MISS_TYPE_CANCELED:
            case ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD:
            case ACTION_MISS_TYPE_CATCH:
                init->targets[i].data.unit.result_animation = EFFECT_HIT_NO_EFFECT;
                break;
            case ACTION_MISS_TYPE_GOLEM_GUARD:
                init->targets[i].data.unit.result_animation = EFFECT_HIT_GOLEM;
                break;
            default:
                if (action->special_effect & ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT) {
                    init->targets[i].data.unit.result_animation = EFFECT_HIT_BREAK;
                } else if (action->critical != 0) {
                    init->targets[i].data.unit.result_animation = EFFECT_HIT_CRITICAL;
                } else {
                    init->targets[i].data.unit.result_animation = EFFECT_HIT_NORMAL;
                }
                break;
            }
        }
    } else {
        init->targets[0].target_type = EFFECT_TARGET_TYPE_TILE;
        init->targets[0].data.tile.vx = g_cursor_x;
        init->targets[0].data.tile.vy = g_cursor_layer;
        init->targets[0].data.tile.vz = g_cursor_y;
    }
}

/* Start the weapon-hit secondary effect of the unit's action, in the element
 * of the used weapon. `is_bow` is 1 from the projectile-hit path and 0
 * otherwise; effect_secondary_weapon_hit_init ignores it. */
void effect_secondary_weapon_hit_start(unit_runtime_t* unit, s32 is_bow) {
    effect_context_t secondary_init;

    action_secondary_init_build(unit, &secondary_init);
    effect_secondary_weapon_hit_init(g_item_weapon_data[unit->used_item_or_weapon_id].element, &secondary_init, is_bow);
}

/* Start the projectile secondary effect of the unit's action: the stone for
 * Throw Stone, the thrown item for the Item and Throw abilities, and for
 * anything else the animation of the equipped weapon's type (an arrow for
 * bows and crossbows, none for the rest). The retail code tests Throw
 * Shuriken on its own before the Item/Throw span, which already holds it. */
void effect_secondary_projectile_start(unit_runtime_t* unit) {
    effect_context_t secondary_init;
    u16 ability_id;
    u8 animation_id;

    action_secondary_init_build(unit, &secondary_init);
    ability_id = unit->used_ability_id;
    if (ability_id == ABILITY_ID_BASIC_SKILL_THROW_STONE) {
        animation_id = EFFECT_SECONDARY_ANIMATION_THROWN_STONE;
    } else {
        animation_id = EFFECT_SECONDARY_ANIMATION_THROWN_ITEM;
        if ((ability_id != ABILITY_ID_THROW_SHURIKEN)
            && ((u32)(unit->used_ability_id - ABILITY_ID_ITEM_FIRST)
                >= (ABILITY_ID_JUMP_FIRST - ABILITY_ID_ITEM_FIRST))) {
            animation_id = g_weapon_projectile_animations[unit->equipped_weapon_type];
        }
    }
    effect_secondary_projectile_init(animation_id, &secondary_init);
}

/* Start a secondary effect on a unit as both its caster and its target, with the
 * unit's sprite palette as the effect's parameter. A unit without battle data
 * starts nothing.
 *
 * The init record is a stack local of which effect_secondary_init_build
 * sets five fields; effect_secondary_init copies the rest of its caster
 * and target blocks and its used_weapon_id as the stack left them. */
static void unit_secondary_start(unit_runtime_t* unit, s32 effect_type) {
    effect_context_t data;

    effect_secondary_init_build(unit, &data);
    if (unit->stats != 0) {
        effect_secondary_init(effect_type, unit->stats->sprite_palette, &data);
    }
}

/* Start the puff a unit gives off as it turns to crystal or treasure. */
void effect_secondary_zodiac_poof_start(unit_runtime_t* unit) {
    unit_secondary_start(unit, EFFECT_SECONDARY_ANIMATION_ZODIAC_POOF);
}

/* Start the smoke of a chocobo or monster unit dying. */
void effect_secondary_death_smoke_start(unit_runtime_t* unit) {
    unit_secondary_start(unit, EFFECT_SECONDARY_ANIMATION_DEATH_SMOKE);
}

/* Start the level-up effect on a unit, for both level and job level ups. */
void effect_secondary_level_up_start(unit_runtime_t* unit) {
    unit_secondary_start(unit, EFFECT_SECONDARY_ANIMATION_LEVEL_UP);
}

/* Start the trap effect on a unit that stepped on a tile trap; every trap kind uses
 * this one effect. */
void effect_secondary_trap_start(unit_runtime_t* unit) {
    unit_secondary_start(unit, EFFECT_SECONDARY_ANIMATION_TRAP);
}

/* Start the splash of a unit landing in swamp, marsh or open water. */
void effect_secondary_splash_start(unit_runtime_t* unit) {
    unit_secondary_start(unit, EFFECT_SECONDARY_ANIMATION_SPLASH);
}

/* Start the dust of a unit landing on hard ground, a floor or a structure. */
void effect_secondary_fall_dust_start(unit_runtime_t* unit) {
    unit_secondary_start(unit, EFFECT_SECONDARY_ANIMATION_FALL_DUST);
}

/* Start the teleport effect on a unit, used both as it leaves and as it arrives. */
void effect_secondary_teleport_start(unit_runtime_t* unit) {
    unit_secondary_start(unit, EFFECT_SECONDARY_ANIMATION_TELEPORT);
}

/* Start the reaction animation of the unit with this runtime id to the casting unit's action, for
 * effect keyframe actions. Always returns 1. */
s32 action_start_target_response_by_runtime_id(s32 runtime_id) {
    unit_runtime_t* target = unit_runtime_get(runtime_id);
    unit_runtime_t* caster = unit_casting_get();

    action_start_target_response(caster, target);
    return 1;
}

/* Starts the reaction animation of the unit in slot battle_id to the casting
 * unit's action. Always returns 1. No retail code calls it. */
s32 action_start_target_response_by_battle_id(u16 battle_id) {
    unit_runtime_t* target = unit_battle_get(battle_id);
    unit_runtime_t* caster = unit_casting_get();

    action_start_target_response(caster, target);
    return 1;
}

/* result_display_prepare for a runtime id. An absent unit raises
 * a pointer exception and returns 0. */
s32 result_runtime_display_prepare(s32 runtime_id) {
    unit_runtime_t* unit = unit_runtime_get(runtime_id);
    if (unit != 0) {
        result_display_prepare(unit);
        return 1;
    }
    system_pointer_exception_handle(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
    return 0;
}

/* Lands a projectile strike on its target: the weapon hit sound, the target's
 * reaction animation, its post-action display and the evade and weapon-element
 * effect data. Does nothing when either unit is missing. */
static void projectile_hit_response_start(unit_runtime_t* attacker, unit_runtime_t* target) {
    if ((attacker != 0) && (target != 0)) {
        action_play_weapon_sfx(attacker, 1);
        action_start_target_response(attacker, target);
        result_runtime_display_prepare(target->runtime_id);
        effect_secondary_weapon_hit_start(attacker, 1);
    }
}

/* projectile_hit_response_start for two battle ids. No retail code calls it. */
void action_start_projectile_hit_response_by_battle_id(u16 attacker_id, u16 target_id) {
    unit_runtime_t* attacker = unit_battle_get(attacker_id);
    unit_runtime_t* target = unit_battle_get(target_id);

    projectile_hit_response_start(attacker, target);
}

/* projectile_hit_response_start for two runtime ids: called by the arrow-arc
 * and linear projectile effects for the unit the projectile hits. */
void action_start_projectile_hit_response_by_runtime_id(u16 attacker_id, u16 target_id) {
    unit_runtime_t* attacker = unit_runtime_get(attacker_id);
    unit_runtime_t* target = unit_runtime_get(target_id);

    projectile_hit_response_start(attacker, target);
}

/* Apply a unit's queued status changes (removals, then additions) and refresh everything that
 * shows them: position after death or dismount, transparency, status bubble, tile and status
 * palette, transformations, status animations, spritesheet and float height. Then clear the queues
 * and the item display flag. */
static void display_update(unit_runtime_t* unit) {
    battle_map_tile_t* tile;

    if (unit != 0) {
        tile = battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_layer);
        unit->status_flags_1_4 &= ~unit->statuses_to_remove_1_4;
        unit->status_flags_5_6 &= ~unit->statuses_to_remove_5_6;
        unit->status_flags_1_4 |= unit->statuses_to_add_1_4;
        unit->status_flags_5_6 |= unit->statuses_to_add_5_6;
        unit_dismount_dead_rider(unit);
        sprite_transparency_update(unit);
        sprite_status_bubble_active_update(unit);
        if (g_state_animation_continue_check != 0) {
            sprite_palette_check_tile_status_mod(unit, tile->flags.shading, 0);
        } else {
            sprite_palette_check_tile_status_mod(unit, tile->flags.shading, 1);
        }
        unit_apply_poach_morbol_transformation(unit);
        animation_status_set(unit);
        animation_status_changes_update(unit);
        sprite_sheet_apply_status_change(unit, tile->flags.shading);
        unit_float_jump_height_update(unit);
        unit->statuses_to_remove_5_6 = 0;
        unit->statuses_to_remove_1_4 = 0;
        unit->statuses_to_add_5_6 = 0;
        unit->statuses_to_add_1_4 = 0;
        unit->item_display_active = 0;
    }
}

/* display_update for the unit with this runtime id; an id without a unit does nothing. */
void unit_runtime_display_update(s32 runtime_id) {
    display_update(unit_runtime_get(runtime_id));
}

/* display_update for the unit in slot battle_id. No retail code calls
 * it. */
void unit_battle_display_update(u16 battle_id) {
    display_update(unit_battle_get(battle_id));
}

/*
 * Queue the renderer-side status flags associated with a simulation status.
 *
 * The status argument is the callback-table index (canonical status ID + 1).
 * Removing Crystal shares the Chicken case, so it queues the Chicken bit for
 * removal and leaves the Crystal bit set. Rebuilding the unit's AI ability
 * data when Frog changes keeps its usable-ability list synchronized with the
 * graphics transition.
 */
s32 status_queue_display_flag_change(s32 status, s32 change_type, s32 battle_id) {
    unit_runtime_t* unit = unit_battle_get(battle_id);

    if (unit == 0) {
        return 0;
    }
    switch (change_type) {
    case STATUS_DISPLAY_CHANGE_REMOVE:
        switch (status) {
        case STATUS_HANDLER_INDEX(STATUS_ID_CRYSTAL):
        case STATUS_HANDLER_INDEX(STATUS_ID_CHICKEN):
            unit->statuses_to_remove_5_6 |= UNIT_STATUS_CHICKEN;
            unit->statuses_to_add_5_6 &= ~UNIT_STATUS_CHICKEN;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_FROG):
            unit->statuses_to_remove_5_6 |= UNIT_STATUS_FROG;
            unit->statuses_to_add_5_6 &= ~UNIT_STATUS_FROG;
            ai_unit_abilities_init(battle_id);
            return 1;
        case STATUS_HANDLER_INDEX(STATUS_ID_JUMP):
        case STATUS_HANDLER_INDEX(STATUS_ID_FLOAT):
        case STATUS_HANDLER_INDEX(STATUS_ID_TRANSPARENT):
            unit->statuses_to_remove_5_6 |= g_status_mask_by_handler_index[status];
            unit->statuses_to_add_5_6 &= ~g_status_mask_by_handler_index[status];
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_UNNAMED_00):
        case STATUS_HANDLER_INDEX(STATUS_ID_INVITE):
        case STATUS_HANDLER_INDEX(STATUS_ID_TREASURE):
        case STATUS_HANDLER_INDEX(STATUS_ID_WALL):
            break;
        default:
            unit->statuses_to_remove_1_4 |= g_status_mask_by_handler_index[status];
            unit->statuses_to_add_1_4 &= ~g_status_mask_by_handler_index[status];
            break;
        }
        break;
    case STATUS_DISPLAY_CHANGE_ADD:
        switch (status) {
        case STATUS_HANDLER_INDEX(STATUS_ID_CRYSTAL):
            unit->statuses_to_add_5_6 |= UNIT_STATUS_CRYSTAL;
            unit->statuses_to_remove_5_6 &= ~UNIT_STATUS_CRYSTAL;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_CHICKEN):
            unit->statuses_to_add_5_6 |= UNIT_STATUS_CHICKEN;
            unit->statuses_to_remove_5_6 &= ~UNIT_STATUS_CHICKEN;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_FROG):
            unit->statuses_to_add_5_6 |= UNIT_STATUS_FROG;
            unit->statuses_to_remove_5_6 &= ~UNIT_STATUS_FROG;
            ai_unit_abilities_init(battle_id);
            return 1;
        case STATUS_HANDLER_INDEX(STATUS_ID_TREASURE):
            unit->statuses_to_add_5_6 |= UNIT_STATUS_TREASURE;
            unit->statuses_to_remove_5_6 &= ~UNIT_STATUS_TREASURE;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_JUMP):
        case STATUS_HANDLER_INDEX(STATUS_ID_FLOAT):
        case STATUS_HANDLER_INDEX(STATUS_ID_TRANSPARENT):
            unit->statuses_to_add_5_6 |= g_status_mask_by_handler_index[status];
            unit->statuses_to_remove_5_6 &= ~g_status_mask_by_handler_index[status];
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_UNNAMED_00):
        case STATUS_HANDLER_INDEX(STATUS_ID_INVITE):
        case STATUS_HANDLER_INDEX(STATUS_ID_WALL):
            break;
        default:
            unit->statuses_to_add_1_4 |= g_status_mask_by_handler_index[status];
            unit->statuses_to_remove_1_4 &= ~g_status_mask_by_handler_index[status];
            break;
        }
        break;
    case STATUS_DISPLAY_CHANGE_KEEP:
        /* No effect, but the label makes 1 the root of the target's
         * compare tree. */
        break;
    }
    return 1;
}

/* sprite_thrown_item_palette_set for the unit in slot battle_id. No
 * retail code calls it. */
void sprite_battle_thrown_item_palette_set(s32 item_id, u16 battle_id) {
    sprite_thrown_item_palette_set(item_id, unit_battle_get(battle_id));
}

/* Seat the rider on the mount: link both records, copy the mount's facing,
 * position and map coordinates to the rider and raise the rider 10 world units
 * above it (negative y is up), then refresh both units' display. */
void unit_rider_seat(unit_runtime_t* rider, unit_runtime_t* mount) {
    rider->mount_state = UNIT_MOUNT_STATE_RIDER;
    rider->mount_partner_runtime_id = mount->runtime_id;
    mount->mount_state = UNIT_MOUNT_STATE_MOUNT;
    mount->mount_partner_runtime_id = rider->runtime_id;
    rider->facing = mount->facing;
    rider->camera_facing_quadrant = mount->camera_facing_quadrant;
    rider->camera_facing_sixteenth = mount->camera_facing_sixteenth;
    rider->world_coords = mount->world_coords;
    rider->world_coords.vy -= 10;
    rider->real = mount->real;
    rider->standing_on_battle_id_plus_one = 0;
    rider->real.vy -= 10 * ONE; /* the same raise in 20.12 fixed point */
    rider->map_x = mount->map_x;
    rider->map_y = mount->map_y;
    rider->map_layer = mount->map_layer;
    rider->status_flags_1_4 |= UNIT_STATUS_MOUNTED;
    animation_status_set(rider);
    rider->animation.script_pos = mount->animation.script_pos;
    unit_runtime_display_update(rider->runtime_id);
    unit_runtime_display_update(mount->runtime_id);
}

/* Unlinks a rider from its mount: both records return to the unmounted state and
 * the rider loses its mounted display flag. Does nothing for a unit that is not a
 * rider. */
void unit_rider_dismount(unit_runtime_t* unit) {
    unit_runtime_t* mount;
    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        mount = unit_runtime_get(unit->mount_partner_runtime_id);
        unit->standing_on_battle_id_plus_one = 0;
        unit->mount_state = UNIT_MOUNT_STATE_NONE;
        unit->mount_partner_runtime_id = 0;
        unit->status_flags_1_4 &= ~UNIT_STATUS_MOUNTED;
        if (mount != 0) {
            mount->mount_state = UNIT_MOUNT_STATE_NONE;
            mount->mount_partner_runtime_id = 0;
        }
    }
}

/* unit_rider_dismount, followed by a display update of the rider and of
 * the mount. */
void unit_rider_dismount_display_update(unit_runtime_t* rider) {
    unit_runtime_t* mount;

    if (rider->mount_state == UNIT_MOUNT_STATE_RIDER) {
        mount = unit_runtime_get(rider->mount_partner_runtime_id);
        rider->standing_on_battle_id_plus_one = 0;
        rider->mount_state = UNIT_MOUNT_STATE_NONE;
        rider->mount_partner_runtime_id = 0;
        rider->status_flags_1_4 &= ~UNIT_STATUS_MOUNTED;
        unit_runtime_display_update(rider->runtime_id);
        if (mount != 0) {
            mount->mount_state = UNIT_MOUNT_STATE_NONE;
            mount->mount_partner_runtime_id = 0;
            unit_runtime_display_update(mount->runtime_id);
        }
    }
}
