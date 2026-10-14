#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draws the sprite described by a 9-byte command header (x, y, w, h, u, v)
 * and returns the next command. */
menu_instruction_t* world_menu_script_draw_sprite(menu_instruction_t* instruction) {
    RECT rect;
    s32 y;
    s16 step;
    rgb24_t* color;

    if (g_world_menu_use_scroll_position == 0) {
        y = instruction->sprite.sprite.y;
    } else {
        step = g_world_menu_row_height;
        y = step * g_world_menu_scroll_row_offset + instruction->sprite.sprite.y - g_world_menu_scroll_pixel_offset;
        if (g_world_menu_scroll_pixel_offset < 0) {
            y -= step;
        }
    }
    rect.x = instruction->sprite.sprite.x;
    rect.y = y;
    rect.w = instruction->sprite.sprite.w;
    rect.h = instruction->sprite.sprite.h;
    if (instruction->sprite.opcode == MENU_SCRIPT_OP_UNCOLORED_QUAD) {
        color = 0;
    } else {
        color = &g_world_menu_sprite_color;
    }
    world_gfx_enqueue_textured_quad(&rect, instruction->sprite.sprite.u, instruction->sprite.sprite.v, color,
        g_world_menu_semi_trans, g_world_menu_texture_page, g_world_menu_clut, g_world_menu_draw_priority);
    return menu_next_instruction(instruction);
}
