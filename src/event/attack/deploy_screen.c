/* The deployment screen, including roster tiles, navigation input, unit selection, cursor primitives, valid tiles, the
 * render thread and portrait quads. */

#include "fft/battle_map.h"
#include "fft/data_item.h"
#include "fft/data_skillset.h"
#include "fft/event_attack.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_gfx.h"
#include "fft/main_item.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_list.h"
#include "fft/menu_panel.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world_shop.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

static void attack_deploy_build_portrait_quads(s32 column, s32 row, s32 index, POLY_FT4* poly);
static void attack_deploy_handle_roster_navigation_input(void);
static void attack_deploy_init_cursor_primitives(void);
static void attack_deploy_prepare_valid_tiles(SPRT* sprites);
static void attack_deploy_run_render_thread(void);
static void attack_deploy_select_next_roster_unit(void);
static void attack_deploy_select_previous_roster_unit(void);
static void attack_deploy_select_unit_for_stats_display(void);
static void attack_deploy_update_cursor_primitives(s32 frame, attack_deploy_render_buffer_t* buffer);
static void attack_deploy_update_screen_graphics(void);
static void attack_gfx_init_frame_ft4_array(void);

/* Take roster unit `roster_id` off the 5x5 deployment grid by clearing the first tile
 * that holds it. */
static void attack_deploy_remove_roster_unit_from_tiles(s32 roster_id) {
    s32 row;
    s32 column;

    for (row = 0; row < 5; row++) {
        for (column = 0; column < 5; column++) {
            if (g_attack_deploy_roster_id_by_tile[row][column] == roster_id) {
                g_attack_deploy_roster_id_by_tile[row][column] = ATTACK_DEPLOY_ROSTER_ID_NONE;
                return;
            }
        }
    }
}

/* Move roster unit `roster_id` to deployment tile (column, row), swapping it
 * with whatever that tile held. Does nothing when the unit is not on the
 * grid. */
static void attack_deploy_swap_roster_unit_tile(s32 column, s32 row, s32 roster_id) {
    s32 scan_row;
    s32 scan_column;
    s32 old_value;
    for (scan_row = 0; scan_row < 5; scan_row++) {
        for (scan_column = 0; scan_column < 5; scan_column++) {
            old_value = g_attack_deploy_roster_id_by_tile[scan_row][scan_column];
            if (old_value == roster_id) {
                g_attack_deploy_roster_id_by_tile[scan_row][scan_column]
                    = g_attack_deploy_roster_id_by_tile[row][column];
                g_attack_deploy_roster_id_by_tile[row][column] = old_value;
                return;
            }
        }
    }
}

/* Empty the deployment tables: the 5x5 grid of roster ids by tile and the
 * four squads' 25-entry unit lists. */
