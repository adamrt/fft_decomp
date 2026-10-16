/* The menu command that draws a textured quad at its own position or on the current list row. */

#include "fft/gfx.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draw a textured quad at its literal position or current list row. */
menu_instruction_t* menu_opcode_04_draw_textured_quad(menu_instruction_t* cmd) {
    RECT rect;
    s32 y;
    rgb24_t* color;

    if (g_menu_list_row_mode == 0) {
        y = cmd->sprite.sprite.y;
    } else {
        y = g_menu_list_row_height * g_menu_list_row_index + cmd->sprite.sprite.y - g_menu_list_scroll_offset;
        if (g_menu_list_scroll_offset < 0) {
            y -= g_menu_list_row_height;
        }
    }

    rect.x = cmd->sprite.sprite.x;
    rect.y = y;
    rect.w = cmd->sprite.sprite.w;
    rect.h = cmd->sprite.sprite.h;

    color = &g_menu_text_color;
    if (cmd->header.opcode == MENU_SCRIPT_OP_UNCOLORED_QUAD) {
        color = 0;
    }

    menu_gfx_enqueue_textured_quad(&rect, cmd->sprite.sprite.u, cmd->sprite.sprite.v, color,
        g_menu_gfx_semitransparency, g_menu_gfx_texture_page, g_menu_gfx_clut_id, g_menu_gfx_otag_index);
    return menu_next_instruction(cmd);
}
