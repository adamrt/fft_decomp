#include "fft/equip.h"
#include "psx/types.h"

/* Stream command: set the sprite RGB; the compact layout uses the second
   colour triple of the command. */
u8* equip_cmd_set_draw_color_handler(u8* cmd) {
    u8 blue;
    if (g_equip_text_compact_layout != 0) {
        g_equip_gfx_sprite_color[0] = cmd[5];
        g_equip_gfx_sprite_color[1] = cmd[6];
        blue = cmd[7];
    } else {
        g_equip_gfx_sprite_color[0] = cmd[2];
        g_equip_gfx_sprite_color[1] = cmd[3];
        blue = cmd[4];
    }
    g_equip_gfx_sprite_color[2] = blue;
    return cmd + cmd[1];
}
