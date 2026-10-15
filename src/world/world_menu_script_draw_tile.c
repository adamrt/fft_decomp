#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draws the rectangle described by a 6-byte command header (x, y, w, h)
 * followed by its payload, and returns the next command. */
menu_instruction_t* world_menu_script_draw_tile(menu_instruction_t* instruction) {
    RECT rect;
    s32 y;
    s16 step;

    if (g_world_menu_use_scroll_position == 0) {
        y = instruction->tile.y;
    } else {
        step = g_world_menu_row_height;
        y = step * g_world_menu_scroll_row_offset + instruction->tile.y + g_menu_list_scroll_offset;
        if (g_menu_list_scroll_offset < 0) {
            y -= step;
        }
    }
    rect.x = instruction->tile.x;
    rect.y = y;
    rect.w = instruction->tile.width;
    rect.h = instruction->tile.height;
    gfx_enqueue_tile(&rect, &instruction->tile.color, g_menu_gfx_semitransparency, g_menu_gfx_otag_index);
    return menu_next_instruction(instruction);
}
