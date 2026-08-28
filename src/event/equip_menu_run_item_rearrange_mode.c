#include "fft/battle_text.h"
#include "fft/equip.h"
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
s32 equip_menu_run_item_rearrange_mode(void) {
    RECT rect;
    volatile s32 unused[2];
    s32 cursor;
    s32 picked;
    s32 g_main_item_item_flags;
    s32 frame;
    u16 tile;
    u16 tile2;
    s32 scroll_offset;
    s32 row;

    if (g_equip_rearrange_active == 0) {
        g_equip_rearrange_active = 1;
        g_equip_rearrange_picked_cursor = (s16)g_equip_menu_selected_list_index;
        g_equip_rearrange_preview_cursor = (s16)g_equip_menu_selected_list_index;
        g_equip_rearrange_picked_scroll = g_equip_menu_scroll_base_index;
    }
    g_equip_text_help_message_id = -1;
    if (g_equip_input_primary_repeat & PSX_PAD_CROSS) {
        g_equip_rearrange_active = 0;
        g_equip_sound_queued_effect_id = MAIN_SFX_CANCEL;
        return 0;
    }
    if (g_equip_input_primary_repeat & PSX_PAD_CIRCLE) {
        cursor = (s16)g_equip_menu_selected_list_index;
        picked = (s16)g_equip_rearrange_picked_cursor;
        g_equip_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        if (cursor == picked) {
            g_equip_rearrange_active = 0;
            g_equip_item_action_menu.selected_index = 0;
            return 2;
        }
        g_main_item_item_flags = g_equip_item_list_entries[picked];
        g_equip_item_list_entries[picked] = g_equip_item_list_entries[cursor];
        g_equip_item_list_entries[cursor] = g_main_item_item_flags;
        equip_item_store_category_list(g_equip_selected_slot, g_equip_item_list_entries);
        equip_menu_init_scrollable_list_core(
            g_equip_item_list_entries, (s16)g_equip_menu_selected_list_index, g_battle_text_section_pointers[7]);
        equip_gfx_set_transition_frame(10);
        equip_collect_item_stat_deltas(
            g_main_item_item_flags, &g_equip_selected_unit_stat_summary, &g_equip_item_preview_stat_detail, 0);
        g_equip_item_numeric_panel_redraw = 1;
        g_equip_rearrange_active = 0;
        return 0;
    }
    scroll_offset = g_equip_menu_scroll_base_index;
    row = (s16)g_equip_rearrange_picked_cursor - scroll_offset;
    if ((u32)row < 8) {
        frame = g_equip_rearrange_anim_frame++;
        if (frame & 1) {
            tile = g_equip_menu_cursor_mode0_foreground_clut;
            tile2 = g_equip_menu_cursor_mode0_background_clut;
        } else {
            tile = g_equip_menu_cursor_mode1_foreground_clut;
            tile2 = g_equip_menu_cursor_mode1_background_clut;
        }
        rect.w = 0x10;
        rect.h = 0x10;
        rect.x = 0;
        rect.y = row * 16 - (s16)(g_equip_menu_list_scroll_offset - 0x5E);
        equip_gfx_enqueue_textured_quad(&rect, 0xA8, 0, 0, 0, 0x5F, tile, 0x1F);
        rect.x += 2;
        rect.y += 2;
        equip_gfx_enqueue_textured_quad(&rect, 0xB8, 0, 0, 1, 0x5F, tile2, 0x1F);
    }
    if ((s16)g_equip_menu_selected_list_index != (s16)g_equip_rearrange_preview_cursor) {
        if (g_equip_menu_list_entry_count != 0) {
            g_main_item_item_flags = g_equip_item_list_entries[(s16)g_equip_menu_selected_list_index];
        } else {
            g_main_item_item_flags = 0;
            /* Emits nothing; hides the constant so the (s16) conversion below
             * keeps the target's sll/sra pair at the join. */
            __asm__("" : "=r"(g_main_item_item_flags) : "0"(g_main_item_item_flags));
        }
        g_equip_rearrange_preview_cursor = (s16)g_equip_menu_selected_list_index;
        equip_collect_item_stat_deltas(
            (s16)g_main_item_item_flags, &g_equip_selected_unit_stat_summary, &g_equip_item_preview_stat_detail, 0);
        g_equip_item_numeric_panel_redraw = 1;
    }
    return 1;
}