static void attack_deploy_reset_tables(void) {
    s32 row = 0;
    s32 column;

    do {
        column = 4;
        do {
            g_attack_deploy_roster_id_by_tile[row][column] = ATTACK_DEPLOY_ROSTER_ID_NONE;
            column--;
        } while (column >= 0);
        row++;
    } while (row < 5);

    row = 0;
    do {
        column = 24;
        do {
            g_attack_deploy_units_by_squad[row][column] = ATTACK_DEPLOY_ROSTER_ID_NONE;
            column--;
        } while (column >= 0);
        row++;
    } while (row < 4);
}

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
    s32 first_pass = 1;
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
    u8 cursor_unit;

    g_menu_thread_menu_data = g_attack_deploy_menu_entries;
    MoveImage(&g_attack_deploy_vram_copy_rect, g_attack_deploy_vram_copy_x, g_attack_deploy_vram_copy_y);
    LoadImage(&g_attack_deploy_arrow_clut_rect, g_attack_deploy_arrow_clut_data);
    thread_yield();

    g_attack_deploy_current_squad_id = script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID);
    g_attack_deploy_current_squad_data = &g_attack_deploy_squad_data[g_attack_deploy_current_squad_id];
    script_set_event_speed(1);
    g_gfx_screen_fade = 0xff;
    thread_yield();

    /* The grid and the status panel start 0x9c pixels off screen; the first squad slides them in. */
    g_attack_deploy_grid_slide_y = 0x9c;
    g_attack_deploy_deployed_unit_count = 0;
    g_attack_deploy_cleared_word = 0;
    g_attack_deploy_selected_roster_index = 0;
    g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_EMPTY_TILE;
    g_attack_deploy_active_cursor = 0;
    g_attack_status_display_offset_y[0] = -0x9c;
    /* Prompt icon states (see attack_menu_build_status_group_primitives): 0xff hidden,
     * 0 lit, 1 dimmed; 7 shows a press then lights, 0x11 a press then dims, 0xc0 fades in. */
    for (i = 0; i < 5; i++) {
        g_attack_panel_status_animation[i] = 0xff;
    }

    mem_copy_bytes_nonempty(g_attack_deploy_cursor_palette_cycle, g_attack_deploy_cursor_palette_source, 0x40);
    attack_deploy_reset_tables();
    attack_deploy_prepare_valid_tiles(g_attack_deploy_render_buffers[0].tiles[0]);

    /* When Ramza (roster id 0) must deploy, place him on the first valid tile. */
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
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, menu_run_unit_status_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)&g_attack_status_display_thread_params, 0, 0);

    /* transition: 0 for the first squad, 1 after CROSS went back a squad, 2 after START moved on. */
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
        /* Count the units already on this squad's grid. */
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
        /* Both cursors start on the first placed unit, else at the grid center. */
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
        /* Only a squad change steps the roster: to the first placed unit, or on from a
         * selected unit this squad cannot field. */
        if (transition == 0) {
            g_attack_deploy_pending_roster_index = -1;
        }
        attack_deploy_find_deployable_units();
        if (transition != 0
            && (g_attack_deploy_roster_unit_deployable[g_attack_deploy_selected_roster_index] != 1
                || g_attack_deploy_pending_roster_index != -1)
            && g_attack_deploy_pending_roster_index != g_attack_deploy_selected_roster_index) {
            thread_start(ATTACK_THREAD_SLOT_ROSTER_STEP, attack_deploy_select_next_roster_unit);
        }

        /* Slide the new squad's grid in from the side opposite the old one's exit. */
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

        /* The first squad fades the screen in and slides the panel and grid into place. */
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
                /* Start the icon fade-in as the panel slide nears its end. */
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

        /* Placement loop, one pass per frame, until START or CROSS leaves the squad. */
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

            /* Update the idle icons (state below 2) for the cursor tile and the unit count. */
            if (g_attack_panel_status_animation[2] < 2) {
                cursor_unit
                    = g_attack_deploy_roster_id_by_tile[g_attack_deploy_cursor_row[g_attack_deploy_active_cursor]]
                                                       [g_attack_deploy_cursor_column[g_attack_deploy_active_cursor]];
                if (cursor_unit != ATTACK_DEPLOY_ROSTER_ID_NONE) {
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

            /* The grid is isometric: a higher column draws up and to the left, a higher row
             * up and to the right. */
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

            /* Choose the tile menu for the cursor tile; cursor 1 carries the unit picked up
             * with cursor 0. */
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
                    /* Notices reuse menu entry 2, retexted with each deployment message (bound
                     * at text id 0x9000); 0x0b: no unit is placed. */
                    g_attack_deploy_menu_entries[2].text_id = 0x900b;
                    thread_start(ATTACK_THREAD_SLOT_NOTICE, menu_icon_linked_entry_thread);
                    thread_set_parameters(ATTACK_THREAD_SLOT_NOTICE, (s32)&g_attack_deploy_menu_entries[2], 0, 0);
                    thread_wait_until_inactive(ATTACK_THREAD_SLOT_NOTICE);
                } else {
                    g_sound_pending_sfx_id = MAIN_SFX_COMPLETE;
                    g_attack_panel_status_animation[4] = 7;
                    /* With another squad to fill and units left over, slide on to it; otherwise
                     * confirm the whole deployment. */
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

                        /* Show the selected unit as deployed while the confirm menu is open; `cursor`
                         * holds the saved flag meanwhile. */
                        cursor = *deployed;
                        *deployed = 1;
                        thread_start(ATTACK_PROMPT_THREAD_SLOT_CONFIRM_MENU, menu_icon_linked_entry_thread);
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
                    /* Bring in the next squad's saved layout (four squads at most). */
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
                /* Once more, so the squad loop's increment lands on the previous squad. */
                g_attack_deploy_current_squad--;
                break;
            }

            /* CIRCLE with cursor 0 places the selected roster unit on an empty tile or picks
             * up the unit on an occupied one; with cursor 1 it drops the carried unit. */
            if ((*input & PSX_PAD_CIRCLE) && g_attack_deploy_active_cursor == 0) {
                column = g_attack_deploy_cursor_column[cursor];
                row = g_attack_deploy_cursor_row[cursor];
                if (g_attack_deploy_valid_tiles[row][column] != 0) {
                    if (g_attack_deploy_roster_id_by_tile[row][column] == ATTACK_DEPLOY_ROSTER_ID_NONE) {
                        /* A new unit needs room under the squad limit; a placed one just moves here. */
                        if (attack_deploy_has_roster_id_on_tiles(g_attack_deploy_selected_roster_index) == 0) {
                            if (g_attack_deploy_deployed_unit_count < g_attack_deploy_current_squad_data->unit_limit) {
                                g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
                                g_attack_deploy_roster_id_by_tile[row][column] = g_attack_deploy_selected_roster_index;
                                g_attack_deploy_deployed_unit_count++;
                            } else {
                                g_sound_pending_sfx_id = MAIN_SFX_INVALID;
                                /* Deployment message 0x0c: the squad is full. */
                                g_attack_deploy_menu_entries[2].text_id = 0x900c;
                                thread_start(ATTACK_THREAD_SLOT_NOTICE, menu_icon_linked_entry_thread);
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
                        /* Dropped where it was picked up: remove the unit, or put the selected roster
                         * unit in its place; a mandatory Ramza cannot leave the grid. */
                        if (g_attack_deploy_cursor_column[0] == g_attack_deploy_cursor_column[1]
                            && g_attack_deploy_cursor_row[0] == g_attack_deploy_cursor_row[1]) {
                            if (g_attack_deploy_roster_id_by_tile[row][column] == 0
                                && script_get_variable(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD) == 1) {
                                g_sound_pending_sfx_id = MAIN_SFX_INVALID;
                                /* Deployment message 0x0d: Ramza must deploy. */
                                g_attack_deploy_menu_entries[2].text_id = 0x900d;
                                thread_start(ATTACK_THREAD_SLOT_NOTICE, menu_icon_linked_entry_thread);
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
                thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, menu_run_numeric_editor_thread);
                thread_set_parameters(
                    THREAD_SCREEN_SLOT_NUMERIC_EDITOR, (s32)g_attack_numeric_editor_thread_params, 0, 0);
                thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_status_panel_thread);
                thread_set_parameters(
                    THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)g_attack_character_status_frame_config, 0, 0);
                thread_start(ATTACK_PROMPT_THREAD_SLOT_DETAIL_MARKER, thread_wait_forever);
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

            /* TRIANGLE makes the unit under the cursor the selected roster unit. */
            if (*input & PSX_PAD_TRIANGLE) {
                cursor_unit
                    = g_attack_deploy_roster_id_by_tile[g_attack_deploy_cursor_row[g_attack_deploy_active_cursor]]
                                                       [g_attack_deploy_cursor_column[g_attack_deploy_active_cursor]];
                if (cursor_unit != ATTACK_DEPLOY_ROSTER_ID_NONE) {
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
        /* This squad holds every fieldable unit, so no later squad can be filled. */
        if (g_attack_deploy_deployed_unit_count >= g_attack_deploy_fieldable_unit_count) {
            break;
        }
    }

    /* Close the menu and slide the grid, status panel and slot columns off screen. */
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

    /* Convert each squad's grid into map coordinates. Debug battles restart the
     * count per squad and write the second squad from slot 6. */
    slots = g_attack_deployed_unit_map_coordinates;
    count = 0;
    for (i = 0; i < script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT); i++) {
        if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
            count = 0;
            if (i == 1) {
                slots = &g_attack_deployed_unit_map_coordinates[6];
            }
        }
        /* Dead store, kept from the original code. */
        g_attack_deploy_current_squad_id = i;
        g_attack_deploy_current_squad_id = script_get_variable(i + EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID);
        g_attack_deploy_current_squad_data = &g_attack_deploy_squad_data[g_attack_deploy_current_squad_id];
        flags = g_attack_deploy_current_squad_data->orientation_flags;
        /* Grid rotation: the low nibble (stored 1-based) plus the high nibble, modulo 4. */
        orientation = flags & ATTACK_DEPLOYMENT_ORIENTATION_MASK;
        orientation--;
        orientation += flags >> 4;
        if (orientation < 0) {
            orientation += 4;
        }
        orientation &= 3;

        /* Rotate each grid offset into map space and face the units to match. */
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

    battle_deployment_mark_finished();
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYED_UNIT_COUNT, count);
    thread_start(ATTACK_THREAD_SLOT_SCREEN_GRAPHICS, attack_map_show_title);
    thread_set_parameters(ATTACK_THREAD_SLOT_SCREEN_GRAPHICS, 0, 0, 0);
    thread_wait_until_inactive(ATTACK_THREAD_SLOT_SCREEN_GRAPHICS);

    /* Fade the music to volume 0 (low half) over fade time 0x1e (high half). */
    g_battle_sound_music_volume_transition_request = 0x1e0000;
    thread_wait_frames(0x20);
    g_gfx_screen_fade = 0;
    do {
        thread_yield();
        g_gfx_screen_fade += 4;
    } while (g_gfx_screen_fade < 0x100);

    /* A third parameter of 1 asks each display thread to finish. */
    thread_set_parameters(ATTACK_THREAD_SLOT_DEPLOY_RENDER, 0, 0, 1);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
    thread_set_parameters(THREAD_SCREEN_SLOT_COMPARISON_STATUS, 0, 0, 1);
    thread_wait_frames(3);
    g_gfx_screen_fade = 0;
    script_set_event_speed(1);
    main_sound_unload_scenario_music_and_tunes();
    thread_exit_current();
}

/* Return the deployed-unit coordinate slots to fill: the first six on the
 * first call, the slots from index 6 on every later call. */
unit_deployment_coordinates_t* attack_deploy_get_coordinate_slots(void) {
    unit_deployment_coordinates_t* coordinate_slots;

    if (g_attack_deploy_second_pass != 0) {
        coordinate_slots = &g_attack_deployed_unit_map_coordinates[6];
    } else {
        g_attack_deploy_second_pass = 1;
        coordinate_slots = g_attack_deployed_unit_map_coordinates;
    }

    return coordinate_slots;
}

/* Return 0. Nothing calls it. */
s32 attack_return_zero(void) {
    return 0;
}

/* L1 and R1 on the deployment screen: start the previous- or next-unit
 * thread in the roster step slot, unless one is still running. */
static void attack_deploy_handle_roster_navigation_input(void) {
    u32* flags = script_get_controller_input_pointer(0);

    if ((*flags & PSX_PAD_L1) && (thread_is_running(ATTACK_THREAD_SLOT_ROSTER_STEP) == 0)) {
        g_attack_panel_status_animation[0] = 7;
        g_sound_pending_sfx_id = MAIN_SFX_PAGE_SWITCH;
        thread_start(ATTACK_THREAD_SLOT_ROSTER_STEP, attack_deploy_select_previous_roster_unit);
    }
    if ((*flags & PSX_PAD_R1) && (thread_is_running(ATTACK_THREAD_SLOT_ROSTER_STEP) == 0)) {
        g_attack_panel_status_animation[1] = 7;
        g_sound_pending_sfx_id = MAIN_SFX_PAGE_SWITCH;
        thread_start(ATTACK_THREAD_SLOT_ROSTER_STEP, attack_deploy_select_next_roster_unit);
    }
}

/* Move the roster cursor forward (wrapping at 20) to the first unit the
 * current squad may still deploy and load it into the status panels. The
 * search has no exit when no slot is deployable, so the screen would hang. */
static void attack_deploy_select_unit_for_stats_display(void) {
    attack_deploy_find_deployable_units();
    while (1) {
        if (g_attack_deploy_selected_roster_index >= PARTY_ROSTER_SLOT_COUNT) {
            g_attack_deploy_selected_roster_index = 0;
        }
        if (g_attack_deploy_roster_unit_deployable[g_attack_deploy_selected_roster_index] != 0) {
            break;
        }
        g_attack_deploy_selected_roster_index++;
    }
    g_attack_status_display_thread_params = 0;
    attack_editor_load_party_unit(1, g_attack_deploy_selected_roster_index);
}

/* ATTACK twin of require_editor_load_party_unit. Load a party roster unit into the ATTACK status/editor display.
 *
 * Party counterpart of menu_load_selected_unit: the unit is
 * rebuilt from the roster into a scratch unit_stats_t, shown at full HP/MP
 * and CT, and without the monster-skill or name-text refresh.
 *
 * team_kind (0 player-controlled, 1 another team, 2 the player's team without player
 * control) is stored through `gauges`: written as g_panel_selected_billboard.team_kind, GCC keeps the
 * field's address in a register across the three stores (4 bytes longer than the
 * target). */
void attack_editor_load_party_unit(s32 unused, s32 party_index) {
    unit_stats_t* unit;
    menu_status_panel_slot_state_t* editor;
    u16* innate_items;
    item_data_t* item;
    s32 i;
    s32 item_id;
    u8 item_flags;
    u8 field_value;
    u16 maximum_hp;
    u16 maximum_mp;
    menu_unit_status_record_t* gauges;

    main_unit_init_from_party_slot(&g_editor_party_unit_stats, party_index, 0);
    editor = &g_menu_selected_unit_fields.state;
    unit = &g_editor_party_unit_stats;
    gauges = &g_panel_selected_billboard;

    g_attack_panel_selected_unit_data.name_id = party_index;
    g_attack_panel_selected_unit_data.battle_id = party_index;
    gauges->team_kind = 0;
    g_attack_panel_selected_unit_data.job_id = unit->job_id;
    g_attack_panel_selected_unit_data.brave = unit->brave;
    g_attack_panel_selected_unit_data.faith = unit->faith;
    g_attack_panel_selected_unit_data.zodiac = unit->birthday.zodiac;
    if (unit->initial_team_flags & BATTLE_TEAM_MASK) {
        gauges->team_kind = 1;
    }
    if (!(unit->initial_team_flags & BATTLE_TEAM_OR_PLAYER_CONTROL_MASK)) {
        gauges->team_kind = 2;
    }

    g_panel_selected_billboard.list_index = 0;
    g_panel_selected_billboard.unit_count = 0;
    /* Offset keeps party slots apart from other unit indices; the status thread
     * starts a portrait transition whenever this index changes. */
    g_panel_selected_billboard.unit_index = party_index + 0x200;
    g_panel_selected_billboard.level = unit->level;
    g_panel_selected_billboard.experience = unit->experience;

    maximum_hp = unit->max_hp;
    g_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_HP].max = maximum_hp;
    if (maximum_hp == 0) {
        g_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_HP].max = maximum_hp + 1;
    }
    g_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_HP].value
        = g_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_HP].max;
    maximum_mp = unit->max_mp;
    g_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_MP].max = maximum_mp;
    if (maximum_mp == 0) {
        g_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_MP].max = maximum_mp + 1;
    }
    g_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_MP].value
        = g_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_MP].max;
    g_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_CT].max = UNIT_TURN_CT_THRESHOLD;
    g_panel_selected_billboard.gauges[MENU_UNIT_STATUS_GAUGE_CT].value = UNIT_TURN_CT_THRESHOLD;

    editor->move = unit->move;
    editor->speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    editor->jump = unit->jump;
    editor->right_weapon_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    editor->left_weapon_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    editor->right_weapon_power = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    editor->left_weapon_power = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];

    editor->physical_attack = unit->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    editor->physical_class_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    /* Each shield evade shows the larger of the two hands' values. */
    editor->physical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    if (unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE]
        <= unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE])
        editor->physical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    editor->physical_accessory_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE];

    editor->magical_attack = unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    /* Units carry no class magic evade stat. */
    editor->magical_class_evade = 0;
    editor->magical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    if (unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE]
        <= unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE])
        editor->magical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    editor->magical_accessory_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];

    for (i = 4; i >= 0; i--)
        editor->equipment[i] = 0;

    /* Monsters show no equipment; each hand shows its weapon, else its shield. */
    if (!(unit->unit_flags & UNIT_FLAG_MONSTER)) {
        editor->equipment[0] = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE
            ? unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]
            : unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
        editor->equipment[1] = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE
            ? unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]
            : unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
        editor->equipment[2] = unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
        editor->equipment[3] = unit->equipment[UNIT_EQUIPMENT_SLOT_BODY];
        editor->equipment[4] = unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
    }

    for (i = 0; i < 5; i++) {
        if (editor->equipment[i] == 0 || editor->equipment[i] == 0xff)
            editor->equipment[i] = -1;
    }

    editor->generic_monster = 0;
    if (unit->primary_skillset >= SKILLSET_ID_MONSTER_FIRST && unit->primary_skillset < SKILLSET_ID_END)
        editor->generic_monster = 1;

    if (editor->generic_monster != 0) {
        innate_items = main_ability_store_skillset_abilities(unit->primary_skillset, SKILLSET_ABILITY_FILTER_ACTION);
        i = 0;
        do {
            editor->ability_ids[i] = *innate_items++;
            i++;
        } while (i < 5);
        /* No monster skill check runs here (unlike menu_load_selected_unit),
         * so the fourth monster ability is always cleared. */
        editor->ability_ids[3] = 0;
    } else {
        field_value = unit->primary_skillset;
        editor->ability_ids[0] = field_value;
        /* A missing skillset blanks its name with a terminator; the secondary name
         * takes it at byte 1, as in every copy of this loader. */
        if (field_value == 0)
            g_editor_primary_skillset_name[0] = 0xfe;
        field_value = unit->secondary_skillset;
        editor->ability_ids[1] = field_value;
        if (field_value == 0)
            g_editor_secondary_skillset_name[1] = 0xfe;
        editor->ability_ids[2] = unit->abilities.reaction;
        editor->ability_ids[3] = unit->abilities.support;
        editor->ability_ids[4] = unit->abilities.movement;
    }

    /* Two Hands applies only when exactly one hand holds an item. */
    item_id = 0;
    editor->two_hands = 0;
    if (editor->equipment[0] != -1) {
        if (editor->equipment[1] == -1)
            item_id = editor->equipment[0];
    } else if (editor->equipment[1] != -1) {
        item_id = editor->equipment[1];
    }

    if (item_id != 0) {
        item = main_item_get_data_pointer(item_id);
        if (item->type_flags & ITEM_TYPE_FLAG_WEAPON) {
            item_flags = g_main_item_weapon_data[item_id].flags;
            if ((item_flags & WEAPON_FLAG_FORCED_TWO_HANDS)
                || ((unit->ability_flags.support[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS)
                    && (item_flags & WEAPON_FLAG_TWO_HANDS_COMPATIBLE)))
                editor->two_hands = 1;
        }
    }
}

