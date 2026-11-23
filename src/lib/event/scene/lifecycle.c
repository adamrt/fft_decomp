/* Initialize and reset scene menu and unit state, then advance native menu and event frames. */

#include "fft/battle/ai.h"
#include "fft/battle/camera.h"
#include "fft/battle/command.h"
#include "fft/battle/deploy.h"
#include "fft/battle/move.h"
#include "fft/game/battle_unit.h"
#include "fft/gfx/fade.h"
#include "fft/gfx/gfx_image.h"
#include "fft/map/map.h"
#include "fft/map/map_light.h"
#include "fft/map/map_unit.h"
#include "fft/menu/help.h"
#include "fft/menu/menu.h"
#include "fft/menu/menu_gfx.h"
#include "fft/menu/menu_input.h"
#include "fft/menu/menu_window.h"
#include "fft/script/script.h"
#include "fft/script/script_variable.h"
#include "fft/sys/memory.h"
#include "fft/sys/system.h"
#include "fft/sys/thread.h"
#include "fft/text/text.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Two bytes per map_surface_type_e; all 64 pairs are 0x19, 0x19. */
extern u8 g_terrain_tile_info_pairs[64][2];

/* Set while a menu thread forces event speed 2; the previous speed is saved. */
extern u16 g_menu_vsync_interval_forced;
extern u16 g_menu_saved_vsync_interval;
extern u8 g_text_glyph_bitmaps[];
static void store_selected_tile_data(void);

/* Reset the menu library for a battle's setup: subsystem pointers, palette and text, the
 * menu subsystems, the script light transform and the panel fade tiles. */
void menu_subsystems_init(void) {
    menu_init_subsystem_pointers();
    menu_init_palette_text(0);
    menu_init_stub();
    menu_subsystems_reset();
    map_light_reset_script_light();
    menu_init_panel_fade_tiles();
}

/* Bind the scene working pointers and clear its menu-session flags. */
void menu_init_subsystem_pointers(void) {
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
    script_seed_random();
    menu_reset_window_buffer_slots();
    gfx_clear_texture_allocation_grid();
    menu_init_buffer_allocator();
    thread_reset_scheduler();
}

/* Put the map light transform of the event scripts back to the map's own light: no rotation, unit
 * scale, and the current light direction and color matrices as the base map_light_thread
 * interpolates from. WORLD has no map light, so its copy is empty. */
void map_light_reset_script_light(void) {
#ifdef FFT_BATTLE
    MATRIX* buffer;

    setVector(&g_script_light_angles, 0, 0, 0);
    setVector(&g_script_light_scale, ONE, ONE, ONE);
    buffer = map_light_dispatch_matrix_command(MAP_LIGHT_MATRIX_COMMAND_GET_LIGHT, buffer, buffer, 0);
    mem_copy_bytes_nonempty(
        &g_script_light_base_direction_matrix, buffer, sizeof(g_script_light_base_direction_matrix));
    mem_copy_bytes_nonempty(&g_script_light_base_color_matrix,
        map_light_dispatch_matrix_command(MAP_LIGHT_MATRIX_COMMAND_GET_COLOR, buffer, buffer, 0),
        sizeof(g_script_light_base_color_matrix));
#endif
}

/* Stop every unit's event rotation and pending animation, and record each present unit's
 * facing as its initial facing (0xff for an empty slot). */
