#include "fft/world.h"
#include "psx/types.h"

u8* world_menu_script_draw_callback_sprite(u8* cmd) {
    void* (*handler)(s32);
    u8* entry;
    u8* dst;
    s32 index;

    handler = (void* (*)(s32))g_world_menu_script_callbacks[cmd[2]];
    if (g_world_menu_use_scroll_position == 0) {
        index = cmd[3];
    } else {
        index = g_world_menu_scroll_offset + g_world_menu_scroll_row_offset;
        if (g_world_menu_scroll_pixel_offset < 0) {
            index -= 1;
        }
    }
    entry = handler(index);
    if (entry != 0) {
        dst = &g_world_menu_script_sprite_command[3];
        /* Keeps dst in one base register instead of absolute stores. */
        __asm__("" : "=r"(dst) : "0"(dst));
        dst[0] = cmd[4];
        dst[1] = cmd[5];
        dst[2] = entry[4];
        dst[3] = entry[6];
        dst[4] = entry[0];
        dst[5] = entry[2];
        g_world_menu_clut = *(u16*)(entry + 8);
        g_world_menu_texture_page = *(u16*)(entry + 10);
        world_menu_script_draw_sprite(dst - 3);
    }
    return cmd + cmd[1];
}
