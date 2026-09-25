#include "fft/event_attack.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* ATTACK deployment controller: runs the formation screen for every squad of
 * the scenario, then writes the deployed units' map coordinates and starts
 * the map title.
 *
 * Several globals are indexed as arrays (cursor column/row pairs, the fade
 * word at g_attack_status_display_offset_y, the selected-unit flag, the unit-state bytes): their
 * constant-index accesses must go through one address register so GCC hoists
 * it, which a scalar declaration does not reproduce. The per-squad unit count
 * walks rows with explicit pointers rather than nested loops: loop.c would
 * otherwise hoist its 0xff compare and grid base inside the squad loop first,
 * and those already-moved registers double the squad loop's cost estimate so
 * the constant 2 below is no longer hoisted as in the target.
 *
 * The first-row `end` store is dead (the row loop recomputes it) but is
 * required: it gives `tiles` a second use, so loop.c does not substitute the
 * grid address back into its only use. `tiles` then stays a movable that
 * joins the squad loop's other grid-address loads, which yields the target's
 * hoisted register order and its `move a0,s7` row-pointer start. */
void attack_deploy_run_screen(void) {
    s32 transition;
    s32 first_pass;
    s32 i;
    s32 row;
    s32 column;
    s32 cursor;
    s32 tile;
    s32 count;
    s32 orientation;
    u8* row_entries;
    u8* entry;
    u8* end;
    u8 flags;
    u32* input;
    attack_deployed_unit_coordinate_t* slots;
    u8* tiles;

    first_pass = 1;
    g_battle_menu_thread_menu_data = g_attack_deploy_menu_entries;
    MoveImage(&g_attack_deploy_vram_copy_rect, g_attack_deploy_vram_copy_x, g_attack_deploy_vram_copy_y);
    LoadImage(&g_attack_deploy_arrow_clut_rect, g_attack_deploy_arrow_clut_data);
    battle_thread_yield();
    g_attack_deploy_current_squad_id = battle_script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID);
    g_attack_deploy_current_squad_data = &g_attack_deploy_squad_data[g_attack_deploy_current_squad_id];
    battle_script_set_event_speed(1);
    g_battle_screen_fade = 0xff;
    battle_thread_yield();
    g_attack_deploy_grid_slide_y = 0x9c;
    g_attack_deploy_deployed_unit_count = 0;
    g_world_shop_purchase_unit_price = 0;
    g_attack_deploy_selected_roster_index = 0;
    g_attack_deploy_menu_state = 0;
    g_attack_deploy_active_cursor = 0;
    g_attack_status_display_offset_y[0] = -0x9c;
    for (i = 0; i < 5; i++) {
        g_attack_panel_status_animation[i] = 0xff;
    }
    battle_copy_bytes(g_attack_deploy_cursor_palette_cycle, g_attack_deploy_cursor_palette_source, 0x40);
    attack_deploy_reset_tables();
    attack_out_prepare_valid_deployment_tiles(g_attack_deploy_render_buffers[0].tiles[0]);
    if (battle_script_get_variable(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD) != 0) {
        for (row = 0; row < 5; row++) {
            for (column = 0; column < 5; column++) {
                if (g_attack_deploy_valid_tiles[row][column] != 0) {
                    g_attack_deploy_roster_id_by_tile[row][column] = 0;
                    goto ramza_placed; /* leave both loops */
                }
            }
        }
    }
