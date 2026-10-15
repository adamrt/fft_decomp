#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_script.h"
#include "fft/battle_status.h"
#include "fft/battle_unit.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/status.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* The target allocates/releases a 48-byte stack frame, then returns.
 * The unused array reproduces the frame without claiming original locals. */
void script_noop_11(void) {
    s32 unused[12];
}

/* Resolve a single selector in place; groups keep their ID and set the filter mode. */
s32 script_prepare_unit_filter(u16* unit_id, s32* filter_mode) {
    u32 id;

    id = *unit_id;
    if (id != 0 && id < EVENT_UNIT_GROUP_FIRST) {
        *unit_id = script_resolve_runtime_id(id);
        *filter_mode = 0;
        if (*unit_id == EVENT_RUNTIME_ID_NONE) {
            return 0;
        }
    } else {
        *filter_mode = id != 0 ? id - 0xfe : 1;
    }
    return 1;
}

/* Answer one of six unit queries about the runtime id in `in_id`, copying it to
 * `out_id` first.  `mode` selects the query:
 *
 *   0  the id is always accepted
 *   1  the unit exists
 *   2  the unit is on the blue team
 *   3  blue team and free of the check-set 8 statuses
 *   4  the unit is on a non-blue team
 *   5  non-blue team and free of the check-set 8 statuses, ignoring Undead
 *
 * Unsupported modes fall off the end without a defined return value.
 */
s32 script_filter_unit_id_by_mode(u16* out_id, u16* in_id, s32* mode) {
    unit_stats_t* unit;
    s32 index;
    s32 i;
    s32 blocked;
    s32 query;
    u8 first;

    if (*mode == 0) {
        return 1;
    }
    *out_id = *in_id;
    if (battle_unit_has_runtime_id(*out_id) != 0) {
        index = battle_unit_get_battle_id_by_runtime_id(*out_id);
        if (index != -1) {
            unit = battle_unit_get_stats_from_battle_id(index);
        }
    }
    if (*mode == 1) {
        return battle_unit_has_runtime_id(*out_id);
    }
    if (battle_unit_has_runtime_id(*out_id) == 0) {
        return 0;
    }
    query = *mode;
    if (query == 2) {
        if (index == -1) {
            return 1;
        }
        return (unit->team_flags & BATTLE_TEAM_MASK) == BATTLE_TEAM_BLUE;
    }
    if (query == 3) {
        if (index == -1) {
            return 1;
        }
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if ((unit->status_sets.current[i]
                    & g_main_status_check_sets[MAIN_STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED][i])
                != 0) {
                break;
            }
        }
        if ((unit->team_flags & BATTLE_TEAM_MASK) == BATTLE_TEAM_BLUE && i == STATUS_BYTE_COUNT) {
            return 1;
        }
        return 0;
    }
    if (query == 4) {
        if (index == -1) {
            return 0;
        }
        return (unit->team_flags & BATTLE_TEAM_MASK) != BATTLE_TEAM_BLUE;
    }
    if (query == 5) {
        if (index == -1) {
            return 0;
        }
        first = g_main_status_check_sets[MAIN_STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED][0];
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if (i == 0) {
                s32 status = unit->status_sets.current[0] & (0xff ^ STATUS_BYTE_MASK(STATUS_ID_UNDEAD));

                blocked = first & status;
            } else {
                blocked = unit->status_sets.current[i]
                    & g_main_status_check_sets[MAIN_STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED][i];
            }
            if (blocked != 0) {
                break;
            }
        }
        if ((unit->team_flags & BATTLE_TEAM_MASK) != BATTLE_TEAM_BLUE && i == STATUS_BYTE_COUNT) {
            return 1;
        }
        return 0;
    }
}

/* TeleportOut: play the teleport sound and secondary effect on the unit, raise
 * its hands, modulate its palette by +0x1f per channel, wait 60 frames, modulate
 * it by -0x1f and wait 60 frames more. Then, when `remove` is set, mark its
 * battle unit as not existing. A unit id that does not resolve does nothing. The
 * interpreter always passes remove = 1. */
void script_teleportout_event_instruction(s32 unit_id, s32 remove) {
    s32 unit;
    s32 idx;

    unit = script_resolve_runtime_id(unit_id);
    if (unit == EVENT_RUNTIME_ID_NONE) {
        return;
    }
    g_sound_pending_sfx_id = MAIN_SFX_TELEPORT_OUT;
    g_thread_call_target = (void (*)(void))effect_set_secondary_teleport_by_runtime_id;
    thread_call_on_main_stack(unit);
    idx = battle_unit_get_battle_id_by_runtime_id(unit);
    unit_set_animation(unit, BATTLE_UNIT_ANIMATION_RAISE_HANDS);
    battle_gfx_start_event_unit_fade_out(unit, 2, 0x1F, 0x1F, 0x1F);
    thread_wait_frames(0x3C);
    battle_gfx_start_event_unit_fade_out(unit, 2, -0x1F, -0x1F, -0x1F);
    thread_wait_frames(0x3C);
    if (remove != 0) {
        battle_unit_set_cannot_exist(idx);
    }
}