/* Thread body: step the deployment roster cursor to the next deployable
 * unit, wrapping over the 20 slots (or jump to the pending index when one is
 * set), load that unit into the panels and request their redraw with slide
 * direction 1. */
static void attack_deploy_select_next_roster_unit(void) {
    s32 pending;

    attack_deploy_find_deployable_units();
    pending = g_attack_deploy_pending_roster_index;
    if (pending != -1) {
        g_attack_deploy_selected_roster_index = pending;
        g_attack_deploy_pending_roster_index = -1;
    } else {
        do {
            g_attack_deploy_selected_roster_index++;
            if (g_attack_deploy_selected_roster_index >= PARTY_ROSTER_SLOT_COUNT) {
                g_attack_deploy_selected_roster_index = 0;
            }
        } while (g_attack_deploy_roster_unit_deployable[g_attack_deploy_selected_roster_index] == 0);
    }
    attack_editor_load_party_unit(1, g_attack_deploy_selected_roster_index);
    g_attack_deploy_roster_navigation_direction = 1;
    g_attack_status_display_thread_params = 0;
    g_numeric_editor_redraw_request = 1;
    g_character_status_redraw_request = 1;
    g_attack_deploy_selected_unit_deployed
        = attack_deploy_is_roster_unit_deployed(g_attack_deploy_selected_roster_index);
    thread_yield();
    thread_exit_current();
}

