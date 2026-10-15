/* No shared theme: secondary-effect init records and starters, projectile hit reaction, display updates, mount/dismount
 * and step velocity builders. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/main.h"
#include "fft/sound.h"
#include "psx/types.h"

void battle_menu_clear_status_menu_state(void) {
    g_battle_menu_status_requested = 0;
    g_status_menu_open = 0;
}

/* Fill an init record for a secondary effect that one unit both casts and
 * receives: the caster and the single target are the unit. Every other field of
 * the record is left as the caller's stack held it. */
void effect_build_secondary_init(battle_unit_t* unit, effect_secondary_init_t* effect) {
    effect->targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect->targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = unit->runtime_id;
    effect->target_count = 1;
    effect->targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
    effect->targets[0].data.unit.runtime_id = unit->runtime_id;
}

/* Fill a secondary-effect init record with the unit's current action targets.
 *
 * Each target block records the target's unit id and a result animation
 * derived from its action data. With no targets the first block instead holds
 * the cursor tile position. The record's target blocks form an array of up to
 * 16 entries starting at the header's first `target` block. */
void effect_build_secondary_init_from_action(battle_unit_t* unit, effect_secondary_init_t* init) {
    battle_unit_t* target;
    action_data_t* action;
    s32 i;

    init->target_count = unit->strike.target_count;
    init->targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    init->targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = unit->runtime_id;
    init->used_weapon_id = unit->strike.used_weapon_id;
    if (init->target_count != 0) {
        for (i = 0; i < init->target_count; i++) {
            target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
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
        init->targets[0].data.tile.vx = g_battle_cursor_x;
        init->targets[0].data.tile.vy = g_battle_cursor_z;
        init->targets[0].data.tile.vz = g_battle_cursor_y;
    }
}

/* Starts the weapon-hit secondary effect of the unit's action, in the element
 * of the used weapon; is_bow is 1 for the bow hardcoding path and 0
 * otherwise, and unused. */
void effect_set_evade_type_data_and_weapon_element(battle_unit_t* unit, s32 is_bow) {
    effect_secondary_init_t secondary_init;
    effect_build_secondary_init_from_action(unit, &secondary_init);
    effect_init_weapon_hit_secondary(
        g_main_item_weapon_data[unit->used_item_or_weapon_id].element, &secondary_init, is_bow);
}

/* Starts the projectile secondary effect of the unit's action: the stone for
 * Throw Stone, the thrown item for the Item and Throw abilities, and for
 * anything else the animation of the equipped weapon's type (an arrow for
 * bows and crossbows, none for the rest). The target retains a redundant
 * Shuriken test before checking the complete Item/Throw span. */
void effect_start_action_projectile_secondary(battle_unit_t* unit) {
    effect_secondary_init_t secondary_init;
    u16 ability_id;
    u8 animation_id;
    effect_build_secondary_init_from_action(unit, &secondary_init);
    ability_id = unit->used_ability_id;
    if (ability_id == ABILITY_ID_BASIC_SKILL_THROW_STONE) {
        animation_id = EFFECT_SECONDARY_ANIMATION_THROWN_STONE;
    } else {
        animation_id = EFFECT_SECONDARY_ANIMATION_THROWN_ITEM;
        if ((ability_id != ABILITY_ID_THROW_SHURIKEN)
            && ((u32)(unit->used_ability_id - ABILITY_ID_ITEM_FIRST)
                >= (ABILITY_ID_JUMP_FIRST - ABILITY_ID_ITEM_FIRST))) {
            animation_id = g_effect_weapon_projectile_animations[unit->equipped_weapon_type];
        }
    }
    effect_init_projectile_secondary(animation_id, &secondary_init);
}

/* Start a secondary effect on a unit as both its caster and its target, with the
 * unit's sprite palette as the effect's parameter. A unit without battle data
 * starts nothing.
 *
 * The init record is a stack local of which effect_build_secondary_init
 * sets five fields; effect_init_secondary copies the rest of its caster
 * and target blocks and its used_weapon_id as the stack left them. */
void effect_set_secondary(battle_unit_t* unit, s32 effect_type) {
    effect_secondary_init_t data;

    effect_build_secondary_init(unit, &data);
    if (unit->stats != 0) {
        effect_init_secondary(effect_type, unit->stats->sprite_palette, &data);
    }
}

void effect_set_secondary_zodiac_poof(battle_unit_t* unit) {
    effect_set_secondary(unit, EFFECT_SECONDARY_ANIMATION_ZODIAC_POOF);
}

void effect_set_secondary_death_smoke(battle_unit_t* unit) {
    effect_set_secondary(unit, EFFECT_SECONDARY_ANIMATION_DEATH_SMOKE);
}

void effect_set_secondary_level_up(battle_unit_t* unit) {
    effect_set_secondary(unit, EFFECT_SECONDARY_ANIMATION_LEVEL_UP);
}

void effect_set_secondary_venom_trap(battle_unit_t* unit) {
    effect_set_secondary(unit, EFFECT_SECONDARY_ANIMATION_VENOM_TRAP);
}

void effect_set_secondary_splash(battle_unit_t* unit) {
    effect_set_secondary(unit, EFFECT_SECONDARY_ANIMATION_SPLASH);
}

void effect_set_secondary_fall_dust(battle_unit_t* unit) {
    effect_set_secondary(unit, EFFECT_SECONDARY_ANIMATION_FALL_DUST);
}

void effect_set_secondary_teleport(battle_unit_t* unit) {
    effect_set_secondary(unit, EFFECT_SECONDARY_ANIMATION_TELEPORT);
}

s32 battle_unit_set_target_animation_by_runtime_id(s32 runtime_id) {
    battle_unit_t* target = battle_unit_get_by_runtime_id(runtime_id);
    battle_unit_t* caster = battle_unit_get_casting();

    battle_unit_set_target_animation_from_attack_type(caster, target);
    return 1;
}

/* Starts the reaction animation of the unit in slot battle_id to the casting
 * unit's action. Always returns 1. No retail code calls it. */
s32 battle_unit_set_target_anim_by_battle_id(u16 battle_id) {
    battle_unit_t* target = battle_unit_get_by_battle_id(battle_id);
    battle_unit_t* caster = battle_unit_get_casting();

    battle_unit_set_target_animation_from_attack_type(caster, target);
    return 1;
}

/* battle_gfx_prepare_post_action_display for a runtime id. An absent unit raises
 * pointer exception 0xc and returns 0. */
s32 battle_gfx_prepare_post_action_display_by_runtime_id(s32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);
    if (unit != 0) {
        battle_gfx_prepare_post_action_display(unit);
        return 1;
    }
    main_system_handle_pointer_exception(0xc);
    return 0;
}