/* Start the teleport-in effect for an event unit. The interpreter supplies 1
 * as its second argument; this entry forwards only the unit ID. */
void script_teleportin_event_instruction(s32 unit_id, s32 unused) {
    s32 runtime_id = script_resolve_runtime_id(unit_id);
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        g_thread_call_target = (void (*)(void))effect_set_secondary_teleport_by_runtime_id;
        thread_call_on_main_stack(runtime_id);
    }
}

/* CallFunction 4: tint blue (red and green modulated by -0x1f) every displayed
 * unit whose initial team is not the blue one, skipping units whose ENTD lookup
 * reports -2. */
void script_blue_remove_all_enemy_units(void) {
    s32 i;
    s32 idx;
    s32 out_index;
    unit_stats_t* stats;
    unit_stats_t* found;

    for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
        if (battle_unit_has_runtime_id(i) != 0) {
            idx = battle_unit_get_battle_id_by_runtime_id(i);
            if (idx != -1) {
                stats = battle_unit_get_stats_from_battle_id(idx);
                found = battle_unit_find_stats_by_entd_unit_id(stats->unit_id, &out_index);
                if ((found->initial_team_flags & BATTLE_TEAM_MASK) && out_index != -2) {
                    battle_gfx_start_event_unit_fade_out(i, 2, -0x1F, -0x1F, 0);
                }
            }
        }
    }
}

/* BlueRemoveUnit (event instruction 0x99): tints every non-player unit whose
 * ENTD unit id is unit_id blue (red and green modulated by -0x1f).
 *
 * The team test reads the current team byte (0x05), not the initial one at
 * 0x1ba. */
void script_blue_remove_unit(s32 unit_id) {
    s32 runtime_id;
    s32 battle_index;
    unit_stats_t* unit;
    s32 found_battle_id[2];

    for (runtime_id = 0; runtime_id < EVENT_UNIT_SLOT_COUNT; runtime_id++) {
        if (battle_unit_has_runtime_id(runtime_id) == 0) {
            continue;
        }
        battle_index = battle_unit_get_battle_id_by_runtime_id(runtime_id);
        if (battle_index == -1) {
            continue;
        }
        unit = battle_unit_find_stats_by_entd_unit_id(
            battle_unit_get_stats_from_battle_id(battle_index)->unit_id, found_battle_id);
        if ((unit->team_flags & BATTLE_TEAM_MASK) == BATTLE_TEAM_BLUE) {
            continue;
        }
        if (found_battle_id[0] == -2) {
            continue;
        }
        if (unit->unit_id != unit_id) {
            continue;
        }
        battle_gfx_start_event_unit_fade_out(runtime_id, 2, -0x1F, -0x1F, 0);
    }
}

/*
 * WarpUnit event instruction: move an ENTD unit to a new tile and facing.
 *
 * Parameters: 0x00 ENTD unit id (16-bit value), 0x02 x, 0x03 y, 0x04 higher
 * elevation, 0x05 facing.
 */
void script_warp_unit(const event_warp_unit_parameters_t* instruction) {
    /* Separate instruction and placement pointers change register allocation. */
    union {
        const event_warp_unit_parameters_t* instruction;
        const event_unit_placement_t* placement;
    } cursor;
    unit_stats_t* unit;
    s32 battle_id;
    s32 runtime_id;
    s32 result;
    s16 unit_id;
    map_tile_coordinates_t tile;
    unit_animation_state_t* rotation_state;

    cursor.instruction = instruction;
    unit_id = script_read_s16(cursor.instruction->unit_id_le);
    cursor.placement = &cursor.instruction->placement;
    unit = battle_unit_find_stats_by_entd_unit_id(unit_id, &result);
    battle_id = 0;
    if (result != -2 && result < 0) {
        return;
    }
    for (; battle_id < EVENT_UNIT_SLOT_COUNT; battle_id++) {
        if (battle_unit_get_stats_from_battle_id(battle_id) == unit) {
            break;
        }
    }
    unit->x = cursor.placement->x;
    unit->position.y = cursor.placement->y;
    unit->position.higher_elevation = cursor.placement->elevation;
    unit->position.facing = cursor.placement->facing;
    runtime_id = battle_unit_get_runtime_id_by_battle_id(battle_id);
    if (runtime_id == -1) {
        return;
    }
    tile.x = unit->x;
    tile.layer = unit->position.higher_elevation;
    tile.y = unit->position.y;
    rotation_state = &g_unit_animation_states[runtime_id];
    rotation_state->target_facing = cursor.placement->facing * 4;
    rotation_state->delay = 0;
    rotation_state->rotating = 0;
    battle_unit_place_in_new_location(runtime_id, &tile, cursor.placement->facing);
    unit_set_facing(runtime_id, cursor.placement->facing * 4);
}