/* Empty and unreferenced. */
void attack_noop_801c61dc(void) {
}

/* Thread body: step the deployment roster cursor to the previous deployable
 * unit, wrapping over the 20 slots (or jump to the pending index when one is
 * set), load that unit into the panels and request their redraw with slide
 * direction 2. */
static void attack_deploy_select_previous_roster_unit(void) {
    s32 pending;

    attack_deploy_find_deployable_units();
    pending = g_attack_deploy_pending_roster_index;
    if (pending != -1) {
        g_attack_deploy_selected_roster_index = pending;
        g_attack_deploy_pending_roster_index = -1;
    } else {
        do {
            g_attack_deploy_selected_roster_index--;
            if (g_attack_deploy_selected_roster_index < 0) {
                g_attack_deploy_selected_roster_index = 19;
            }
        } while (g_attack_deploy_roster_unit_deployable[g_attack_deploy_selected_roster_index] == 0);
    }
    attack_editor_load_party_unit(1, g_attack_deploy_selected_roster_index);
    g_attack_deploy_roster_navigation_direction = 2;
    g_attack_status_display_thread_params = 0;
    g_numeric_editor_redraw_request = 1;
    g_character_status_redraw_request = 1;
    g_attack_deploy_selected_unit_deployed
        = attack_deploy_is_roster_unit_deployed(g_attack_deploy_selected_roster_index);
    thread_yield();
    thread_exit_current();
}

/* Build the textured deployment banner and its two arrowhead triangles.
 * The banner fields are written in libgpu setRGB0/setXY4/setUV4 order. */
static void attack_deploy_build_screen_arrow(void) {
    SetPolyFT4(&g_attack_deploy_screen_arrow.poly);
    SetSemiTrans(&g_attack_deploy_screen_arrow.poly, 1);
    SetShadeTex(&g_attack_deploy_screen_arrow.poly, 0);
    setRGB0(&g_attack_deploy_screen_arrow.poly, 0x80, 0x80, 0x80);
    setXY4(&g_attack_deploy_screen_arrow.poly, 0xAF, 0xB6, 0xEF, 0xB6, 0xAF, 0xDE, 0xEF, 0xDE);
    setUV4(&g_attack_deploy_screen_arrow.poly, 0x58, 0xB8, 0x98, 0xB8, 0x58, 0xE0, 0x98, 0xE0);
    g_attack_deploy_screen_arrow.poly.tpage = GetTPage(0, 1, 0x100, 0);
    g_attack_deploy_screen_arrow.poly.clut
        = GetClut(g_attack_deploy_arrow_clut_rect.x, g_attack_deploy_arrow_clut_rect.y);

    SetPolyF3(&g_attack_deploy_screen_arrow.tri[0]);
    SetSemiTrans(&g_attack_deploy_screen_arrow.tri[0], 0);
    SetShadeTex(&g_attack_deploy_screen_arrow.tri[0], 1);
    setRGB0(&g_attack_deploy_screen_arrow.tri[0], 0xFF, 0xFF, 0xFF);
    if (g_attack_deploy_tiles_only_mode != 0) {
        g_attack_deploy_screen_arrow.tri[0].x0 = 0x99;
        g_attack_deploy_screen_arrow.tri[0].x1 = 0xC0;
        g_attack_deploy_screen_arrow.tri[0].x2 = 0xA2;
        g_attack_deploy_screen_arrow.tri[0].y0 = 0x9E;
        g_attack_deploy_screen_arrow.tri[0].y1 = 0xB1;
        g_attack_deploy_screen_arrow.tri[0].y2 = 0xB0;
    } else {
        g_attack_deploy_screen_arrow.tri[0].x0 = 0xB9;
        g_attack_deploy_screen_arrow.tri[0].x1 = 0xE0;
        g_attack_deploy_screen_arrow.tri[0].x2 = 0xC2;
        g_attack_deploy_screen_arrow.tri[0].y0 = 0xBE;
        g_attack_deploy_screen_arrow.tri[0].y1 = 0xD1;
        g_attack_deploy_screen_arrow.tri[0].y2 = 0xD0;
    }

    SetPolyF3(&g_attack_deploy_screen_arrow.tri[1]);
    SetSemiTrans(&g_attack_deploy_screen_arrow.tri[1], 0);
    SetShadeTex(&g_attack_deploy_screen_arrow.tri[1], 1);
}

/* Build and submit the button hint for the cursor tile's menu state `mode`: hint 0 on an
 * empty tile (dimmed on a blocked one), hint 1 on the selected unit, hints 0 and 1 on
 * another unit and hints 2 and 3 for a swap, the pair dimming in turn every 32 frames. */
