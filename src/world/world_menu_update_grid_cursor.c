#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* One cell of a directional grid menu: a description text id (base plus
 * offset), the cell index reached by each d-pad direction, and the cursor
 * position. A non-zero `vertical` draws the vertical cursor. */
typedef struct world_menu_grid_cell {
    s16* text_id;
    u16 text_offset;
    u8 next_up;
    u8 next_down;
    u8 next_right;
    u8 next_left;
    world_menu_point_t cursor;
    s16 vertical;
} world_menu_grid_cell_t;

extern world_menu_grid_cell_t g_world_grid_menu_default_cells[];
/* g_world_grid_menu_default_cells[23]/[24].text_offset, bound separately: as
 * members GCC addresses the second store from the first's base. */
extern s16 g_world_grid_menu_cell_23_text_offset;
extern s16 g_world_grid_menu_cell_24_text_offset;
extern world_menu_grid_cell_t g_world_grid_menu_shop_cells[];
extern world_menu_grid_cell_t g_world_grid_menu_shop_scrolled_cells[];
extern world_menu_grid_cell_t* g_world_active_grid_menu_cells; /* active grid */

/* Runs the d-pad grid menu selected by g_world_grid_menu_id (0x20000, 0x20001, else
 * the default grid) and draws its cursor.
 *
 * Cross (0x40) latches the menu id as the description text and closes the
 * grid; circle (0x20) sets the cell's description text id, except for empty
 * cells 18-27. The description-thread call only latches such ids, so its
 * record argument is never dereferenced. */
void world_menu_update_grid_cursor(s16 alternate) {
    world_menu_point_t cursor;
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
        g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
        g_world_grid_menu_initialized = 1;
        g_world_grid_menu_selected_cell = 0;
        if (g_world_grid_menu_id == 0x20000) {
            g_world_active_grid_menu_cells = g_world_grid_menu_shop_cells;
        } else if (g_world_grid_menu_id == 0x20001) {
            g_world_active_grid_menu_cells = g_world_grid_menu_shop_scrolled_cells;
        } else {
            g_world_active_grid_menu_cells = g_world_grid_menu_default_cells;
        }
    }
    busy = world_thread_is_running(1);
    if (busy == 0) {
        input = g_world_input_primary_repeat_latched;
        cell = g_world_grid_menu_selected_cell;
        if (input & PSX_PAD_CROSS) {
            s32 menu_id = g_world_grid_menu_id;
            g_world_grid_menu_id = 0;
            g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
            g_world_grid_menu_initialized = 0;
            g_world_menu_description_text_id = menu_id;
        } else if (input & PSX_PAD_CIRCLE) {
            text_id
                = g_world_active_grid_menu_cells[cell].text_id != 0 ? *g_world_active_grid_menu_cells[cell].text_id : 0;
            if (g_world_grid_menu_selected_cell < 0x12 || g_world_grid_menu_selected_cell >= 0x1C || text_id != 0) {
                g_world_menu_description_text_id
                    = text_id + g_world_active_grid_menu_cells[g_world_grid_menu_selected_cell].text_offset;
                world_menu_start_description_text_thread((world_menu_description_record_t*)3);
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
        if (g_world_grid_menu_id == 0x20003) {
            if (g_world_grid_menu_selected_cell == 6 && cell == 7) {
                cell = 8;
            }
            if (g_world_grid_menu_selected_cell == 8 && cell == 7) {
                cell = 6;
            }
        }
        if (cell != g_world_grid_menu_selected_cell) {
            g_world_menu_sound_effect_id = MAIN_SFX_CURSOR_MOVE;
            g_world_grid_menu_selected_cell = cell;
        }
    }
    cursor.x = g_world_active_grid_menu_cells[g_world_grid_menu_selected_cell].cursor.x;
    cursor.y = g_world_active_grid_menu_cells[g_world_grid_menu_selected_cell].cursor.y;
    world_menu_set_draw_priority(0x3C);
    if (g_world_active_grid_menu_cells[g_world_grid_menu_selected_cell].vertical != 0) {
        cursor.y++;
        world_menu_draw_animated_vertical_cursor(&cursor, &g_world_grid_menu_cursor_anim, busy);
    } else {
        cursor.x -= 2;
        cursor.y += 6;
        world_menu_draw_animated_cursor(&cursor, &g_world_grid_menu_cursor_anim, busy);
    }
}
