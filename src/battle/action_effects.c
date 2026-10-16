/* AT list activation, team palette tints, secondary effect starts, post-action unit display and rider mounting. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/ai.h"
#include "fft/animation.h"
#include "fft/at_list.h"
#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/command.h"
#include "fft/cursor.h"
#include "fft/effect.h"
#include "fft/effect_secondary.h"
#include "fft/item.h"
#include "fft/move.h"
#include "fft/move_step.h"
#include "fft/result.h"
#include "fft/sprite.h"
#include "fft/state.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/target.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Projectile secondary animation of each weapon item type: BOW_ARROW for bows, CROSSBOW_BOLT for
 * crossbows, NONE for the rest. */
u8 g_effect_weapon_projectile_animations[32]
    = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* Show the map cursor and its tile glow again; the counterpart of
 * cursor_hide. */
void cursor_show(void) {
    g_cursor_visible = 1;
}

/* Hide the map cursor and its tile glow: the flag is read only by
 * cursor_update. */
void cursor_hide(void) {
    g_cursor_visible = 0;
}

/* Move the map cursor to a unit's tile and elevation; a null unit leaves it where it
 * is. */
void cursor_move_to_unit(const unit_t* unit) {
    if (unit != 0) {
        g_cursor_x = unit->map_x;
        g_cursor_y = unit->map_y;
        g_cursor_z = unit->map_z;
    }
}

/* Move the cursor to the target of the unit's stored action: the target tile, or
 * the tile of the target unit. */
void cursor_move_to_action_target(const unit_t* unit) {
    unit_stats_t* stats = unit->stats;

    if (stats->action_command.targeting_type == TARGET_TYPE_TILE) {
        /* The retail code reloads unit->stats for the elevation and y reads. */
        g_cursor_x = stats->action_command.target_position.vx;
        g_cursor_z = unit->stats->action_command.target_position.vy;
        g_cursor_y = unit->stats->action_command.target_position.vz;
        return;
    }
    cursor_move_to_unit(unit_get_by_battle_id(stats->action_command.target_id));
}

/* Stop every running color animation on the unit CLUT row and restore those palettes
 * to their original colors. */
void sprite_palette_restore_all(void) {
    s32 palette_index;

    for (palette_index = 0; palette_index < BATTLE_MAP_CLUTS_PER_ROW; palette_index++) {
        if (g_battle_map_palette_state.clut_rows[BATTLE_MAP_CLUT_ROW_UNITS].animations[palette_index].active != 0) {
            battle_map_palette_modify(
                BATTLE_MAP_COLOR_MODE_STOP_REPEAT, 4, BATTLE_MAP_CLUT_ROW_UNITS, palette_index, 0, 0, 0, 0);
            battle_map_palette_modify(
                BATTLE_MAP_COLOR_MODE_RESTORE, 4, BATTLE_MAP_CLUT_ROW_UNITS, palette_index, 0, 0, 0, 0);
        }
    }
}

/* Tint a unit's palette by its team: blue, red, green or light blue (green plus
 * blue). The tint is a repeating base-color pulse on the unit's own CLUT;
 * `amount` passes through to battle_map_palette_modify. A null unit or one
 * without battle data is left alone. */
void sprite_palette_tint_by_team(unit_t* unit, s32 amount) {
    if (unit == 0) {
        return;
    }
    if (unit->stats == 0) {
        return;
    }
    switch ((unit->stats->team_flags & UNIT_TEAM_MASK) >> UNIT_TEAM_SHIFT) {
    case UNIT_TEAM_BLUE >> UNIT_TEAM_SHIFT:
        battle_map_palette_modify(
            BATTLE_MAP_COLOR_MODE_BASE_REPEAT, amount, BATTLE_MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0, 0, 0x18);
        break;
    case UNIT_TEAM_RED >> UNIT_TEAM_SHIFT:
        battle_map_palette_modify(
            BATTLE_MAP_COLOR_MODE_BASE_REPEAT, amount, BATTLE_MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0x10, 0, 0);
        break;
    case UNIT_TEAM_GREEN >> UNIT_TEAM_SHIFT:
        battle_map_palette_modify(
            BATTLE_MAP_COLOR_MODE_BASE_REPEAT, amount, BATTLE_MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0, 0x10, 0);
        break;
    default:
        battle_map_palette_modify(
            BATTLE_MAP_COLOR_MODE_BASE_REPEAT, amount, BATTLE_MAP_CLUT_ROW_UNITS, unit->runtime_id, 0, 0, 0x10, 0x10);
        break;
    }
}