static void attack_deploy_build_menu_cursor_primitives(
    s32 frame, u32 mode, attack_deploy_render_buffer_t* render_buffer) {
    s32 i;
    s32 palette;
    POLY_FT4* ptr;

    gfx_set_draw_mode_for_texture_page(&render_buffer->menu_draw_mode, MENU_TEXTURE_PAGE_FRAME);

    for (i = 0; i < 4; i++) {
        ptr = &render_buffer->menu_cursor[i];
        menu_init_default_poly_ft4(ptr);
        SetSemiTrans(ptr, 0);
        SetShadeTex(ptr, 1);
        /* The draw area's x/y serve as the screen base point. */
        gfx_init_image_loading(
            ptr, g_attack_deploy_texture_location, &g_menu_draw_area_template, &g_attack_deploy_menu_cursor_cells[i]);
    }

    if (mode != ATTACK_DEPLOY_MENU_BLOCKED_TILE) {
        palette = 0x7D7C;
        for (i = 3; i >= 0; i--) {
            render_buffer->menu_cursor[i].clut = palette;
        }
    } else {
        palette = 0x7DFC;
        for (i = 3; i >= 0; i--) {
            render_buffer->menu_cursor[i].clut = palette;
        }
    }

    gfx_draw_or_append_gpu_primitive(&render_buffer->menu_draw_mode);
    if (mode < 2) {
        gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[0]);
    } else if (mode == ATTACK_DEPLOY_MENU_SELECTED_UNIT) {
        gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[1]);
    } else if (mode == ATTACK_DEPLOY_MENU_OTHER_UNIT) {
        gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[1]);
        gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[0]);
        if ((frame & 0x3F) < 0x20) {
            render_buffer->menu_cursor[0].clut = 0x7DFC;
        } else {
            render_buffer->menu_cursor[1].clut = 0x7DFC;
        }
    } else if (mode == ATTACK_DEPLOY_MENU_SWAP_UNIT) {
        gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[3]);
        gfx_draw_or_append_gpu_primitive(&render_buffer->menu_cursor[2]);
        if ((frame & 0x3F) < 0x20) {
            render_buffer->menu_cursor[2].clut = 0x7DFC;
        } else {
            render_buffer->menu_cursor[3].clut = 0x7DFC;
        }
    }
}

/* Mark whether the deployment grid cell at (column, row) accepts a unit. Nothing calls it. */
void attack_deploy_set_valid_tile(s32 column, s32 row, u8 is_valid) {
    g_attack_deploy_valid_tiles[row][column] = is_valid;
}

/* Return whether the deployment grid cell at (column, row) accepts a unit. Nothing calls it. */
u8 attack_deploy_get_valid_tile(s32 column, s32 row) {
    return g_attack_deploy_valid_tiles[row][column];
}

/* Move the first deployment cursor to (column, row). Nothing calls it. */
void attack_deploy_set_cursor_tile(s32 column, s32 row) {
    g_attack_deploy_cursor_column[0] = column;
    g_attack_deploy_cursor_row[0] = row;
}

/* Choose which of the three places the tiles-only view draws its arrow (0, 1 or 2). Nothing calls
 * it; attack_deploy_init_valid_tiles resets the mode to 0. */
void attack_deploy_set_arrow_position_mode(s32 mode) {
    g_attack_deploy_arrow_position_mode = mode;
}

/* Enter the deployment screen's tiles-only view: hide the menu, reset the
 * cursor and grid slide, prepare the valid-tile primitives and start the
 * deployment render thread. */
void attack_deploy_init_valid_tiles(void) {
    g_attack_deploy_tiles_only_mode = 1;
    g_attack_deploy_menu_state = ATTACK_DEPLOY_MENU_HIDDEN;
    g_attack_deploy_active_cursor = 0;
    g_attack_deploy_grid_slide_x = 0;
    g_attack_deploy_grid_slide_y = 0;
    g_attack_deploy_arrow_position_mode = 0;
    attack_deploy_prepare_valid_tiles(g_attack_deploy_render_buffers[0].tiles[0]);
    thread_start(ATTACK_THREAD_SLOT_DEPLOY_RENDER, attack_deploy_run_render_thread);
    thread_set_parameters(ATTACK_THREAD_SLOT_DEPLOY_RENDER, 0, 0, 0);
}

/* Deployment-screen render thread: builds the double-buffered primitive block,
 * then each frame draws the arrow, portraits, cursors and tile grid until the
 * thread's third parameter is set.
 *
 * Both arms of the arrow test end with the same tile call and screen-offset
 * computation; cross-jumping merges them, which keeps the polarity offset
 * chain at the top of the joined block as in the target. The offset has its
 * own variable rather than reusing `x`. */