/* Set facing and animation immediately; the animation high byte also controls facing-update suppression. */
void script_unit_animation_rotate_event_instruction(const event_unit_anim_rotate_parameters_t* parameters) {
    u8 rotation;
    s16 animation;
    s16 call_runtime_id;
    s16 runtime_id;
    s32 index;
    unit_animation_state_t* rotation_state;

    runtime_id = script_read_s16(parameters->unit_id_le);
    rotation = parameters->rotation;
    animation = script_read_s16(parameters->animation_le);
    call_runtime_id = script_resolve_runtime_id(runtime_id);
    runtime_id = call_runtime_id;
    if (parameters->animation_le[1] == 0) {
        battle_unit_clear_facing_update_suppression(call_runtime_id);
    } else {
        battle_unit_set_facing_update_suppression(call_runtime_id);
    }
    index = runtime_id;
    rotation_state = &g_unit_animation_states[index];
    rotation_state->target_facing = rotation;
    rotation_state->delay = 0;
    rotation_state->rotating = 0;
    unit_set_facing(index, rotation);
    unit_set_animation(index, animation);
}

/* FACE_UNIT and FACE_EACH_OTHER event instructions:
 * turn two units toward each other.
 *
 * Parameters: first 16-bit unit id, second 16-bit unit id, rotate mode, speed index
 * into g_unit_rotation_speed_frames, and a delay multiplier. Every unit record of the second unit turns
 * toward the first; unless second_only is set (FaceUnit), the first unit also turns
 * toward the second. Successive second-unit records are staggered like FaceTile.
 * The angle is built in place (negate, then add) so combine cannot fold the two
 * constant additions into one. */
void script_face_unit(const event_face_unit_parameters_t* parameters, s32 second_only) {
    SVECTOR position;
    SVECTOR target;
    u16 unit_id;
    s32 filter_mode;
    u16 runtime_id;
    s32 i;
    s32 order;
    s32 facing;
    s32 reverse;
    s32 dx;
    s32 dy;
    unit_animation_state_t* state;

    order = 1;
    runtime_id = script_read_s16(parameters->first_unit_id_le);
    unit_id = script_read_s16(parameters->second_unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        runtime_id = script_resolve_runtime_id(runtime_id);
        if (runtime_id != EVENT_RUNTIME_ID_NONE) {
            for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
                if (script_filter_unit_id_by_mode(&unit_id, &g_script_unit_face_slot_order[i], &filter_mode) != 0
                    && runtime_id != unit_id) {
                    unit_copy_world_coords(runtime_id, &position);
                    unit_copy_world_coords(unit_id, &target);
                    dx = target.vx - position.vx;
                    dy = target.vz - position.vz;
                    facing = -ratan2(dy, dx);
                    facing += 0xC00;
                    reverse = facing + 0x800;
                    facing &= 0xF00;
                    reverse &= 0xF00;
                    facing >>= 8;
                    reverse >>= 8;
                    if (second_only == 0) {
                        state = &g_unit_animation_states[runtime_id];
                        state->target_facing = facing;
                        state->rotate_mode = parameters->mode;
                        state->frames_per_step = g_unit_rotation_speed_frames[parameters->speed];
                        state->step_counter = 0;
                        state->rotating = 1;
                        state->delay = parameters->delay >> 2;
                    }
                    state = &g_unit_animation_states[unit_id];
                    state->target_facing = reverse;
                    state->rotate_mode = parameters->mode;
                    state->frames_per_step = g_unit_rotation_speed_frames[parameters->speed];
                    state->step_counter = 0;
                    state->rotating = 1;
                    state->delay = parameters->delay * order++ / 4;
                    if (filter_mode == 0) {
                        break;
                    }
                }
            }
        }
    }
}

/* RotateUnit event instruction: turn the addressed unit(s) to a facing.
 *
 * Parameters: 0x00 unit id (16-bit value), 0x02 facing code, 0x03 rotate mode,
 * 0x04 rotation speed (index into g_unit_rotation_speed_frames), 0x05 delay multiplier.
 * Facing codes 0x11-0x13 are relative to the unit's current facing, 0x10 is
 * relative to the camera quadrant, 0x14 restores the saved facing, and any
 * other value is absolute. Matching records are staggered by delay * n / 4.
 *
 * The restore arm carries its own copy of the filter_mode test, which is what
 * retail does: it reloads filter_mode and jumps past the absolute arm's own
 * reload, so cross-jumping merges only the branch and the loop increment. */
