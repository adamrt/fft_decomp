#include "fft/world.h"

/* One pointer holds each loaded resource in turn. */
typedef union world_menu_graphics_resource {
    world_menu_image_pages_t pages;
    world_menu_font_image_t font;
} world_menu_graphics_resource_t;

/*
 * Load the common menu images, palettes, and optional extra image pages.
 *
 * The first resource is cached across calls. Each temporary allocation stays
 * alive until its queued VRAM transfers finish.
 */
void world_menu_load_common_graphics(s32 load_extra_pages) {
    world_menu_graphics_resource_t* resource;

    if (g_main_common_menu_graphics_loaded == 0) {
        g_world_thread_inner_subroutine_callback = main_file_get_bin_as_tim;
        resource = (world_menu_graphics_resource_t*)world_thread_call_on_main_stack(0x1899, 0x8800);
        LoadImage(&g_world_menu_item_image_rect, resource->pages.first_page);
        LoadImage(&g_world_menu_item_palette_rect, resource->pages.second_page);
        DrawSync(0);
        main_heap_call_free(resource);
        g_main_common_menu_graphics_loaded++;
    }
    LoadImage(&g_world_menu_common_clut_rect, g_world_menu_common_clut_data);
    DrawSync(0);
    g_world_thread_inner_subroutine_callback = main_file_get_bin_as_tim;
    resource = (world_menu_graphics_resource_t*)world_thread_call_on_main_stack(0xe68, 0x9800);
    LoadImage(&g_world_menu_frame_image_rect, resource->font.pixels);
    LoadImage(&g_world_menu_frame_palette_rect, resource->font.palettes_a);
    LoadImage(&g_world_menu_frame_palette_tail_rect, resource->font.palettes_b);
    world_menu_set_palette_colors(g_world_menu_glyph_sheet->colors);
    DrawSync(0);
    main_heap_call_free(resource);
    if (load_extra_pages != 0) {
        g_world_thread_inner_subroutine_callback = main_file_get_bin_as_tim;
        resource = (world_menu_graphics_resource_t*)world_thread_call_on_main_stack(0x166b, 0x10000);
        LoadImage(&g_world_menu_unit_image_rect_a, resource->pages.first_page);
        LoadImage(&g_world_menu_unit_image_rect_b, resource->pages.second_page);
        DrawSync(0);
        main_heap_call_free(resource);
    }
}