static void attack_deploy_run_render_thread(void) {
    s32 frame;
    s32 i;
    s32 x;
    s32 y;
    s32 count;
    SPRT* tile;
    s32 offset_y;

    attack_deploy_init_cursor_primitives();
    attack_deploy_build_screen_arrow();
    attack_gfx_init_frame_ft4_array();
    menu_init_sprite_array(g_attack_deploy_render_buffers[0].tiles[0], 0x19, 0x7ebc);

    LoadImage(&g_attack_deploy_tile_clut_rect, (u32*)g_attack_deploy_tile_clut_data);
    StoreImage(&g_attack_deploy_tile_clut_rect, (u32*)g_attack_deploy_tile_clut_buffer);
    StoreImage(&g_attack_deploy_rect_09c, (u32*)g_attack_deploy_semitrans_clut_buffer);
    /* Let the readbacks finish, then upload semi-transparent copies of both CLUTs:
     * every color but entry 0 gets the STP bit, and entry 0 becomes transparent black. */
    thread_wait_frames(1);
    for (i = 1; i < 48; i++) {
        g_attack_deploy_tile_clut_buffer[i].semi_transparent = 1;
    }
    g_attack_deploy_tile_clut_buffer[0].red = g_attack_deploy_tile_clut_buffer[0].green
        = g_attack_deploy_tile_clut_buffer[0].blue = g_attack_deploy_tile_clut_buffer[0].semi_transparent = 0;
    LoadImage(&g_attack_deploy_rect_0cc, (u32*)g_attack_deploy_tile_clut_buffer);
    for (i = 1; i < 16; i++) {
        g_attack_deploy_semitrans_clut_buffer[i].semi_transparent = 1;
    }
    g_attack_deploy_semitrans_clut_buffer[0].red = g_attack_deploy_semitrans_clut_buffer[0].green
        = g_attack_deploy_semitrans_clut_buffer[0].blue = g_attack_deploy_semitrans_clut_buffer[0].semi_transparent = 0;
    LoadImage(&g_attack_deploy_rect_0ac, (u32*)g_attack_deploy_semitrans_clut_buffer);

    SetDrawMode(
        &g_attack_deploy_render_buffers[0].draw_mode_9c4, 0, 0, GetTPage(0, 1, 0x100, 0), &g_menu_draw_area_template);
    SetDrawMode(
        &g_attack_deploy_render_buffers[0].draw_mode_9d0, 0, 0, GetTPage(0, 1, 0x100, 0), &g_menu_draw_area_template);
    SetDrawMode(
        &g_attack_deploy_render_buffers[0].draw_mode_9dc, 0, 0, GetTPage(0, 1, 0x100, 0), &g_menu_draw_area_template);
    SetDrawMode(
        &g_attack_deploy_render_buffers[0].draw_mode_9e8, 0, 0, GetTPage(0, 1, 0x100, 0), &g_menu_draw_area_template);
    SetDrawMode(&g_attack_deploy_render_buffers[0].draw_mode_9f4, 0, 0, GetTPage(0, 1, 0x3c0, 0x100),
        &g_menu_draw_area_template);
    attack_deploy_prepare_valid_tiles(g_attack_deploy_render_buffers[0].tiles[0]);
    mem_copy_bytes_nonempty(
        &g_attack_deploy_render_buffers[1], &g_attack_deploy_render_buffers[0], sizeof(attack_deploy_render_buffer_t));

    frame = 0;
    while (1) {
        thread_yield();
        if (g_attack_deploy_tiles_only_mode == 0 && g_attack_deploy_menu_state != ATTACK_DEPLOY_MENU_HIDDEN) {
            attack_deploy_build_menu_cursor_primitives(
                frame, g_attack_deploy_menu_state, &g_attack_deploy_render_buffers[frame & 1]);
        }
        gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].draw_mode_9c4);

        if (g_attack_deploy_tiles_only_mode != 0) {
            if (g_attack_deploy_arrow_position_mode == 0) {
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x0 = 0xb9;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x1 = 0xe0;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x2 = 0xc2;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y0 = 0x7e;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y1 = 0x91;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y2 = 0x90;
            } else if (g_attack_deploy_arrow_position_mode == 1) {
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x0 = 0x140;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x1 = 0x119;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x2 = 0x137;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y0 = 0x7e;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y1 = 0x91;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y2 = 0x90;
            } else if (g_attack_deploy_arrow_position_mode == 2) {
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x0 = 0x140;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x1 = 0x119;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x2 = 0x137;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y0 = 0x5e;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y1 = 0x4b;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y2 = 0x4c;
            } else {
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x0 = 0xb9;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x1 = 0xe0;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x2 = 0xc2;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y0 = 0x5c;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y1 = 0x49;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y2 = 0x4a;
            }
            attack_deploy_prepare_valid_tiles(g_attack_deploy_render_buffers[frame & 1].tiles[0]);
            offset_y = g_main_gfx_screen_polarity == 0 ? 0xf0 : 0;
        } else {
            attack_deploy_prepare_valid_tiles(g_attack_deploy_render_buffers[frame & 1].tiles[0]);
            offset_y = g_main_gfx_screen_polarity == 0 ? 0xf0 : 0;
        }

        g_attack_deploy_render_buffers[frame & 1].world_position.x = g_attack_deploy_grid_slide_x - 0x80;
        g_attack_deploy_render_buffers[frame & 1].world_position.y = g_attack_deploy_grid_slide_y + offset_y;
        SetDrawOffset(&g_attack_deploy_render_buffers[frame & 1].world_offset,
            (u16*)&g_attack_deploy_render_buffers[frame & 1].world_position);
        g_attack_deploy_render_buffers[frame & 1].screen_position.x = -0x80;
        g_attack_deploy_render_buffers[frame & 1].screen_position.y = offset_y;
        SetDrawOffset(&g_attack_deploy_render_buffers[frame & 1].screen_offset,
            (u16*)&g_attack_deploy_render_buffers[frame & 1].screen_position);

        if (thread_get_current_parameter_3() != 0) {
            break;
        }

        /* Each primitive is prepended to the current ordering-table entry, so the GPU draws
         * them in reverse: offset and draw-mode packets follow the primitives they govern. */
        if (g_attack_deploy_tiles_only_mode == 0) {
            attack_menu_build_status_group_primitives(g_attack_deploy_render_buffers[frame & 1].status);
        }
        gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].screen_offset);
        attack_deploy_update_cursor_primitives(frame, &g_attack_deploy_render_buffers[frame & 1]);
        if (g_attack_deploy_tiles_only_mode == 0) {
            gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].screen_arrow.poly);
        }

        count = 0;
        tile = g_attack_deploy_render_buffers[frame & 1].tiles[0];
        g_attack_deploy_cursor_1_submitted = 0;
        g_attack_deploy_cursor_0_submitted = 0;
        if (g_attack_deploy_active_cursor == 1) {
            g_attack_deploy_cursor_1_poly = &g_attack_deploy_render_buffers[frame & 1].cursor[1][0];
            g_attack_deploy_cursor_1_submitted = 1;
            gfx_draw_or_append_gpu_primitive(g_attack_deploy_cursor_1_poly);
        }
        g_attack_deploy_cursor_0_poly = &g_attack_deploy_render_buffers[frame & 1].cursor[0][0];
        g_attack_deploy_cursor_0_submitted = 1;
        gfx_draw_or_append_gpu_primitive(g_attack_deploy_cursor_0_poly);

        /* Each placed unit takes two quads: its portrait and the marker under it. */
        for (y = 0; y < 5; y++) {
            for (x = 0; x < 5; x++) {
                if (g_attack_deploy_tiles_only_mode == 0
                    && g_attack_deploy_roster_id_by_tile[y][x] != ATTACK_DEPLOY_ROSTER_ID_NONE) {
                    attack_deploy_build_portrait_quads(x, y, g_attack_deploy_roster_id_by_tile[y][x],
                        &g_attack_deploy_render_buffers[frame & 1].portraits[count]);
                    gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].portraits[count]);
                    count++;
                    gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].portraits[count]);
                    count++;
                }
            }
        }

        gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].draw_mode_9f4);
        for (y = 0; y < 5; y++) {
            for (x = 0; x < 5; x++) {
                if (g_attack_deploy_cursor_column[1] == x && g_attack_deploy_cursor_row[1] == y
                    && g_attack_deploy_active_cursor == 1) {
                    gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].draw_mode_9d0);
                    gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].cursor[1][1]);
                }
                if (g_attack_deploy_cursor_column[0] == x && g_attack_deploy_cursor_row[0] == y) {
                    gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].draw_mode_9dc);
                    gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].cursor[0][1]);
                }
                if (g_attack_deploy_tiles_only_mode == 0) {
                    if (g_attack_deploy_valid_tiles[y][x] != 0) {
                        tile->u0 = 0x28;
                        if (g_attack_deploy_roster_id_by_tile[y][x] == ATTACK_DEPLOY_ROSTER_ID_NONE) {
                            tile->clut
                                = GetClut(g_attack_deploy_tile_clut_rect.x + 0x10, g_attack_deploy_tile_clut_rect.y);
                        } else {
                            tile->clut = GetClut(g_attack_deploy_tile_clut_rect.x, g_attack_deploy_tile_clut_rect.y);
                        }
                    } else {
                        tile->u0 = 0;
                        tile->clut = GetClut(g_attack_deploy_tile_clut_rect.x + 0x20, g_attack_deploy_tile_clut_rect.y);
                    }
                } else if (g_attack_deploy_valid_tiles[y][x] != 0) {
                    tile->u0 = 0x28;
                    tile->clut = GetClut(g_attack_deploy_tile_clut_rect.x + 0x30, g_attack_deploy_tile_clut_rect.y);
                    setRGB0(tile, 0xe0, 0x60, 0x60);
                } else {
                    tile->u0 = 0;
                    tile->clut = GetClut(g_attack_deploy_tile_clut_rect.x + 0x50, g_attack_deploy_tile_clut_rect.y);
                    setRGB0(tile, 0x80, 0x80, 0xc0);
                }
                SetSemiTrans(tile, 1);
                SetShadeTex(tile, 0);
                gfx_draw_or_append_gpu_primitive(tile);
                tile++;
            }
        }

        gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].draw_mode_9e8);
        gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].world_offset);
        g_attack_deploy_zodiac_draw_context.ot = g_gfx_current_otag_entry;
        /* The zodiac sign shrinks by 1/16 per frame until it reaches full size. */
        if (g_attack_deploy_zodiac_draw_context.scale_x > ONE) {
            g_attack_deploy_zodiac_draw_context.scale_x -= 0x100;
            g_attack_deploy_zodiac_draw_context.scale_y -= 0x100;
        }
        if (g_attack_deploy_tiles_only_mode == 0) {
            main_gfx_call_build_zodiac_primitives(
                &g_attack_deploy_zodiac_draw_context, g_main_zodiac_primitives[frame & 1]);
        }
        frame++;
    }
    thread_exit_current();
}

/* Expands the squad's 5-by-5 deployment bitmap into g_attack_deploy_valid_tiles
 * for its orientation (skipped while g_attack_deploy_tiles_only_mode is set), then lays out one
 * valid/invalid tile sprite per cell.
 *
 * The y0 assignment is written out in both arms: the target's shared
 * subtract/add tail comes from cross-jumping those identical arm tails. */
