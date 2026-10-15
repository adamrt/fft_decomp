#include "fft/world.h"
#include "psx/types.h"

/* Ask the selected callback for the literal row's icon, or the current
 * scrolling row's icon, then draw it at the command's position. */
menu_instruction_t* world_menu_script_draw_callback_sprite(menu_instruction_t* instruction) {
    menu_icon_source_t* (*handler)(s32);
    menu_icon_source_t* entry;
    s32 index;

    handler = g_world_menu_script_callbacks[instruction->callback_sprite.callback_index].icon;
    if (g_world_menu_use_scroll_position == 0) {
        index = instruction->callback_sprite.row;
    } else {
        index = g_menu_scroll_base_index + g_world_menu_scroll_row_offset;
        if (g_menu_list_scroll_offset < 0) {
            index -= 1;
        }
    }
    entry = handler(index);
    if (entry != 0) {
        g_world_menu_script_sprite_command.sprite.x = instruction->callback_sprite.x;
        g_world_menu_script_sprite_command.sprite.y = instruction->callback_sprite.y;
        /* The icon source stores 16-bit values; the menu script copies their low bytes. */
        g_world_menu_script_sprite_command.sprite.w = entry->w;
        g_world_menu_script_sprite_command.sprite.h = entry->h;
        g_world_menu_script_sprite_command.sprite.u = entry->u;
        g_world_menu_script_sprite_command.sprite.v = entry->v;
        g_world_menu_clut = entry->clut;
        g_world_menu_texture_page = entry->tpage;
        world_menu_script_draw_sprite((menu_instruction_t*)&g_world_menu_script_sprite_command);
    }
    return menu_next_instruction(instruction);
}
