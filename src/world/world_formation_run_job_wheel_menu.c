#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

void world_formation_run_job_wheel_menu(void) {
    u8 unused[0x28];
    s32 i;
    u8 banner;
    u32 pad;
    u32 buttons;
    u16 job;
    s32 angle;

    if (*(s8*)&g_world_job_wheel_initialized == 0) {
        g_world_job_wheel_spin_progress = 0;
        world_gfx_bind_data_pointer(5);
        g_world_job_wheel_saved_browse_enabled = g_world_formation_unit_browse_enabled;
        g_world_formation_unit_browse_enabled = 0;
        world_menu_stop_unit_status_banner_thread(9);
        world_menu_stop_unit_status_banner_thread(0xC);
        world_menu_init_scrollable_list_core(0, 0, 0);
        g_world_menu_script_callbacks[0] = (s32 (*)(s32))world_formation_get_job_wheel_cursor_job_level;
        g_world_menu_script_callbacks[1] = (s32 (*)(s32))world_formation_is_job_wheel_selection_available;
        g_world_job_wheel_job_selected = 0;
        g_world_job_change_confirmed = 0;
        g_world_job_wheel_initialized = 1;
        g_world_job_wheel_unit_index = g_world_formation_selected_unit_index;
    }
    i = g_world_formation_current_menu;
    if (i == 0xE) {
        /* The target uses v0 without the u8 return's mask. */
        if (((s32 (*)(s32))world_formation_step_grid_transition)(2) == 0) {
            world_formation_update_job_wheel_state();
            world_formation_build_job_wheel_job_name();
            g_world_formation_current_menu = 0x15;
            world_formation_build_job_wheel_availability();
        }
        return;
    }
    if (i == 0xF) {
        if (g_world_job_wheel_job_selected != 0) {
            world_formation_step_job_wheel_close_animation();
            world_formation_draw_unit_sprite(
                g_world_formation_selected_unit_index, &g_world_job_wheel_unit_sprite_position, 0, 0x32);
            if (g_world_formation_current_menu == 0x15) {
                g_world_job_wheel_job_selected = 0;
                g_world_formation_current_menu = i;
            }
        } else if (world_formation_step_grid_return_transition(2) == 0) {
            g_world_formation_current_menu = 0;
            g_world_job_wheel_initialized = 0;
        }
        return;
    }
    if (g_world_job_change_anim_state != 0) {
        g_world_job_change_anim_state
            = world_formation_process_job_change(((s16*)g_world_job_wheel_jobs)[g_world_job_wheel_cursor_index]);
        if (g_world_job_change_anim_state == 0) {
            i = g_main_game_options.value & GAME_OPTIONS_MAX_EQUIP_AT_JOB_CHANGE_MASK;
            world_formation_change_unit_job(
                g_world_formation_selected_unit_index, g_world_job_wheel_jobs[g_world_job_wheel_cursor_index], i == 0);
            world_formation_stage_selected_unit();
            world_job_build_unit_list(g_world_formation_selected_unit_index, (s16*)g_world_job_wheel_jobs, 2);
            g_world_formation_unit_banner_enabled = g_world_formation_saved_banner_enabled;
            world_formation_layout_job_wheel_portrait();
            g_world_formation_current_menu = 3;
        }
        if (g_world_job_change_anim_state == 1) {
            g_world_job_change_flash_colors[0][0]
                = g_world_job_change_flash_colors[0][0] < 0xFE ? g_world_job_change_flash_colors[0][0] + 1 : 0xFF;
            g_world_job_change_flash_colors[0][1] = g_world_job_change_flash_colors[0][0] == 0xFF
                ? g_world_job_change_flash_colors[0][1] + 1
                : g_world_job_change_flash_colors[0][1] - 1;
            g_world_job_change_flash_colors[0][2] = g_world_job_change_flash_colors[0][0] == 0xFF
                ? g_world_job_change_flash_colors[0][2] + 1
                : g_world_job_change_flash_colors[0][2] - 1;
            g_world_job_change_flash_colors[1][1]
                = g_world_job_change_flash_colors[1][1] < 0xFE ? g_world_job_change_flash_colors[1][1] + 1 : 0xFF;
            g_world_job_change_flash_colors[1][0] = g_world_job_change_flash_colors[1][1] == 0xFF
                ? g_world_job_change_flash_colors[1][0] + 1
                : g_world_job_change_flash_colors[1][0] - 1;
            g_world_job_change_flash_colors[1][2] = g_world_job_change_flash_colors[1][1] == 0xFF
                ? g_world_job_change_flash_colors[1][2] + 1
                : g_world_job_change_flash_colors[1][2] - 1;
            g_world_job_change_flash_colors[2][2]
                = g_world_job_change_flash_colors[2][2] < 0xFE ? g_world_job_change_flash_colors[2][2] + 1 : 0xFF;
            g_world_job_change_flash_colors[2][0] = g_world_job_change_flash_colors[2][2] == 0xFF
                ? g_world_job_change_flash_colors[2][0] + 1
                : g_world_job_change_flash_colors[2][0] - 1;
            g_world_job_change_flash_colors[2][1] = g_world_job_change_flash_colors[2][2] == 0xFF
                ? g_world_job_change_flash_colors[2][1] + 1
                : g_world_job_change_flash_colors[2][1] - 1;
            g_world_job_change_flash_colors[3][0]
                = g_world_job_change_flash_colors[3][0] < 0xFE ? g_world_job_change_flash_colors[3][0] + 1 : 0xFF;
            g_world_job_change_flash_colors[3][1]
                = g_world_job_change_flash_colors[3][1] < 0xFE ? g_world_job_change_flash_colors[3][1] + 1 : 0xFF;
            g_world_job_change_flash_colors[3][2] = g_world_job_change_flash_colors[3][0] == 0xFF
                ? g_world_job_change_flash_colors[3][2] + 1
                : g_world_job_change_flash_colors[3][2] - 1;
        } else if (g_world_job_change_anim_state == 2) {
            for (i = 0; i < 4; i++) {
                g_world_job_change_flash_colors[i][0] = g_world_job_change_flash_colors[i][0] < 0x80
                    ? g_world_job_change_flash_colors[i][0] + 1
                    : g_world_job_change_flash_colors[i][0] - 1;
                g_world_job_change_flash_colors[i][1] = g_world_job_change_flash_colors[i][1] >= 0x80
                    ? g_world_job_change_flash_colors[i][1] - 1
                    : g_world_job_change_flash_colors[i][1] + 1;
                g_world_job_change_flash_colors[i][2] = g_world_job_change_flash_colors[i][2] >= 0x80
                    ? g_world_job_change_flash_colors[i][2] - 1
                    : g_world_job_change_flash_colors[i][2] + 1;
            }
        }
        world_gfx_append_poly_gt4_to_otag((world_gfx_textured_rect_source_t*)&g_world_job_wheel_portrait_quad,
            g_world_job_change_flash_colors[0], 0, 0x33);
        world_gfx_append_poly_gt4_to_otag((world_gfx_textured_rect_source_t*)&g_world_job_wheel_new_portrait_quad,
            g_world_job_change_flash_colors[0], 0, 0x33);
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
            world_formation_cycle_selected_unit((s16)g_world_input_secondary_repeat);
        }
        world_formation_draw_unit_sprite(
            g_world_formation_selected_unit_index, &g_world_job_wheel_unit_sprite_position, 0, 0x32);
        if (g_world_formation_current_menu == 0x14) {
            world_formation_step_job_wheel_close_animation();
        } else if (g_world_formation_current_menu == 0x15) {
            world_formation_step_job_wheel_open_animation();
        } else {
            job = g_world_job_wheel_jobs[g_world_job_wheel_cursor_index];
            if (g_world_job_wheel_spin_progress == 0) {
                pad = g_world_input_primary_repeat;
                if (pad & PSX_PAD_CROSS) {
                    i = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->roster_slot;
                    world_formation_build_record_list(0, g_world_formation_unit_pointers, 0);
                    g_world_formation_selected_unit_index = world_formation_find_index_by_roster_slot(i);
                    g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
                    g_world_job_wheel_job_selected = 1;
                    g_world_formation_current_menu = 0xF;
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
                        if (!(job & 0x4000)) {
                            g_world_job_change_confirmed = 1;
                            for (i = 0; i < 4; i++) {
                                g_world_job_change_flash_colors[i][0] = g_world_job_change_flash_colors[i][1]
                                    = g_world_job_change_flash_colors[i][2] = 0x80;
                            }
                            g_world_job_change_anim_state = 1;
                            g_world_formation_saved_banner_enabled = g_world_formation_unit_banner_enabled;
                            g_world_formation_unit_banner_enabled = 0;
                            g_world_formation_unit_cycle_mode = 0;
                            g_world_formation_current_menu = 0x18;
                        } else if ((job & 0x3FF)
                            != g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->job_id) {
                            /* The definition's s16 conversions would change this call's codegen. */
                            world_text_show_message_and_play_sound(
                                ((s32 (*)(s32))world_job_get_text_index)(job & 0x3FF) + 0xE800, 0x30);
                            g_world_status_display_thread_params.style = 1;
                        } else {
                            g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
                        }
                    }
                    if (g_world_input_secondary_repeat & PSX_PAD_SELECT) {
                        if (g_world_job_wheel_job_count >= 2) {
                            /* The definition's s16 conversions would change this call's codegen. */
                            i = ((s32 (*)(s32))world_job_get_text_index)(job & 0x3FF);
                            if (i >= 0) {
                                world_text_show_message_and_play_sound(i + 0xE800, 0x12);
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
                angle = ((g_world_job_wheel_spin_direction << 12) * g_world_job_wheel_spin_progress
                            / g_world_job_wheel_job_count)
                    / 36;
                world_formation_draw_job_wheel_sprites((s16)(angle % ONE), ONE, ONE);
            }
            if (g_world_thread_task_active == 0) {
                g_world_status_display_thread_params.style = 0;
            }
        }
    }
    world_gfx_enqueue_textured_quad(
        &g_world_job_wheel_name_rect, 0x30, 0x30, 0, 0, g_world_menu_window_tpage, g_world_menu_window_clut, 0x33);
    world_menu_run_script_with_palette_mode(g_world_job_wheel_script, 0, g_world_thread_task_active);
}
