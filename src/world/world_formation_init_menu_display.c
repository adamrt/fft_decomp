#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/world.h"
#include "psx/libgpu.h"

/* Formation menu display setup: clears the display area unless g_world_menu_context_id is
 * -2, uploads the window-frame texture and CLUTs, derives two 16-entry palettes
 * from VRAM rows 0x1f0/0x1f4 with entries 4, 8 and 12 made transparent, then
 * resets the menu, fade, cursor and graphics-pointer state.
 *
 * Compiled with the -O1 profile: -O2 scheduling reorders the RECT stores. */
void world_formation_init_menu_display(void) {
    RECT rect;
    palette_t palette;

    world_formation_reset_menu_context();
    menu_set_vsync_mode_and_event_speed(0);
    if (g_world_menu_context_id != WORLD_MENU_CONTEXT_NAME_ENTRY) {
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
    gfx_load_image_and_wait(&rect, g_menu_glyph_sheet->pixels);
    rect.x = 0x240;
    rect.y = 0x1FA;
    rect.w = 0x40;
    rect.h = 4;
    gfx_load_image_and_wait(&rect, g_world_formation_menu_palettes);
    rect.x = 0x3C0;
    rect.y = 0x1F0;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, palette);
    palette[4].red = palette[4].green = palette[4].blue = palette[4].semi_transparent = 0;
    palette[8].red = palette[8].green = palette[8].blue = palette[8].semi_transparent = 0;
    palette[12].red = palette[12].green = palette[12].blue = palette[12].semi_transparent = 0;
    rect.x = 0x260;
    rect.y = 0x1FE;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette);
    rect.x = 0x240;
    rect.y = 0x1FE;
    rect.w = 0xC;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &palette[4]);
    rect.x = 0x3C0;
    rect.y = 0x1F4;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, palette);
    palette[4].red = palette[4].green = palette[4].blue = palette[4].semi_transparent = 0;
    palette[8].red = palette[8].green = palette[8].blue = palette[8].semi_transparent = 0;
    palette[12].red = palette[12].green = palette[12].blue = palette[12].semi_transparent = 0;
    rect.x = 0x270;
    rect.y = 0x1FE;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette);
    rect.x = 0x250;
    rect.y = 0x1FE;
    rect.w = 0xC;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &palette[4]);
    gfx_load_image_and_wait(&g_world_formation_status_label_image_rect, g_world_formation_status_label_image);
    g_world_menu_description_text_id = 0;
    world_menu_init_scrollable_list_core(0, 0, 0);
    world_gfx_start_decreasing_fade();
    world_menu_clear_cursor_positions();
    g_world_formation_unit_cycle_mode = 0;
    world_menu_reset_selection_results();
    world_text_bind_section_data(WORLD_TEXT_DATA_SKILLSET_NAMES);
    world_text_bind_section_data(WORLD_TEXT_DATA_JOB_NAMES);
    world_text_bind_section_data(WORLD_TEXT_DATA_ABILITY_NAMES);
    world_text_bind_section_data(WORLD_TEXT_DATA_FORMATION_MESSAGES);
    g_text_section_pointers[TEXT_SECTION_COMPOSED_TEXT] = g_world_formation_menu_text_buffer;
    g_text_section_pointers[TEXT_SECTION_OVERLAY_SCREEN] = g_world_formation_menu_text_table;
    g_world_formation_cursor_ot_override = 0;
    g_world_script_tutorial_id = 0;
}