ramza_placed:
    battle_thread_start(15, attack_deploy_run_render_thread);
    battle_thread_set_parameters(15, 0, 0, 0);
    attack_deploy_select_unit_for_stats_display();
    battle_thread_start(8, attack_render_unit_status_panel_thread);
    battle_thread_set_parameters(8, (s32)&g_attack_status_display_thread_params, 0, 0);
    transition = 0;
    for (g_attack_deploy_current_squad = 0;
        g_attack_deploy_current_squad < battle_script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT);
        g_attack_deploy_current_squad++) {
        g_attack_deploy_current_squad_id = battle_script_get_variable(g_attack_deploy_current_squad + 0x35);
        g_attack_deploy_current_squad_data = &g_attack_deploy_squad_data[g_attack_deploy_current_squad_id];
        if (battle_script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            g_attack_deploy_current_squad_data->unit_limit = 4;
        }
        g_attack_deploy_deployed_unit_count = 0;
        g_attack_deploy_menu_state = 5;
        g_attack_deploy_active_cursor = 0;
        row = 0;
        tiles = g_attack_deploy_roster_id_by_tile[0];
        end = tiles + 5;
        row_entries = tiles;
        /* Label loops on purpose: with the loop notes of a for or do-while,
           the four cursor stores below share one `li 2`, where the target
           loads 2 into two registers. */
    count_row:
        entry = row_entries;
        end = row_entries + 5;
    count_entry:
        if (*entry++ != 0xff) {
            g_attack_deploy_deployed_unit_count++;
        }
        if ((s32)entry < (s32)end) {
            goto count_entry;
        }
        row++;
        row_entries += 5;
        if (row < 5) {
            goto count_row;
        }
        battle_thread_wait_until_inactive(4);
        g_attack_deploy_pending_roster_index = -1;
        g_attack_deploy_cursor_column[0] = 2;
        g_attack_deploy_cursor_row[0] = 2;
        g_attack_deploy_cursor_column[1] = 2;
        g_attack_deploy_cursor_row[1] = 2;
        for (row = 0; row < 5; row++) {
            for (column = 0; column < 5; column++) {
                if (g_attack_deploy_roster_id_by_tile[row][column] != 0xff) {
                    g_attack_deploy_cursor_column[0] = column;
                    g_attack_deploy_cursor_row[0] = row;
                    g_attack_deploy_cursor_column[1] = column;
                    g_attack_deploy_cursor_row[1] = row;
                    g_attack_deploy_pending_roster_index = g_attack_deploy_roster_id_by_tile[row][column];
                    goto cursor_placed; /* leave both loops */
                }
            }
        }
    cursor_placed:
        if (transition == 0) {
            g_attack_deploy_pending_roster_index = -1;
        }
        attack_deploy_mark_other_squad_units_undeployable();
        if (transition != 0
            && (g_attack_deploy_roster_unit_deployable[g_attack_deploy_selected_roster_index] != 1
                || g_attack_deploy_pending_roster_index != -1)
            && g_attack_deploy_pending_roster_index != g_attack_deploy_selected_roster_index) {
            battle_thread_start(4, attack_deploy_select_next_roster_unit);
        }
        if (transition == 1) {
            battle_thread_set_parameters(14, 0, 0, 1);
            g_attack_deploy_grid_slide_x = 0x100;
            for (i = 0; i < 5; i++) {
                if (g_attack_panel_status_animation[i] < 2) {
                    g_attack_panel_status_animation[i] = 1;
                }
            }
            for (i = 0; i < 0x100; i += 8) {
                g_attack_deploy_grid_slide_x -= 8;
                battle_thread_yield();
            }
        } else if (transition == 2) {
            battle_thread_set_parameters(14, 0, 0, 1);
            g_attack_deploy_grid_slide_x = -0x100;
            for (i = 0; i < 5; i++) {
                if (g_attack_panel_status_animation[i] < 2) {
                    g_attack_panel_status_animation[i] = 1;
                }
            }
            for (i = 0; i < 0x100; i += 8) {
                g_attack_deploy_grid_slide_x += 8;
                battle_thread_yield();
            }
        }
        if (first_pass != 0) {
            g_battle_screen_fade = 0xff;
            while (1) {
                if (g_attack_status_display_offset_y[0] != 0) {
                    g_attack_status_display_offset_y[0] += 4;
                }
                if (g_battle_screen_fade != 0) {
                    g_battle_screen_fade -= 0x10;
                    if (g_battle_screen_fade < 0) {
                        g_battle_screen_fade = 0;
                    }
                }
                if (g_attack_status_display_offset_y[0] == -8) {
                    for (i = 0; i < 5; i++) {
                        g_attack_panel_status_animation[i] = 0xc0;
                    }
                }
                if (g_attack_deploy_grid_slide_y != 0) {
                    g_attack_deploy_grid_slide_y -= 4;
                }
                if (g_battle_screen_fade == 0 && g_attack_status_display_offset_y[0] == 0
                    && g_attack_deploy_grid_slide_y == 0) {
                    break;
                }
                battle_thread_yield();
            }
            first_pass = 0;
        } else {
            g_attack_panel_status_animation[0] = 0;
            g_attack_panel_status_animation[1] = 0;
            g_attack_panel_status_animation[3] = 0;
        }
        battle_thread_start(14, attack_deploy_update_screen_graphics);
        battle_thread_set_parameters(14, 0, 0, 0);
        while (1) {
            battle_thread_yield();
            g_attack_deploy_selected_unit_deployed
                = attack_deploy_is_roster_unit_deployed(g_attack_deploy_selected_roster_index);
            input = battle_script_get_controller_input_pointer(0);
            if (*input & PSX_PAD_SELECT) {
                battle_thread_start(3, attack_deploy_run_select_menu);
                battle_thread_set_parameters(3, 1, 0, 0);
                battle_thread_wait_until_inactive(3);
            }
            if (g_attack_panel_status_animation[2] < 2) {
                if (g_attack_deploy_roster_id_by_tile[g_attack_deploy_cursor_row[g_attack_deploy_active_cursor]]
                                                     [g_attack_deploy_cursor_column[g_attack_deploy_active_cursor]]
                    != 0xff) {
                    g_attack_panel_status_animation[2] = 0;
                } else {
                    g_attack_panel_status_animation[2] = 1;
                }
            }
            if (battle_thread_is_running(13) == 0 && g_attack_panel_status_animation[3] < 2) {
                g_attack_panel_status_animation[3] = 0;
            }
            if (g_attack_panel_status_animation[4] < 2) {
                if (g_attack_deploy_deployed_unit_count == 0) {
                    g_attack_panel_status_animation[4] = 1;
                } else {
                    g_attack_panel_status_animation[4] = 0;
                }
            }
            cursor = g_attack_deploy_active_cursor != 0;
            if (*input & PSX_PAD_LEFT) {
                if (g_attack_deploy_cursor_column[cursor] < 4) {
                    g_attack_deploy_cursor_column[cursor]++;
                    g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                }
            }
            if (*input & PSX_PAD_RIGHT) {
                if (g_attack_deploy_cursor_column[cursor] > 0) {
                    g_attack_deploy_cursor_column[cursor]--;
                    g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                }
            }
            if (*input & PSX_PAD_UP) {
                if (g_attack_deploy_cursor_row[cursor] < 4) {
                    g_attack_deploy_cursor_row[cursor]++;
                    g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                }
            }
            if (*input & PSX_PAD_DOWN) {
                if (g_attack_deploy_cursor_row[cursor] > 0) {
                    g_attack_deploy_cursor_row[cursor]--;
                    g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                }
            }
            column = g_attack_deploy_cursor_column[cursor];
            row = g_attack_deploy_cursor_row[cursor];
            if (g_attack_deploy_active_cursor == 0) {
                tile = g_attack_deploy_roster_id_by_tile[row][column];
                if (tile == 0xff) {
                    if (g_attack_deploy_valid_tiles[row][column] != 0
                        && g_attack_deploy_deployed_unit_count < g_attack_deploy_current_squad_data->unit_limit) {
                        g_attack_deploy_menu_state = 0;
                    } else {
                        g_attack_deploy_menu_state = 1;
                    }
                } else if (g_attack_deploy_selected_roster_index == tile) {
                    g_attack_deploy_menu_state = 2;
                } else {
                    g_attack_deploy_menu_state = 3;
                }
            } else if (g_attack_deploy_active_cursor == 1) {
                tile = g_attack_deploy_roster_id_by_tile[row][column];
                if (tile == 0xff) {
                    if (g_attack_deploy_valid_tiles[row][column] != 0) {
                        g_attack_deploy_menu_state = 0;
                    } else {
                        g_attack_deploy_menu_state = 1;
                    }
                } else if (g_attack_deploy_roster_id_by_tile[g_attack_deploy_cursor_row[0]]
                                                            [g_attack_deploy_cursor_column[0]]
                    == tile) {
                    if (g_attack_deploy_selected_roster_index
                        == g_attack_deploy_roster_id_by_tile[g_attack_deploy_cursor_row[0]]
                                                            [g_attack_deploy_cursor_column[0]]) {
                        g_attack_deploy_menu_state = 2;
                    } else {
                        g_attack_deploy_menu_state = 3;
                    }
                } else {
                    g_attack_deploy_menu_state = 4;
                }
            }
            if (*input & PSX_PAD_START) {
                if (g_attack_deploy_deployed_unit_count == 0) {
                    g_sound_effect_id_to_play = MAIN_SFX_INVALID;
                    g_attack_deploy_menu_entries[2].text_id = 0x900b;
                    battle_thread_start(13, battle_menu_icon_linked_entry_thread);
                    battle_thread_set_parameters(13, (s32)&g_attack_deploy_menu_entries[2], 0, 0);
                    battle_thread_wait_until_inactive(13);
                } else {
                    g_sound_effect_id_to_play = 0x85;
                    g_attack_panel_status_animation[4] = 7;
                    if (battle_script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT) - 1
                            != g_attack_deploy_current_squad
                        && g_attack_deploy_fieldable_unit_count > g_attack_deploy_deployed_unit_count) {
                        g_attack_deploy_menu_state = 5;
                        for (i = 0; i < 5; i++) {
                            if (g_attack_panel_status_animation[i] < 2) {
                                g_attack_panel_status_animation[i] = 1;
                            }
                        }
                        battle_thread_set_parameters(14, 0, 0, 1);
                        for (i = 0; i < 0x100; i += 8) {
                            g_attack_deploy_grid_slide_x += 8;
                            battle_thread_yield();
                        }
                    } else {
                        cursor = g_attack_deploy_selected_unit_state[0];
                        g_attack_deploy_selected_unit_state[0] = 1;
                        battle_thread_start(4, battle_menu_icon_linked_entry_thread);
                        battle_thread_set_parameters(4, (s32)&g_battle_menu_thread_menu_data[3], 0, 0);
                        battle_thread_wait_until_inactive(4);
                        battle_thread_wait_until_inactive(3);
                        g_attack_deploy_selected_unit_state[0] = cursor;
                        if (g_attack_deploy_confirm_result != 0) {
                            continue;
                        }
                        battle_copy_bytes(g_attack_deploy_units_by_squad[g_attack_deploy_current_squad],
                            g_attack_deploy_roster_id_by_tile, 25);
                        break;
                    }
                    battle_copy_bytes(g_attack_deploy_units_by_squad[g_attack_deploy_current_squad],
                        g_attack_deploy_roster_id_by_tile, 25);
                    if (g_attack_deploy_current_squad < 3) {
                        battle_copy_bytes(g_attack_deploy_roster_id_by_tile,
                            g_attack_deploy_units_by_squad[g_attack_deploy_current_squad + 1], 25);
                    }
                    g_attack_deploy_active_cursor = 0;
                    transition = 2;
                    break;
                }
            }
            if ((*input & PSX_PAD_CROSS) && g_attack_deploy_active_cursor == 0 && g_attack_deploy_current_squad != 0) {
                g_sound_effect_id_to_play = MAIN_SFX_CANCEL;
                g_attack_deploy_menu_state = 5;
                for (i = 0; i < 5; i++) {
                    if (g_attack_panel_status_animation[i] < 2) {
                        g_attack_panel_status_animation[i] = 1;
                    }
                }
                battle_thread_set_parameters(14, 0, 0, 1);
                for (i = 0; i < 0x100; i += 8) {
                    g_attack_deploy_grid_slide_x -= 8;
                    battle_thread_yield();
                }
                battle_copy_bytes(g_attack_deploy_units_by_squad[g_attack_deploy_current_squad],
                    g_attack_deploy_roster_id_by_tile, 25);
                g_attack_deploy_current_squad--;
                battle_copy_bytes(g_attack_deploy_roster_id_by_tile,
                    g_attack_deploy_units_by_squad[g_attack_deploy_current_squad], 25);
                transition = 1;
                g_attack_deploy_active_cursor = 0;
                g_attack_deploy_current_squad--;
                break;
            }
            if ((*input & PSX_PAD_CIRCLE) && g_attack_deploy_active_cursor == 0) {
                column = g_attack_deploy_cursor_column[cursor];
                row = g_attack_deploy_cursor_row[cursor];
                if (g_attack_deploy_valid_tiles[row][column] != 0) {
                    if (g_attack_deploy_roster_id_by_tile[row][column] == 0xff) {
                        if (attack_deploy_has_roster_id_on_tiles(g_attack_deploy_selected_roster_index) == 0) {
                            if (g_attack_deploy_deployed_unit_count < g_attack_deploy_current_squad_data->unit_limit) {
                                g_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
                                g_attack_deploy_roster_id_by_tile[row][column] = g_attack_deploy_selected_roster_index;
                                g_attack_deploy_deployed_unit_count++;
                            } else {
                                g_sound_effect_id_to_play = MAIN_SFX_INVALID;
                                g_attack_deploy_menu_entries[2].text_id = 0x900c;
                                battle_thread_start(13, battle_menu_icon_linked_entry_thread);
                                battle_thread_set_parameters(13, (s32)&g_attack_deploy_menu_entries[2], 0, 0);
                                battle_thread_wait_until_inactive(13);
                            }
                        } else {
                            g_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
                            attack_deploy_swap_roster_unit_tile(column, row, g_attack_deploy_selected_roster_index);
                        }
                    } else {
                        g_sound_effect_id_to_play = 0x3c;
                        g_attack_deploy_active_cursor = 1;
                        g_attack_deploy_cursor_column[1] = g_attack_deploy_cursor_column[0];
                        g_attack_deploy_cursor_row[1] = g_attack_deploy_cursor_row[0];
                    }
                } else {
                    g_sound_effect_id_to_play = MAIN_SFX_INVALID;
                }
            } else if (g_attack_deploy_active_cursor == 1) {
                column = g_attack_deploy_cursor_column[cursor];
                row = g_attack_deploy_cursor_row[cursor];
                if (*input & PSX_PAD_CIRCLE) {
                    if (g_attack_deploy_valid_tiles[row][column] != 0) {
                        if (g_attack_deploy_cursor_column[0] == g_attack_deploy_cursor_column[1]
                            && g_attack_deploy_cursor_row[0] == g_attack_deploy_cursor_row[1]) {
                            if (g_attack_deploy_roster_id_by_tile[row][column] == 0
                                && battle_script_get_variable(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD) == 1) {
                                g_sound_effect_id_to_play = MAIN_SFX_INVALID;
                                g_attack_deploy_menu_entries[2].text_id = 0x900d;
                                battle_thread_start(13, battle_menu_icon_linked_entry_thread);
                                battle_thread_set_parameters(13, (s32)&g_attack_deploy_menu_entries[2], 0, 0);
                                battle_thread_wait_until_inactive(13);
                                g_attack_deploy_active_cursor = 0;
                                g_attack_deploy_cursor_column[0] = g_attack_deploy_cursor_column[1];
                                g_attack_deploy_cursor_row[0] = g_attack_deploy_cursor_row[1];
                            } else if (g_attack_deploy_roster_id_by_tile[row][column]
                                != g_attack_deploy_selected_roster_index) {
                                if (attack_deploy_has_roster_id_on_tiles(g_attack_deploy_selected_roster_index) != 0) {
                                    g_attack_deploy_active_cursor = 0;
                                    g_sound_effect_id_to_play = MAIN_SFX_CAMERA_ZOOM_OUT;
                                    attack_deploy_remove_roster_unit_from_tiles(g_attack_deploy_selected_roster_index);
                                    g_attack_deploy_cursor_column[0] = g_attack_deploy_cursor_column[1];
                                    g_attack_deploy_cursor_row[0] = g_attack_deploy_cursor_row[1];
                                    g_attack_deploy_deployed_unit_count--;
                                    g_attack_deploy_roster_id_by_tile[row][column]
                                        = g_attack_deploy_selected_roster_index;
                                } else {
                                    g_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
                                    g_attack_deploy_active_cursor = 0;
                                    g_attack_deploy_cursor_column[0] = g_attack_deploy_cursor_column[1];
                                    g_attack_deploy_cursor_row[0] = g_attack_deploy_cursor_row[1];
                                    g_attack_deploy_roster_id_by_tile[row][column]
                                        = g_attack_deploy_selected_roster_index;
                                }
                            } else {
                                g_sound_effect_id_to_play = MAIN_SFX_CAMERA_ZOOM_OUT;
                                g_attack_deploy_active_cursor = 0;
                                g_attack_deploy_cursor_column[0] = g_attack_deploy_cursor_column[1];
                                g_attack_deploy_cursor_row[0] = g_attack_deploy_cursor_row[1];
                                g_attack_deploy_deployed_unit_count--;
                                g_attack_deploy_roster_id_by_tile[row][column] = 0xff;
                            }
                        } else {
                            attack_deploy_swap_roster_unit_tile(column, row,
                                g_attack_deploy_roster_id_by_tile[g_attack_deploy_cursor_row[0]]
                                                                 [g_attack_deploy_cursor_column[0]]);
                            g_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
                            g_attack_deploy_active_cursor = 0;
                            g_attack_deploy_cursor_column[0] = g_attack_deploy_cursor_column[1];
                            g_attack_deploy_cursor_row[0] = g_attack_deploy_cursor_row[1];
                        }
                    } else {
                        g_sound_effect_id_to_play = MAIN_SFX_INVALID;
                    }
                } else if (*input & PSX_PAD_CROSS) {
                    g_attack_deploy_active_cursor = 0;
                    g_sound_effect_id_to_play = MAIN_SFX_CANCEL;
                }
            }
            attack_deploy_handle_roster_navigation_input();
            if (*input & PSX_PAD_SQUARE) {
                g_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
                battle_script_set_event_speed(2);
                if (g_attack_panel_status_animation[2] < 2) {
                    g_attack_panel_status_animation[2] = 1;
                }
                if (g_attack_panel_status_animation[3] < 2) {
                    g_attack_panel_status_animation[3] = 0x11;
                }
                if (g_attack_panel_status_animation[4] < 2) {
                    g_attack_panel_status_animation[4] = 1;
                }
                battle_thread_start(12, attack_editor_run_numeric_thread);
                battle_thread_set_parameters(12, (s32)g_attack_numeric_editor_thread_params, 0, 0);
                battle_thread_start(9, attack_panel_run_character_status_thread);
                battle_thread_set_parameters(9, (s32)g_attack_character_status_frame_config, 0, 0);
                battle_thread_start(13, attack_thread_wait_forever);
                do {
                    battle_thread_yield();
                    if (*input & PSX_PAD_SELECT) {
                        battle_thread_start(3, attack_deploy_run_select_menu);
                        battle_thread_set_parameters(3, 0, 0, 0);
                        battle_thread_wait_until_inactive(3);
                    }
                    attack_deploy_handle_roster_navigation_input();
                } while (!(*input & PSX_PAD_CROSS));
                g_sound_effect_id_to_play = MAIN_SFX_CANCEL;
                battle_thread_set_parameters(12, 0, 0, 1);
                battle_thread_set_parameters(9, 0, 0, 1);
                battle_thread_wait_until_inactive(12);
                battle_thread_wait_until_inactive(9);
                battle_script_set_event_speed(1);
            }
            if (*input & PSX_PAD_TRIANGLE) {
                if (g_attack_deploy_roster_id_by_tile[g_attack_deploy_cursor_row[g_attack_deploy_active_cursor]]
                                                     [g_attack_deploy_cursor_column[g_attack_deploy_active_cursor]]
                    != 0xff) {
                    g_attack_panel_status_animation[2] = 7;
                    g_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
                    if (battle_thread_is_running(4) == 0) {
                        g_attack_deploy_pending_roster_index = g_attack_deploy_roster_id_by_tile
                            [g_attack_deploy_cursor_row[g_attack_deploy_active_cursor]]
                            [g_attack_deploy_cursor_column[g_attack_deploy_active_cursor]];
                        if (g_attack_deploy_selected_roster_index != g_attack_deploy_pending_roster_index) {
                            battle_thread_start(4, attack_deploy_select_next_roster_unit);
                            battle_thread_wait_until_inactive(4);
                        } else {
                            g_attack_deploy_pending_roster_index = -1;
                        }
                    }
                }
            }
            battle_thread_suspend(13);
        }
        if (g_attack_deploy_deployed_unit_count >= g_attack_deploy_fieldable_unit_count) {
            break;
        }
    }
    battle_thread_wait_frames(0x20);
    g_attack_deploy_menu_state = -1;
    battle_thread_set_parameters(14, 0, 0, 1);
    battle_thread_yield();
    for (i = 0; i < 0x100; i += 2) {
        g_attack_status_display_offset_y[0] = -i;
        g_attack_deploy_grid_slide_y = i;
        g_attack_deploy_slot_offsets[0] = -i;
        g_attack_deploy_slot_offsets[1] = -i;
        g_attack_deploy_slot_offsets[2] = i;
        g_attack_deploy_slot_offsets[3] = i;
        g_attack_deploy_slot_offsets[4] = i;
        battle_thread_yield();
        if (!(i & 3)) {
            g_attack_deploy_zodiac_brightness_word[0]--;
        }
    }
    battle_thread_wait_frames(0x3c);
    g_attack_deployed_unit_map_coordinates[0].roster_id = 0xff;
    g_attack_deployed_unit_map_coordinates[1].roster_id = 0xff;
    g_attack_deployed_unit_map_coordinates[2].roster_id = 0xff;
    g_attack_deployed_unit_map_coordinates[3].roster_id = 0xff;
    g_attack_deployed_unit_map_coordinates[4].roster_id = 0xff;
    g_attack_deployed_unit_map_coordinates[5].roster_id = 0xff;
    g_attack_deployed_unit_map_coordinates[6].roster_id = 0xff;
    g_attack_deployed_unit_map_coordinates[7].roster_id = 0xff;
    g_attack_deployed_unit_map_coordinates[8].roster_id = 0xff;
    g_attack_deployed_unit_map_coordinates[9].roster_id = 0xff;
    g_attack_deployed_unit_map_coordinates[10].roster_id = 0xff;
    g_attack_deployed_unit_map_coordinates[11].roster_id = 0xff;
    slots = g_attack_deployed_unit_map_coordinates;
    count = 0;
    for (i = 0; i < battle_script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT); i++) {
        if (battle_script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            count = 0;
            if (i == 1) {
                slots = &g_attack_deployed_unit_map_coordinates[6];
            }
        }
        g_attack_deploy_current_squad_id = i;
        g_attack_deploy_current_squad_id = battle_script_get_variable(i + 0x35);
        g_attack_deploy_current_squad_data = &g_attack_deploy_squad_data[g_attack_deploy_current_squad_id];
        flags = g_attack_deploy_current_squad_data->orientation_flags;
        orientation = flags & 0xf;
        orientation--;
        orientation += flags >> 4;
        if (orientation < 0) {
            orientation += 4;
        }
        orientation &= 3;
        if (orientation == 0) {
            for (row = 0; row < 5; row++) {
                for (column = 0; column < 5; column++) {
                    if (g_attack_deploy_units_by_squad[i][row * 5 + column] != 0xff) {
                        slots[count].roster_id = g_attack_deploy_units_by_squad[i][row * 5 + column];
                        slots[count].x = g_attack_deploy_current_squad_data->center_x - column + 2;
                        slots[count].y = g_attack_deploy_current_squad_data->center_y + row - 2;
                        slots[count].upper_level = g_attack_deploy_current_squad_data->_unknown_06;
                        slots[count].facing = 0;
                        slots[count].unk_3_2 = 0;
                        count++;
                    }
                }
            }
        } else if (orientation == 1) {
            for (row = 0; row < 5; row++) {
                for (column = 0; column < 5; column++) {
                    if (g_attack_deploy_units_by_squad[i][row * 5 + column] != 0xff) {
                        slots[count].roster_id = g_attack_deploy_units_by_squad[i][row * 5 + column];
                        slots[count].x = g_attack_deploy_current_squad_data->center_x - row + 2;
                        slots[count].y = g_attack_deploy_current_squad_data->center_y - column + 2;
                        slots[count].upper_level = g_attack_deploy_current_squad_data->_unknown_06;
                        slots[count].facing = 3;
                        slots[count].unk_3_2 = 0;
                        count++;
                    }
                }
            }
        } else if (orientation == 2) {
            for (row = 0; row < 5; row++) {
                for (column = 0; column < 5; column++) {
                    if (g_attack_deploy_units_by_squad[i][row * 5 + column] != 0xff) {
                        slots[count].roster_id = g_attack_deploy_units_by_squad[i][row * 5 + column];
                        slots[count].x = g_attack_deploy_current_squad_data->center_x + column - 2;
                        slots[count].y = g_attack_deploy_current_squad_data->center_y - row + 2;
                        slots[count].upper_level = g_attack_deploy_current_squad_data->_unknown_06;
                        slots[count].facing = 2;
                        slots[count].unk_3_2 = 0;
                        count++;
                    }
                }
            }
        } else if (orientation == 3) {
            for (row = 0; row < 5; row++) {
                for (column = 0; column < 5; column++) {
                    if (g_attack_deploy_units_by_squad[i][row * 5 + column] != 0xff) {
                        slots[count].roster_id = g_attack_deploy_units_by_squad[i][row * 5 + column];
                        slots[count].x = g_attack_deploy_current_squad_data->center_x + row - 2;
                        slots[count].y = g_attack_deploy_current_squad_data->center_y + column - 2;
                        slots[count].upper_level = g_attack_deploy_current_squad_data->_unknown_06;
                        slots[count].facing = 1;
                        slots[count].unk_3_2 = 0;
                        count++;
                    }
                }
            }
        }
    }
    battle_map_mark_data_load_complete();
    battle_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYED_UNIT_COUNT, count);
    battle_thread_start(14, attack_map_show_title);
    battle_thread_set_parameters(14, 0, 0, 0);
    battle_thread_wait_until_inactive(14);
    g_battle_music_volume_transition_request = 0x1e0000;
    battle_thread_wait_frames(0x20);
    g_battle_screen_fade = 0;
    do {
        battle_thread_yield();
        g_battle_screen_fade += 4;
    } while (g_battle_screen_fade < 0x100);
    battle_thread_set_parameters(15, 0, 0, 1);
    battle_thread_set_parameters(8, 0, 0, 1);
    battle_thread_set_parameters(7, 0, 0, 1);
    battle_thread_wait_frames(3);
    g_battle_screen_fade = 0;
    battle_script_set_event_speed(1);
    main_sound_unload_scenario_music_and_tunes();
    battle_thread_exit_current();
}
