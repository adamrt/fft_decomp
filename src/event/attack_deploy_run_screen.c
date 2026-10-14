#include "fft/battle_gfx.h"
#include "fft/battle_script.h"
#include "fft/event_attack.h"
#include "fft/memory.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* ATTACK deployment controller: runs the formation screen for every squad of
 * the scenario, then writes the deployed units' map coordinates and starts
 * the map title.
 *
 * Several globals are indexed as arrays (cursor column/row pairs, the 32-bit fade
 * value at g_attack_status_display_offset_y, the selected-unit flag, the unit-state bytes): their
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
    unit_deployment_coordinates_t* slots;
    u8* tiles;

    first_pass = 1;
    g_menu_thread_menu_data = g_attack_deploy_menu_entries;
    MoveImage(&g_attack_deploy_vram_copy_rect, g_attack_deploy_vram_copy_x, g_attack_deploy_vram_copy_y);
    LoadImage(&g_attack_deploy_arrow_clut_rect, g_attack_deploy_arrow_clut_data);
    thread_yield();
    g_attack_deploy_current_squad_id = script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID);
    g_attack_deploy_current_squad_data = &g_attack_deploy_squad_data[g_attack_deploy_current_squad_id];
    script_set_event_speed(1);
    g_gfx_screen_fade = 0xff;
    thread_yield();
    g_attack_deploy_grid_slide_y = 0x9c;
    g_attack_deploy_deployed_unit_count = 0;
    g_world_shop_purchase_unit_price = 0;
    g_attack_deploy_selected_roster_index = 0;
    g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_EMPTY_TILE;
    g_attack_deploy_active_cursor = 0;
    g_attack_status_display_offset_y[0] = -0x9c;
    for (i = 0; i < 5; i++) {
        g_attack_panel_status_animation[i] = 0xff;
    }
    mem_copy_bytes_nonempty(g_attack_deploy_cursor_palette_cycle, g_attack_deploy_cursor_palette_source, 0x40);
    attack_deploy_reset_tables();
    attack_deploy_prepare_valid_tiles(g_attack_deploy_render_buffers[0].tiles[0]);
    if (script_get_variable(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD) != 0) {
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
    thread_start(ATTACK_THREAD_SLOT_DEPLOY_RENDER, attack_deploy_run_render_thread);
    thread_set_parameters(ATTACK_THREAD_SLOT_DEPLOY_RENDER, 0, 0, 0);
    attack_deploy_select_unit_for_stats_display();
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, attack_panel_run_unit_status_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)&g_attack_status_display_thread_params, 0, 0);
    transition = 0;
    for (g_attack_deploy_current_squad = 0;
        g_attack_deploy_current_squad < script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT);
        g_attack_deploy_current_squad++) {
        g_attack_deploy_current_squad_id
            = script_get_variable(g_attack_deploy_current_squad + EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID);
        g_attack_deploy_current_squad_data = &g_attack_deploy_squad_data[g_attack_deploy_current_squad_id];
        if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            g_attack_deploy_current_squad_data->unit_limit = 4;
        }
        g_attack_deploy_deployed_unit_count = 0;
        g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_HIDDEN;
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
        if (*entry++ != ATTACK_DEPLOY_ROSTER_ID_NONE) {
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
        thread_wait_until_inactive(ATTACK_THREAD_SLOT_ROSTER_STEP);
        g_attack_deploy_pending_roster_index = -1;
        g_attack_deploy_cursor_column[0] = 2;
        g_attack_deploy_cursor_row[0] = 2;
        g_attack_deploy_cursor_column[1] = 2;
        g_attack_deploy_cursor_row[1] = 2;
        for (row = 0; row < 5; row++) {
            for (column = 0; column < 5; column++) {
                if (g_attack_deploy_roster_id_by_tile[row][column] != ATTACK_DEPLOY_ROSTER_ID_NONE) {
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
            thread_start(ATTACK_THREAD_SLOT_ROSTER_STEP, attack_deploy_select_next_roster_unit);
        }
        if (transition == 1) {
            thread_set_parameters(ATTACK_THREAD_SLOT_SCREEN_GRAPHICS, 0, 0, 1);
            g_attack_deploy_grid_slide_x = 0x100;
            for (i = 0; i < 5; i++) {
                if (g_attack_panel_status_animation[i] < 2) {
                    g_attack_panel_status_animation[i] = 1;
                }
            }
            for (i = 0; i < 0x100; i += 8) {
                g_attack_deploy_grid_slide_x -= 8;
                thread_yield();
            }
        } else if (transition == 2) {
            thread_set_parameters(ATTACK_THREAD_SLOT_SCREEN_GRAPHICS, 0, 0, 1);
            g_attack_deploy_grid_slide_x = -0x100;
            for (i = 0; i < 5; i++) {
                if (g_attack_panel_status_animation[i] < 2) {
                    g_attack_panel_status_animation[i] = 1;
                }
            }
            for (i = 0; i < 0x100; i += 8) {
                g_attack_deploy_grid_slide_x += 8;
                thread_yield();
            }
        }
        if (first_pass != 0) {
            g_gfx_screen_fade = 0xff;
            while (1) {
                if (g_attack_status_display_offset_y[0] != 0) {
                    g_attack_status_display_offset_y[0] += 4;
                }
                if (g_gfx_screen_fade != 0) {
                    g_gfx_screen_fade -= 0x10;
                    if (g_gfx_screen_fade < 0) {
                        g_gfx_screen_fade = 0;
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
                if (g_gfx_screen_fade == 0 && g_attack_status_display_offset_y[0] == 0
                    && g_attack_deploy_grid_slide_y == 0) {
                    break;
                }
                thread_yield();
            }
            first_pass = 0;
        } else {
            g_attack_panel_status_animation[0] = 0;
            g_attack_panel_status_animation[1] = 0;
            g_attack_panel_status_animation[3] = 0;
        }
        thread_start(ATTACK_THREAD_SLOT_SCREEN_GRAPHICS, attack_deploy_update_screen_graphics);
        thread_set_parameters(ATTACK_THREAD_SLOT_SCREEN_GRAPHICS, 0, 0, 0);
        while (1) {
            thread_yield();
            g_attack_deploy_selected_unit_deployed
                = attack_deploy_is_roster_unit_deployed(g_attack_deploy_selected_roster_index);
            input = script_get_controller_input_pointer(0);
            if (*input & PSX_PAD_SELECT) {
                thread_start(ATTACK_THREAD_SLOT_SELECT_MENU, attack_deploy_run_select_menu);
                thread_set_parameters(ATTACK_THREAD_SLOT_SELECT_MENU, 1, 0, 0);
                thread_wait_until_inactive(ATTACK_THREAD_SLOT_SELECT_MENU);
            }
            if (g_attack_panel_status_animation[2] < 2) {
                if (g_attack_deploy_roster_id_by_tile[g_attack_deploy_cursor_row[g_attack_deploy_active_cursor]]
                                                     [g_attack_deploy_cursor_column[g_attack_deploy_active_cursor]]
                    != ATTACK_DEPLOY_ROSTER_ID_NONE) {
                    g_attack_panel_status_animation[2] = 0;
                } else {
                    g_attack_panel_status_animation[2] = 1;
                }
            }
            if (thread_is_running(ATTACK_PROMPT_THREAD_SLOT_DETAIL_MARKER) == 0
                && g_attack_panel_status_animation[3] < 2) {
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
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                }
            }
            if (*input & PSX_PAD_RIGHT) {
                if (g_attack_deploy_cursor_column[cursor] > 0) {
                    g_attack_deploy_cursor_column[cursor]--;
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                }
            }
            if (*input & PSX_PAD_UP) {
                if (g_attack_deploy_cursor_row[cursor] < 4) {
                    g_attack_deploy_cursor_row[cursor]++;
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                }
            }
            if (*input & PSX_PAD_DOWN) {
                if (g_attack_deploy_cursor_row[cursor] > 0) {
                    g_attack_deploy_cursor_row[cursor]--;
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                }
            }
            column = g_attack_deploy_cursor_column[cursor];
            row = g_attack_deploy_cursor_row[cursor];
            if (g_attack_deploy_active_cursor == 0) {
                tile = g_attack_deploy_roster_id_by_tile[row][column];
                if (tile == ATTACK_DEPLOY_ROSTER_ID_NONE) {
                    if (g_attack_deploy_valid_tiles[row][column] != 0
                        && g_attack_deploy_deployed_unit_count < g_attack_deploy_current_squad_data->unit_limit) {
                        g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_EMPTY_TILE;
                    } else {
                        g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_BLOCKED_TILE;
                    }
                } else if (g_attack_deploy_selected_roster_index == tile) {
                    g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_SELECTED_UNIT;
                } else {
                    g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_OTHER_UNIT;
                }
            } else if (g_attack_deploy_active_cursor == 1) {
                tile = g_attack_deploy_roster_id_by_tile[row][column];
                if (tile == ATTACK_DEPLOY_ROSTER_ID_NONE) {
                    if (g_attack_deploy_valid_tiles[row][column] != 0) {
                        g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_EMPTY_TILE;
                    } else {
                        g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_BLOCKED_TILE;
                    }
                } else if (g_attack_deploy_roster_id_by_tile[g_attack_deploy_cursor_row[0]]
                                                            [g_attack_deploy_cursor_column[0]]
                    == tile) {
                    if (g_attack_deploy_selected_roster_index
                        == g_attack_deploy_roster_id_by_tile[g_attack_deploy_cursor_row[0]]
                                                            [g_attack_deploy_cursor_column[0]]) {
                        g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_SELECTED_UNIT;
                    } else {
                        g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_OTHER_UNIT;
                    }
                } else {
                    g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_SWAP_UNIT;
                }
            }
            if (*input & PSX_PAD_START) {
                if (g_attack_deploy_deployed_unit_count == 0) {
                    g_sound_pending_sfx_id = MAIN_SFX_INVALID;
                    g_attack_deploy_menu_entries[2].text_id = 0x900b;
                    thread_start(ATTACK_THREAD_SLOT_NOTICE, battle_menu_icon_linked_entry_thread);
                    thread_set_parameters(ATTACK_THREAD_SLOT_NOTICE, (s32)&g_attack_deploy_menu_entries[2], 0, 0);
                    thread_wait_until_inactive(ATTACK_THREAD_SLOT_NOTICE);
                } else {
                    g_sound_pending_sfx_id = MAIN_SFX_COMPLETE;
                    g_attack_panel_status_animation[4] = 7;
                    if (script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT) - 1
                            != g_attack_deploy_current_squad
                        && g_attack_deploy_fieldable_unit_count > g_attack_deploy_deployed_unit_count) {
                        g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_HIDDEN;
                        for (i = 0; i < 5; i++) {
                            if (g_attack_panel_status_animation[i] < 2) {
                                g_attack_panel_status_animation[i] = 1;
                            }
                        }
                        thread_set_parameters(ATTACK_THREAD_SLOT_SCREEN_GRAPHICS, 0, 0, 1);
                        for (i = 0; i < 0x100; i += 8) {
                            g_attack_deploy_grid_slide_x += 8;
                            thread_yield();
                        }
                    } else {
                        s32* deployed = &g_attack_deploy_selected_unit_deployed;

                        cursor = *deployed;
                        *deployed = 1;
                        thread_start(ATTACK_PROMPT_THREAD_SLOT_CONFIRM_MENU, battle_menu_icon_linked_entry_thread);
                        thread_set_parameters(
                            ATTACK_PROMPT_THREAD_SLOT_CONFIRM_MENU, (s32)&g_menu_thread_menu_data[3], 0, 0);
                        thread_wait_until_inactive(ATTACK_PROMPT_THREAD_SLOT_CONFIRM_MENU);
                        thread_wait_until_inactive(ATTACK_PROMPT_THREAD_SLOT_CONFIRM_MENU_CHILD);
                        *deployed = cursor;
                        if (g_attack_deploy_confirm_result != 0) {
                            continue;
                        }
                        mem_copy_bytes_nonempty(g_attack_deploy_units_by_squad[g_attack_deploy_current_squad],
                            g_attack_deploy_roster_id_by_tile, 25);
                        break;
                    }
                    mem_copy_bytes_nonempty(g_attack_deploy_units_by_squad[g_attack_deploy_current_squad],
                        g_attack_deploy_roster_id_by_tile, 25);
                    if (g_attack_deploy_current_squad < 3) {
                        mem_copy_bytes_nonempty(g_attack_deploy_roster_id_by_tile,
                            g_attack_deploy_units_by_squad[g_attack_deploy_current_squad + 1], 25);
                    }
                    g_attack_deploy_active_cursor = 0;
                    transition = 2;
                    break;
                }
            }
            if ((*input & PSX_PAD_CROSS) && g_attack_deploy_active_cursor == 0 && g_attack_deploy_current_squad != 0) {
                g_sound_pending_sfx_id = MAIN_SFX_CANCEL;
                g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_HIDDEN;
                for (i = 0; i < 5; i++) {
                    if (g_attack_panel_status_animation[i] < 2) {
                        g_attack_panel_status_animation[i] = 1;
                    }
                }
                thread_set_parameters(ATTACK_THREAD_SLOT_SCREEN_GRAPHICS, 0, 0, 1);
                for (i = 0; i < 0x100; i += 8) {
                    g_attack_deploy_grid_slide_x -= 8;
                    thread_yield();
                }
                mem_copy_bytes_nonempty(g_attack_deploy_units_by_squad[g_attack_deploy_current_squad],
                    g_attack_deploy_roster_id_by_tile, 25);
                g_attack_deploy_current_squad--;
                mem_copy_bytes_nonempty(g_attack_deploy_roster_id_by_tile,
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
                    if (g_attack_deploy_roster_id_by_tile[row][column] == ATTACK_DEPLOY_ROSTER_ID_NONE) {
                        if (attack_deploy_has_roster_id_on_tiles(g_attack_deploy_selected_roster_index) == 0) {
                            if (g_attack_deploy_deployed_unit_count < g_attack_deploy_current_squad_data->unit_limit) {
                                g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
                                g_attack_deploy_roster_id_by_tile[row][column] = g_attack_deploy_selected_roster_index;
                                g_attack_deploy_deployed_unit_count++;
                            } else {
                                g_sound_pending_sfx_id = MAIN_SFX_INVALID;
                                g_attack_deploy_menu_entries[2].text_id = 0x900c;
                                thread_start(ATTACK_THREAD_SLOT_NOTICE, battle_menu_icon_linked_entry_thread);
                                thread_set_parameters(
                                    ATTACK_THREAD_SLOT_NOTICE, (s32)&g_attack_deploy_menu_entries[2], 0, 0);
                                thread_wait_until_inactive(ATTACK_THREAD_SLOT_NOTICE);
                            }
                        } else {
                            g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
                            attack_deploy_swap_roster_unit_tile(column, row, g_attack_deploy_selected_roster_index);
                        }
                    } else {
                        g_sound_pending_sfx_id = MAIN_SFX_SELECT;
                        g_attack_deploy_active_cursor = 1;
                        g_attack_deploy_cursor_column[1] = g_attack_deploy_cursor_column[0];
                        g_attack_deploy_cursor_row[1] = g_attack_deploy_cursor_row[0];
                    }
                } else {
                    g_sound_pending_sfx_id = MAIN_SFX_INVALID;
                }
            } else if (g_attack_deploy_active_cursor == 1) {
                column = g_attack_deploy_cursor_column[cursor];
                row = g_attack_deploy_cursor_row[cursor];
                if (*input & PSX_PAD_CIRCLE) {
                    if (g_attack_deploy_valid_tiles[row][column] != 0) {
                        if (g_attack_deploy_cursor_column[0] == g_attack_deploy_cursor_column[1]
                            && g_attack_deploy_cursor_row[0] == g_attack_deploy_cursor_row[1]) {
                            if (g_attack_deploy_roster_id_by_tile[row][column] == 0
                                && script_get_variable(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD) == 1) {
                                g_sound_pending_sfx_id = MAIN_SFX_INVALID;
                                g_attack_deploy_menu_entries[2].text_id = 0x900d;
                                thread_start(ATTACK_THREAD_SLOT_NOTICE, battle_menu_icon_linked_entry_thread);
                                thread_set_parameters(
                                    ATTACK_THREAD_SLOT_NOTICE, (s32)&g_attack_deploy_menu_entries[2], 0, 0);
                                thread_wait_until_inactive(ATTACK_THREAD_SLOT_NOTICE);
                                g_attack_deploy_active_cursor = 0;
                                g_attack_deploy_cursor_column[0] = g_attack_deploy_cursor_column[1];
                                g_attack_deploy_cursor_row[0] = g_attack_deploy_cursor_row[1];
                            } else if (g_attack_deploy_roster_id_by_tile[row][column]
                                != g_attack_deploy_selected_roster_index) {
                                if (attack_deploy_has_roster_id_on_tiles(g_attack_deploy_selected_roster_index) != 0) {
                                    g_attack_deploy_active_cursor = 0;
                                    g_sound_pending_sfx_id = MAIN_SFX_CAMERA_ZOOM_OUT;
                                    attack_deploy_remove_roster_unit_from_tiles(g_attack_deploy_selected_roster_index);
                                    g_attack_deploy_cursor_column[0] = g_attack_deploy_cursor_column[1];
                                    g_attack_deploy_cursor_row[0] = g_attack_deploy_cursor_row[1];
                                    g_attack_deploy_deployed_unit_count--;
                                    g_attack_deploy_roster_id_by_tile[row][column]
                                        = g_attack_deploy_selected_roster_index;
                                } else {
                                    g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
                                    g_attack_deploy_active_cursor = 0;
                                    g_attack_deploy_cursor_column[0] = g_attack_deploy_cursor_column[1];
                                    g_attack_deploy_cursor_row[0] = g_attack_deploy_cursor_row[1];
                                    g_attack_deploy_roster_id_by_tile[row][column]
                                        = g_attack_deploy_selected_roster_index;
                                }
                            } else {
                                g_sound_pending_sfx_id = MAIN_SFX_CAMERA_ZOOM_OUT;
                                g_attack_deploy_active_cursor = 0;
                                g_attack_deploy_cursor_column[0] = g_attack_deploy_cursor_column[1];
                                g_attack_deploy_cursor_row[0] = g_attack_deploy_cursor_row[1];
                                g_attack_deploy_deployed_unit_count--;
                                g_attack_deploy_roster_id_by_tile[row][column] = ATTACK_DEPLOY_ROSTER_ID_NONE;
                            }
                        } else {
                            attack_deploy_swap_roster_unit_tile(column, row,
                                g_attack_deploy_roster_id_by_tile[g_attack_deploy_cursor_row[0]]
                                                                 [g_attack_deploy_cursor_column[0]]);
                            g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
                            g_attack_deploy_active_cursor = 0;
                            g_attack_deploy_cursor_column[0] = g_attack_deploy_cursor_column[1];
                            g_attack_deploy_cursor_row[0] = g_attack_deploy_cursor_row[1];
                        }
                    } else {
                        g_sound_pending_sfx_id = MAIN_SFX_INVALID;
                    }
                } else if (*input & PSX_PAD_CROSS) {
                    g_attack_deploy_active_cursor = 0;
                    g_sound_pending_sfx_id = MAIN_SFX_CANCEL;
                }
            }
            attack_deploy_handle_roster_navigation_input();
            if (*input & PSX_PAD_SQUARE) {
                g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
                script_set_event_speed(2);
                if (g_attack_panel_status_animation[2] < 2) {
                    g_attack_panel_status_animation[2] = 1;
                }
                if (g_attack_panel_status_animation[3] < 2) {
                    g_attack_panel_status_animation[3] = 0x11;
                }
                if (g_attack_panel_status_animation[4] < 2) {
                    g_attack_panel_status_animation[4] = 1;
                }
                thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, attack_editor_run_numeric_thread);
                thread_set_parameters(
                    THREAD_SCREEN_SLOT_NUMERIC_EDITOR, (s32)g_attack_numeric_editor_thread_params, 0, 0);
                thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, attack_panel_run_character_status_thread);
                thread_set_parameters(
                    THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)g_attack_character_status_frame_config, 0, 0);
                thread_start(ATTACK_PROMPT_THREAD_SLOT_DETAIL_MARKER, attack_thread_wait_forever);
                do {
                    thread_yield();
                    if (*input & PSX_PAD_SELECT) {
                        thread_start(ATTACK_THREAD_SLOT_SELECT_MENU, attack_deploy_run_select_menu);
                        thread_set_parameters(ATTACK_THREAD_SLOT_SELECT_MENU, 0, 0, 0);
                        thread_wait_until_inactive(ATTACK_THREAD_SLOT_SELECT_MENU);
                    }
                    attack_deploy_handle_roster_navigation_input();
                } while (!(*input & PSX_PAD_CROSS));
                g_sound_pending_sfx_id = MAIN_SFX_CANCEL;
                thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, 0, 0, 1);
                thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
                thread_wait_until_inactive(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
                thread_wait_until_inactive(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
                script_set_event_speed(1);
            }
            if (*input & PSX_PAD_TRIANGLE) {
                if (g_attack_deploy_roster_id_by_tile[g_attack_deploy_cursor_row[g_attack_deploy_active_cursor]]
                                                     [g_attack_deploy_cursor_column[g_attack_deploy_active_cursor]]
                    != ATTACK_DEPLOY_ROSTER_ID_NONE) {
                    g_attack_panel_status_animation[2] = 7;
                    g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
                    if (thread_is_running(ATTACK_THREAD_SLOT_ROSTER_STEP) == 0) {
                        g_attack_deploy_pending_roster_index = g_attack_deploy_roster_id_by_tile
                            [g_attack_deploy_cursor_row[g_attack_deploy_active_cursor]]
                            [g_attack_deploy_cursor_column[g_attack_deploy_active_cursor]];
                        if (g_attack_deploy_selected_roster_index != g_attack_deploy_pending_roster_index) {
                            thread_start(ATTACK_THREAD_SLOT_ROSTER_STEP, attack_deploy_select_next_roster_unit);
                            thread_wait_until_inactive(ATTACK_THREAD_SLOT_ROSTER_STEP);
                        } else {
                            g_attack_deploy_pending_roster_index = -1;
                        }
                    }
                }
            }
            thread_suspend(ATTACK_PROMPT_THREAD_SLOT_DETAIL_MARKER);
        }
        if (g_attack_deploy_deployed_unit_count >= g_attack_deploy_fieldable_unit_count) {
            break;
        }
    }
    thread_wait_frames(0x20);
    g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_CLOSED;
    thread_set_parameters(ATTACK_THREAD_SLOT_SCREEN_GRAPHICS, 0, 0, 1);
    thread_yield();
    for (i = 0; i < 0x100; i += 2) {
        g_attack_status_display_offset_y[0] = -i;
        g_attack_deploy_grid_slide_y = i;
        g_attack_deploy_slot_offsets[0] = -i;
        g_attack_deploy_slot_offsets[1] = -i;
        g_attack_deploy_slot_offsets[2] = i;
        g_attack_deploy_slot_offsets[3] = i;
        g_attack_deploy_slot_offsets[4] = i;
        thread_yield();
        if (!(i & 3)) {
            g_attack_deploy_zodiac_brightness_word[0]--;
        }
    }
    thread_wait_frames(0x3c);
    g_attack_deployed_unit_map_coordinates[0].party_id = ATTACK_DEPLOY_ROSTER_ID_NONE;
    g_attack_deployed_unit_map_coordinates[1].party_id = ATTACK_DEPLOY_ROSTER_ID_NONE;
    g_attack_deployed_unit_map_coordinates[2].party_id = ATTACK_DEPLOY_ROSTER_ID_NONE;
    g_attack_deployed_unit_map_coordinates[3].party_id = ATTACK_DEPLOY_ROSTER_ID_NONE;
    g_attack_deployed_unit_map_coordinates[4].party_id = ATTACK_DEPLOY_ROSTER_ID_NONE;
    g_attack_deployed_unit_map_coordinates[5].party_id = ATTACK_DEPLOY_ROSTER_ID_NONE;
    g_attack_deployed_unit_map_coordinates[6].party_id = ATTACK_DEPLOY_ROSTER_ID_NONE;
    g_attack_deployed_unit_map_coordinates[7].party_id = ATTACK_DEPLOY_ROSTER_ID_NONE;
    g_attack_deployed_unit_map_coordinates[8].party_id = ATTACK_DEPLOY_ROSTER_ID_NONE;
    g_attack_deployed_unit_map_coordinates[9].party_id = ATTACK_DEPLOY_ROSTER_ID_NONE;
    g_attack_deployed_unit_map_coordinates[10].party_id = ATTACK_DEPLOY_ROSTER_ID_NONE;
    g_attack_deployed_unit_map_coordinates[11].party_id = ATTACK_DEPLOY_ROSTER_ID_NONE;
    slots = g_attack_deployed_unit_map_coordinates;
    count = 0;
    for (i = 0; i < script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT); i++) {
        if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            count = 0;
            if (i == 1) {
                slots = &g_attack_deployed_unit_map_coordinates[6];
            }
        }
        g_attack_deploy_current_squad_id = i;
        g_attack_deploy_current_squad_id = script_get_variable(i + EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID);
        g_attack_deploy_current_squad_data = &g_attack_deploy_squad_data[g_attack_deploy_current_squad_id];
        flags = g_attack_deploy_current_squad_data->orientation_flags;
        orientation = flags & ATTACK_DEPLOYMENT_ORIENTATION_MASK;
        orientation--;
        orientation += flags >> 4;
        if (orientation < 0) {
            orientation += 4;
        }
        orientation &= 3;
        if (orientation == 0) {
            for (row = 0; row < 5; row++) {
                for (column = 0; column < 5; column++) {
                    if (g_attack_deploy_units_by_squad[i][row * 5 + column] != ATTACK_DEPLOY_ROSTER_ID_NONE) {
                        slots[count].party_id = g_attack_deploy_units_by_squad[i][row * 5 + column];
                        slots[count].x = g_attack_deploy_current_squad_data->center_x - column + 2;
                        slots[count].y = g_attack_deploy_current_squad_data->center_y + row - 2;
                        slots[count].flags.upper_level = g_attack_deploy_current_squad_data->upper_level;
                        slots[count].flags.facing = 0;
                        slots[count].flags.off_field = 0;
                        count++;
                    }
                }
            }
        } else if (orientation == 1) {
            for (row = 0; row < 5; row++) {
                for (column = 0; column < 5; column++) {
                    if (g_attack_deploy_units_by_squad[i][row * 5 + column] != ATTACK_DEPLOY_ROSTER_ID_NONE) {
                        slots[count].party_id = g_attack_deploy_units_by_squad[i][row * 5 + column];
                        slots[count].x = g_attack_deploy_current_squad_data->center_x - row + 2;
                        slots[count].y = g_attack_deploy_current_squad_data->center_y - column + 2;
                        slots[count].flags.upper_level = g_attack_deploy_current_squad_data->upper_level;
                        slots[count].flags.facing = 3;
                        slots[count].flags.off_field = 0;
                        count++;
                    }
                }
            }
        } else if (orientation == 2) {
            for (row = 0; row < 5; row++) {
                for (column = 0; column < 5; column++) {
                    if (g_attack_deploy_units_by_squad[i][row * 5 + column] != ATTACK_DEPLOY_ROSTER_ID_NONE) {
                        slots[count].party_id = g_attack_deploy_units_by_squad[i][row * 5 + column];
                        slots[count].x = g_attack_deploy_current_squad_data->center_x + column - 2;
                        slots[count].y = g_attack_deploy_current_squad_data->center_y - row + 2;
                        slots[count].flags.upper_level = g_attack_deploy_current_squad_data->upper_level;
                        slots[count].flags.facing = 2;
                        slots[count].flags.off_field = 0;
                        count++;
                    }
                }
            }
        } else if (orientation == 3) {
            for (row = 0; row < 5; row++) {
                for (column = 0; column < 5; column++) {
                    if (g_attack_deploy_units_by_squad[i][row * 5 + column] != ATTACK_DEPLOY_ROSTER_ID_NONE) {
                        slots[count].party_id = g_attack_deploy_units_by_squad[i][row * 5 + column];
                        slots[count].x = g_attack_deploy_current_squad_data->center_x + row - 2;
                        slots[count].y = g_attack_deploy_current_squad_data->center_y + column - 2;
                        slots[count].flags.upper_level = g_attack_deploy_current_squad_data->upper_level;
                        slots[count].flags.facing = 1;
                        slots[count].flags.off_field = 0;
                        count++;
                    }
                }
            }
        }
    }
    map_mark_data_load_complete();
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYED_UNIT_COUNT, count);
    thread_start(ATTACK_THREAD_SLOT_SCREEN_GRAPHICS, attack_map_show_title);
    thread_set_parameters(ATTACK_THREAD_SLOT_SCREEN_GRAPHICS, 0, 0, 0);
    thread_wait_until_inactive(ATTACK_THREAD_SLOT_SCREEN_GRAPHICS);
    g_sound_music_volume_transition_request = 0x1e0000;
    thread_wait_frames(0x20);
    g_gfx_screen_fade = 0;
    do {
        thread_yield();
        g_gfx_screen_fade += 4;
    } while (g_gfx_screen_fade < 0x100);
    thread_set_parameters(ATTACK_THREAD_SLOT_DEPLOY_RENDER, 0, 0, 1);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
    thread_set_parameters(THREAD_SCREEN_SLOT_COMPARISON_STATUS, 0, 0, 1);
    thread_wait_frames(3);
    g_gfx_screen_fade = 0;
    script_set_event_speed(1);
    main_sound_unload_scenario_music_and_tunes();
    thread_exit_current();
}