/* Lands a projectile strike on its target: the weapon hit sound, the target's
 * reaction animation, its post-action display and the evade and weapon-element
 * effect data. Does nothing when either unit is missing. */
void battle_unit_set_bow_hardcoding(battle_unit_t* attacker, battle_unit_t* target) {
    if ((attacker != 0) && (target != 0)) {
        sound_play_weapon_sfx(attacker, 1);
        battle_unit_set_target_animation_from_attack_type(attacker, target);
        battle_gfx_prepare_post_action_display_by_runtime_id(target->runtime_id);
        effect_set_evade_type_data_and_weapon_element(attacker, 1);
    }
}

/* battle_unit_set_bow_hardcoding for two battle ids. No retail code calls it. */
void battle_unit_call_bow_hardcoding_by_battle_id(u16 attacker_id, u16 target_id) {
    battle_unit_t* attacker = battle_unit_get_by_battle_id(attacker_id);
    battle_unit_t* target = battle_unit_get_by_battle_id(target_id);

    battle_unit_set_bow_hardcoding(attacker, target);
}

/* battle_unit_set_bow_hardcoding for two unit unit ids: called by the arrow-arc
 * and linear projectile effects for the unit the projectile hits. */
void battle_unit_call_bow_hardcoding_by_runtime_id(u16 attacker_id, u16 target_id) {
    battle_unit_t* attacker = battle_unit_get_by_runtime_id(attacker_id);
    battle_unit_t* target = battle_unit_get_by_runtime_id(target_id);

    battle_unit_set_bow_hardcoding(attacker, target);
}

