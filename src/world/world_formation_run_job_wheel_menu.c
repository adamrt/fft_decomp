#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Per-frame handler of the formation screen's job wheel, called by
 * world_formation_process_frame for the job-wheel menu values.
 *
 * The first call binds the job descriptions, stops the two detail panels and
 * installs the wheel's menu-script callbacks. Menu value
 * WORLD_FORMATION_MENU_JOB_WHEEL_ENTER slides the unit grid out and opens
 * the wheel; WORLD_FORMATION_MENU_JOB_WHEEL_LEAVE closes it and brings the
 * grid back. On the wheel itself:
 *  - Left/Right start a spin of 36 progress units (2 a frame with fewer than
 *    four jobs, 3 otherwise) that moves the cursor one job and wraps;
 *  - Circle on an available job starts the job-change flash
 *    (world_formation_process_job_change), after which the job is changed,
 *    with replacement equipment chosen when the "max equip at job change"
 *    option field is 0, and the job list rebuilt; on an unavailable job other
 *    than the unit's own it shows that job's text with the error sound;
 *  - Select shows the job's text, Square toggles the unit banner (L1/R1
 *    cycle units while it is hidden) and Cross leaves.
 * With fewer than two jobs the wheel is not drawn and Circle is refused.
 */
void world_formation_run_job_wheel_menu(void) {
    u8 unused[0x28];
    s32 i;
    u8 banner;
    u32 pad;
    u32 buttons;
    u16 job;
    s16 angle;

    if (g_world_job_wheel_initialized == 0) {
        g_world_job_wheel_spin_progress = 0;
        world_text_bind_section_data(WORLD_TEXT_DATA_JOB_DESCRIPTIONS);
        g_world_job_wheel_saved_browse_enabled = g_world_formation_unit_browse_enabled;
        g_world_formation_unit_browse_enabled = 0;
        world_menu_stop_unit_status_banner_thread(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
        world_menu_stop_unit_status_banner_thread(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
        world_menu_init_scrollable_list_core(0, 0, 0);
        g_world_menu_script_callbacks[0].value = world_formation_get_job_wheel_cursor_job_level;
        g_world_menu_script_callbacks[1].value = world_formation_is_job_wheel_selection_available;
        g_world_job_wheel_job_selected = 0;
        g_world_job_change_confirmed = 0;
        g_world_job_wheel_initialized = 1;
        g_world_job_wheel_unit_index = g_world_formation_selected_unit_index;
    }
    i = g_world_formation_current_menu;
    if (i == 0xE) {
        if (world_formation_step_grid_transition(2) == 0) {
            world_formation_update_job_wheel_state();
            world_formation_build_job_wheel_job_name();
            g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL_OPENING;
            world_formation_build_job_wheel_availability();
        }
        return;
    }
    if (i == 0xF) {
        if (g_world_job_wheel_job_selected != 0) {
            world_formation_step_job_wheel_close_animation();
            world_formation_draw_unit_sprite(
                g_world_formation_selected_unit_index, &g_world_job_wheel_unit_sprite_position, 0, 0x32);
            if (g_world_formation_current_menu == WORLD_FORMATION_MENU_JOB_WHEEL_OPENING) {
                g_world_job_wheel_job_selected = 0;
                g_world_formation_current_menu = i;
            }
        } else if (world_formation_step_grid_return_transition(2) == 0) {
            g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
            g_world_job_wheel_initialized = 0;
        }
        return;
    }
    if (g_world_job_change_anim_state != WORLD_JOB_CHANGE_ANIM_DONE) {
        g_world_job_change_anim_state
            = world_formation_process_job_change(g_world_job_wheel_jobs[g_world_job_wheel_cursor_index]);
        if (g_world_job_change_anim_state == WORLD_JOB_CHANGE_ANIM_DONE) {
            i = g_main_game_options.value & GAME_OPTIONS_MAX_EQUIP_AT_JOB_CHANGE_MASK;
            world_formation_change_unit_job(
                g_world_formation_selected_unit_index, g_world_job_wheel_jobs[g_world_job_wheel_cursor_index], i == 0);
            world_formation_stage_selected_unit();
            world_job_build_unit_list(g_world_formation_selected_unit_index, g_world_job_wheel_jobs,
                WORLD_JOB_LIST_MODE_INCLUDE_OTHER_UNIT_UNLOCKS);
            g_world_formation_unit_banner_enabled = g_world_formation_saved_banner_enabled;
            world_formation_layout_job_wheel_portrait();
            g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL;
        }
        if (g_world_job_change_anim_state == WORLD_JOB_CHANGE_ANIM_DISSOLVE) {
            g_world_job_change_flash_colors[0].r
                = g_world_job_change_flash_colors[0].r < 0xFE ? g_world_job_change_flash_colors[0].r + 1 : 0xFF;
            g_world_job_change_flash_colors[0].g = g_world_job_change_flash_colors[0].r == 0xFF
                ? g_world_job_change_flash_colors[0].g + 1
                : g_world_job_change_flash_colors[0].g - 1;
            g_world_job_change_flash_colors[0].b = g_world_job_change_flash_colors[0].r == 0xFF
                ? g_world_job_change_flash_colors[0].b + 1
                : g_world_job_change_flash_colors[0].b - 1;
            g_world_job_change_flash_colors[1].g
                = g_world_job_change_flash_colors[1].g < 0xFE ? g_world_job_change_flash_colors[1].g + 1 : 0xFF;
            g_world_job_change_flash_colors[1].r = g_world_job_change_flash_colors[1].g == 0xFF
                ? g_world_job_change_flash_colors[1].r + 1
                : g_world_job_change_flash_colors[1].r - 1;
            g_world_job_change_flash_colors[1].b = g_world_job_change_flash_colors[1].g == 0xFF
                ? g_world_job_change_flash_colors[1].b + 1
                : g_world_job_change_flash_colors[1].b - 1;
            g_world_job_change_flash_colors[2].b
                = g_world_job_change_flash_colors[2].b < 0xFE ? g_world_job_change_flash_colors[2].b + 1 : 0xFF;
            g_world_job_change_flash_colors[2].r = g_world_job_change_flash_colors[2].b == 0xFF
                ? g_world_job_change_flash_colors[2].r + 1
                : g_world_job_change_flash_colors[2].r - 1;
            g_world_job_change_flash_colors[2].g = g_world_job_change_flash_colors[2].b == 0xFF
                ? g_world_job_change_flash_colors[2].g + 1
                : g_world_job_change_flash_colors[2].g - 1;
            g_world_job_change_flash_colors[3].r
                = g_world_job_change_flash_colors[3].r < 0xFE ? g_world_job_change_flash_colors[3].r + 1 : 0xFF;
            g_world_job_change_flash_colors[3].g
                = g_world_job_change_flash_colors[3].g < 0xFE ? g_world_job_change_flash_colors[3].g + 1 : 0xFF;
            g_world_job_change_flash_colors[3].b = g_world_job_change_flash_colors[3].r == 0xFF
                ? g_world_job_change_flash_colors[3].b + 1
                : g_world_job_change_flash_colors[3].b - 1;
        } else if (g_world_job_change_anim_state == WORLD_JOB_CHANGE_ANIM_CLOSING) {
            for (i = 0; i < 4; i++) {
                g_world_job_change_flash_colors[i].r = g_world_job_change_flash_colors[i].r < 0x80
                    ? g_world_job_change_flash_colors[i].r + 1
                    : g_world_job_change_flash_colors[i].r - 1;
                g_world_job_change_flash_colors[i].g = g_world_job_change_flash_colors[i].g >= 0x80
                    ? g_world_job_change_flash_colors[i].g - 1
                    : g_world_job_change_flash_colors[i].g + 1;
                g_world_job_change_flash_colors[i].b = g_world_job_change_flash_colors[i].b >= 0x80
                    ? g_world_job_change_flash_colors[i].b - 1
                    : g_world_job_change_flash_colors[i].b + 1;
            }
        }
        gfx_enqueue_gouraud_textured_quad(&g_world_job_wheel_portrait_quad, g_world_job_change_flash_colors, 0, 0x33);
        gfx_enqueue_gouraud_textured_quad(
            &g_world_job_wheel_new_portrait_quad, g_world_job_change_flash_colors, 0, 0x33);
        world_formation_draw_job_wheel_sprites(0, ONE, ONE);
    } else {
        banner = g_world_formation_unit_banner_enabled;
        if (banner != 0) {
            g_world_formation_unit_cycle_mode = 2;
        }
        if (g_world_input_secondary_repeat & PSX_PAD_SQUARE) {
            g_world_menu_sound_effect_id = MAIN_SFX_PAGE_SWITCH;
            if (banner != 0) {
                g_world_formation_unit_banner_enabled = 0;
                g_world_formation_unit_cycle_mode = 0;
            } else {
                g_world_formation_unit_banner_enabled = 1;
                g_world_formation_unit_cycle_mode = 2;
            }
            g_world_formation_saved_banner_enabled = g_world_formation_unit_banner_enabled;
        }
        if (g_world_formation_selected_unit_index != g_world_job_wheel_unit_index) {
            world_formation_update_job_wheel_state();
        }
        if (g_world_formation_unit_banner_enabled == 0) {
            world_formation_cycle_selected_unit(g_world_input_secondary_repeat);
        }
        world_formation_draw_unit_sprite(
            g_world_formation_selected_unit_index, &g_world_job_wheel_unit_sprite_position, 0, 0x32);
        if (g_world_formation_current_menu == WORLD_FORMATION_MENU_JOB_WHEEL_CLOSING) {
            world_formation_step_job_wheel_close_animation();
        } else if (g_world_formation_current_menu == WORLD_FORMATION_MENU_JOB_WHEEL_OPENING) {
            world_formation_step_job_wheel_open_animation();
        } else {
            job = g_world_job_wheel_jobs[g_world_job_wheel_cursor_index];
            if (g_world_job_wheel_spin_progress == 0) {
                pad = g_world_input_primary_repeat;
                if (pad & PSX_PAD_CROSS) {
                    i = g_world_unit_pointers[g_world_formation_selected_unit_index]->identity.roster_slot;
                    world_formation_build_record_list(0, g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_ALL);
                    g_world_formation_selected_unit_index = world_formation_find_index_by_roster_slot(i);
                    g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
                    g_world_job_wheel_job_selected = 1;
                    g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL_LEAVE;
                    g_world_formation_unit_browse_enabled = g_world_job_wheel_saved_browse_enabled;
                } else if (g_world_job_wheel_job_count < 2) {
                    if (pad & PSX_PAD_CIRCLE) {
                        g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
                    }
                } else {
                    buttons = g_world_input_secondary_repeat;
                    if ((buttons & PSX_PAD_RIGHT) || ((pad >>= 16) & PSX_PAD_RIGHT)) {
                        g_world_job_wheel_spin_progress = 1;
                        g_world_job_wheel_spin_direction = 1;
                        g_world_menu_sound_effect_id = MAIN_SFX_CURSOR_MOVE;
                    } else if ((buttons & PSX_PAD_LEFT) || (pad & PSX_PAD_LEFT)) {
                        g_world_job_wheel_spin_progress = 1;
                        g_world_job_wheel_spin_direction = -1;
                        g_world_menu_sound_effect_id = MAIN_SFX_CURSOR_MOVE;
                    }
                    if (g_world_input_secondary_repeat & PSX_PAD_CIRCLE) {
                        if (!(job & WORLD_JOB_LIST_ENTRY_UNAVAILABLE)) {
                            g_world_job_change_confirmed = 1;
                            for (i = 0; i < 4; i++) {
                                g_world_job_change_flash_colors[i].r = g_world_job_change_flash_colors[i].g
                                    = g_world_job_change_flash_colors[i].b = 0x80;
                            }
                            g_world_job_change_anim_state = WORLD_JOB_CHANGE_ANIM_DISSOLVE;
                            g_world_formation_saved_banner_enabled = g_world_formation_unit_banner_enabled;
                            g_world_formation_unit_banner_enabled = 0;
                            g_world_formation_unit_cycle_mode = 0;
                            g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_CHANGE;
                        } else if ((job & WORLD_JOB_LIST_ENTRY_ID_MASK)
                            != g_world_unit_pointers[g_world_formation_selected_unit_index]->identity.job_id) {
                            world_text_show_message_and_play_sound(
                                world_job_get_text_index(job & WORLD_JOB_LIST_ENTRY_ID_MASK) + 0xE800,
                                MAIN_SFX_ERROR_MESSAGE);
                            g_world_status_display_thread_params.style = 1;
                        } else {
                            g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
                        }
                    }
                    if (g_world_input_secondary_repeat & PSX_PAD_SELECT) {
                        if (g_world_job_wheel_job_count >= 2) {
                            i = world_job_get_text_index(job & WORLD_JOB_LIST_ENTRY_ID_MASK);
                            if (i >= 0) {
                                world_text_show_message_and_play_sound(i + 0xE800, MAIN_SFX_WINDOW_OPEN);
                                g_world_status_display_thread_params.style = 1;
                            }
                        }
                    }
                }
            } else {
                pad = g_world_job_wheel_job_count;
                if ((s32)pad < 4) {
                    g_world_job_wheel_spin_progress += 2;
                }
                if ((s32)pad >= 4) {
                    g_world_job_wheel_spin_progress += 3;
                }
                if (g_world_job_wheel_spin_progress >= 0x24) {
                    g_world_job_wheel_spin_progress = 0;
                    g_world_job_wheel_cursor_index += g_world_job_wheel_spin_direction;
                    if (g_world_job_wheel_cursor_index < 0) {
                        g_world_job_wheel_cursor_index = g_world_job_wheel_job_count - 1;
                    } else if (g_world_job_wheel_job_count == g_world_job_wheel_cursor_index) {
                        g_world_job_wheel_cursor_index = 0;
                    }
                    world_formation_build_job_wheel_job_name();
                }
            }
            if (g_world_job_wheel_job_count >= 2) {
                /* The shift preserves the order of the spin-state loads. */
                angle = ((g_world_job_wheel_spin_direction << 12) * g_world_job_wheel_spin_progress
                            / g_world_job_wheel_job_count)
                    / 36 % ONE;
                world_formation_draw_job_wheel_sprites(angle, ONE, ONE);
            }
            if (g_menu_input_lock_mode == 0) {
                g_world_status_display_thread_params.style = 0;
            }
        }
    }
    world_gfx_enqueue_textured_quad(
        &g_world_job_wheel_name_rect, 0x30, 0x30, 0, 0, g_world_menu_window_tpage, g_world_menu_window_clut, 0x33);
    menu_run_script_with_palette_mode(&g_world_job_wheel_script, 0, g_menu_input_lock_mode);
}