static void attack_deploy_prepare_valid_tiles(SPRT* sprites) {
    s32 x;
    s32 y;
    s32 bit = 1;
    s32 orientation;
    s32 index;
    s32 lift;

    if (g_attack_deploy_tiles_only_mode == 0) {
        orientation = g_attack_deploy_current_squad_data->orientation_flags & ATTACK_DEPLOYMENT_ORIENTATION_MASK;
        if (orientation == ATTACK_DEPLOYMENT_ORIENTATION_NORMAL) {
            for (y = 0; y < 5; y++) {
                for (x = 0; x < 5; x++) {
                    if (bit & g_attack_deploy_current_squad_data->valid_tile_bitmap) {
                        g_attack_deploy_valid_tiles[y][x] = 1;
                    } else {
                        g_attack_deploy_valid_tiles[y][x] = 0;
                    }
                    bit <<= 1;
                }
            }
        } else if (orientation == ATTACK_DEPLOYMENT_ORIENTATION_ROTATE_90) {
            for (x = 4; x >= 0; x--) {
                for (y = 0; y < 5; y++) {
                    if (bit & g_attack_deploy_current_squad_data->valid_tile_bitmap) {
                        g_attack_deploy_valid_tiles[y][x] = 1;
                    } else {
                        g_attack_deploy_valid_tiles[y][x] = 0;
                    }
                    bit <<= 1;
                }
            }
        } else if (orientation == ATTACK_DEPLOYMENT_ORIENTATION_ROTATE_180) {
            for (y = 4; y >= 0; y--) {
                for (x = 4; x >= 0; x--) {
                    if (bit & g_attack_deploy_current_squad_data->valid_tile_bitmap) {
                        g_attack_deploy_valid_tiles[y][x] = 1;
                    } else {
                        g_attack_deploy_valid_tiles[y][x] = 0;
                    }
                    bit <<= 1;
                }
            }
        } else if (orientation == ATTACK_DEPLOYMENT_ORIENTATION_ROTATE_270) {
            for (x = 0; x < 5; x++) {
                for (y = 4; y >= 0; y--) {
                    if (bit & g_attack_deploy_current_squad_data->valid_tile_bitmap) {
                        g_attack_deploy_valid_tiles[y][x] = 1;
                    } else {
                        g_attack_deploy_valid_tiles[y][x] = 0;
                    }
                    bit <<= 1;
                }
            }
        }
    }

    index = 0;
    for (y = 0; y < 5; y++) {
        for (x = 0; x < 5; x++) {
            if (g_attack_deploy_valid_tiles[y][x] != 0) {
                lift = -4;
                sprites[index].u0 = 0x28;
                sprites[index].v0 = 0xc0;
            } else {
                lift = 0;
                sprites[index].u0 = 0;
                sprites[index].v0 = 0xc0;
            }
            sprites[index].x0 = y * 20 + 0x100 - x * 20;
            if (g_attack_deploy_tiles_only_mode != 0) {
                sprites[index].y0 = 0x88 - x * 10 - y * 10 + lift;
            } else {
                sprites[index].y0 = 0xc4 - x * 10 - y * 10 + lift;
            }
            sprites[index].w = 0x28;
            sprites[index].h = 0x1c;
            sprites[index].x0 -= 0x14;
            index++;
        }
    }
}

/* Upload the cursor texture and build both deployment cursors' marker and
 * tile-highlight quads; attack_deploy_update_cursor_primitives positions them each frame. */
static void attack_deploy_init_cursor_primitives(void) {
    s32 i;
    s32 j;

    LoadImage(&g_attack_deploy_cursor_texture_rect, g_attack_deploy_cursor_texture_data);
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            menu_init_default_poly_ft4(&g_attack_deploy_render_buffers[0].cursor[i][j]);
        }
    }
    for (i = 0; i < 2; i++) {
        POLY_FT4* marker = &g_attack_deploy_render_buffers[0].cursor[i][0];
        POLY_FT4* highlight = &g_attack_deploy_render_buffers[0].cursor[i][1];

        setUV4(marker, 0x40, 0x80, 0x50, 0x80, 0x40, 0x98, 0x50, 0x98);
        setUV4(highlight, 0, 0xa0, 0xe, 0xa0, 0, 0xae, 0xe, 0xae);
        SetSemiTrans(marker, 1);
        SetSemiTrans(highlight, 1);
        g_attack_deploy_render_buffers[0].cursor[i][0].tpage = GetTPage(0, 2, 0x3c0, 0x100);
        g_attack_deploy_render_buffers[0].cursor[i][1].tpage = GetTPage(0, 1, 0x3c0, 0x100);
        g_attack_deploy_render_buffers[0].cursor[i][0].clut = GetClut(0x140, 0xf0);
        g_attack_deploy_render_buffers[0].cursor[i][1].clut = GetClut(0x140, 0xf2);
    }
}

/* Animates the deployment cursor palette and positions both cursor sprites.
 *
 * Each cursor n owns a bobbing marker quad and a tile-highlight diamond in
 * buffer->cursor[n]; the cursor tile comes from the two-entry row/column
 * arrays at g_attack_deploy_cursor_row/column. The ternary with the full
 * expression in each arm and the separate height variable reproduce the
 * target's cross-jumped arm tails and its unfolded lift + 10 addition. */
static void attack_deploy_update_cursor_primitives(s32 frame, attack_deploy_render_buffer_t* buffer) {
    thread_t* thread = &g_threads[g_thread_current_id];
    s32 bob = menu_get_cursor_bob_offset(0);
    s32 i;
    u8(*tiles)[5];

    thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] += g_script_event_speed;
    mem_copy_bytes_nonempty(
        &g_attack_deploy_cursor_clut[1], &g_attack_deploy_cursor_palette_cycle[0][1 + (frame / 4) % 14], 0x1e);
    g_attack_deploy_cursor_clut[0].red = g_attack_deploy_cursor_clut[0].green = g_attack_deploy_cursor_clut[0].blue
        = g_attack_deploy_cursor_clut[0].semi_transparent = 0;
    LoadImage(&g_attack_deploy_cursor_clut_rect, (u32*)g_attack_deploy_cursor_clut);

    for (i = 0, tiles = g_attack_deploy_valid_tiles; i < 2; i++) {
        s32 row = g_attack_deploy_cursor_row[i];
        s32 column = g_attack_deploy_cursor_column[i];
        s32 x;
        s32 y;
        s32 top;
        s32 lift;
        s32 height;
        s32 offset = bob - 0x40;

        height = 10;
        lift = -4;
        if (tiles[row][column] == 0) {
            lift = 4;
        }
        x = row * 20 + 0x100 - column * 20 + i;
        y = (g_attack_deploy_tiles_only_mode != 0 ? 0x88 - row * 10 - column * 10 + (lift + height)
                                                  : 0xc4 - row * 10 - column * 10 + (lift + height))
            - i * 3;
        top = y + offset;

        setXY4(&buffer->cursor[i][0], x - 8, top, x + 8, top, x - 8, top + 0x18, x + 8, top + 0x18);
        setXY4(&buffer->cursor[i][1], x, y - 10, x + 20, y, x - 20, y, x, y + 10);
    }
}

/* Initialize the five pairs of frame quads: an opaque quad on the texture
 * page at (0x140, 0) and a semi-transparent one on the page at
 * (0x3c0, 0x100). */
static void attack_gfx_init_frame_ft4_array(void) {
    s32 i;

    for (i = 0; i < 10; i += 2) {
        menu_init_default_poly_ft4(&g_attack_gfx_frame_ft4[i]);
        menu_init_default_poly_ft4(&g_attack_gfx_frame_ft4[i + 1]);
        g_attack_gfx_frame_ft4[i].tpage = GetTPage(0, 0, 0x140, 0);
        g_attack_gfx_frame_ft4[i + 1].tpage = GetTPage(0, 2, 0x3c0, 0x100);
        SetSemiTrans(&g_attack_gfx_frame_ft4[i + 1], 1);
    }
}