void script_rotate_unit(const event_rotate_unit_parameters_t* parameters) {
    u16 unit_id;
    s32 filter_mode;
    s32 i;
    s32 order;
    s32 facing;
    s32 saved_filter_mode;
    unit_animation_state_t* state;

    order = 1;
    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
            if (script_filter_unit_id_by_mode(&unit_id, &g_script_unit_face_slot_order[i], &filter_mode) != 0) {
                facing = parameters->facing;
                if (facing >= EVENT_ROTATE_UNIT_FACING_RELATIVE_FIRST && facing < EVENT_ROTATE_UNIT_FACING_RESTORE) {
                    facing = facing & 0xF;
                    facing = battle_unit_get_facing_nibble_by_runtime_id(unit_id) + facing * 4;
                    facing = facing & 0xF;
                } else if (facing == EVENT_ROTATE_UNIT_FACING_CAMERA) {
                    facing = g_unit_rotation_speed_frames[4 + ((camera_wrap_yaw_angle() & 0xC00) >> 10)] * 4;
                } else if (facing == EVENT_ROTATE_UNIT_FACING_RESTORE) {
                    state = &g_unit_animation_states[unit_id];
                    if (state->initial_facing != 0xFF) {
                        state->rotate_mode = UNIT_ROTATE_MODE_SHORTEST;
                        state->target_facing = state->initial_facing;
                        state->frames_per_step = g_unit_rotation_speed_frames[parameters->speed];
                        state->step_counter = 0;
                        state->rotating = 1;
                        state->delay = parameters->delay * order++ / 4;
                    }
                    saved_filter_mode = filter_mode;
                    /* Keep the restore arm's stack reload before its loop exit. */
                    __asm__("" : "=r"(saved_filter_mode) : "0"(saved_filter_mode));
                    if (saved_filter_mode == 0) {
                        return;
                    }
                    continue;
                }
                state = &g_unit_animation_states[unit_id];
                state->target_facing = facing;
                state->rotate_mode = parameters->mode;
                state->frames_per_step = g_unit_rotation_speed_frames[parameters->speed];
                state->step_counter = 0;
                state->rotating = 1;
                state->delay = parameters->delay * order++ / 4;
                if (filter_mode == 0) {
                    return;
                }
            }
        }
    }
}

/* Queue the renderer status flags for each of a unit's current statuses on
 * the main stack; the Draw event instruction runs this only for the Zalera
 * event. */
void status_queue_current_status_graphics(s32 battle_id, s32 enabled) {
    unit_stats_t* stats;
    s32 i;
    s32 mask;

    stats = battle_unit_get_stats_from_battle_id(battle_id);
    g_thread_call_target = (void (*)(void))status_queue_runtime_graphics_flag_change;
    for (i = 0; i < STATUS_COUNT; i++) {
        mask = 0x80 >> (i % 8);
        if ((stats->status_sets.current[i / 8] & mask) != 0) {
            thread_call_on_main_stack(STATUS_HANDLER_INDEX(i), enabled, battle_id);
        }
    }
}

/* Resets every unit slot's status staging state and inflict-status mode to none. */
void unit_clear_status_staging_data(void) {
    s32 i;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        g_unit_status_staging_data->state[i] = UNIT_STATUS_STAGING_NONE;
        g_unit_status_staging_data->inflict_status_mode[i] = UNIT_INFLICT_STATUS_MODE_NONE;
    }
}

/* Snapshot a unit into the event staging record and queue its status removal.
 *
 * Returns 0 when the slot is already staged. Otherwise the unit's statuses,
 * timers, HP and team are saved, the removal mask is chosen by the pending
 * staged-status variable and the unit type (monsters, Altima and the undead
 * jobs use their own mask), and the removal is applied on the main stack.
 *
 * Each mask branch repeats the whole store tail; cross-jumping merges the
 * copies, and the repeated tail uses give the strength-reduced status walker
 * the target's s1 over the stats pointer. */
s32 unit_save_staged_status_data(s32 battle_id) {
    unit_stats_t* stats;
    s32 i;
    s32 value;
    unit_status_staging_t* st;

    if (g_unit_status_staging_data->state[battle_id] == UNIT_STATUS_STAGING_NONE) {
        stats = battle_unit_get_stats_from_battle_id(battle_id);
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_unit_status_staging_data->innate[battle_id][i] = stats->status_sets.innate[i];
            g_unit_status_staging_data->inflicted[battle_id][i] = stats->inflicted_status[i];
            g_unit_status_staging_data->current[battle_id][i] = stats->status_sets.current[i];
        }
        for (i = 0; i < BATTLE_TIMED_STATUS_COUNT; i++) {
            g_unit_status_staging_data->status_ct[battle_id][i] = stats->status_ct[i];
        }
        g_unit_status_staging_data->charged_ability_ct[battle_id] = stats->charged_ability_ct;
        g_unit_status_staging_data->death_counter[battle_id] = stats->death_counter;
        g_unit_status_staging_data->hp[battle_id] = stats->hp;
        g_unit_status_staging_data->team_flags[battle_id] = stats->team_flags;
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if (script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
                if ((stats->primary_skillset >= SKILLSET_ID_MONSTER_FIRST && stats->primary_skillset < SKILLSET_ID_END)
                    || stats->character_identity == CHARACTER_IDENTITY_ALTIMA_FIRST_FORM
                    || stats->character_identity == CHARACTER_IDENTITY_ALTIMA_SECOND_FORM
                    || stats->job_id == JOB_ID_KNIGHT_UNDEAD || stats->job_id == JOB_ID_ARCHER_UNDEAD
                    || stats->job_id == JOB_ID_WIZARD_UNDEAD || stats->job_id == JOB_ID_TIME_MAGE_UNDEAD
                    || stats->job_id == JOB_ID_ORACLE_UNDEAD || stats->job_id == JOB_ID_SUMMONER_UNDEAD) {
                    value = stats->status_sets.current[i];
                    value &= g_unit_status_staging_masks[UNIT_STATUS_STAGING_MASK_PRE_BATTLE_MONSTER_OR_UNDEAD][i];
                    st = g_unit_status_staging_data;
                    stats->action.status_changes.removal[i] = value;
                    st->added[battle_id][i] = value;
                    st->removed[battle_id][i] = value;
                } else {
                    value = stats->status_sets.current[i];
                    value &= g_unit_status_staging_masks[UNIT_STATUS_STAGING_MASK_PRE_BATTLE][i];
                    st = g_unit_status_staging_data;
                    stats->action.status_changes.removal[i] = value;
                    st->added[battle_id][i] = value;
                    st->removed[battle_id][i] = value;
                }
            } else {
                value = stats->status_sets.current[i];
                value &= g_unit_status_staging_masks[UNIT_STATUS_STAGING_MASK_BATTLE_STARTED][i];
                st = g_unit_status_staging_data;
                stats->action.status_changes.removal[i] = value;
                st->added[battle_id][i] = value;
                st->removed[battle_id][i] = value;
            }
        }
        if (g_unit_status_staging_data->removed[battle_id][STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
            & STATUS_BYTE_MASK(STATUS_ID_JUMP)) {
            g_unit_status_staging_data->jump_float_flags |= UNIT_STATUS_STAGING_JUMP_REMOVED;
        }
        if (g_unit_status_staging_data->removed[battle_id][STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
            g_unit_status_staging_data->jump_float_flags |= UNIT_STATUS_STAGING_FLOAT_REMOVED;
        }
        if (stats->action.status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_CHARM)]
            & STATUS_BYTE_MASK(STATUS_ID_CHARM)) {
            stats->team_flags = stats->initial_team_flags;
        }
        /* WORLD's copy targets this BATTLE address too (see QUIRKS.md). */
        g_thread_call_target = (void (*)(void))status_apply_unit_action_removal;
        thread_call_on_main_stack(stats);
        g_unit_status_staging_data->state[battle_id] = UNIT_STATUS_STAGING_SAVED;
        return 1;
    }
    return 0;
}

