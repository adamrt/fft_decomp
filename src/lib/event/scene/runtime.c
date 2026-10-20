/* Initialize and reset scene menu and unit state, then advance native menu and event frames. */

#include "fft/ai.h"
#include "fft/battle_map.h"
#include "fft/battle_map_light.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/deploy.h"
#include "fft/fade.h"
#include "fft/gfx_image.h"
#include "fft/help.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_window.h"
#include "fft/move.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_event.h"
#include "fft/unit_runtime.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Two bytes per battle_map_surface_type_e; all 64 pairs are 0x19, 0x19. */
extern u8 g_terrain_tile_info_pairs[64][2];

/* Set while a menu thread forces event speed 2; the previous speed is saved. */
extern u16 g_menu_vsync_interval_forced;
extern u16 g_menu_saved_vsync_interval;
extern u8 g_text_glyph_bitmaps[];
static void selected_tile_data_store(void);

/* Reset the menu library for a battle's setup: subsystem pointers, palette and text, the
 * menu subsystems, the script light transform and the panel fade tiles. */
void menu_subsystems_init(void) {
    menu_subsystem_pointers_init();
    menu_palette_text_init(0);
    menu_init_stub();
    menu_subsystems_reset();
    battle_map_script_light_reset();
    menu_init_panel_fade_tiles();
}

/* Bind the scene working pointers and clear its menu-session flags. */
void menu_subsystem_pointers_init(void) {
    g_threads = g_thread_contexts;
    g_text_glyph_bitmap_data = g_text_glyph_bitmaps;
    g_menu_glyph_sheet = &g_menu_glyph_image_data;
    g_script_event_block = &g_script_event_block_buffer;
    g_script_variables = g_script_variable_storage;
    g_menu_thread_menu_data = g_command_idle_action_entries;
    g_help_active_text_id_tables = (u16* const*)-1;
    g_unit_graphics_load_pending = 0;
    g_script_threads_idle = 0;
    g_script_frame_result_override = 0;
}

/* Reset what the menu layer shares: reseed the event script's random generator,
 * free every window buffer pair, the tpage-7 VRAM grid and the menu buffer arena,
 * and reset the thread scheduler. */
void menu_subsystems_reset(void) {
    script_random_seed();
    menu_reset_window_buffer_slots();
    gfx_clear_texture_allocation_grid();
    menu_init_buffer_allocator();
    thread_scheduler_reset();
}

/* Put the map light transform of the event scripts back to the map's own light: no rotation, unit
 * scale, and the current light direction and color matrices as the base battle_map_light_thread
 * interpolates from. WORLD has no map light, so its copy is empty. */
void battle_map_script_light_reset(void) {
#ifdef FFT_BATTLE
    MATRIX* buffer;

    setVector(&g_script_light_angles, 0, 0, 0);
    setVector(&g_script_light_scale, ONE, ONE, ONE);
    buffer = battle_map_light_matrix_command(BATTLE_MAP_LIGHT_MATRIX_COMMAND_GET_LIGHT, buffer, buffer, 0);
    mem_copy_bytes_nonempty(
        &g_script_light_base_direction_matrix, buffer, sizeof(g_script_light_base_direction_matrix));
    mem_copy_bytes_nonempty(&g_script_light_base_color_matrix,
        battle_map_light_matrix_command(BATTLE_MAP_LIGHT_MATRIX_COMMAND_GET_COLOR, buffer, buffer, 0),
        sizeof(g_script_light_base_color_matrix));
#endif
}

/* Stop every unit's event rotation and pending animation, and record each present unit's
 * facing as its initial facing (0xff for an empty slot). */
void unit_animation_states_reset(void) {
    unit_animation_state_t* state;
    s32 i = 0;

    state = g_unit_animation_states;
    for (; i < UNIT_SLOT_COUNT; i++, state++) {
        state->rotating = 0;
        state->delay = 0;
        if (unit_has_runtime_id(i) != 0) {
            state->initial_facing = unit_runtime_facing_sixteenth_get(i);
        } else {
            state->initial_facing = 0xff;
        }
        g_unit_pending_animation_ids[i] = 0;
        g_unit_was_moving_latch[i] = 0;
    }
}

/* Per frame, apply each unit's pending animation on the frame it stops moving
 * and step each rotating unit one facing (of 16) toward its target, after its
 * delay and once every frames_per_step frames. The shortest mode turns
 * whichever way is nearer; the other two modes always increment or decrement.
 * Nothing calls it in WORLD, where its caller pending_requests_process
 * is empty. */
void unit_pending_animations_update(void) {
    s32 i;
    s32 facing;
    s32 current;
    s32 next;
    s32 moving;
    unit_animation_state_t* state = g_unit_animation_states;

    for (i = 0; i < UNIT_SLOT_COUNT; i++, state++) {
        if (unit_has_runtime_id(i) != 0) {
            if (g_unit_pending_animation_ids[i] != 0) {
                moving = move_is_unit_moving_by_runtime_id(i);
                if (moving == 0 && g_unit_was_moving_latch[i] != 0) {
                    unit_event_animation_set(i, g_unit_pending_animation_ids[i]);
                    g_unit_pending_animation_ids[i] = 0;
                }
                g_unit_was_moving_latch[i] = moving;
            }
            current = unit_runtime_facing_sixteenth_get(i);
            if (state->rotating == 0) {
                continue;
            }
            if (current == state->target_facing) {
                state->rotating = 0;
                continue;
            }
            state->step_counter++;
            if (state->delay != 0) {
                state->delay--;
                continue;
            }
            if (state->step_counter < state->frames_per_step) {
                continue;
            }
            state->step_counter = 0;
            if (state->rotate_mode == UNIT_ROTATE_MODE_SHORTEST) {
                moving = state->target_facing - current + 0x10;
                facing = current - state->target_facing + 0x10;
                moving &= 0xF;
                facing &= 0xF;
                if (facing < moving) {
                    next = current + 0xF;
                } else {
                    next = current + 1;
                }
                facing = next & 0xF;
            } else if (state->rotate_mode == UNIT_ROTATE_MODE_INCREMENT) {
                facing = (current + 1) & 0xF;
            } else if (state->rotate_mode == UNIT_ROTATE_MODE_DECREMENT) {
                facing = (current + 0xF) & 0xF;
            }
            unit_event_facing_set(i, facing);
        } else {
            g_unit_was_moving_latch[i] = 0;
            g_unit_pending_animation_ids[i] = 0;
        }
    }
}

