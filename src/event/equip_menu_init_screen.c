#include "fft/battle_text.h"
#include "fft/equip.h"
#include "psx/types.h"

/* Size the primitive pools, load the unit `battle_id` and start the equipment screen threads. */
void equip_menu_init_screen(s32 battle_id) {
    g_equip_gfx_otag_length = 0x40;
    g_equip_gfx_poly_ft4_capacity = 0x190;
    g_equip_gfx_line_f2_capacity = 0xA;
    g_equip_gfx_tile_capacity = 0xA;
    g_equip_gfx_draw_move_capacity = 0x14;
    g_equip_gfx_draw_area_capacity = 6;
    g_equip_gfx_draw_mode_capacity = 0x1E;
    g_equip_gfx_poly_f3_capacity = 0;
    g_equip_gfx_poly_ft3_capacity = 0;
    g_equip_gfx_poly_f4_capacity = 0;
    g_equip_gfx_poly_g3_capacity = 0;
    g_equip_gfx_poly_gt3_capacity = 0;
    g_equip_gfx_poly_g4_capacity = 0;
    g_equip_gfx_poly_gt4_capacity = 0;
    g_equip_gfx_line_f3_capacity = 0;
    g_equip_gfx_line_f4_capacity = 0;
    g_equip_gfx_line_g2_capacity = 0;
    g_equip_gfx_line_g3_capacity = 0;
    g_equip_gfx_line_g4_capacity = 0;
    g_equip_gfx_tile_16_capacity = 0;
    g_equip_gfx_tile_8_capacity = 0;
    g_equip_gfx_tile_1_capacity = 0;
    g_equip_gfx_sprite_capacity = 0;
    g_equip_gfx_sprite_16_capacity = 0;
    g_equip_gfx_sprite_8_capacity = 0;
    equip_gfx_init_contexts(g_equip_gfx_contexts, g_equip_gfx_otag_buffer, 0, 0, 0, g_equip_gfx_poly_ft4_buffer, 0, 0,
        0, 0, g_equip_gfx_line_f2_buffer, 0, 0, 0, 0, 0, g_equip_gfx_tile_buffer, 0, 0, 0, 0, 0, 0,
        g_equip_gfx_draw_move_buffer, g_equip_gfx_draw_area_buffer, g_equip_gfx_draw_mode_buffer);
    equip_unit_load_data_from_battle_stats(battle_id);
    equip_menu_load_images_and_reset_lists();
    g_equip_unit_status_panel_flags = battle_id;
    g_battle_text_section_pointers[27] = g_equip_text_data;
    equip_unit_load_selected_data();
    equip_panel_toggle_unit_status_thread(1);
    equip_panel_toggle_equipment_threads(1);
    equip_panel_toggle_numeric_thread(1);
}
