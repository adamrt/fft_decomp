/* The menu command that selects the normal or alternate sprite color. */

#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Select the normal or alternate sprite color. */
menu_instruction_t* menu_cmd_set_draw_color_handler(menu_instruction_t* data) {
    u8 blue;
    if (g_menu_use_alternate_palette != 0) {
        g_menu_text_color.r = data->sprite_color.alternate.r;
        g_menu_text_color.g = data->sprite_color.alternate.g;
        blue = data->sprite_color.alternate.b;
    } else {
        g_menu_text_color.r = data->sprite_color.normal.r;
        g_menu_text_color.g = data->sprite_color.normal.g;
        blue = data->sprite_color.normal.b;
    }
    g_menu_text_color.b = blue;
    return menu_next_instruction(data);
}