/* Reset the unit animation states and, when the current-event variable is set,
 * fade the screen color modulation to zero and start the event thread (slot 1)
 * on script_event_thread_run.
 *
 * While the battle has not started it first stores the camera in the script
 * variables and resets the script light transform. Where WORLD's copy calls two
 * empty functions, BATTLE does that camera work. */
void script_current_event_thread_start(void) {
    unit_animation_states_reset();
    g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_NORMAL;
    if (script_variable_get(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
        camera_state_to_script_variables_store();
        battle_map_script_light_reset();
        g_camera_position_locked = 0;
        g_camera_speed_curve = 0;
    }
    if (script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT) != 0) {
        g_camera_position_locked = 0;
        g_camera_speed_curve = 0;
        fade_tint_fade_start(2, 0, 0, 0, 2);
        thread_start(THREAD_SLOT_EVENT, script_event_thread_run);
    }
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * script_current_event_thread_start and script_event_state_reset. Nothing on
 * the disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void script_noop_8(void) {
}

/* Reset unit animation state; while the event slot is idle, run the scene
 * camera/light reset hooks and clear camera interpolation and position lock. */
void script_event_state_reset(void) {
    unit_animation_states_reset();
    if (thread_is_running_asm(THREAD_SLOT_EVENT) == 0) {
        camera_state_to_script_variables_store();
        battle_map_script_light_reset();
        g_camera_speed_curve = 0;
        g_camera_position_locked = 0;
    }
}

/* Run one event frame, then return whether the deployment thread (borrowed slot
 * 6) is still running. */
s32 deploy_run_frame_and_check(u32* ot, u32 buttons) {
    script_event_frame_run(ot, buttons);
    return thread_is_running(THREAD_BORROWED_SLOT_DEPLOYMENT);
}

/* Run one menu and event frame and return the resolved menu selection. It records the cursor
 * tile's info, clears the pending selections, and forces event speed 2 while a command menu other
 * than the mini menu is open, restoring the saved speed afterwards. Without player input it uses
 * the unfiltered script input. */
s32 menu_event_frame_update(u32* otag, s32 controller_input) {
    s32 i;
    s32 result;
    s32 fill;

    g_event_work = (event_work_t*)g_ai_workspace;
    if (g_menu_input_mode != MENU_INPUT_PLAYER) {
        controller_input = g_script_unfiltered_controller_input;
    }
    selected_tile_data_store();
    fill = -2;
    for (i = 3; i >= 0; i--) {
        g_command_pending_selection[i] = fill;
    }
    script_event_frame_run(otag, controller_input);
    menu_update_panel_fade();
    result = command_resolve_selection();
    if ((thread_is_running_asm(THREAD_SLOT_MENU) != 0
            && g_thread_contexts[THREAD_SLOT_MENU].task_id != THREAD_TASK_MINI_MENU)
        || thread_is_running_asm(THREAD_SLOT_MENU_CHILD) != 0
        || thread_is_running_asm(THREAD_SLOT_MENU_GRANDCHILD) != 0) {
        if (g_menu_vsync_interval_forced == 0) {
            g_menu_vsync_interval_forced = 1;
            g_menu_saved_vsync_interval = system_vsync_interval_get();
            script_event_speed_set(2);
        }
    } else if (g_menu_vsync_interval_forced == 1) {
        g_menu_vsync_interval_forced = 0;
        script_event_speed_set(g_menu_saved_vsync_interval);
    }
    return result;
}

/* Fill g_battle_map_selected_tile_data for the tile under the cursor, which the
 * tile information display shows: the surface type, the depth, the displayed
 * height in half-height units (height * 2 + slope half-height + depth * 2) and
 * the surface type's two bytes of g_terrain_tile_info_pairs. */
static void selected_tile_data_store(void) {
    SVECTOR coords;
    s16* surface_type;
    s32 depth;

    battle_map_selected_tile_store(&coords);
    surface_type = &g_battle_map_selected_tile_data.surface_type;
    *surface_type
        = battle_map_tile_get_value(BATTLE_MAP_TILE_DATA_SURFACE_TYPE, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_battle_map_selected_tile_data.display_height
        = battle_map_tile_get_value(BATTLE_MAP_TILE_DATA_HALF_HEIGHT, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_battle_map_selected_tile_data.display_height
        += (battle_map_tile_get_value(BATTLE_MAP_TILE_DATA_HEIGHT, coords.vx, coords.vz, coords.vy) & 0xFF) * 2;
    depth = battle_map_tile_get_value(BATTLE_MAP_TILE_DATA_DEPTH, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_battle_map_selected_tile_data.depth = depth;
    g_battle_map_selected_tile_data.display_height += depth * 2;
    g_battle_map_selected_tile_data._unknown_02 = g_terrain_tile_info_pairs[*surface_type][0];
    g_battle_map_selected_tile_data._unknown_04 = g_terrain_tile_info_pairs[*surface_type][1];
}