/* Pulse every unit's palette in its team color (sprite_palette_tint_by_team): the free
 * cursor's Square highlight and the target-flashing option of the ability preview. */
void sprite_palette_tint_all_by_team(void) {
    s32 i;

    for (i = 0; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
        sprite_palette_tint_by_team(unit_get_by_runtime_id(i), 4);
    }
}

/* Advance the AT list cursor, wrapping after the list's 40 entries. */
static void at_list_advance_cursor(void) {
    g_at_list_cursor++;
    if (g_at_list_cursor >= AT_LIST_CAPACITY) {
        g_at_list_cursor = 0;
    }
}

/* Show the next AT list entry and advance the AT list cursor past it.
 *
 * Empty entries are skipped. A charged action moves the cursor to its target and
 * marks the tiles it will hit; a unit's turn moves the cursor to the unit and
 * removes the tile marks. The free cursor's Start button and the status-menu
 * request call it. The loop has no exit when every entry is empty. */
void at_list_show_next_entry(void) {
    s32 id;
    unit_t* unit;

    for (;;) {
        id = at_list_get_entry_unit_id(g_at_list_cursor);
        if (id >= 0) {
            unit = unit_get_by_battle_id(id & AT_LIST_ENTRY_BATTLE_ID_MASK);
            if ((id & AT_LIST_ENTRY_CHARGED_ACTION) != 0) {
                cursor_move_to_action_target(unit);
                target_area_set_for_action(&unit->stats->action_command);
                target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_SHOW_TARGETS, 3);
            } else {
                cursor_move_to_unit(unit);
                target_set_tile_mark_colors(BATTLE_MAP_TILE_MARK_NONE, 0);
            }
            break;
        }
        at_list_advance_cursor();
    }
    at_list_advance_cursor();
}

/* Open the status menu once: mark it open and restart the AT list cursor at entry
 * 0. */
void menu_status_open(void) {
    if (g_menu_status_open == 0) {
        g_menu_status_open = 1;
        g_at_list_cursor = 0;
    }
}

/* Restart the AT list cursor at entry 0, so the free cursor's next Start press shows the first
 * entry. */
void at_list_reset_cursor(void) {
    g_at_list_cursor = 0;
}

/* Drop a pending status-menu request (Triangle during a unit's turn the player does not control)
 * and mark the status menu closed. */
void menu_status_clear(void) {
    g_menu_status_requested = 0;
    g_menu_status_open = 0;
}

/* Fill an init record for a secondary effect that one unit both casts and
 * receives: the caster and the single target are the unit. Every other field of
 * the record is left as the caller's stack held it. */
