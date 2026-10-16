/* The description text thread, the grid cursor, the unit status banner countdown and the bits reader init. */

#include "fft/bits.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_overlay.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "fft/world_formation.h"
#include "fft/world_menu.h"
#include "fft/world_script.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Open the pending menu description in a Help dialog at the requested position.
 * Grid-menu ids above 0x1ffff are latched without opening a text thread. */
void world_menu_start_description_text_thread(s32 position) {

    if (g_menu_help_message_text_id > 0x1ffff) {
        g_world_grid_menu_id = g_menu_help_message_text_id;
        return;
    }
    if (g_menu_help_message_text_id > 0) {
        thread_start(THREAD_SCREEN_SLOT_TEXT, text_run_dialog_thread_variant);
        thread_set_parameters(THREAD_SCREEN_SLOT_TEXT, position + (DIALOG_BOX_TYPE_HELP | DIALOG_OPTIONS_NO_TAIL),
            g_menu_help_message_text_id, 0);
        g_menu_input_lock_mode = 1;
    }
}

/* g_world_grid_menu_default_cells[23]/[24].text_offset, bound separately: as
 * members GCC addresses the second store from the first's base. */
extern s16 g_world_grid_menu_cell_23_text_offset;
extern s16 g_world_grid_menu_cell_24_text_offset;

/* Runs the d-pad grid menu selected by g_world_grid_menu_id (0x20000, 0x20001, else
 * the default grid) and draws its cursor.
 *
 * Cross (0x40) latches the menu id as the description text and closes the
 * grid; circle (0x20) sets the cell's description text id, except for empty
 * cells 18-27. */
void world_menu_update_grid_cursor(s16 alternate) {
    point16_t cursor;
    s32 busy;
    s32 cell;
    s32 text_id;
    u32 input;

    if (g_world_grid_menu_initialized == 0) {
        if (alternate == 0) {
            g_world_grid_menu_cell_23_text_offset = -0x6800;
            g_world_grid_menu_cell_24_text_offset = -0x6800;
        } else {
            g_world_grid_menu_cell_23_text_offset = 0x7800;
            g_world_grid_menu_cell_24_text_offset = 0x7800;
        }
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        g_world_grid_menu_initialized = 1;
        g_world_grid_menu_selected_cell = 0;
        if (g_world_grid_menu_id == MENU_GRID_HELP_BANNER_TOP) {
            g_world_active_grid_menu_cells = g_world_grid_menu_shop_cells;
        } else if (g_world_grid_menu_id == MENU_GRID_HELP_BANNER_BOTTOM) {
            g_world_active_grid_menu_cells = g_world_grid_menu_shop_scrolled_cells;
        } else {
            g_world_active_grid_menu_cells = g_world_grid_menu_default_cells;
        }
    }
    busy = thread_is_running(THREAD_SCREEN_SLOT_TEXT);
    if (busy == 0) {
        input = g_world_input_primary_repeat_latched;
        cell = g_world_grid_menu_selected_cell;
        if (input & PSX_PAD_CROSS) {
            s32 menu_id = g_world_grid_menu_id;
            g_world_grid_menu_id = MENU_GRID_HELP_NONE;
            g_sound_queued_effect_id = MAIN_SFX_CANCEL;
            g_world_grid_menu_initialized = 0;
            g_menu_help_message_text_id = menu_id;
        } else if (input & PSX_PAD_CIRCLE) {
            text_id
                = g_world_active_grid_menu_cells[cell].text_id != 0 ? *g_world_active_grid_menu_cells[cell].text_id : 0;
            if (g_world_grid_menu_selected_cell < 0x12 || g_world_grid_menu_selected_cell >= 0x1C || text_id != 0) {
                g_menu_help_message_text_id
                    = text_id + g_world_active_grid_menu_cells[g_world_grid_menu_selected_cell].text_offset;
                world_menu_start_description_text_thread(DIALOG_POSITION_CENTER);
            }
        } else if (input & PSX_PAD_UP) {
            cell = g_world_active_grid_menu_cells[cell].next_up;
        } else if (input & PSX_PAD_DOWN) {
            cell = g_world_active_grid_menu_cells[cell].next_down;
        } else if (input & PSX_PAD_RIGHT) {
            cell = g_world_active_grid_menu_cells[cell].next_right;
        } else if (input & PSX_PAD_LEFT) {
            cell = g_world_active_grid_menu_cells[cell].next_left;
        }
        if (g_world_grid_menu_id == MENU_GRID_HELP_RECRUIT_PREVIEW) {
            if (g_world_grid_menu_selected_cell == 6 && cell == 7) {
                cell = 8;
            }
            if (g_world_grid_menu_selected_cell == 8 && cell == 7) {
                cell = 6;
            }
        }
        if (cell != g_world_grid_menu_selected_cell) {
            g_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
            g_world_grid_menu_selected_cell = cell;
        }
    }
    cursor.x = g_world_active_grid_menu_cells[g_world_grid_menu_selected_cell].cursor.x;
    cursor.y = g_world_active_grid_menu_cells[g_world_grid_menu_selected_cell].cursor.y;
    gfx_set_otag_index(0x3C);
    if (g_world_active_grid_menu_cells[g_world_grid_menu_selected_cell].vertical != 0) {
        cursor.y++;
        menu_update_and_draw_bouncing_cursor_variant(&cursor, &g_world_grid_menu_cursor_anim, busy);
    } else {
        cursor.x -= 2;
        cursor.y += 6;
        menu_update_and_draw_sliding_cursor_variant(&cursor, &g_world_grid_menu_cursor_anim, busy);
    }
}

/* Starts the thread-6 banner for `message_id` (0xD811 after auto-equip) and counts `frames` calls down
 * before stopping it; returns 0 once the banner has closed. */
s32 world_menu_run_unit_status_banner_countdown(s32 message_id, s32 frames) {
    s8 current;

    if (g_world_unit_status_banner_state == 0) {
        g_world_unit_status_banner_state = frames;
        g_world_unit_status_banner_menu.text_id = message_id;
        thread_start(THREAD_SCREEN_SLOT_MENU, g_world_unit_status_banner_menu.thread_entry);
        thread_set_parameters(THREAD_SCREEN_SLOT_MENU, (s32)&g_world_unit_status_banner_menu, 0, 0);
        g_world_unit_status_banner_active = 1;
    }
    current = g_world_unit_status_banner_state;
    if (current == 1) {
        menu_request_thread_stop(THREAD_SCREEN_SLOT_MENU);
        if (thread_is_running(THREAD_SCREEN_SLOT_MENU) == 0) {
            g_world_unit_status_banner_active = 0;
            g_world_unit_status_banner_state = 0;
            return 0;
        }
        return 1;
    }
    g_world_unit_status_banner_state = current - 1;
    return 1;
}

/* Selects the primary bitstream and resets its next operation to bit 7. */
void bits_init_primary_reader_variant(const u8* data) {
    g_bits_primary_reset = 1;
    g_bits_primary_cursor = (u8*)data;
}

/* Selects the secondary read-only bitstream and resets it to bit 7. */
void bits_init_secondary_reader_variant(const u8* data) {
    g_bits_reader_2_reset = 1;
    g_bits_reader_2_stream = data;
}
