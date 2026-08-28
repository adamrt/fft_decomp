#include "fft/equip.h"
#include "psx/types.h"

/* Enqueue a draw-mode primitive for the command's texture page and return the
 * next command. */
u8* equip_cmd_enqueue_draw_mode_handler(u8* command) {
    s32 tpage = GetTPage(0, command[2], 0x100, 0);
    equip_gfx_enqueue_draw_mode(0, 0, tpage & 0xFFFF, 0, g_equip_gfx_sprite_ot_index);
    return command + command[1];
}