void map_unit_reset_animation_states(void) {
    unit_animation_state_t* state;
    s32 i = 0;

    state = g_unit_animation_states;
    for (; i < BATTLE_UNIT_SLOT_COUNT; i++, state++) {
        state->rotating = 0;
        state->delay = 0;
        if (map_unit_exists(i) != 0) {
            state->initial_facing = map_unit_get_facing_sixteenth(i);
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
 * Nothing calls it in WORLD, where its caller process_pending_requests
 * is empty. */
void map_unit_update_pending_animations(void) {
    s32 i;
    s32 facing;
    s32 current;
    s32 next;
    s32 moving;
    unit_animation_state_t* state = g_unit_animation_states;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++, state++) {
        if (map_unit_exists(i) != 0) {
            if (g_unit_pending_animation_ids[i] != 0) {
                moving = move_map_unit_is_moving(i);
                if (moving == 0 && g_unit_was_moving_latch[i] != 0) {
                    map_unit_set_event_animation(i, g_unit_pending_animation_ids[i]);
                    g_unit_pending_animation_ids[i] = 0;
                }
                g_unit_was_moving_latch[i] = moving;
            }
            current = map_unit_get_facing_sixteenth(i);
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
            map_unit_set_event_facing(i, facing);
        } else {
            g_unit_was_moving_latch[i] = 0;
            g_unit_pending_animation_ids[i] = 0;
        }
    }
}

/* Reset the unit animation states and, when the current-event variable is set,
 * fade the screen color modulation to zero and start the event thread (slot 1)
 * on script_event_thread.
 *
 * While the battle has not started it first stores the camera in the script
 * variables and resets the script light transform. Where WORLD's copy calls two
 * empty functions, BATTLE does that camera work. */
void script_start_current_event_thread(void) {
    map_unit_reset_animation_states();
    g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_NORMAL;
    if (script_variable_get(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
        camera_store_state_to_script_variables();
        map_light_reset_script_light();
        g_camera_position_locked = 0;
        g_camera_speed_curve = 0;
    }
    if (script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT) != 0) {
        g_camera_position_locked = 0;
        g_camera_speed_curve = 0;
        fade_tint_start(2, 0, 0, 0, 2);
        thread_start(THREAD_SLOT_EVENT, script_event_thread);
    }
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * script_start_current_event_thread and script_reset_event_state. Nothing on
 * the disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void script_noop_8(void) {
}

/* Reset unit animation state; while the event slot is idle, run the scene
 * camera/light reset hooks and clear camera interpolation and position lock. */
void script_reset_event_state(void) {
    map_unit_reset_animation_states();
    if (thread_is_running_asm(THREAD_SLOT_EVENT) == 0) {
        camera_store_state_to_script_variables();
        map_light_reset_script_light();
        g_camera_speed_curve = 0;
        g_camera_position_locked = 0;
    }
}

/* Run one event frame, then return whether the deployment thread (borrowed slot
 * 6) is still running. */
s32 deploy_update_frame(u32* ot, u32 buttons) {
    script_update_event_frame(ot, buttons);
    return thread_is_running(THREAD_BORROWED_SLOT_DEPLOYMENT);
}

/* Run one menu and event frame and return the resolved menu selection. It records the cursor
 * tile's info, clears the pending selections, and forces event speed 2 while a command menu other
 * than the mini menu is open, restoring the saved speed afterwards. Without player input it uses
 * the unfiltered script input. */
s32 menu_update_event_frame(u32* otag, s32 controller_input) {
    s32 i;
    s32 result;
    s32 fill;

    g_event_work = (event_work_t*)g_ai_workspace;
    if (g_menu_input_mode != MENU_INPUT_PLAYER) {
        controller_input = g_script_unfiltered_controller_input;
    }
    store_selected_tile_data();
    fill = -2;
    for (i = 3; i >= 0; i--) {
        g_command_pending_selection[i] = fill;
    }
    script_update_event_frame(otag, controller_input);
    menu_update_panel_fade();
    result = command_resolve_selection();
    if ((thread_is_running_asm(THREAD_SLOT_MENU) != 0
            && g_thread_contexts[THREAD_SLOT_MENU].task_id != THREAD_TASK_MINI_MENU)
        || thread_is_running_asm(THREAD_SLOT_MENU_CHILD) != 0
        || thread_is_running_asm(THREAD_SLOT_MENU_GRANDCHILD) != 0) {
        if (g_menu_vsync_interval_forced == 0) {
            g_menu_vsync_interval_forced = 1;
            g_menu_saved_vsync_interval = system_get_vsync_interval();
            script_set_event_speed(2);
        }
    } else if (g_menu_vsync_interval_forced == 1) {
        g_menu_vsync_interval_forced = 0;
        script_set_event_speed(g_menu_saved_vsync_interval);
    }
    return result;
}

/* Fill g_map_selected_tile_data for the tile under the cursor, which the
 * tile information display shows: the surface type, the depth, the displayed
 * height in half-height units (height * 2 + slope half-height + depth * 2) and
 * the surface type's two bytes of g_terrain_tile_info_pairs. */
static void store_selected_tile_data(void) {
    SVECTOR coords;
    s16* surface_type;
    s32 depth;

    map_store_selected_tile(&coords);
    surface_type = &g_map_selected_tile_data.surface_type;
    *surface_type = map_get_tile_value(MAP_TILE_DATA_SURFACE_TYPE, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.display_height
        = map_get_tile_value(MAP_TILE_DATA_HALF_HEIGHT, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.display_height
        += (map_get_tile_value(MAP_TILE_DATA_HEIGHT, coords.vx, coords.vz, coords.vy) & 0xFF) * 2;
    depth = map_get_tile_value(MAP_TILE_DATA_DEPTH, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.depth = depth;
    g_map_selected_tile_data.display_height += depth * 2;
    g_map_selected_tile_data._unknown_02 = g_terrain_tile_info_pairs[*surface_type][0];
    g_map_selected_tile_data._unknown_04 = g_terrain_tile_info_pairs[*surface_type][1];
}