/* Build the two POLY_FT4 quads for one deployment roster portrait.
 *
 * The first quad is a 48x48 cell of the formation sprite sheet (five cells per
 * row, CLUT selected by index % 4 and index / 4) placed over the isometric tile
 * at (column, row), using the same screen projection as
 * attack_deploy_update_cursor_primitives; a nonzero
 * g_attack_formation_sprite_tall_cell[index] draws it four pixels lower. The
 * second quad squeezes the 20x20 texel cell at (0x90, 0x40) into a 20x10
 * marker under it. Each quad is written in libgpu setXYWH/setUVWH field order,
 * which the target's store order and constant offsets follow. */
static void attack_deploy_build_portrait_quads(s32 column, s32 row, s32 index, POLY_FT4* poly) {
    s32 u = (index % 5) * 0x30;
    s32 v = (index / 5) * 0x30;
    s32 x;
    s32 y;
    s32 marker_y;
    s32 x0;
    s32 y0;

    setUV4(poly, u, v, u + 0x30, v, u, v + 0x30, u + 0x30, v + 0x30);
    x = row * 20 + 0x100 - column * 20;
    y = 0xc4 - row * 10 - column * 10;
    marker_y = y + 2;
    poly->clut = GetClut((index % 4) * 0x10 + 0x140, index / 4 + 0xe0);
    if (g_attack_formation_sprite_tall_cell[index] != 0) {
        x0 = x - 0x17;
        y0 = y - 0x21;
        setXY4(poly, x0, y0, x0 + 0x30, y0, x0, y0 + 0x30, x0 + 0x30, y0 + 0x30);
    } else {
        x0 = x - 0x17;
        y0 = y - 0x25;
        setXY4(poly, x0, y0, x0 + 0x30, y0, x0, y0 + 0x30, x0 + 0x30, y0 + 0x30);
    }
    poly++;
    setXY4(poly, x - 10, marker_y - 1, x + 10, marker_y - 1, x - 10, marker_y + 9, x + 10, marker_y + 9);
    setUV4(poly, 0x90, 0x40, 0xa4, 0x40, 0x90, 0x54, 0xa4, 0x54);
    poly->clut = GetClut(0x140, 0xf8);
}

/* Thread body that draws the deployment screen's nine text graphics, double buffered:
 * three groups of three, with the squad caption (first or later squad), the squad's
 * unit limit and the units it may still take (each digit picked by its x_load,
 * 8 pixels per digit from 0xc8). Each graphic grows upward one line a frame to its
 * 12-line height; the units-left group (6-8) shrinks away once the squad is full. A
 * nonzero third thread parameter shrinks every graphic, and the thread exits once the
 * first graphic of each group is gone.
 *
 * The v/y adjustments and the tpage/clut stores address the current
 * primitive through a byte offset recomputed at the top of each iteration:
 * GCC then strength-reduces that offset last, after the g_attack_deploy_text_graphic_params[i]
 * parameter pointer, which is the target's induction-register order. The
 * equivalent g_attack_deploy_text_polys[parity][i] (or a flat index) form reduces a second
 * copy of the offset after the join below and changes the loop increments. */
static void attack_deploy_update_screen_graphics(void) {
    s32 i;
    s32 parity = 0;
    s32 squad;
    s32* value;
    s32 off;

    for (i = 0; i < 9; i++) {
        g_attack_deploy_text_reveal_steps[i] = 1;
        g_attack_deploy_text_reveal_heights[i] = 0;
    }
    while (1) {
        squad = g_attack_deploy_current_squad;
        parity ^= 1;
        if (g_attack_deploy_current_squad >= script_get_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT)) {
            squad--;
        }
        if (squad == 0) {
            g_attack_deploy_text_graphic_params[1].x_load = 0xc0;
            g_attack_deploy_text_graphic_params[1].y_load = 0x18;
        } else {
            g_attack_deploy_text_graphic_params[1].x_load = 0xe8;
            g_attack_deploy_text_graphic_params[1].y_load = 0xc;
        }
        g_attack_deploy_text_graphic_params[4].x_load = g_attack_deploy_current_squad_data->unit_limit * 8 + 0xc8;
        g_attack_deploy_text_graphic_params[7].x_load
            = (g_attack_deploy_current_squad_data->unit_limit - g_attack_deploy_deployed_unit_count) * 8 + 0xc8;
        for (i = 0; i < 9; i++) {
            off = parity * sizeof(g_attack_deploy_text_polys[0]) + i * sizeof(POLY_FT4);
            if ((g_attack_deploy_current_squad_data->unit_limit - g_attack_deploy_deployed_unit_count - 1 < 0 && i >= 6)
                || thread_get_current_parameter_3() != 0) {
                if (g_attack_deploy_current_squad_data->unit_limit - g_attack_deploy_deployed_unit_count - 1 < 0) {
                    g_attack_deploy_text_graphic_params[7].x_load = 0xd0;
                }
                g_attack_deploy_text_reveal_steps[6] = -1;
                g_attack_deploy_text_reveal_steps[7] = -1;
                g_attack_deploy_text_reveal_steps[8] = -1;
            } else {
                g_attack_deploy_text_reveal_steps[6] = 1;
                g_attack_deploy_text_reveal_steps[7] = 1;
                g_attack_deploy_text_reveal_steps[8] = 1;
            }
            menu_init_default_poly_ft4(&g_attack_deploy_text_polys[parity][i]);
            ((POLY_FT4*)((u8*)g_attack_deploy_text_polys + off))->tpage = GetTPage(0, 0, 0x100, 0);
            ((POLY_FT4*)((u8*)g_attack_deploy_text_polys + off))->clut = 0x7d7c;
            gfx_init_image_loading(&g_attack_deploy_text_polys[parity][i], g_attack_deploy_texture_location,
                &g_attack_deploy_text_screen_origin, &g_attack_deploy_text_graphic_params[i]);
            if (g_attack_deploy_text_reveal_steps[i] != 0) {
                if (g_attack_deploy_text_reveal_steps[i] > 0) {
                    value = &g_attack_deploy_text_reveal_heights[i];
                    if (*value != 12) {
                        *value += g_attack_deploy_text_reveal_steps[i];
                    }
                } else {
                    value = &g_attack_deploy_text_reveal_heights[i];
                    if (*value != 0) {
                        *value += g_attack_deploy_text_reveal_steps[i];
                    }
                }
                ((POLY_FT4*)((u8*)g_attack_deploy_text_polys + off))->v0 -= *value - 12;
                ((POLY_FT4*)((u8*)g_attack_deploy_text_polys + off))->v1 -= *value - 12;
                ((POLY_FT4*)((u8*)g_attack_deploy_text_polys + off))->y0 -= *value - 12;
                ((POLY_FT4*)((u8*)g_attack_deploy_text_polys + off))->y1 -= *value - 12;
            }
            gfx_draw_or_append_gpu_primitive(&g_attack_deploy_text_polys[parity][i]);
        }
        thread_yield();
        if (thread_get_current_parameter_3() == 0) {
            continue;
        }
        for (i = 8; i >= 0; i--) {
            g_attack_deploy_text_reveal_steps[i] = -1;
        }
        if (g_attack_deploy_text_reveal_heights[0] == 0 && g_attack_deploy_text_reveal_heights[3] == 0
            && g_attack_deploy_text_reveal_heights[6] == 0) {
            break;
        }
    }
    thread_exit_current();
}

/* Call attack_gfx_prepare_formation_sprites. */
void attack_gfx_build_formation_sprites(void) {
    attack_gfx_prepare_formation_sprites();
}

/* Call attack_gfx_prepare_party_portrait_textures. */
void attack_gfx_load_portraits(void) {
    attack_gfx_prepare_party_portrait_textures();
}
