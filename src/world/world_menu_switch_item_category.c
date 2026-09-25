#include "fft/world.h"

/* Switch the active item category and restore its saved list cursor. */
void world_menu_switch_item_category(s32 input) {
    s16 x;
    s16 y;
    s32 category;
    s32 cursor_slot;
    s16 item;
    s32 description;

    category = world_menu_step_cursor_with_sound_2(5, 0, input, 6);
    cursor_slot = g_world_shop_item_category;
    if (cursor_slot != category) {
        if (g_world_shop_menu_step != 1 && g_world_shop_menu_step != 15) {
            cursor_slot += 5;
        }
        world_gfx_set_sprite_slot(
            cursor_slot, g_world_menu_cursor_position, g_world_menu_scroll_offset, (u16*)g_world_menu_entry_ids);
        g_world_shop_item_category = category;
        if (g_world_shop_menu_step == 1 || g_world_shop_menu_step == 15) {
            world_shop_build_item_list(-1, g_world_shop_id, category != 4 ? (s16)(category + 1) : 7,
                (world_item_list_entry_t*)g_world_menu_entry_ids, 0);
            cursor_slot = g_world_shop_item_category;
        } else {
            /* The definition's u16/s8/u8 parameter conversions would change this call's codegen. */
            ((void (*)(s32, s32, s8, s16*, s32))world_item_build_category_list)(
                -1, g_world_shop_item_category + 1, category != 4 ? (s8)(category + 1) : 7, g_world_menu_entry_ids, 0);
            cursor_slot = g_world_shop_item_category + 5;
        }
        world_gfx_get_sprite_slot(cursor_slot, &x, &y, g_world_menu_entry_ids);
        world_menu_init_scrollable_list(g_world_menu_entry_ids, x, y, g_world_text_item_names);
        world_menu_set_window_scale_step(10);
    }
    world_menu_draw_row_window_and_cursor(1, g_world_shop_item_category, g_world_thread_task_active);
    item = g_world_menu_entry_ids[g_world_menu_cursor_position];
    if (item != -1) {
        description = (item & WORLD_ITEM_ID_MASK) + 0x6800;
    } else {
        description = 0;
    }
    g_world_menu_description_text_id = description;
}
