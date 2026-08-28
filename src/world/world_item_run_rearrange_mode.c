#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Runs item-list rearrangement mode.
 *
 * Confirm swaps the picked entry with the current entry, while cancel exits
 * without changing the list. A blinking hand marks the picked row and the
 * item-stat preview follows the cursor. */
s32 world_item_run_rearrange_mode(void) {
    RECT rect;
    volatile s32 unused[2];
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
    if (g_world_input_primary_repeat & PSX_PAD_CROSS) {
        g_world_item_rearrange_initialized = 0;
        g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
        return 0;
    }
    if (g_world_input_primary_repeat & PSX_PAD_CIRCLE) {
        cursor = g_world_menu_cursor_position;
        picked = (s16)g_world_item_rearrange_picked_cursor;
        g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
        if (cursor == picked) {
            g_world_item_rearrange_initialized = 0;
            g_world_item_action_menu.cursor = 0;
            return 2;
        }
        item = g_world_menu_entry_ids[picked];
        g_world_menu_entry_ids[picked] = g_world_menu_entry_ids[cursor];
        g_world_menu_entry_ids[cursor] = item;
        world_item_finalize_sorted_list(g_world_item_menu_category, g_world_menu_entry_ids);
        world_menu_init_scrollable_list_core(
            g_world_menu_entry_ids, g_world_menu_cursor_position, g_world_text_item_names);
        world_menu_set_window_scale_step(10);
        world_item_populate_stat_preview(
            item, &g_world_selected_unit_stat_summary, &g_world_item_preview_stat_detail, 0);
        g_world_preview_stats_thread_params.redraw_request = 1;
        g_world_item_rearrange_initialized = 0;
        return 0;
    }
    scroll_offset = g_world_menu_scroll_offset;
    row = (s16)g_world_item_rearrange_picked_cursor - scroll_offset;
    if ((u32)row < 8) {
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
        world_gfx_enqueue_textured_quad(&rect, 0xA8, 0, 0, 0, 0x5F, tile, 0x14);
        rect.x += 2;
        rect.y += 2;
        world_gfx_enqueue_textured_quad(&rect, 0xB8, 0, 0, 1, 0x5F, tile2, 0x14);
    }
    if (g_world_menu_cursor_position != (s16)g_world_item_rearrange_preview_cursor) {
        if (g_world_menu_option_count != 0) {
            item = g_world_menu_entry_ids[g_world_menu_cursor_position];
        } else {
            item = ITEM_ID_NOTHING;
            /* Hides the constant so the (s16) extension at the call is still emitted (sll/sra). */
            __asm__("" : "=r"(item) : "0"(item));
        }
        g_world_item_rearrange_preview_cursor = g_world_menu_cursor_position;
        world_item_populate_stat_preview(
            (s16)item, &g_world_selected_unit_stat_summary, &g_world_item_preview_stat_detail, 0);
        g_world_preview_stats_thread_params.redraw_request = 1;
    }
    return 1;
}
