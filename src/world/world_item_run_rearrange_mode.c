#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Runs item-list rearrangement mode.
 *
 * Confirm swaps the picked entry with the current entry, while cancel exits
 * without changing the list. A blinking hand marks the picked row and the
 * item-stat preview follows the cursor. */
s32 world_item_run_rearrange_mode(void) {
    RECT rect;
    s32 unused[2];
    s32 cursor;
    s32 picked;
    s32 item;
    s32 frame;
    u16 tile;
    u16 tile2;
    s32 scroll_offset;
    s32 row;

    if (g_world_item_rearrange_initialized == 0) {
        g_world_item_rearrange_initialized = 1;
        g_world_item_rearrange_picked_cursor = g_world_menu_cursor_position;
        g_world_item_rearrange_preview_cursor = g_world_menu_cursor_position;
        g_world_item_rearrange_picked_scroll = g_world_menu_scroll_offset;
    }
    g_world_menu_description_text_id = -1;
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_world_item_rearrange_initialized = 0;
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return MENU_ITEM_LIST_STATE_BROWSE;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        cursor = g_world_menu_cursor_position;
        picked = g_world_item_rearrange_picked_cursor;
        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        if (cursor == picked) {
            g_world_item_rearrange_initialized = 0;
            g_world_item_action_menu.selected_index = 0;
            return MENU_ITEM_LIST_STATE_ACTION;
        }
        item = g_item_list_entries[picked];
        g_item_list_entries[picked] = g_item_list_entries[cursor];
        g_item_list_entries[cursor] = item;
        item_store_category_list(g_world_item_menu_category, g_item_list_entries);
        world_menu_init_scrollable_list_core(
            g_item_list_entries, g_world_menu_cursor_position, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        world_menu_set_window_scale_step(10);
        item_collect_stat_deltas_variant(
            item, &g_world_selected_unit_stat_summary, &g_world_item_preview_stat_detail, 0);
        g_world_preview_stats_thread_params.redraw_request = 1;
        g_world_item_rearrange_initialized = 0;
        return MENU_ITEM_LIST_STATE_BROWSE;
    }
    scroll_offset = g_world_menu_scroll_offset;
    row = g_world_item_rearrange_picked_cursor - scroll_offset;
    if (row >= 0 && row < 8) {
        frame = g_world_item_rearrange_anim_frame++;
        if (frame & 1) {
            tile = g_world_menu_clut_front;
            tile2 = g_world_menu_clut_back;
        } else {
            tile = g_world_menu_clut_front_alt;
            tile2 = g_world_menu_clut_back_alt;
        }
        rect.w = 0x10;
        rect.h = 0x10;
        rect.x = 0;
        rect.y = row * 16 - (s16)(g_world_menu_scroll_pixel_offset - 0x5E);
        menu_gfx_enqueue_textured_quad(&rect, 0xA8, 0, 0, 0, 0x5F, tile, 0x14);
        rect.x += 2;
        rect.y += 2;
        menu_gfx_enqueue_textured_quad(&rect, 0xB8, 0, 0, 1, 0x5F, tile2, 0x14);
    }
    if (g_world_menu_cursor_position != g_world_item_rearrange_preview_cursor) {
        if (g_world_menu_option_count != 0) {
            item = g_item_list_entries[g_world_menu_cursor_position];
        } else {
            item = ITEM_ID_NOTHING;
            /* Hides the constant so the (s16) extension at the call is still emitted (sll/sra). */
            __asm__("" : "=r"(item) : "0"(item));
        }
        g_world_item_rearrange_preview_cursor = g_world_menu_cursor_position;
        item_collect_stat_deltas_variant(
            item, &g_world_selected_unit_stat_summary, &g_world_item_preview_stat_detail, 0);
        g_world_preview_stats_thread_params.redraw_request = 1;
    }
    return MENU_ITEM_LIST_STATE_REARRANGE;
}
