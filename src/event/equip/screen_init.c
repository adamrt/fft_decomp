/* Image loading, screen init and the unit status and equipment thread toggles. */

#include "fft/equipment.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/menu_gfx.h"
#include "fft/menu_list.h"
#include "fft/menu_selection.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

extern clut_t g_equipment_palette_image;

static menu_gfx_context_t g_equipment_gfx_contexts[2] = { 0 };
static DR_MODE g_equipment_gfx_draw_mode_buffer[60] = { 0 };
static LINE_F2 g_equipment_gfx_line_f2_buffer[20] = { 0 };
static TILE g_equipment_gfx_tile_buffer[20] = { 0 };

/* Upload the menu glyph sheet and palette image, copy two window-frame palettes into VRAM row 254
 * (each also as a 12-color copy from color 4, with color 4 cleared), then reset the screen's
 * selections and rebuild the inventory totals and item category lists. */
static void equipment_load_images(void) {
    RECT rect;
    palette_t palette;

    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    gfx_load_image_and_wait(&rect, g_menu_glyph_sheet->pixels);
    rect.x = 0x100;
    rect.y = 0xFA;
    rect.w = 0x40;
    rect.h = 4;
    gfx_load_image_and_wait(&rect, g_equipment_palette_image);
    rect.x = 0x3C0;
    rect.y = 0x1F0;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, palette);
    palette[4].red = palette[4].green = palette[4].blue = palette[4].semi_transparent = 0;
    rect.x = 0x120;
    rect.y = 0xFE;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette);
    rect.x = 0x100;
    rect.y = 0xFE;
    rect.w = 0xC;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &palette[4]);
    rect.x = 0x3C0;
    rect.y = 0x1F4;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, palette);
    palette[4].red = palette[4].green = palette[4].blue = palette[4].semi_transparent = 0;
    rect.x = 0x130;
    rect.y = 0xFE;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette);
    rect.x = 0x110;
    rect.y = 0xFE;
    rect.w = 0xC;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &palette[4]);
    menu_clear_selection_values();
    g_help_message_text_id = 0;
    equipment_reset_selection();
    item_build_inventory_totals();
    item_rebuild_category_list(0);
    item_rebuild_category_list(1);
    item_rebuild_category_list(2);
    item_rebuild_category_list(3);
    item_rebuild_category_list(4);
    menu_clear_selection_records();
}

/* Size the primitive pools, load the unit `battle_id` and start the equipment screen threads. */
void equipment_init(s32 battle_id) {
    g_gfx_otag_length = 0x40;
    g_gfx_poly_ft4_capacity = 0x190;
    g_gfx_line_f2_capacity = 0xA;
    g_gfx_tile_capacity = 0xA;
    g_equipment_gfx_draw_move_capacity = 0x14;
    g_equipment_gfx_draw_area_capacity = 6;
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
    g_equipment_gfx_line_g2_capacity = 0;
    g_equipment_gfx_line_g3_capacity = 0;
    g_gfx_line_g4_capacity = 0;
    g_gfx_tile_1_capacity = 0;
    g_gfx_tile_8_capacity = 0;
    g_gfx_tile_16_capacity = 0;
    g_gfx_sprite_capacity = 0;
    g_gfx_sprite_8_capacity = 0;
    g_gfx_sprite_16_capacity = 0;
    equipment_gfx_init_contexts(g_equipment_gfx_contexts, g_gfx_otag_buffer, 0, 0, 0, g_gfx_poly_ft4_buffer, 0, 0, 0, 0,
        g_equipment_gfx_line_f2_buffer, 0, 0, 0, 0, 0, g_equipment_gfx_tile_buffer, 0, 0, 0, 0, 0, 0,
        g_gfx_draw_move_buffer, g_gfx_draw_area_buffer, g_equipment_gfx_draw_mode_buffer);
    equipment_load_unit(battle_id);
    equipment_load_images();
    g_unit_panel_unit_status_panel_params.flags = battle_id;
    g_text_section_pointers[TEXT_SECTION_OVERLAY_SCREEN] = g_text_data;
    equipment_load_selected_unit();
    unit_panel_toggle_banner_thread(1);
    unit_panel_toggle_equipment_threads(1);
    unit_panel_toggle_numbers_thread(1);
}

/* Starts the unit status thread on unit_panel_banner_thread
 * (parameter g_unit_panel_unit_status_panel_params) when `enable` is set and the
 * thread is idle; stops it otherwise. */
void unit_panel_toggle_banner_thread(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(EQUIPMENT_THREAD_SLOT_UNIT_STATUS) == 0) {
            thread_start(EQUIPMENT_THREAD_SLOT_UNIT_STATUS, unit_panel_banner_thread);
            thread_set_parameters(EQUIPMENT_THREAD_SLOT_UNIT_STATUS, (s32)&g_unit_panel_unit_status_panel_params, 0, 0);
        }
    } else {
        menu_request_thread_stop(EQUIPMENT_THREAD_SLOT_UNIT_STATUS);
    }
}

/* Start the equipment list thread (when idle) and the numeric panel thread
 * when `enable` is set; ask both to stop otherwise. */
void unit_panel_toggle_equipment_threads(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(EQUIPMENT_THREAD_SLOT_EQUIPMENT_LIST) == 0) {
            thread_start(EQUIPMENT_THREAD_SLOT_EQUIPMENT_LIST, unit_panel_equipment_thread);
            thread_set_parameters(EQUIPMENT_THREAD_SLOT_EQUIPMENT_LIST, (s32)&g_equipment_panel_params, 0, 0);
        }
        unit_panel_toggle_numbers_thread(enable);
        return;
    }
    menu_request_thread_stop(EQUIPMENT_THREAD_SLOT_EQUIPMENT_LIST);
    menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
}