/*
 * Restore the saved unit state and replay status changes made during an event.
 *
 * The status masks track additions and removals relative to the snapshot.
 * Each changed status is applied through the main-thread dispatcher before
 * the unit graphics are refreshed. State 2 prevents applying the snapshot twice.
 */
void unit_apply_staged_status_data(s32 battle_id, s32 runtime_id) {
    unit_stats_t* unit;
    s32 i;
    s32 byte_index;
    s32 mask;
    u8 inflict_status_mode;
    if (!(script_get_current_scenario_finish_operation() & 0x8000)
        && g_unit_status_staging_data->state[battle_id] == UNIT_STATUS_STAGING_SAVED) {
        unit = battle_unit_get_stats_from_battle_id(battle_id);
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_unit_status_staging_data->added[battle_id][i]
                = unit->status_sets.current[i] ^ g_unit_status_staging_data->current[battle_id][i];
            g_unit_status_staging_data->removed[battle_id][i]
                |= g_unit_status_staging_data->added[battle_id][i] & g_unit_status_staging_data->current[battle_id][i];
            g_unit_status_staging_data->added[battle_id][i] &= unit->status_sets.current[i];
        }
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            unit->status_sets.innate[i] = g_unit_status_staging_data->innate[battle_id][i];
            unit->inflicted_status[i] = g_unit_status_staging_data->inflicted[battle_id][i];
            unit->status_sets.current[i] = g_unit_status_staging_data->current[battle_id][i];
        }
        for (i = 0; i < BATTLE_TIMED_STATUS_COUNT; i++) {
            unit->status_ct[i] = g_unit_status_staging_data->status_ct[battle_id][i];
        }
        unit->hp = g_unit_status_staging_data->hp[battle_id];
        unit->team_flags = g_unit_status_staging_data->team_flags[battle_id];
        unit->death_counter = g_unit_status_staging_data->death_counter[battle_id];
        /* WORLD's copy targets this BATTLE address too (see QUIRKS.md). */
        g_thread_call_target = (void (*)(void))status_queue_runtime_graphics_flag_change;
        for (i = 0; i < STATUS_COUNT; i++) {
            byte_index = i / 8;
            mask = 0x80 >> (i % 8);
            if (g_unit_status_staging_data->added[battle_id][byte_index] & mask) {
                thread_call_on_main_stack(STATUS_HANDLER_INDEX(i), 0, battle_id);
            }
            if (g_unit_status_staging_data->removed[battle_id][byte_index] & mask) {
                thread_call_on_main_stack(STATUS_HANDLER_INDEX(i), 1, battle_id);
            }
        }
        if (g_unit_status_staging_data->removed[battle_id][STATUS_BYTE_INDEX(STATUS_ID_JUMP)]
            & STATUS_BYTE_MASK(STATUS_ID_JUMP)) {
            g_unit_status_staging_data->jump_float_flags |= UNIT_STATUS_STAGING_JUMP_REMOVED;
        }
        if (g_unit_status_staging_data->removed[battle_id][STATUS_BYTE_INDEX(STATUS_ID_FLOAT)]
            & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
            g_unit_status_staging_data->jump_float_flags |= UNIT_STATUS_STAGING_FLOAT_REMOVED;
        }
        inflict_status_mode = g_unit_status_staging_data->inflict_status_mode[battle_id];
        if (inflict_status_mode != UNIT_INFLICT_STATUS_MODE_NONE) {
            if (inflict_status_mode == UNIT_INFLICT_STATUS_MODE_POISON) {
                unit->action.status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_POISON)]
                    |= STATUS_BYTE_MASK(STATUS_ID_POISON);
            } else if (inflict_status_mode == UNIT_INFLICT_STATUS_MODE_CRYSTAL) {
                unit->action.status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                    |= STATUS_BYTE_MASK(STATUS_ID_CRYSTAL);
            } else if (inflict_status_mode == UNIT_INFLICT_STATUS_MODE_REVIVE) {
                unit->hp = 1;
                unit->action.status_changes.infliction[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)]
                    |= STATUS_BYTE_MASK(STATUS_ID_CRITICAL);
                unit->action.status_changes.removal[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
                    |= STATUS_BYTE_MASK(STATUS_ID_DEAD);
            }
            g_thread_call_target = (void (*)(void))status_resolve_unit_changes;
            thread_call_on_main_stack(battle_id, 1);
        }
        if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)]
                & (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
                    | STATUS_BYTE_MASK(STATUS_ID_PERFORMING)))) {
            unit->charged_ability_ct = 0xff;
        } else {
            unit->charged_ability_ct = g_unit_status_staging_data->charged_ability_ct[battle_id];
        }
        g_unit_status_staging_data->state[battle_id] = UNIT_STATUS_STAGING_RESTORED;
        g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
        thread_call_on_main_stack(runtime_id);
    }
}

