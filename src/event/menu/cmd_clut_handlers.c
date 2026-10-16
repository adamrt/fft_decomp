/* CLUT, layout CLUT and texture page command handlers. */

#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"

/* Menu script command: set the sprite CLUT from the command's VRAM coordinates and return the next
 * command. */
menu_instruction_t* menu_cmd_set_clut_handler(menu_instruction_t* data) {
    g_menu_gfx_clut_id
        = GetClut(data->fixed_clut.x_div_16 << 4, data->fixed_clut.y_low | (data->fixed_clut.y_high << 8));
    return menu_next_instruction(data);
}

/* Set the sprite CLUT from the command's normal or compact-layout coordinates
 * and return the next command. */
menu_instruction_t* menu_cmd_set_layout_clut_handler(menu_instruction_t* command) {
    if (g_menu_use_alternate_palette != 0) {
        g_menu_gfx_clut_id = GetClut(command->clut.alternate_x_div_16 << 4,
            command->clut.alternate_y_low | (command->clut.alternate_y_high << 8));
    } else {
        g_menu_gfx_clut_id = GetClut(
            command->clut.normal_x_div_16 << 4, command->clut.normal_y_low | (command->clut.normal_y_high << 8));
    }
    return menu_next_instruction(command);
}

/* Select the texture page and blend rate. */
menu_instruction_t* menu_cmd_set_tpage_handler(menu_instruction_t* data) {
    g_menu_gfx_texture_page = GetTPage(data->texture_page.depth, data->texture_page.y_blend >> 4,
        data->texture_page.x_div_16 << 4, data->texture_page.y_blend << 8);
    return menu_next_instruction(data);
}
