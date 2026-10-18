/* The item list's rearrange mode, swapping a picked entry with the entry under the cursor. */

#include "fft/help.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern s16 g_item_rearrange_picked_cursor;
extern s16 g_item_rearrange_preview_cursor;
extern u16 g_item_rearrange_picked_scroll;
extern u8 g_item_rearrange_anim_frame;

/* Runs item-list rearrangement mode.
 *
 * Confirm swaps the picked entry with the current entry and stores the new order as the
 * category list (confirming on the picked entry itself opens the item action menu instead),
 * while cancel exits without changing the list. A blinking hand marks the picked row and the
 * item-stat preview follows the cursor. Returns the item list state to continue in. */
s32 item_run_rearrange_mode(void) {
    RECT rect;
    s32 unused[2];
    s32 cursor;
    s32 picked;
    s32 item;
    s32 frame;
    u16 cursor_clut;
    u16 shadow_clut;
    s32 scroll_offset;
    s32 row;

    if (g_item_rearrange_initialized == 0) {
        g_item_rearrange_initialized = 1;
        g_item_rearrange_picked_cursor = g_menu_list_selected_index;
        g_item_rearrange_preview_cursor = g_menu_list_selected_index;
        g_item_rearrange_picked_scroll = g_menu_scroll_base_index;
    }
    g_help_message_text_id = -1;
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_item_rearrange_initialized = 0;
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        return MENU_ITEM_LIST_STATE_BROWSE;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        cursor = g_menu_list_selected_index;
        picked = g_item_rearrange_picked_cursor;
        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
        if (cursor == picked) {
            g_item_rearrange_initialized = 0;
            g_item_action_menu.selected_index = 0;
            return MENU_ITEM_LIST_STATE_ACTION;
        }
        item = g_item_list_entries[picked];
        g_item_list_entries[picked] = g_item_list_entries[cursor];
        g_item_list_entries[cursor] = item;
        item_store_category_list(g_menu_selected_slot, g_item_list_entries);
        menu_init_scrollable_list_core(
            g_item_list_entries, g_menu_list_selected_index, g_text_section_pointers[TEXT_SECTION_ITEM_NAMES]);
        menu_set_window_scale_step(10);
        item_collect_stat_deltas(item, &g_unit_panel_selected_unit_stat_summary, &g_item_preview_stat_detail, 0);
        g_unit_panel_preview_stats_thread_params.redraw_request = 1;
        g_item_rearrange_initialized = 0;
        return MENU_ITEM_LIST_STATE_BROWSE;
    }
    scroll_offset = g_menu_scroll_base_index;
    row = g_item_rearrange_picked_cursor - scroll_offset;
    if (row >= 0 && row < 8) {
        frame = g_item_rearrange_anim_frame++;
        if (frame & 1) {
            cursor_clut = g_menu_cursor_mode0_foreground_clut;
            shadow_clut = g_menu_cursor_mode0_background_clut;
        } else {
            cursor_clut = g_menu_cursor_mode1_foreground_clut;
            shadow_clut = g_menu_cursor_mode1_background_clut;
        }
        rect.w = 0x10;
        rect.h = 0x10;
        rect.x = 0;
        rect.y = row * 16 - (s16)(g_menu_list_scroll_offset - 0x5E);
        menu_gfx_enqueue_textured_quad(&rect, 0xA8, 0, 0, 0, 0x5F, cursor_clut, VARIANT_MENU_ITEM_CURSOR_DEPTH);
        rect.x += 2;
        rect.y += 2;
        menu_gfx_enqueue_textured_quad(&rect, 0xB8, 0, 0, 1, 0x5F, shadow_clut, VARIANT_MENU_ITEM_CURSOR_DEPTH);
    }
    if (g_menu_list_selected_index != g_item_rearrange_preview_cursor) {
        if (g_menu_list_entry_count != 0) {
            item = g_item_list_entries[g_menu_list_selected_index];
        } else {
            item = ITEM_ID_NOTHING;
            /* Hides the constant so the (s16) extension at the call is still emitted (sll/sra). */
            __asm__("" : "=r"(item) : "0"(item));
        }
        g_item_rearrange_preview_cursor = g_menu_list_selected_index;
        item_collect_stat_deltas(item, &g_unit_panel_selected_unit_stat_summary, &g_item_preview_stat_detail, 0);
        g_unit_panel_preview_stats_thread_params.redraw_request = 1;
    }
    return MENU_ITEM_LIST_STATE_REARRANGE;
}