/* Battle twin of unit_update_staged_status_data. Saves or restores the event status snapshot for every unit
 * record of a unit.
 *
 * With restore clear, each resolved battle unit is snapshotted and its graphics are
 * refreshed on the main stack; otherwise the snapshot is replayed through
 * battle_unit_apply_staged_status_data. Follow-up flags add 40-frame waits. */
void unit_update_staged_status_data(u16 unit_id, u16 restore) {
    s32 filter_mode;
    s32 unit_index;
    s32 battle_id;
    s32 battle_unit_index;

    g_unit_status_staging_data = &g_ai_workspace_ptr->status_staging;
    if (g_menu_input_mode == MENU_INPUT_PLAYER && script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        unit_index = 0;
        g_unit_status_staging_data->jump_float_flags = 0;
        for (; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &filter_mode) != 0) {
                battle_unit_index = battle_unit_get_battle_id_by_runtime_id(unit_id);
                if (battle_unit_index != -1) {
                    battle_unit_find_stats_by_entd_unit_id(
                        battle_unit_get_stats_from_battle_id(battle_unit_index)->unit_id, &battle_id);
                    if (battle_id >= 0) {
                        if (restore == 0) {
                            if (unit_save_staged_status_data(battle_unit_index) != 0) {
                                g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
                                thread_call_on_main_stack(unit_id);
                            }
                        } else {
                            unit_apply_staged_status_data(battle_unit_index, unit_id);
                        }
                    }
                }
                if (filter_mode == 0) {
                    break;
                }
            }
        }
        if (g_unit_status_staging_data->jump_float_flags != 0) {
            thread_wait_frames(40);
        }
        if (g_unit_status_staging_data->jump_float_flags & UNIT_STATUS_STAGING_JUMP_REMOVED) {
            thread_wait_frames(40);
        }
        if (restore == 0) {
            g_thread_call_target = battle_gfx_reset_jumping_unit_graphic_triggers;
            thread_call_on_main_stack();
        }
    }
}