void battle_unit_update_display(battle_unit_t* unit) {
    map_tile_t* tile;

    if (unit != 0) {
        tile = map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
        unit->status_flags_1_4 &= ~unit->statuses_to_remove_1_4;
        unit->status_flags_5_6 &= ~unit->statuses_to_remove_5_6;
        unit->status_flags_1_4 |= unit->statuses_to_add_1_4;
        unit->status_flags_5_6 |= unit->statuses_to_add_5_6;
        battle_unit_set_map_coords_after_death_dismount(unit);
        battle_gfx_update_sprite_transparency_flag(unit);
        battle_gfx_update_status_bubble_graphic_trigger(unit);
        if (g_battle_state_animation_continue_check != 0) {
            battle_gfx_check_tile_status_palette_mod(unit, tile->flags.shading, 0);
        } else {
            battle_gfx_check_tile_status_palette_mod(unit, tile->flags.shading, 1);
        }
        battle_unit_apply_poach_morbol_transformation(unit);
        battle_unit_set_animation_based_on_status(unit);
        battle_unit_update_animation_for_status_changes(unit);
        battle_gfx_apply_status_spritesheet_change(unit, tile->flags.shading);
        battle_unit_update_float_and_jump_height(unit);
        unit->statuses_to_remove_5_6 = 0;
        unit->statuses_to_remove_1_4 = 0;
        unit->statuses_to_add_5_6 = 0;
        unit->statuses_to_add_1_4 = 0;
        unit->item_display_active = 0;
    }
}

void battle_unit_update_display_by_runtime_id(s32 runtime_id) {
    battle_unit_update_display(battle_unit_get_by_runtime_id(runtime_id));
}

/* battle_unit_update_display for the unit in slot battle_id. No retail code calls
 * it. */
void battle_unit_update_display_by_battle_id(u16 battle_id) {
    battle_unit_update_display(battle_unit_get_by_battle_id(battle_id));
}

/*
 * Queue the renderer-side status flags associated with a simulation status.
 *
 * The status argument is the callback-table index (canonical status ID + 1).
 * Crystal removal deliberately stages the Chicken transformation bit, as in
 * the original transition logic. Rebuilding the unit's AI ability data when
 * Frog changes keeps its usable-ability list synchronized with the graphics
 * transition.
 */
s32 status_queue_runtime_graphics_flag_change(s32 status, s32 enabled, s32 battle_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_battle_id(battle_id & 0xffff);
    if (unit == 0) {
        return 0;
    }
    switch (enabled) {
    case 0:
        switch (status) {
        case STATUS_HANDLER_INDEX(STATUS_ID_CRYSTAL):
        case STATUS_HANDLER_INDEX(STATUS_ID_CHICKEN):
            unit->statuses_to_remove_5_6 |= BATTLE_UNIT_STATUS_CHICKEN;
            unit->statuses_to_add_5_6 &= ~BATTLE_UNIT_STATUS_CHICKEN;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_FROG):
            unit->statuses_to_remove_5_6 |= BATTLE_UNIT_STATUS_FROG;
            unit->statuses_to_add_5_6 &= ~BATTLE_UNIT_STATUS_FROG;
            ai_init_unit_abilities(battle_id);
            return 1;
        case STATUS_HANDLER_INDEX(STATUS_ID_JUMP):
        case STATUS_HANDLER_INDEX(STATUS_ID_FLOAT):
        case STATUS_HANDLER_INDEX(STATUS_ID_TRANSPARENT):
            unit->statuses_to_remove_5_6 |= g_battle_unit_status_mask_by_handler_index[status];
            unit->statuses_to_add_5_6 &= ~g_battle_unit_status_mask_by_handler_index[status];
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_UNNAMED_00):
        case STATUS_HANDLER_INDEX(STATUS_ID_INVITE):
        case STATUS_HANDLER_INDEX(STATUS_ID_TREASURE):
        case STATUS_HANDLER_INDEX(STATUS_ID_WALL):
            break;
        default:
            unit->statuses_to_remove_1_4 |= g_battle_unit_status_mask_by_handler_index[status];
            unit->statuses_to_add_1_4 &= ~g_battle_unit_status_mask_by_handler_index[status];
            break;
        }
        break;
    case 1:
        switch (status) {
        case STATUS_HANDLER_INDEX(STATUS_ID_CRYSTAL):
            unit->statuses_to_add_5_6 |= BATTLE_UNIT_STATUS_CRYSTAL;
            unit->statuses_to_remove_5_6 &= ~BATTLE_UNIT_STATUS_CRYSTAL;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_CHICKEN):
            unit->statuses_to_add_5_6 |= BATTLE_UNIT_STATUS_CHICKEN;
            unit->statuses_to_remove_5_6 &= ~BATTLE_UNIT_STATUS_CHICKEN;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_FROG):
            unit->statuses_to_add_5_6 |= BATTLE_UNIT_STATUS_FROG;
            unit->statuses_to_remove_5_6 &= ~BATTLE_UNIT_STATUS_FROG;
            ai_init_unit_abilities(battle_id);
            return 1;
        case STATUS_HANDLER_INDEX(STATUS_ID_TREASURE):
            unit->statuses_to_add_5_6 |= BATTLE_UNIT_STATUS_TREASURE;
            unit->statuses_to_remove_5_6 &= ~BATTLE_UNIT_STATUS_TREASURE;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_JUMP):
        case STATUS_HANDLER_INDEX(STATUS_ID_FLOAT):
        case STATUS_HANDLER_INDEX(STATUS_ID_TRANSPARENT):
            unit->statuses_to_add_5_6 |= g_battle_unit_status_mask_by_handler_index[status];
            unit->statuses_to_remove_5_6 &= ~g_battle_unit_status_mask_by_handler_index[status];
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_UNNAMED_00):
        case STATUS_HANDLER_INDEX(STATUS_ID_INVITE):
        case STATUS_HANDLER_INDEX(STATUS_ID_WALL):
            break;
        default:
            unit->statuses_to_add_1_4 |= g_battle_unit_status_mask_by_handler_index[status];
            unit->statuses_to_remove_1_4 &= ~g_battle_unit_status_mask_by_handler_index[status];
            break;
        }
        break;
    case 2:
        /* No effect, but the label makes 1 the root of the target's
         * compare tree. */
        break;
    }
    return 1;
}

