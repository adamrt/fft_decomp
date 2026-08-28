#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Formation menu display setup: clears the display area unless g_world_shop_id is
 * -2, uploads the window-frame texture and CLUTs, derives two 16-entry CLUTs
 * from VRAM rows 0x1f0/0x1f4 with entries 4, 8 and 12 made transparent, then
 * resets the menu, fade, cursor and graphics-pointer state.
 *
 * Compiled with the -O1 profile: -O2 scheduling reorders the RECT stores. */
void world_formation_init_menu_display(void) {
    RECT rect;
    u16 clut[16];

    world_formation_reset_menu_context();
    world_script_set_vsync_mode_and_event_speed(0);
    if (g_world_shop_id != -2) {
        rect.x = 0;
        rect.y = 0;
        rect.w = 0x100;
        rect.h = 0x1E0;
        ClearImage(&rect, 0, 0, 0);
        SetDispMask(1);
    }
    rect.x = 0x240;
    rect.y = 0x100;
    rect.w = 0x40;
    rect.h = 0x30;
    world_gfx_load_image_sync(&rect, (u32*)g_world_menu_glyph_sheet);
    rect.x = 0x240;
    rect.y = 0x1FA;
    rect.w = 0x40;
    rect.h = 4;
    world_gfx_load_image_sync(&rect, g_world_formation_menu_palettes);
    rect.x = 0x3C0;
    rect.y = 0x1F0;
    rect.w = 0x10;
    rect.h = 1;
    world_gfx_move_image_to_ram_from_vram_and_wait(&rect, (u32*)clut);
    clut[4] = 0;
    clut[8] = 0;
    clut[12] = 0;
    rect.x = 0x260;
    rect.y = 0x1FE;
    rect.w = 0x10;
    rect.h = 1;
    world_gfx_load_image_sync(&rect, (u32*)clut);
    rect.x = 0x240;
    rect.y = 0x1FE;
    rect.w = 0xC;
    rect.h = 1;
    world_gfx_load_image_sync(&rect, (u32*)&clut[4]);
    rect.x = 0x3C0;
    rect.y = 0x1F4;
    rect.w = 0x10;
    rect.h = 1;
    world_gfx_move_image_to_ram_from_vram_and_wait(&rect, (u32*)clut);
    clut[4] = 0;
    clut[8] = 0;
    clut[12] = 0;
    rect.x = 0x270;
    rect.y = 0x1FE;
    rect.w = 0x10;
    rect.h = 1;
    world_gfx_load_image_sync(&rect, (u32*)clut);
    rect.x = 0x250;
    rect.y = 0x1FE;
    rect.w = 0xC;
    rect.h = 1;
    world_gfx_load_image_sync(&rect, (u32*)&clut[4]);
    world_gfx_load_image_sync(&g_world_formation_status_label_image_rect, g_world_formation_status_label_image);
    g_world_menu_description_text_id = 0;
    world_menu_init_scrollable_list_core(0, 0, 0);
    world_gfx_start_decreasing_fade();
    world_menu_clear_cursor_positions();
    g_world_formation_unit_cycle_mode = 0;
    world_menu_reset_selection_results();
    world_gfx_bind_data_pointer(0);
    world_gfx_bind_data_pointer(1);
    world_gfx_bind_data_pointer(2);
    world_gfx_bind_data_pointer(3);
    g_world_text_section_pointers[25] = g_world_formation_menu_text_buffer;
    g_world_text_section_pointers[27] = g_world_formation_menu_text_table;
    g_world_formation_cursor_ot_override = 0;
    g_world_script_tutorial_id = 0;
}