/* Apply the event status mode and stage its matching exit action. */
void script_inflict_status_thread(void) {
    event_inflict_status_parameters_t* parameters;
    s32 unit_id;
    s32 mode;
    s32 wait_frames;
    s32 battle_unit_index;
    s32 runtime_id;
    unit_stats_t* unit;
    s32 state;

    thread_set_current_task_id(THREAD_TASK_INFLICT_STATUS);
    parameters = (event_inflict_status_parameters_t*)thread_get_current_parameter_1_variant();
    unit_id = script_read_s16(parameters->unit_id_le);
    mode = parameters->mode;
    wait_frames = script_read_s16(parameters->wait_frames_le);
    battle_unit_index = unit_get_battle_id_by_event_unit_id(unit_id);
    runtime_id = script_resolve_runtime_id(unit_id);
    if (runtime_id == EVENT_RUNTIME_ID_NONE) {
        thread_exit_current();
    }
    unit_update_staged_status_data(unit_id, 0);
    unit = battle_unit_get_stats_from_battle_id(battle_unit_index);
    if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD)) {
        state = 0;
    } else {
        state = 2;
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)] & STATUS_BYTE_MASK(STATUS_ID_CRITICAL)) {
            state = 1;
        }
    }
    if (mode == EVENT_INFLICT_STATUS_POISON) {
        status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_POISON), 1, 1);
        g_unit_status_staging_data->inflict_status_mode[battle_unit_index] = UNIT_INFLICT_STATUS_MODE_POISON;
        g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
        thread_call_on_main_stack(runtime_id);
        unit_set_animation(runtime_id, BATTLE_UNIT_ANIMATION_KNEEL);
    } else if (mode == EVENT_INFLICT_STATUS_CRYSTAL) {
        status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_DEAD), 0, 1);
        status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_CRITICAL), 0, 1);
        status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_CRYSTAL), 1, 1);
        g_unit_status_staging_data->inflict_status_mode[battle_unit_index] = UNIT_INFLICT_STATUS_MODE_CRYSTAL;
        g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
        thread_call_on_main_stack(runtime_id);
    } else if (mode == EVENT_INFLICT_STATUS_REVIVE) {
        if (state == 0) {
            g_unit_status_staging_data->inflict_status_mode[battle_unit_index] = UNIT_INFLICT_STATUS_MODE_REVIVE;
            status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_DEAD), 0, 1);
            unit->hp = 1;
            status_inflict_by_event_unit_id(unit_id, STATUS_LSB_INDEX(STATUS_ID_CRITICAL), 1, 1);
            g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
            thread_call_on_main_stack(runtime_id);
            g_sound_pending_sfx_id = MAIN_SFX_EVENT_REVIVE;
        } else if (state == 2) {
            g_thread_call_target = (void (*)(void))battle_unit_set_mounted_animation_by_runtime_id;
            thread_call_on_main_stack(runtime_id);
        }
    }
    if (wait_frames != 0) {
        thread_wait_frames(wait_frames);
    }
    thread_exit_current();
}

/* Event-script status change: inflicts (which != 0) or removes one status on the
 * unit an event unit id resolves to.
 *
 * The unit's pending infliction and removal bytes are cleared, the one status bit
 * is set and status_resolve_unit_changes runs on the main stack with
 * removal_only. Nothing happens when the id resolves to no unit on the field. */
void status_inflict_by_event_unit_id(s32 event_unit_id, s32 status, s32 which, s32 removal_only) {
    s32 idx;
    unit_stats_t* unit;
    s32 i;
    s32 byte;
    s32 bit;

    idx = unit_get_battle_id_by_event_unit_id(event_unit_id);
    if (idx == 0x7D0) {
        return;
    }
    unit = battle_unit_get_stats_from_battle_id(idx);
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        unit->action.status_changes.infliction[i] = 0;
        unit->action.status_changes.removal[i] = 0;
    }
    /* Action infliction/removal bytes are LSB-first; stored status sets use
     * STATUS_BYTE_MASK's MSB-first representation. */
    byte = status / 8;
    bit = 1 << (status - byte * 8);
    if (which != 0) {
        unit->action.status_changes.infliction[byte] = bit;
    } else {
        unit->action.status_changes.removal[byte] = bit;
    }
    g_thread_call_target = (void (*)(void))status_resolve_unit_changes;
    thread_call_on_main_stack(idx, removal_only);
}

/* ChangeStats event instruction: add a signed delta to one stat of each
 * resolved unit.
 *
 * Parameters: 16-bit unit id, stat selector (event_change_stats_e), then a
 * 16-bit delta. HP clamps to max HP and the other stats to 0-100. */
void script_change_stats(const event_change_stats_parameters_t* parameters) {
    u16 unit_id;
    s32 filter_mode;
    s32 unit_index;
    s32 battle_id;
    s32 value;
    s16 delta;
    s32 battle_unit_index;
    unit_stats_t* unit;

    delta = script_read_s16(parameters->delta_le);
    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &filter_mode) != 0) {
        for (unit_index = 0; unit_index < EVENT_UNIT_SLOT_COUNT; unit_index++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &filter_mode) != 0) {
                battle_unit_index = battle_unit_get_battle_id_by_runtime_id(unit_id);
                if (battle_unit_index != -1) {
                    unit = battle_unit_find_stats_by_entd_unit_id(
                        battle_unit_get_stats_from_battle_id(battle_unit_index)->unit_id, &battle_id);
                    if (battle_id >= 0) {
                        if (parameters->stat == EVENT_CHANGE_STATS_HP) {
                            value = unit->hp + delta;
                            script_clamp_s32_to_range(&value, 0, unit->max_hp);
                            unit->hp = value;
                        }
                        if (parameters->stat == EVENT_CHANGE_STATS_CT) {
                            value = unit->ct + delta;
                            script_clamp_s32_to_range(&value, 0, UNIT_TURN_CT_THRESHOLD);
                            unit->ct = value;
                        }
                        if (parameters->stat == EVENT_CHANGE_STATS_BRAVE) {
                            value = unit->brave + delta;
                            script_clamp_s32_to_range(&value, 0, UNIT_BRAVE_FAITH_MAX);
                            unit->brave = value;
                        }
                        if (parameters->stat == EVENT_CHANGE_STATS_FAITH) {
                            value = unit->faith + delta;
                            script_clamp_s32_to_range(&value, 0, UNIT_BRAVE_FAITH_MAX);
                            unit->faith = value;
                        }
                    }
                }
                if (filter_mode == 0) {
                    break;
                }
            }
        }
    }
}

