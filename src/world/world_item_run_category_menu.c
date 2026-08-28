/* Item-list menu thread for the formation item screen: builds the item
 * category list on entry, steps the cursor, and refreshes the stat preview.
 *
 * The empty asm before the category reload is load-bearing twice over. Its
 * memory clobber keeps the target's reload of the category byte after the
 * store: the store and the reload name two symbols at one address, so alias
 * analysis sees no dependence and the scheduler would hoist the load above
 * the store (naming one symbol for both instead lets cse forward the stored
 * value, where the target reloads). Its "r"(four) operand materialises the
 * `!= 4` compare constant before the store pair, as the target does.
 */
#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/pad.h"

extern world_order_menu_entry_t g_world_formation_panel_windows[];

s32 world_item_run_category_menu(void) {
    s16 x;
    s16 y;
    s32 input;
    u8 category;
    s32 selected;
    s32 description;

    if (*(s8*)&g_world_item_category_menu_initialized == 0) {
        g_world_item_category_menu_initialized = 1;
        world_script_set_vsync_mode_and_event_speed(2);
        category = g_world_item_menu_category;
        g_world_item_category_previewed_cursor = -1;
        /* The definition's u16/s8/u8 parameter conversions would change this call's codegen. */
        ((void (*)(s32, s32, s8, s16*, s32))world_item_build_category_list)(g_world_formation_selected_unit_index,
            category + 1, g_world_item_category_view != 4 ? (s8)(category + 1) : 7, g_world_menu_entry_ids, 0);
        world_gfx_get_sprite_slot(g_world_item_menu_category, &x, &y, g_world_menu_entry_ids);
        world_menu_init_scrollable_list(g_world_menu_entry_ids, x, y, g_world_text_item_names);
        g_world_menu_script_callbacks[0] = world_menu_get_entry_item_count_2;
        g_world_menu_script_callbacks[1] = world_menu_get_entry_item_count;
        g_world_menu_aux_callback = (s32 (*)(void))world_item_build_type_icon_source;
        g_world_menu_script_callbacks[3] = (s32 (*)(s32))world_menu_get_entry_item_icon_source;
        g_world_menu_script_callbacks[4] = (s32 (*)(s32))world_menu_get_display_value_1;
        g_world_menu_script_callbacks[5] = (s32 (*)(s32))world_menu_get_display_value_2;
        g_world_menu_script_callbacks[6] = (s32 (*)(s32))world_menu_get_value_1;
        g_world_menu_script_callbacks[7] = (s32 (*)(s32))world_menu_get_value_2;
    }
    world_menu_toggle_preview_stats_window(1);
    if (g_world_menu_option_count != 0) {
        description = g_world_menu_entry_ids[g_world_menu_cursor_position] + 0x6800;
    } else {
        description = -1;
    }
    input = g_world_input_primary_repeat;
    g_world_menu_description_text_id = description;
    if (input & PSX_PAD_CROSS) {
        world_gfx_set_sprite_slot(g_world_item_menu_category, g_world_menu_cursor_position, g_world_menu_scroll_offset,
            (u16*)g_world_menu_entry_ids);
        g_world_item_category_menu_initialized = 0;
        return -1;
    }
    selected = world_menu_step_cursor_with_sound_2(5, 7, input, 6);
    if (selected != g_world_item_menu_category) {
        s32 four = 4;

        world_gfx_set_sprite_slot(g_world_item_menu_category, g_world_menu_cursor_position, g_world_menu_scroll_offset,
            (u16*)g_world_menu_entry_ids);

        g_world_item_category_view = selected;
        __asm__ volatile("" : : "r"(four) : "memory");
        category = g_world_item_menu_category;
        /* The definition's u16/s8/u8 parameter conversions would change this call's codegen. */
        ((void (*)(s32, s32, s8, s16*, s32))world_item_build_category_list)(g_world_formation_selected_unit_index,
            category + 1, g_world_item_category_view != four ? (s8)(category + 1) : 7, g_world_menu_entry_ids, 0);
        g_world_item_category_previewed_cursor = -1;
        world_gfx_get_sprite_slot(g_world_item_menu_category, &x, &y, g_world_menu_entry_ids);
        world_menu_init_scrollable_list(g_world_menu_entry_ids, x, y, g_world_text_item_names);
        world_menu_set_window_scale_step(10);
    } else if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (g_world_menu_option_count != 0) {
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
            world_gfx_set_sprite_slot(
                selected, g_world_menu_cursor_position, g_world_menu_scroll_offset, (u16*)g_world_menu_entry_ids);
            return 1;
        }
        g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
    } else if (g_world_input_primary_repeat & PSX_PAD_TRIANGLE) {
        g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
        if (world_count_item_equipped_by_party(g_world_menu_entry_ids[g_world_menu_cursor_position]) != 0) {
            g_world_item_category_menu_initialized = 0;
            world_gfx_set_sprite_slot(g_world_item_menu_category, g_world_menu_cursor_position,
                g_world_menu_scroll_offset, (u16*)g_world_menu_entry_ids);
            return 6;
        }
        g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
    }
    if (g_world_menu_cursor_position != g_world_item_category_previewed_cursor) {
        if (g_world_menu_option_count != 0) {
            selected = g_world_menu_entry_ids[g_world_menu_cursor_position];
        } else {
            selected = 0;
        }
        g_world_item_category_previewed_cursor = g_world_menu_cursor_position;
        world_item_populate_stat_preview(
            (s16)selected, &g_world_selected_unit_stat_summary, &g_world_item_preview_stat_detail, 0);
        g_world_formation_panel_windows[0].enabled = 1;
        g_world_formation_panel_windows[7].enabled = 1;
    }
    return 0;
}
