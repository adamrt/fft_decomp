/* Image loading, screen init and the unit status and equipment thread toggles. */

#include "fft/event_equip.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

void equip_menu_load_images_and_reset_lists(void) {
    RECT rect;
    u16 buf1[4];
    u16 buf2[12];

    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    gfx_load_image_and_wait(&rect, g_menu_glyph_sheet->pixels);
    rect.x = 0x100;
    rect.y = 0xFA;
    rect.w = 0x40;
    rect.h = 4;
    gfx_load_image_and_wait(&rect, g_equip_menu_palette_image);
    rect.x = 0x3C0;
    rect.y = 0x1F0;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, buf1);
    buf2[0] = 0;
    rect.x = 0x120;
    rect.y = 0xFE;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, buf1);
    rect.x = 0x100;
    rect.y = 0xFE;
    rect.w = 0xC;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, buf2);
    rect.x = 0x3C0;
    rect.y = 0x1F4;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, buf1);
    buf2[0] = 0;
    rect.x = 0x130;
    rect.y = 0xFE;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, buf1);
    rect.x = 0x110;
    rect.y = 0xFE;
    rect.w = 0xC;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, buf2);
    menu_clear_selection_values();
    g_menu_help_message_text_id = 0;
    equip_menu_reset_selection_indices();
    equip_item_build_inventory_totals();
    item_rebuild_category_list(0);
    item_rebuild_category_list(1);
    item_rebuild_category_list(2);
    item_rebuild_category_list(3);
    item_rebuild_category_list(4);
    menu_clear_selection_records();
}

/* Size the primitive pools, load the unit `battle_id` and start the equipment screen threads. */
void equip_menu_init_screen(s32 battle_id) {
    g_gfx_otag_length = 0x40;
    g_gfx_poly_ft4_capacity = 0x190;
    g_gfx_line_f2_capacity = 0xA;
    g_gfx_tile_capacity = 0xA;
    g_equip_gfx_draw_move_capacity = 0x14;
    g_equip_gfx_draw_area_capacity = 6;
    g_gfx_draw_mode_capacity = 0x1E;
    g_gfx_poly_f3_capacity = 0;
    g_gfx_poly_ft3_capacity = 0;
    g_gfx_poly_f4_capacity = 0;
    g_gfx_poly_g3_capacity = 0;
    g_gfx_poly_gt3_capacity = 0;
    g_gfx_poly_g4_capacity = 0;
    g_gfx_poly_gt4_capacity = 0;
    g_gfx_line_f3_capacity = 0;
    g_gfx_line_f4_capacity = 0;
    g_equip_gfx_line_g2_capacity = 0;
    g_equip_gfx_line_g3_capacity = 0;
    g_gfx_line_g4_capacity = 0;
    g_gfx_tile_1_capacity = 0;
    g_gfx_tile_8_capacity = 0;
    g_gfx_tile_16_capacity = 0;
    g_gfx_sprite_capacity = 0;
    g_gfx_sprite_8_capacity = 0;
    g_gfx_sprite_16_capacity = 0;
    equip_gfx_init_contexts(g_equip_gfx_contexts, g_gfx_otag_buffer, 0, 0, 0, g_gfx_poly_ft4_buffer, 0, 0, 0, 0,
        g_equip_gfx_line_f2_buffer, 0, 0, 0, 0, 0, g_equip_gfx_tile_buffer, 0, 0, 0, 0, 0, 0, g_gfx_draw_move_buffer,
        g_gfx_draw_area_buffer, g_equip_gfx_draw_mode_buffer);
    equip_unit_load_data_from_stats(battle_id);
    equip_menu_load_images_and_reset_lists();
    g_equip_unit_status_panel_flags = battle_id;
    g_text_section_pointers[TEXT_SECTION_OVERLAY_SCREEN] = g_text_data;
    equip_unit_load_selected_data();
    equip_panel_toggle_unit_status_thread(1);
    equip_panel_toggle_equipment_threads(1);
    menu_toggle_numeric_editor_thread(1);
}

/* Starts the unit status thread on equip_panel_run_unit_status_thread
 * (parameter g_equip_unit_status_panel_params) when `enable` is set and the
 * thread is idle; stops it otherwise. */
void equip_panel_toggle_unit_status_thread(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(EQUIP_THREAD_SLOT_UNIT_STATUS) == 0) {
            thread_start(EQUIP_THREAD_SLOT_UNIT_STATUS, menu_run_unit_status_thread_variant);
            thread_set_parameters(EQUIP_THREAD_SLOT_UNIT_STATUS, (s32)g_equip_unit_status_panel_params, 0, 0);
        }
    } else {
        menu_request_thread_stop(EQUIP_THREAD_SLOT_UNIT_STATUS);
    }
}

/* Start the equipment list thread (when idle) and the numeric editor thread
 * when `enable` is set; ask both to stop otherwise. */
void equip_panel_toggle_equipment_threads(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(EQUIP_THREAD_SLOT_EQUIPMENT_LIST) == 0) {
            thread_start(EQUIP_THREAD_SLOT_EQUIPMENT_LIST, menu_run_equipment_list_thread_variant);
            thread_set_parameters(EQUIP_THREAD_SLOT_EQUIPMENT_LIST, (s32)g_equip_equipment_panel_params, 0, 0);
        }
        menu_toggle_numeric_editor_thread(enable);
        return;
    }
    menu_request_thread_stop(EQUIP_THREAD_SLOT_EQUIPMENT_LIST);
    menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
}
