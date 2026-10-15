#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Nonzero when the unit's facing, relative to the camera yaw quadrant, is
 * one of the six "away from camera" directions 7..12 (of 16). */
s32 unit_check_facing_relative_to_camera(s32 event_unit_id) {
    s32 runtime_id;
    s32 camera_facing;

    runtime_id = script_resolve_runtime_id(event_unit_id);
    camera_facing = camera_wrap_yaw_angle();
    camera_facing += 0x200;
    camera_facing &= 0xf00;
    /* Preserve the retail arithmetic shift after the masked camera angle. */
    camera_facing >>= 8;
    camera_facing += battle_unit_get_facing_nibble_by_runtime_id(runtime_id);
    camera_facing &= 0xf;
    camera_facing -= 7;
    return camera_facing < 6U;
}

/*
 * Event instruction 0x69 (FaceTile): turn the addressed unit(s) to face a tile.
 *
 * Parameters: 0x00 unit id (16-bit value), 0x02 tile x, 0x03 tile y, 0x04 unused,
 * 0x05 clockwise flag, 0x06 rotation speed (index into g_unit_rotation_speed_frames), 0x07
 * rotation time. Each matching unit's screen position is compared with the
 * tile center to pick one of 16 facings; the n-th matching unit gets
 * rotation time * n / 4.
 */
void script_face_tile(const event_face_tile_parameters_t* parameters) {
    SVECTOR screen;
    SVECTOR tile;
    u16 unit_id;
    s32 state;
    s32 i;
    unit_animation_state_t* rotation_state;
    s32 order;
    s32 facing;
    s32 dx;
    s32 dy;

    order = 1;
    tile.vx = parameters->x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    tile.vz = parameters->y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit_id = script_read_s16(parameters->unit_id_le);
    if (script_prepare_unit_filter(&unit_id, &state) == 0) {
        return;
    }
    for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
        if (script_filter_unit_id_by_mode(&unit_id, &g_script_unit_face_slot_order[i], &state) != 0) {
            unit_copy_world_coords(unit_id, &screen);
            dx = tile.vx - screen.vx;
            dy = tile.vz - screen.vz;
            facing = ((-ratan2(dy, dx) + 0xC00) & 0xF00) >> 8;
            rotation_state = &g_unit_animation_states[unit_id];
            rotation_state->target_facing = facing;
            rotation_state->rotate_mode = parameters->mode;
            rotation_state->frames_per_step = g_unit_rotation_speed_frames[parameters->speed];
            rotation_state->step_counter = 0;
            rotation_state->rotating = 1;
            rotation_state->delay = (parameters->delay * order++) / 4;
            if (state == 0) {
                return;
            }
        }
    }
}

/* WaitRotateUnit/All: resolved units yield at least once; -1 waits for all 21 slots. */
void script_waitrotateunit_and_waitrotateall_event_instruction(s32 unit_id) {
    s32 i;
    s32 index;

    if (unit_id == -1) {
        do {
            thread_yield();
            for (i = 0; i < EVENT_UNIT_SLOT_COUNT; i++) {
                if (g_unit_animation_states[i].rotating != 0) {
                    break;
                }
            }
        } while (i != EVENT_UNIT_SLOT_COUNT);
    } else {
        index = script_resolve_runtime_id(unit_id);
        if (index != EVENT_RUNTIME_ID_NONE) {
            do {
                thread_yield();
            } while (g_unit_animation_states[index].rotating != 0);
        }
    }
}

/* Thread task 0x35, started by the event script's background-sound instruction:
 * restart an environment sound (sound resource ENV), on a free SFX channel pair
 * when find_channel is set and as the remembered background sound otherwise, and
 * set its volume (0 becomes 1).
 *
 * It then continues in sound_run_background_fade_thread, which reads the same
 * parameter record to fade from volume_from to volume_to over frames yields. */
void sound_run_background_sfx_thread(void) {
    event_background_sound_parameters_t* request;
    s32 sound_id;
    s32 sound;
    s32 volume;
    s32 find_channel;
    s32 resource = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, 0);

    thread_set_current_task_id(THREAD_TASK_BG_SOUND);
    request = (event_background_sound_parameters_t*)thread_get_current_parameter_1_variant();
    find_channel = request->find_channel;
    sound_id = request->sound_id;
    if (find_channel != 0) {
        sound = sound_id + resource;
        main_sound_stop_sfx(sound);
        main_sound_play_sfx_find_channel(sound);
    } else {
        sound = sound_id + resource;
        main_sound_stop_sfx(sound);
        main_sound_play_bg_sfx(sound);
    }
    volume = request->volume_from;
    if (volume == 0) {
        volume = 1;
    }
    main_sound_set_sfx_echo(sound_id | resource, volume);
    sound_run_background_fade_thread();
}

