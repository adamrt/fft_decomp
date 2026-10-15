#include "fft/world.h"

/* Switch the active item category and restore its saved list cursor. */
void world_menu_switch_item_category(s32 input) {
    s16 x;
    s16 y;
    s32 category;
    s32 cursor_slot;
    s16 item;
    s32 description;

    category = world_menu_update_horizontal_selection_and_mark_change(
        5, WORLD_MENU_CURSOR_SHOP_ITEM_TAB, input, MAIN_SFX_PAGE_SWITCH);
    cursor_slot = g_world_shop_item_category;
    if (cursor_slot != category) {
        if (g_world_shop_menu_step != WORLD_SHOP_STEP_BUY_LIST
            && g_world_shop_menu_step != WORLD_SHOP_STEP_FUR_BUY_LIST) {
            cursor_slot += 5;
        }
        menu_set_selection_record(
            cursor_slot, g_world_menu_cursor_position, g_world_menu_scroll_offset, g_item_list_entries);
        g_world_shop_item_category = category;
        if (g_world_shop_menu_step == WORLD_SHOP_STEP_BUY_LIST
            || g_world_shop_menu_step == WORLD_SHOP_STEP_FUR_BUY_LIST) {
            world_shop_build_item_list(-1, g_world_menu_context_id,
                category != 4 ? (category + 1) : ITEM_LIST_FILTER_OTHER, g_item_list_entries, ITEM_LIST_MODE_ALL);
            cursor_slot = g_world_shop_item_category;
        } else {
            world_item_build_category_list(-1, g_world_shop_item_category + 1,
                category != 4 ? category + 1 : ITEM_LIST_FILTER_OTHER, g_item_list_entries, ITEM_LIST_MODE_ALL);
            cursor_slot = g_world_shop_item_category + 5;
        }
        world_menu_get_selection_record(cursor_slot, &x, &y, g_item_list_entries);
        world_menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        world_menu_set_window_scale_step(10);
    }
    world_menu_draw_row_window_and_cursor(1, g_world_shop_item_category, g_menu_input_lock_mode);
    item = g_item_list_entries[g_world_menu_cursor_position];
    if (item != -1) {
        description = (item & WORLD_ITEM_ID_MASK) + 0x6800;
    } else {
        description = 0;
    }
    g_world_menu_description_text_id = description;
}