/* No retail code calls it. */
void battle_gfx_set_thrown_item_graphic_palette_by_battle_id(s32 item_id, u16 battle_id) {
    battle_gfx_set_thrown_item_graphic_palette(item_id, battle_unit_get_by_battle_id(battle_id));
}

/* Seat the rider on the mount: link both records, copy the mount's facing,
 * position and map coordinates to the rider and raise the rider above it. */
void battle_unit_mount_rider_onto_unit(battle_unit_t* rider, battle_unit_t* mount) {
    rider->mount_state = BATTLE_UNIT_MOUNT_STATE_RIDER;
    rider->mount_partner_runtime_id = mount->runtime_id;
    mount->mount_state = BATTLE_UNIT_MOUNT_STATE_MOUNT;
    mount->mount_partner_runtime_id = rider->runtime_id;
    rider->facing = mount->facing;
    rider->camera_facing_quadrant = mount->camera_facing_quadrant;
    rider->camera_facing_sixteenth = mount->camera_facing_sixteenth;
    rider->world_coords = mount->world_coords;
    rider->world_coords.vy -= 10;
    rider->real = mount->real;
    rider->standing_on_battle_id_plus_one = 0;
    rider->real.vy += -0xa000;
    rider->map_x = mount->map_x;
    rider->map_y = mount->map_y;
    rider->map_z = mount->map_z;
    rider->status_flags_1_4 |= BATTLE_UNIT_STATUS_MOUNTED;
    battle_unit_set_animation_based_on_status(rider);
    rider->animation.script_pos = mount->animation.script_pos;
    battle_unit_update_display_by_runtime_id(rider->runtime_id);
    battle_unit_update_display_by_runtime_id(mount->runtime_id);
}

/* Unlinks a rider from its mount: both records return to the unmounted state and
 * the rider loses its mounted display flag. Does nothing for a unit that is not a
 * rider. */
void battle_unit_dismount_rider(battle_unit_t* unit) {
    battle_unit_t* mount;
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        mount = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        unit->standing_on_battle_id_plus_one = 0;
        unit->mount_state = BATTLE_UNIT_MOUNT_STATE_NONE;
        unit->mount_partner_runtime_id = 0;
        unit->status_flags_1_4 = unit->status_flags_1_4 & ~BATTLE_UNIT_STATUS_MOUNTED;
        if (mount != 0) {
            mount->mount_state = BATTLE_UNIT_MOUNT_STATE_NONE;
            mount->mount_partner_runtime_id = 0;
        }
    }
}

/* battle_unit_dismount_rider, followed by a display update of the rider and of
 * the mount. */
