#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/pad.h"

/* Moves and draws the name-entry keyboard cursor.
 *
 * Row 0 is the six special keys; character rows have 11 columns. Crossing
 * between them remaps the column through
 * g_world_name_entry_special_to_char_column/g_world_name_entry_char_to_special_column. Left/right wrap within the row,
 * and a column change requests cursor sound 3. */
void world_name_update_entry_cursor(void) {
    world_menu_point_t cursor;
    s32 previous;
    u32 input;

    previous = g_world_name_entry_cursor_index;
    if (g_world_menu_cursor_position > 0) {
        if (g_world_name_entry_list_cursor == 0) {
            g_world_name_entry_cursor_index = g_world_name_entry_special_to_char_column[previous];
            previous = g_world_name_entry_cursor_index;
        }
        if (g_world_input_primary_repeat & PSX_PAD_LEFT) {
            if (g_world_name_entry_cursor_index == 0) {
                g_world_name_entry_cursor_index = 10;
            } else {
                g_world_name_entry_cursor_index--;
            }
        }
        if (g_world_input_primary_repeat & PSX_PAD_RIGHT) {
            g_world_name_entry_cursor_index = (g_world_name_entry_cursor_index + 1) % 11;
        }
        g_world_name_entry_list_cursor = g_world_menu_cursor_position;
        cursor.x = g_world_name_entry_cursor_index * 20 + 2;
        cursor.y = (g_world_name_entry_list_cursor - g_world_menu_scroll_offset) * 16 + 0x80;
    } else if (g_world_menu_cursor_position == 0) {
        if (g_world_name_entry_list_cursor != 0) {
            g_world_name_entry_cursor_index = g_world_name_entry_char_to_special_column[previous];
            previous = g_world_name_entry_cursor_index;
        }
        input = g_world_input_primary_repeat;
        if (input & PSX_PAD_RIGHT) {
            g_world_name_entry_cursor_index = (g_world_name_entry_cursor_index + 1) % 6;
        }
        if (input & PSX_PAD_LEFT) {
            if (g_world_name_entry_cursor_index == 0) {
                g_world_name_entry_cursor_index = 5;
            } else {
                g_world_name_entry_cursor_index--;
            }
        }
        cursor.x = g_world_name_entry_cursor_x_positions[g_world_name_entry_cursor_index];
        g_world_name_entry_list_cursor = 0;
        cursor.y = 0x80;
    }
    if (g_world_name_entry_cursor_index != previous) {
        g_world_menu_sound_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
    world_menu_set_draw_priority(0x28);
    world_menu_draw_animated_cursor(&cursor, &g_world_name_entry_key_cursor_anim, g_world_thread_task_active);
}