void effect_secondary_build_init(unit_t* unit, effect_secondary_init_t* effect) {
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
static void effect_secondary_build_init_from_action(unit_t* unit, effect_secondary_init_t* init) {
    unit_t* target;
    action_data_t* action;
    s32 i;

    init->target_count = unit->strike.target_count;
    init->targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    init->targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id = unit->runtime_id;
    init->used_weapon_id = unit->strike.used_weapon_id;
    if (init->target_count != 0) {
        for (i = 0; i < init->target_count; i++) {
            target = unit_get_by_battle_id(unit->strike.target_list[i]);
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
        init->targets[0].data.tile.vy = g_cursor_z;
        init->targets[0].data.tile.vz = g_cursor_y;
    }
}

/* Start the weapon-hit secondary effect of the unit's action, in the element
 * of the used weapon. `is_bow` is 1 from the projectile-hit path and 0
 * otherwise; effect_secondary_init_weapon_hit ignores it. */
void effect_secondary_start_weapon_hit(unit_t* unit, s32 is_bow) {
    effect_secondary_init_t secondary_init;

    effect_secondary_build_init_from_action(unit, &secondary_init);
    effect_secondary_init_weapon_hit(g_item_weapon_data[unit->used_item_or_weapon_id].element, &secondary_init, is_bow);
}

/* Start the projectile secondary effect of the unit's action: the stone for
 * Throw Stone, the thrown item for the Item and Throw abilities, and for
 * anything else the animation of the equipped weapon's type (an arrow for
 * bows and crossbows, none for the rest). The retail code tests Throw
 * Shuriken on its own before the Item/Throw span, which already holds it. */
void effect_secondary_start_projectile(unit_t* unit) {
    effect_secondary_init_t secondary_init;
    u16 ability_id;
    u8 animation_id;

    effect_secondary_build_init_from_action(unit, &secondary_init);
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
    effect_secondary_init_projectile(animation_id, &secondary_init);
}

/* Start a secondary effect on a unit as both its caster and its target, with the
 * unit's sprite palette as the effect's parameter. A unit without battle data
 * starts nothing.
 *
 * The init record is a stack local of which effect_secondary_build_init
 * sets five fields; effect_secondary_init copies the rest of its caster
 * and target blocks and its used_weapon_id as the stack left them. */
static void effect_secondary_set(unit_t* unit, s32 effect_type) {
    effect_secondary_init_t data;

    effect_secondary_build_init(unit, &data);
    if (unit->stats != 0) {
        effect_secondary_init(effect_type, unit->stats->sprite_palette, &data);
    }
}

/* Start the puff a unit gives off as it turns to crystal or treasure. */
void effect_secondary_set_zodiac_poof(unit_t* unit) {
    effect_secondary_set(unit, EFFECT_SECONDARY_ANIMATION_ZODIAC_POOF);
}

/* Start the smoke of a chocobo or monster unit dying. */
void effect_secondary_set_death_smoke(unit_t* unit) {
    effect_secondary_set(unit, EFFECT_SECONDARY_ANIMATION_DEATH_SMOKE);
}

/* Start the level-up effect on a unit, for both level and job level ups. */
void effect_secondary_set_level_up(unit_t* unit) {
    effect_secondary_set(unit, EFFECT_SECONDARY_ANIMATION_LEVEL_UP);
}

/* Start the trap effect on a unit that stepped on a tile trap; every trap kind uses
 * this one effect. */
void effect_secondary_set_trap(unit_t* unit) {
    effect_secondary_set(unit, EFFECT_SECONDARY_ANIMATION_TRAP);
}

/* Start the splash of a unit landing in swamp, marsh or open water. */
void effect_secondary_set_splash(unit_t* unit) {
    effect_secondary_set(unit, EFFECT_SECONDARY_ANIMATION_SPLASH);
}

/* Start the dust of a unit landing on hard ground, a floor or a structure. */
void effect_secondary_set_fall_dust(unit_t* unit) {
    effect_secondary_set(unit, EFFECT_SECONDARY_ANIMATION_FALL_DUST);
}

/* Start the teleport effect on a unit, used both as it leaves and as it arrives. */
void effect_secondary_set_teleport(unit_t* unit) {
    effect_secondary_set(unit, EFFECT_SECONDARY_ANIMATION_TELEPORT);
}

/* Start the reaction animation of the unit with this runtime id to the casting unit's action, for
 * effect keyframe actions. Always returns 1. */
s32 animation_set_target_by_runtime_id(s32 runtime_id) {
    unit_t* target = unit_get_by_runtime_id(runtime_id);
    unit_t* caster = unit_get_casting();

    animation_set_target(caster, target);
    return 1;
}

/* Starts the reaction animation of the unit in slot battle_id to the casting
 * unit's action. Always returns 1. No retail code calls it. */
s32 animation_set_target_by_battle_id(u16 battle_id) {
    unit_t* target = unit_get_by_battle_id(battle_id);
    unit_t* caster = unit_get_casting();

    animation_set_target(caster, target);
    return 1;
}

/* result_prepare_display for a runtime id. An absent unit raises
 * a pointer exception and returns 0. */
s32 result_prepare_display_by_runtime_id(s32 runtime_id) {
    unit_t* unit = unit_get_by_runtime_id(runtime_id);
    if (unit != 0) {
        result_prepare_display(unit);
        return 1;
    }
    system_handle_pointer_exception(SYSTEM_POINTER_EXCEPTION_NO_UNIT);
    return 0;
}

/* Lands a projectile strike on its target: the weapon hit sound, the target's
 * reaction animation, its post-action display and the evade and weapon-element
 * effect data. Does nothing when either unit is missing. */
static void action_resolve_projectile_hit(unit_t* attacker, unit_t* target) {
    if ((attacker != 0) && (target != 0)) {
        action_play_weapon_sfx(attacker, 1);
        animation_set_target(attacker, target);
        result_prepare_display_by_runtime_id(target->runtime_id);
        effect_secondary_start_weapon_hit(attacker, 1);
    }
}

/* action_resolve_projectile_hit for two battle ids. No retail code calls it. */
void action_resolve_projectile_hit_by_battle_id(u16 attacker_id, u16 target_id) {
    unit_t* attacker = unit_get_by_battle_id(attacker_id);
    unit_t* target = unit_get_by_battle_id(target_id);

    action_resolve_projectile_hit(attacker, target);
}

/* action_resolve_projectile_hit for two runtime ids: called by the arrow-arc
 * and linear projectile effects for the unit the projectile hits. */
void action_resolve_projectile_hit_by_runtime_id(u16 attacker_id, u16 target_id) {
    unit_t* attacker = unit_get_by_runtime_id(attacker_id);
    unit_t* target = unit_get_by_runtime_id(target_id);

    action_resolve_projectile_hit(attacker, target);
}

/* Apply a unit's queued status changes (removals, then additions) and refresh everything that
 * shows them: position after death or dismount, transparency, status bubble, tile and status
 * palette, transformations, status animations, spritesheet and float height. Then clear the queues
 * and the item display flag. */
static void unit_update_display(unit_t* unit) {
    battle_map_tile_t* tile;

    if (unit != 0) {
        tile = battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_z);
        unit->status_flags_1_4 &= ~unit->statuses_to_remove_1_4;
        unit->status_flags_5_6 &= ~unit->statuses_to_remove_5_6;
        unit->status_flags_1_4 |= unit->statuses_to_add_1_4;
        unit->status_flags_5_6 |= unit->statuses_to_add_5_6;
        unit_dismount_dead_rider(unit);
        sprite_update_transparency_flag(unit);
        status_bubble_update_active(unit);
        if (g_state_animation_continue_check != 0) {
            sprite_palette_check_tile_status_mod(unit, tile->flags.shading, 0);
        } else {
            sprite_palette_check_tile_status_mod(unit, tile->flags.shading, 1);
        }
        unit_apply_poach_morbol_transformation(unit);
        animation_set_for_status(unit);
        animation_update_for_status_changes(unit);
        sprite_sheet_apply_status_change(unit, tile->flags.shading);
        unit_update_float_and_jump_height(unit);
        unit->statuses_to_remove_5_6 = 0;
        unit->statuses_to_remove_1_4 = 0;
        unit->statuses_to_add_5_6 = 0;
        unit->statuses_to_add_1_4 = 0;
        unit->item_display_active = 0;
    }
}

/* unit_update_display for the unit with this runtime id; an id without a unit does nothing. */
void unit_update_display_by_runtime_id(s32 runtime_id) {
    unit_update_display(unit_get_by_runtime_id(runtime_id));
}

/* unit_update_display for the unit in slot battle_id. No retail code calls
 * it. */
void unit_update_display_by_battle_id(u16 battle_id) {
    unit_update_display(unit_get_by_battle_id(battle_id));
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
s32 status_queue_display_flag_change(s32 status, s32 enabled, s32 battle_id) {
    unit_t* unit = unit_get_by_battle_id(battle_id);

    if (unit == 0) {
        return 0;
    }
    switch (enabled) {
    case 0:
        switch (status) {
        case STATUS_HANDLER_INDEX(STATUS_ID_CRYSTAL):
        case STATUS_HANDLER_INDEX(STATUS_ID_CHICKEN):
            unit->statuses_to_remove_5_6 |= UNIT_STATUS_CHICKEN;
            unit->statuses_to_add_5_6 &= ~UNIT_STATUS_CHICKEN;
            break;
        case STATUS_HANDLER_INDEX(STATUS_ID_FROG):
            unit->statuses_to_remove_5_6 |= UNIT_STATUS_FROG;
            unit->statuses_to_add_5_6 &= ~UNIT_STATUS_FROG;
            ai_init_unit_abilities(battle_id);
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
    case 1:
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
            ai_init_unit_abilities(battle_id);
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
    case 2:
        /* No effect, but the label makes 1 the root of the target's
         * compare tree. */
        break;
    }
    return 1;
}

/* sprite_palette_set_thrown_item for the unit in slot battle_id. No
 * retail code calls it. */
void sprite_palette_set_thrown_item_by_battle_id(s32 item_id, u16 battle_id) {
    sprite_palette_set_thrown_item(item_id, unit_get_by_battle_id(battle_id));
}

/* Seat the rider on the mount: link both records, copy the mount's facing,
 * position and map coordinates to the rider and raise the rider 10 world units
 * above it (negative y is up), then refresh both units' display. */
void unit_mount_seat_rider(unit_t* rider, unit_t* mount) {
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
    rider->map_z = mount->map_z;
    rider->status_flags_1_4 |= UNIT_STATUS_MOUNTED;
    animation_set_for_status(rider);
    rider->animation.script_pos = mount->animation.script_pos;
    unit_update_display_by_runtime_id(rider->runtime_id);
    unit_update_display_by_runtime_id(mount->runtime_id);
}

/* Unlinks a rider from its mount: both records return to the unmounted state and
 * the rider loses its mounted display flag. Does nothing for a unit that is not a
 * rider. */
void unit_mount_dismount_rider(unit_t* unit) {
    unit_t* mount;
    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        mount = unit_get_by_runtime_id(unit->mount_partner_runtime_id);
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

/* unit_mount_dismount_rider, followed by a display update of the rider and of
 * the mount. */
void unit_mount_dismount_rider_and_update_display(unit_t* rider) {
    unit_t* mount;

    if (rider->mount_state == UNIT_MOUNT_STATE_RIDER) {
        mount = unit_get_by_runtime_id(rider->mount_partner_runtime_id);
        rider->standing_on_battle_id_plus_one = 0;
        rider->mount_state = UNIT_MOUNT_STATE_NONE;
        rider->mount_partner_runtime_id = 0;
        rider->status_flags_1_4 &= ~UNIT_STATUS_MOUNTED;
        unit_update_display_by_runtime_id(rider->runtime_id);
        if (mount != 0) {
            mount->mount_state = UNIT_MOUNT_STATE_NONE;
            mount->mount_partner_runtime_id = 0;
            unit_update_display_by_runtime_id(mount->runtime_id);
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
static s32 move_step_calculate_layer_offset(s32 x, s32 y, u32 layer) {
    battle_map_tile_t* from = battle_map_tile_get_pointer(x, y, layer);
    battle_map_tile_t* to = battle_map_tile_get_pointer(x, y, layer ^ 1);
    s32 from_height;
    s32 to_height;
    s32 limit;

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

enum {
    MOVE_STEP_HEIGHT_SCALE = 6, /* world units per half height unit; world y grows downward */
};

/*
 * Clamp a world height (world_coords.vy) to a band above one tile. World y
 * grows downward, so the smaller bound is the higher one: `lo` is the tile
 * surface raised by the clearance of move_step_calculate_layer_offset,
 * and `hi` is the surface raised by 0x30 world units (8 half heights). When the
 * band is empty the result is `lo`.
 */
static s32 move_step_clamp_z_to_headroom(s32 value, s32 x, s32 y, u32 layer) {
    battle_map_tile_t* tile = battle_map_tile_get_pointer(x, y, layer);
    s32 clearance = move_step_calculate_layer_offset(x, y, layer);
    s32 height;
    s32 lo;
    s32 hi;
    battle_map_tile_depth_t depth = tile->depth_half_height;

    height = ((tile->height + depth.depth) * 2) + depth.half_height;
    lo = -(height + clearance) * MOVE_STEP_HEIGHT_SCALE;
    if (value < lo) {
        return lo;
    }
    hi = (-height * MOVE_STEP_HEIGHT_SCALE) - 0x30;
    if (value >= hi) {
        if (hi < lo) {
            return lo;
        }
        return hi;
    }
    return value;
}

enum {
    MOVE_STEP_HALF_WIDTH = BATTLE_MAP_TILE_SIZE / 2,
    MOVE_STEP_NEAR_EDGE = -7,
    MOVE_STEP_FAR_EDGE = 35,
};

/*
 * Build the per-frame velocity for a run of identical movement steps: walk
 * the remaining path while the steps keep the same direction and the clamped
 * ground height stays at the unit's current height, then normalize the delta
 * from the unit to the far edge of the run and scale it by the vsync interval.
 * move_step_set_velocity_with_final_height is the
 * same walk with the target tile's own surface height on the path's last step.
 *
 * The end point stops 7 world units short of the near edge of the run's last
 * tile (MOVE_STEP_NEAR_EDGE when x or y grows, MOVE_STEP_FAR_EDGE, a tile plus 7,
 * when it shrinks), on the center line of the other axis.
 *
 * The step code is copied to a 16-bit value before the switch: read through the
 * byte again, cse reuses the direction test's load and the reload is lost.
 */
void move_step_set_velocity_clamped(unit_t* unit, const move_path_t* path, const move_step_t* step) {
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

    battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_z); /* result unused */
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
        /* Bits 6-7 are the step's direction, bits 0-1 its length less one. */
        code = *(u8*)current;
        switch (code >> 6) {
        case MOVE_STEP_DIRECTION_EAST:
            x += (code & 3) + 1;
            end_x = (x * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_NEAR_EDGE;
            end_y = (y * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            break;
        case MOVE_STEP_DIRECTION_WEST:
            x -= (code & 3) + 1;
            end_x = (x * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_FAR_EDGE;
            end_y = (y * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            break;
        case MOVE_STEP_DIRECTION_SOUTH:
            y -= (code & 3) + 1;
            end_x = (x * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            end_y = (y * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_FAR_EDGE;
            break;
        case MOVE_STEP_DIRECTION_NORTH:
            y += (code & 3) + 1;
            end_x = (x * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            end_y = (y * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_NEAR_EDGE;
            break;
        }
        end_z = move_step_clamp_z_to_headroom(start_z, x, y, current->higher);
        if (end_z != start_z) {
            break;
        }
        index++;
    }

    direction.vx = end_x - start_x;
    direction.vz = end_y - start_y;
    direction.vy = end_z - start_z;
    VectorNormal(&direction, &unit->velocity);
    unit->velocity.vx = unit->velocity.vx * g_system_vsync_interval;
    unit->velocity.vy = unit->velocity.vy * g_system_vsync_interval;
    unit->velocity.vz = unit->velocity.vz * g_system_vsync_interval;
}

/*
 * Build the per-frame velocity for a run of identical movement steps, as
 * move_step_set_velocity_clamped does, but aiming at the
 * far edge of the run's last tile, and on the path's last step at that tile's
 * own surface height instead of the clamped one.
 *
 * The step code is copied to a 16-bit value before the switch, as in the clamped
 * twin: read through the byte again, cse reuses the direction test's load.
 */
void move_step_set_velocity_with_final_height(unit_t* unit, const move_path_t* path, const move_step_t* step) {
    VECTOR direction;
    battle_map_tile_t* tile;
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
    battle_map_tile_depth_t depth;
    s16 code;

    battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_z); /* result unused */
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
            end_x = (x * BATTLE_MAP_TILE_SIZE) + BATTLE_MAP_TILE_SIZE;
            end_y = (y * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            break;
        case MOVE_STEP_DIRECTION_WEST:
            x -= (code & 3) + 1;
            end_x = x * BATTLE_MAP_TILE_SIZE;
            end_y = (y * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            break;
        case MOVE_STEP_DIRECTION_SOUTH:
            y -= (code & 3) + 1;
            end_x = (x * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            end_y = y * BATTLE_MAP_TILE_SIZE;
            break;
        case MOVE_STEP_DIRECTION_NORTH:
            y += (code & 3) + 1;
            end_x = (x * BATTLE_MAP_TILE_SIZE) + MOVE_STEP_HALF_WIDTH;
            end_y = (y * BATTLE_MAP_TILE_SIZE) + BATTLE_MAP_TILE_SIZE;
            break;
        }
        layer = current->higher;
        if (unit->movement_path_offset == path->count) {
            tile = battle_map_tile_get_pointer(x, y, layer);
            depth = tile->depth_half_height;
            end_z = -(((tile->height + depth.depth) * 2) + depth.half_height) * MOVE_STEP_HEIGHT_SCALE;
        } else {
            end_z = move_step_clamp_z_to_headroom(start_z, x, y, layer);
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
    unit->velocity.vx = unit->velocity.vx * g_system_vsync_interval;
    unit->velocity.vy = unit->velocity.vy * g_system_vsync_interval;
    unit->velocity.vz = unit->velocity.vz * g_system_vsync_interval;
}
