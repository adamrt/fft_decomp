#include "fft/event_equip.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Runs value-list rearrangement mode.
 *
 * Confirm swaps the picked entry with the current entry, while cancel exits
 * without changing the list. A blinking hand marks the picked row and the
 * item-stat preview follows the cursor. */
s32 equip_item_run_rearrange_mode(void) {
    RECT rect;
    s32 cursor;
    s32 picked;
    s16 id;
    s32 value; /* the 32-bit pad value for the confirm test, then an item id */
    s32 frame;
    u16 tile;
    u16 tile2;
    s32 scroll_offset;
    s32 row;

    if (g_equip_rearrange_active == 0) {
        g_equip_rearrange_active = 1;
        g_equip_rearrange_picked_cursor = g_equip_menu_selected_list_index;
        g_equip_rearrange_preview_cursor = g_equip_menu_selected_list_index;
        g_equip_rearrange_picked_scroll = g_menu_scroll_base_index;
    }
    g_equip_text_help_message_id = -1;
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_equip_rearrange_active = 0;
        g_equip_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return MENU_ITEM_LIST_STATE_BROWSE;
    }
    value = g_menu_input_primary_repeat;
    if (value & PSX_PAD_CIRCLE) {
        cursor = g_equip_menu_selected_list_index;
        picked = g_equip_rearrange_picked_cursor;
        g_equip_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        if (cursor == picked) {
            g_equip_rearrange_active = 0;
            g_equip_item_action_menu.selected_index = 0;
            return MENU_ITEM_LIST_STATE_ACTION;
        }
        id = g_item_list_entries[picked];
        value = id;
        g_item_list_entries[picked] = g_item_list_entries[cursor];
        g_item_list_entries[cursor] = id;
        equip_item_store_category_list(g_equip_selected_slot, g_item_list_entries);
        equip_menu_init_scrollable_list_core(
            g_item_list_entries, g_equip_menu_selected_list_index, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        equip_gfx_set_transition_frame(10);
        item_collect_stat_deltas_variant(
            value, &g_equip_selected_unit_stat_summary, &g_equip_item_preview_stat_detail, 0);
        g_equip_item_numeric_panel_redraw = 1;
        g_equip_rearrange_active = 0;
        return MENU_ITEM_LIST_STATE_BROWSE;
    }
    scroll_offset = g_menu_scroll_base_index;
    row = g_equip_rearrange_picked_cursor - scroll_offset;
    if (row >= 0 && row < 8) {
        frame = g_equip_rearrange_anim_frame++;
        if (frame & 1) {
            tile = g_menu_cursor_mode0_foreground_clut;
            tile2 = g_menu_cursor_mode0_background_clut;
        } else {
            tile = g_menu_cursor_mode1_foreground_clut;
            tile2 = g_menu_cursor_mode1_background_clut;
        }
        rect.w = 0x10;
        rect.h = 0x10;
        rect.x = 0;
        rect.y = row * 16 + 0x5E - g_menu_list_scroll_offset;
        menu_gfx_enqueue_textured_quad(&rect, 0xA8, 0, 0, 0, 0x5F, tile, 0x1F);
        rect.x += 2;
        rect.y += 2;
        menu_gfx_enqueue_textured_quad(&rect, 0xB8, 0, 0, 1, 0x5F, tile2, 0x1F);
    }
    if (g_equip_menu_selected_list_index != g_equip_rearrange_preview_cursor) {
        if (g_equip_menu_list_entry_count != 0) {
            value = g_item_list_entries[g_equip_menu_selected_list_index];
        } else {
            value = 0;
        }
        g_equip_rearrange_preview_cursor = g_equip_menu_selected_list_index;
        item_collect_stat_deltas_variant(
            value, &g_equip_selected_unit_stat_summary, &g_equip_item_preview_stat_detail, 0);
        g_equip_item_numeric_panel_redraw = 1;
    }
    return MENU_ITEM_LIST_STATE_REARRANGE;
}