void battle_unit_dismount_rider_and_update_display(battle_unit_t* rider) {
    battle_unit_t* mount;

    if (rider->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        mount = battle_unit_get_by_runtime_id(rider->mount_partner_runtime_id);
        rider->standing_on_battle_id_plus_one = 0;
        rider->mount_state = BATTLE_UNIT_MOUNT_STATE_NONE;
        rider->mount_partner_runtime_id = 0;
        rider->status_flags_1_4 &= ~BATTLE_UNIT_STATUS_MOUNTED;
        battle_unit_update_display_by_runtime_id(rider->runtime_id);
        if (mount != 0) {
            mount->mount_state = BATTLE_UNIT_MOUNT_STATE_NONE;
            mount->mount_partner_runtime_id = 0;
            battle_unit_update_display_by_runtime_id(mount->runtime_id);
        }
    }
}

/* Clearance above a tile for move_clamp_z_to_tile_headroom, in half-height
 * units.
 *
 * When the position's other layer is not blocked and its surface is higher, the
 * result is twice the gap between that layer's underside (its height less its
 * ceiling depth) and a point 3 levels above this tile's surface. Otherwise it is
 * 0x40. */
s32 move_calculate_tile_layer_step_offset(s32 x, s32 y, u32 layer) {
    map_tile_t* from;
    map_tile_t* to;
    s32 from_height;
    s32 to_height;
    s32 limit;

    from = map_get_tile_data_pointer(x, y, layer);
    to = map_get_tile_data_pointer(x, y, layer ^ 1);
    if ((to->flags.blocked) == 0) {
        from_height = from->height + (from->depth_half_height.depth);
        to_height = to->height + (to->depth_half_height.depth);
        if (from_height < to_height) {
            limit = from_height + 3;
            return (to->height - (to->ceiling_depth_and_marks.ceiling_depth) - limit) * 2;
        }
    }
    return 0x40;
}

/*
 * Clamps a camera/height value into the walkable band above one tile: the
 * lower bound subtracts the layer step clearance from move_calculate_tile_layer_step_offset, the
 * upper bound is the tile surface height alone less 0x30.
 */
s32 move_clamp_z_to_tile_headroom(s32 value, s32 x, s32 y, u32 layer) {
    map_tile_t* tile;
    s32 clearance;
    s32 height;
    s32 lo;
    s32 hi;
    map_tile_depth_t depth;

    tile = map_get_tile_data_pointer(x, y, layer);
    clearance = move_calculate_tile_layer_step_offset(x, y, layer);
    depth = tile->depth_half_height;
    height = ((tile->height + depth.depth) * 2) + depth.half_height;
    lo = -(height + clearance) * 6;
    if (value < lo) {
        return lo;
    }
    hi = (-height * 6) - 0x30;
    if (value >= hi) {
        if (hi < lo) {
            return lo;
        }
        return hi;
    }
    return value;
}

enum {
    MOVE_STEP_HALF_WIDTH = MAP_TILE_SIZE / 2,
    MOVE_STEP_NEAR_EDGE = -7,
    MOVE_STEP_FAR_EDGE = 35,
};

/*
 * Builds the per-frame velocity for a run of identical movement steps: walks
 * the remaining path while the steps keep the same direction quadrant and the
 * clamped ground height stays at the unit's current height, then hands the
 * accumulated edge-to-edge delta to the velocity solver.
 * move_set_velocity_for_contiguous_steps_with_final_tile_height does
 * the same for the final step, where it reads the tile directly.
 *
 * The step code is copied to a 16-bit value before the switch: read through the
 * byte again, cse reuses the direction test's load and the reload is lost.
 */
