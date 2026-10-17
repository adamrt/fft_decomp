/* The description text thread, the grid cursor, the unit status banner countdown and the bits reader init. */

#include "fft/bits.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/message.h"
#include "fft/screen.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Open the pending menu description in a Help dialog at the requested position.
 * Grid-menu ids above 0x1ffff are latched without opening a text thread. */
void screen_start_help_text(s32 position) {
    if (g_help_message_text_id > 0x1ffff) {
        g_screen_grid_id = g_help_message_text_id;
        return;
    }
    if (g_help_message_text_id > 0) {
        thread_start(THREAD_SCREEN_SLOT_TEXT, message_dialog_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_TEXT, position + (DIALOG_BOX_TYPE_HELP | DIALOG_OPTIONS_NO_TAIL),
            g_help_message_text_id, 0);
        g_menu_input_lock_mode = 1;
    }
}

/* g_screen_grid_default_cells[23]/[24].text_offset, bound separately: as
 * members GCC addresses the second store from the first's base. */
extern s16 g_screen_grid_cell_23_text_offset;
extern s16 g_screen_grid_cell_24_text_offset;

/* Runs the d-pad help grid selected by g_screen_grid_id (the shop banner
 * grids for MENU_GRID_HELP_BANNER_TOP and _BOTTOM, else the default grid) and
 * draws its cursor. `alternate` (the unit uses a monster skillset) switches
 * the text offset of cells 23 and 24.
 *
 * Cross latches the menu id as the description text and closes the grid;
 * Circle opens the cell's description (its text id plus the cell's offset),
 * except for empty cells 18-27. */
void screen_update_grid_cursor(s16 alternate) {
    point16_t cursor;
    s32 busy;
    s32 cell;
    s32 text_id;
    u32 input;

    if (g_screen_grid_initialized == 0) {
        if (alternate == 0) {
            g_screen_grid_cell_23_text_offset = (s16)HELP_TEXT_ID_SKILLSET_BASE;
            g_screen_grid_cell_24_text_offset = (s16)HELP_TEXT_ID_SKILLSET_BASE;
        } else {
            g_screen_grid_cell_23_text_offset = HELP_TEXT_ID_ABILITY_BASE;
            g_screen_grid_cell_24_text_offset = HELP_TEXT_ID_ABILITY_BASE;
        }
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        g_screen_grid_initialized = 1;
        g_screen_grid_selected_cell = 0;
        if (g_screen_grid_id == MENU_GRID_HELP_BANNER_TOP) {
            g_screen_grid_active_cells = g_screen_grid_shop_cells;
        } else if (g_screen_grid_id == MENU_GRID_HELP_BANNER_BOTTOM) {
            g_screen_grid_active_cells = g_screen_grid_shop_scrolled_cells;
        } else {
            g_screen_grid_active_cells = g_screen_grid_default_cells;
        }
    }
    busy = thread_is_running(THREAD_SCREEN_SLOT_TEXT);
    if (busy == 0) {
        input = g_screen_input_primary_repeat_latched;
        cell = g_screen_grid_selected_cell;
        if (input & PSX_PAD_CROSS) {
            s32 menu_id = g_screen_grid_id;
            g_screen_grid_id = MENU_GRID_HELP_NONE;
            g_sound_queued_effect_id = SOUND_SFX_CANCEL;
            g_screen_grid_initialized = 0;
            g_help_message_text_id = menu_id;
        } else if (input & PSX_PAD_CIRCLE) {
            text_id = g_screen_grid_active_cells[cell].text_id != 0 ? *g_screen_grid_active_cells[cell].text_id : 0;
            if (g_screen_grid_selected_cell < 0x12 || g_screen_grid_selected_cell >= 0x1C || text_id != 0) {
                g_help_message_text_id = text_id + g_screen_grid_active_cells[g_screen_grid_selected_cell].text_offset;
                screen_start_help_text(DIALOG_POSITION_CENTER);
            }
        } else if (input & PSX_PAD_UP) {
            cell = g_screen_grid_active_cells[cell].next_up;
        } else if (input & PSX_PAD_DOWN) {
            cell = g_screen_grid_active_cells[cell].next_down;
        } else if (input & PSX_PAD_RIGHT) {
            cell = g_screen_grid_active_cells[cell].next_right;
        } else if (input & PSX_PAD_LEFT) {
            cell = g_screen_grid_active_cells[cell].next_left;
        }
        /* The recruit preview grid steps over cell 7 between cells 6 and 8. */
        if (g_screen_grid_id == MENU_GRID_HELP_RECRUIT_PREVIEW) {
            if (g_screen_grid_selected_cell == 6 && cell == 7) {
                cell = 8;
            }
            if (g_screen_grid_selected_cell == 8 && cell == 7) {
                cell = 6;
            }
        }
        if (cell != g_screen_grid_selected_cell) {
            g_sound_queued_effect_id = SOUND_SFX_CURSOR_MOVE;
            g_screen_grid_selected_cell = cell;
        }
    }
    cursor.x = g_screen_grid_active_cells[g_screen_grid_selected_cell].cursor.x;
    cursor.y = g_screen_grid_active_cells[g_screen_grid_selected_cell].cursor.y;
    gfx_set_otag_index(0x3C);
    if (g_screen_grid_active_cells[g_screen_grid_selected_cell].vertical != 0) {
        cursor.y++;
        menu_update_and_draw_bouncing_cursor(&cursor, &g_screen_grid_cursor_anim, busy);
    } else {
        cursor.x -= 2;
        cursor.y += 6;
        screen_draw_sliding_cursor(&cursor, &g_screen_grid_cursor_anim, busy);
    }
}

/* Starts the banner menu for `message_id` (0xD811 after auto-equip) in the screen menu thread
 * slot and counts `frames` calls down before stopping it; returns 1 while the banner is up and 0
 * once it has closed. */
s32 formation_banner_countdown(s32 message_id, s32 frames) {
    s8 current;

    if (g_formation_banner_state == 0) {
        g_formation_banner_state = frames;
        g_formation_banner_menu.text_id = message_id;
        thread_start(THREAD_SCREEN_SLOT_MENU, g_formation_banner_menu.thread_entry);
        thread_set_parameters(THREAD_SCREEN_SLOT_MENU, (s32)&g_formation_banner_menu, 0, 0);
        g_formation_banner_active = 1;
    }
    current = g_formation_banner_state;
    if (current == 1) {
        menu_request_thread_stop(THREAD_SCREEN_SLOT_MENU);
        if (thread_is_running(THREAD_SCREEN_SLOT_MENU) == 0) {
            g_formation_banner_active = 0;
            g_formation_banner_state = 0;
            return 0;
        }
        return 1;
    }
    g_formation_banner_state = current - 1;
    return 1;
}

/* Selects the primary bitstream and resets its next operation to bit 7. */
void bits_init_primary_reader(u8* data) {
    g_bits_primary_reset = 1;
    g_bits_primary_cursor = data;
}

/* Selects the secondary read-only bitstream and resets it to bit 7. */
void bits_init_secondary_reader(const u8* data) {
    g_bits_secondary_reset = 1;
    g_bits_secondary_cursor = data;
}