/* Thread task 0x35: ramp the volume of an environment sound from volume_from to
 * volume_to over `steps` yields, then settle on the final level and exit the
 * thread.
 *
 * A step's volume is never 0, which would stop the sound: a negative level is
 * negated and 0 becomes 1. The event script's edit-background-sound instruction
 * starts it as a thread, and sound_run_background_sfx_thread continues in it. */
void sound_run_background_fade_thread(void) {
    event_background_sound_parameters_t* request;
    s32 sound_id;
    s32 volume_from;
    s32 volume_to;
    s32 steps;
    s32 delta;
    s32 scaled;
    s32 volume;
    s32 step;
    s32 resource;
    /* The target reserves eight frame bytes that no other local
     * accounts for; the pad reproduces the 0x40-byte frame. */
    s32 stack_pad[2];

    thread_set_current_task_id(THREAD_TASK_BG_SOUND);
    step = 0;
    request = (event_background_sound_parameters_t*)thread_get_current_parameter_1_variant();
    sound_id = request->sound_id;
    volume_from = request->volume_from;
    steps = request->frames;
    volume_to = request->volume_to;
    resource = SUZUKI_SOUND_ID(MAIN_SOUND_RESOURCE_ENV, 0);
    if (steps != 0) {
        delta = volume_to - volume_from;
        scaled = 0;
        do {
            thread_yield();
            volume = (scaled / steps) + volume_from;
            if (volume < 0) {
                volume = -volume;
            }
            if (volume == 0) {
                volume = 1;
            }
            scaled += delta;
            main_sound_set_sfx_echo(sound_id + resource, volume);
            step++;
        } while (step < steps);
    }
    main_sound_set_sfx_echo(sound_id | resource, volume_to);
    thread_exit_current();
}

/* Event instruction 0x37 (unused by the scenario scripts): toggles bit 0 of
 * portrait_flipped of the menu window buffer whose message_id matches the
 * operand.
 *
 * record_index is never assigned: the target indexes with whatever $s0 holds
 * on entry and tests that one record six times. */
void script_toggle_message_portrait_flip(const u8* parameters) {
    s32 i;
    s32 record_index;
    s16 record_id;
    menu_window_buffer_t* record;

    record_id = script_read_s16(parameters);
    for (i = 0; i < 6; i++) {
        if (g_menu_window_buffers[record_index].message_id == record_id) {
            record = &g_menu_window_buffers[record_index];
            record->portrait_flipped = g_menu_window_buffers[record_index].portrait_flipped + 1;
            g_menu_window_buffers[record_index].portrait_flipped
                = g_menu_window_buffers[record_index].portrait_flipped & 1;
        }
    }
}

/* EVENT_OPCODE_UNKNOWN_74: receives the operand pointer and does nothing. */
void script_opcode_74_event_instruction(const u8* unused_parameters) {
}

/* Returns the slot a thread should start in: a requested id below 16 as it is,
 * otherwise (THREAD_SLOT_FIRST_IDLE) the first idle slot from 1 up. With
 * every slot busy the calling thread exits, and the function has no return value
 * for that path. */
s32 thread_resolve_id(s32 requested_thread_id) {
    s32 i;

    if (requested_thread_id < THREAD_RESET_SLOT_COUNT) {
        return requested_thread_id;
    }
    i = 1;
    do {
        if (thread_is_running_asm_variant(i) == 0) {
            return i;
        }
        i++;
    } while (i < VARIANT_THREAD_TABLE_SLOT_COUNT);
    thread_exit_current();
}

/* As thread_resolve_id, but the search for an idle slot starts above the
 * current thread's slot. */
s32 thread_resolve_id_after_current(s32 thread_id) {
    s32 index;

    if (thread_id < THREAD_RESET_SLOT_COUNT) {
        return thread_id;
    }

    index = g_thread_current_id + 1;
    if (index >= VARIANT_THREAD_TABLE_SLOT_COUNT) {
        thread_exit_current();
    } else {
        do {
            if (thread_is_running_asm_variant(index) == 0) {
                return index;
            }
            index++;
        } while (index < VARIANT_THREAD_TABLE_SLOT_COUNT);

        thread_exit_current();
    }
}

/* Return the id of another running thread whose 32-bit current task value equals
   task_id, or 0 when none matches. Skips slot 0 and the caller's own thread. */
s32 thread_find_running_by_task(s32 task_id) {
    s32 thread_id;

    for (thread_id = 1; thread_id < VARIANT_THREAD_TABLE_SLOT_COUNT; thread_id++) {
        if (thread_id != g_thread_current_id && thread_is_running_asm_variant(thread_id) != 0
            && g_thread_contexts[thread_id].task_id == task_id) {
            return thread_id;
        }
    }
    return 0;
}

void thread_set_current_task_id(s32 task_id) {
    g_thread_contexts[g_thread_current_id].task_id = task_id;
}