void move_set_velocity_for_contiguous_clamped_steps(
    battle_unit_t* unit, const battle_walk_path_t* path, const move_step_t* step) {
    VECTOR direction;
    const move_step_t* current;
    s32 start_x;
    s32 start_y;
    s32 start_z;
    s32 index;
    s32 x;
    s32 y;
    s32 end_x;
    s32 end_y;
    s32 end_z;
    s16 code;

    map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
    x = unit->map_x;
    y = unit->map_y;
    index = unit->movement_path_offset - 1;
    start_x = unit->world_coords.vx;
    start_y = unit->world_coords.vz;
    start_z = unit->world_coords.vy;

    while (index < path->count) {
        current = &path->data.steps[index];
        if (current->direction != step->direction) {
            break;
        }
        code = *(u8*)current;
        switch (code >> 6) {
        case MOVE_STEP_DIRECTION_EAST:
            x += (code & 3) + 1;
            end_x = (x * MAP_TILE_SIZE) + MOVE_STEP_NEAR_EDGE;
            end_y = (y * MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            break;
        case MOVE_STEP_DIRECTION_WEST:
            x -= (code & 3) + 1;
            end_x = (x * MAP_TILE_SIZE) + MOVE_STEP_FAR_EDGE;
            end_y = (y * MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            break;
        case MOVE_STEP_DIRECTION_SOUTH:
            y -= (code & 3) + 1;
            end_x = (x * MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            end_y = (y * MAP_TILE_SIZE) + MOVE_STEP_FAR_EDGE;
            break;
        case MOVE_STEP_DIRECTION_NORTH:
            y += (code & 3) + 1;
            end_x = (x * MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            end_y = (y * MAP_TILE_SIZE) + MOVE_STEP_NEAR_EDGE;
            break;
        }
        end_z = move_clamp_z_to_tile_headroom(start_z, x, y, current->higher);
        if (end_z != start_z) {
            break;
        }
        index++;
    }

    direction.vx = end_x - start_x;
    direction.vz = end_y - start_y;
    direction.vy = end_z - start_z;
    VectorNormal(&direction, &unit->velocity);
    unit->velocity.vx = unit->velocity.vx * g_main_system_animation_speed;
    unit->velocity.vy = unit->velocity.vy * g_main_system_animation_speed;
    unit->velocity.vz = unit->velocity.vz * g_main_system_animation_speed;
}

enum {
    MOVE_STEP_HEIGHT_SCALE = 6,
};

/*
 * Builds the per-frame velocity for a run of identical movement steps: walks
 * the remaining path while the steps keep the same direction quadrant and the
 * ground stays at the unit's current height, then hands the accumulated
 * edge-to-edge delta to the velocity solver.
 *
 * The step code is copied to a 16-bit value before the switch, as in the clamped
 * twin: read through the byte again, cse reuses the direction test's load.
 */
void move_set_velocity_for_contiguous_steps_with_final_tile_height(
    battle_unit_t* unit, const battle_walk_path_t* path, const move_step_t* step) {
    VECTOR direction;
    map_tile_t* tile;
    const move_step_t* current;
    s32 start_x;
    s32 start_y;
    s32 start_z;
    s32 index;
    s32 x;
    s32 y;
    s32 end_x;
    s32 end_y;
    s32 end_z;
    u32 layer;
    map_tile_depth_t depth;
    s16 code;

    map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);
    x = unit->map_x;
    y = unit->map_y;
    index = unit->movement_path_offset - 1;
    start_x = unit->world_coords.vx;
    start_y = unit->world_coords.vz;
    start_z = unit->world_coords.vy;

    while (index < path->count) {
        current = &path->data.steps[index];
        if (current->direction != step->direction) {
            break;
        }
        code = *(u8*)current;
        switch (code >> 6) {
        case MOVE_STEP_DIRECTION_EAST:
            x += (code & 3) + 1;
            end_x = (x * MAP_TILE_SIZE) + MAP_TILE_SIZE;
            end_y = (y * MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            break;
        case MOVE_STEP_DIRECTION_WEST:
            x -= (code & 3) + 1;
            end_x = x * MAP_TILE_SIZE;
            end_y = (y * MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            break;
        case MOVE_STEP_DIRECTION_SOUTH:
            y -= (code & 3) + 1;
            end_x = (x * MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            end_y = y * MAP_TILE_SIZE;
            break;
        case MOVE_STEP_DIRECTION_NORTH:
            y += (code & 3) + 1;
            end_x = (x * MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            end_y = (y * MAP_TILE_SIZE) + MAP_TILE_SIZE;
            break;
        }
        layer = current->higher;
        if (unit->movement_path_offset == path->count) {
            tile = map_get_tile_data_pointer(x, y, layer);
            depth = tile->depth_half_height;
            end_z = -(((tile->height + depth.depth) * 2) + depth.half_height) * MOVE_STEP_HEIGHT_SCALE;
        } else {
            end_z = move_clamp_z_to_tile_headroom(start_z, x, y, layer);
        }
        if (end_z != start_z) {
            break;
        }
        index++;
    }

    direction.vx = end_x - start_x;
    direction.vz = end_y - start_y;
    direction.vy = end_z - start_z;
    VectorNormal(&direction, &unit->velocity);
    unit->velocity.vx = unit->velocity.vx * g_main_system_animation_speed;
    unit->velocity.vy = unit->velocity.vy * g_main_system_animation_speed;
    unit->velocity.vz = unit->velocity.vz * g_main_system_animation_speed;
}
