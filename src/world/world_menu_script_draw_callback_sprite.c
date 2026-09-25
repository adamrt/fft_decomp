#include "fft/world.h"
#include "psx/types.h"

u8* world_menu_script_draw_callback_sprite(u8* cmd) {
    world_item_icon_source_t* (*handler)(s32);
    world_item_icon_source_t* entry;
    u8* dst;
    s32 index;

    handler = (world_item_icon_source_t * (*)(s32)) g_world_menu_script_callbacks[cmd[2]];
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
        /* The icon source stores halfwords; the menu script copies their low bytes. */
        dst[2] = *(u8*)&entry->w;
        dst[3] = *(u8*)&entry->h;
        dst[4] = *(u8*)&entry->u;
        dst[5] = *(u8*)&entry->v;
        g_world_menu_clut = entry->clut;
        g_world_menu_texture_page = entry->tpage;
        world_menu_script_draw_sprite(dst - 3);
    }
    return cmd + cmd[1];
}