/*
 * UnitAnim event instruction.  Parameters: 0x00 unit id
 * (16-bit value), 0x02 animation id (16-bit value), 0x04 move-flag mode (0 unset,
 * 1 flip).  A unit id above 0xff selects a group, so the unit-state loop
 * runs over all 21 unit indices.
 */
void script_unit_anim(const event_unit_anim_parameters_t* parameters) {
    u16 runtime_id;
    s32 state;
    s32 unit_index;
    s32 animation;
    s32 move_flag_mode;

    runtime_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&runtime_id, &state) == 0) {
        return;
    }
    for (unit_index = 0; unit_index < EVENT_UNIT_SLOT_COUNT; unit_index++) {
        if (script_filter_unit_id_by_mode(&runtime_id, (u16*)&unit_index, &state) != 0) {
            move_flag_mode = parameters->facing_update_mode;
            if (move_flag_mode == 0) {
                battle_unit_clear_facing_update_suppression(runtime_id);
            } else if (move_flag_mode == 1) {
                battle_unit_set_facing_update_suppression(runtime_id);
            }
            animation = script_read_s16(parameters->animation_le);
            unit_set_animation(runtime_id, animation);
            if (animation >= BATTLE_UNIT_ANIMATION_BOW_HIGH && animation <= BATTLE_UNIT_ANIMATION_BOW_LOW) {
                script_play_effect_and_wait();
            }
            if (state == 0) {
                return;
            }
        }
    }
}

/*
 * Event instruction: put the addressed unit(s) into their status idle
 * animation and wait.  Parameters: 0x00 unit id (16-bit value), 0x02 wait length
 * in frames.  A unit id above 0xff selects a group, so the unit-state loop
 * runs over all 21 unit indices.
 */
void script_march_units(const event_march_parameters_t* parameters) {
    u16 runtime_id;
    s32 state;
    s32 unit_index;

    runtime_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&runtime_id, &state) == 0) {
        return;
    }
    for (unit_index = 0; unit_index < EVENT_UNIT_SLOT_COUNT; unit_index++) {
        if (script_filter_unit_id_by_mode(&runtime_id, (u16*)&unit_index, &state) != 0) {
            battle_unit_set_status_animation_by_runtime_id(runtime_id);
            thread_wait_frames(parameters->wait_frames);
            if (state == 0) {
                return;
            }
        }
    }
}

/*
 * Event instruction: set or clear movement-effect suppression on the addressed unit(s).
 * Parameters: 0x00 unit id (16-bit value), 0x02 nonzero to set, zero to clear.
 * A unit id above 0xff selects a group, so the unit-state loop runs over all
 * 21 unit indices.
 */
void script_set_units_movement_effect_suppression(const event_movement_effect_parameters_t* parameters) {
    u16 unit_id;
    s32 state;
    s32 unit_index;

    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &state) != 0) {
        for (unit_index = 0; unit_index < EVENT_UNIT_SLOT_COUNT; unit_index++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&unit_index, &state) != 0) {
                if (parameters->suppressed != 0) {
                    battle_unit_set_movement_effect_suppression(unit_id);
                } else {
                    battle_unit_clear_movement_effect_suppression(unit_id);
                }
                if (state == 0) {
                    break;
                }
            }
        }
    }
}

/* ColorUnit: start a palette modulation on a unit or a unit group.
 *
 * Operands: unit id or group (16-bit value), blend mode, red, green, blue (s8 each)
 * and a time. A plain unit id colors that one unit; id 0 and the group ids from
 * EVENT_UNIT_GROUP_FIRST run all 21 unit ids through
 * script_filter_unit_id_by_mode. Nothing happens when a plain unit id does
 * not resolve. */
void script_color_unit_event_instruction(const event_color_unit_parameters_t* parameters) {
    const event_unit_color_t* color;
    u16 unit_id;
    s32 out;
    s32 i;

    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &out) != 0) {
        color = &parameters->color;
        for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&i, &out) != 0) {
                battle_gfx_apply_unit_palette_modulation_to_units(
                    color->mode, color->duration, unit_id, color->rgb.r, color->rgb.g, color->rgb.b);
                if (out == 0) {
                    return;
                }
            }
        }
    }
}

/* Event instructions 0x6c (enable 0) and 0x6d (enable 1): clear or set
 * palette-update suppression on the event unit, or on every unit its group
 * mode selects.
 *
 * 0x80149698 stores a0 directly as a 16-bit value. The interpreter passes a
 * zero-extended 32-bit value; signed argument normalization is not part of this ABI. */
void script_set_units_palette_update_suppression(s32 event_unit_id, s32 enable) {
    u16 unit_id;
    s32 mode;
    s32 i;

    unit_id = event_unit_id;
    if (script_prepare_unit_filter(&unit_id, &mode) != 0) {
        for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
            if (script_filter_unit_id_by_mode(&unit_id, (u16*)&i, &mode) != 0) {
                if (enable == 0) {
                    battle_unit_clear_palette_update_suppression(unit_id);
                } else {
                    battle_unit_set_palette_update_suppression(unit_id);
                }
                if (mode == 0) {
                    return;
                }
            }
        }
    }
}
