#include "fft/main_gfx.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Thread task 0x23: renders three text images into VRAM, builds two
 * alternating 18-sprite panel records with line boxes, then redraws the panel
 * every frame (highlighted palette while thread 6 runs) until the thread's
 * third parameter becomes non-zero. */
void world_menu_run_numeric_display_panel_thread(void) {
    world_panel_record_t* records;
    world_panel_record_t* record;
    u8* buffer;
    s32 i;
    s32 frame;

    record = g_world_numeric_display_panel_records;
    records = record;
    world_thread_set_current_task_id(NATIVE_THREAD_TASK_NUMERIC_DISPLAY_PANEL);
    world_gfx_set_image_draw_mode(&record->mode0, 0);
    world_gfx_set_image_draw_mode(&record->mode1, 2);
    world_gfx_reset_record_texture_window(&record->draw_areas);
    buffer = world_menu_alloc_ui_buffer(0x918);
    world_clear_menu_render_buffer(buffer, 0x918);
    g_world_menu_text_state.stride = 0x14;
    world_menu_set_text_origin(0, 0);
    world_menu_draw_numeric_display_entries(
        (s32)buffer, g_world_numeric_display_entries, (world_glyph_blit_t*)&g_world_menu_text_state.origin_x, 3);
    g_world_menu_text_state.stride = 0x40;
    world_menu_set_text_origin(0, 0);
    world_menu_draw_numeric_display_entries((s32)(buffer + 0x168), &g_world_numeric_display_entries[3],
        (world_glyph_blit_t*)&g_world_menu_text_state.origin_x, 4);
    g_world_menu_text_state.stride = 0x64;
    world_menu_set_text_origin(0, 0);
    world_menu_draw_numeric_display_entries((s32)(buffer + 0x468), &g_world_numeric_display_entries[7],
        (world_glyph_blit_t*)&g_world_menu_text_state.origin_x, 8);
    LoadImage(&g_world_numeric_display_text_rect_a, (u32*)buffer);
    LoadImage(&g_world_numeric_display_text_rect_b, (u32*)(buffer + 0x168));
    LoadImage(&g_world_numeric_display_text_rect_c, (u32*)(buffer + 0x468));
    world_thread_wait_frames(1);
    world_menu_free_memory(buffer);
    for (i = 0; i < 18; i++) {
        world_menu_init_sprite(&record->sprites[i]);
        world_gfx_init_image_loading((POLY_FT4*)&record->sprites[i], &g_world_gfx_menu_image_source,
            (const world_image_location_t*)&g_world_numeric_display_frame_rect,
            &g_world_numeric_display_image_params[i]);
    }
    world_script_copy_bytes(&record[1], record, 0x1AC);
    record[0].palette = &g_world_numeric_display_palettes[0].primitives;
    record[1].palette = &g_world_numeric_display_palettes[1].primitives;
    world_menu_build_line_box(&g_world_numeric_display_frame_rect, record[0].palette);
    world_menu_build_line_box(&g_world_numeric_display_frame_rect, record[1].palette);
    for (frame = 0;; frame++) {
        world_thread_yield();
        record = &records[frame & 1];
        world_gfx_build_scaled_draw_area_pair_swapped(
            &record->draw_areas, &g_world_numeric_display_texture_window, frame, g_main_gfx_screen_polarity);
        if (world_thread_get_current_parameter_3() != 0) {
            break;
        }
        if (world_thread_is_running_80100164(6) != 0) {
            for (i = 17; i >= 0; i--) {
                record->sprites[i].clut = 0x7D3C;
            }
            record->sprites[10].clut = 0x7EBC;
            world_menu_init_primitive_colors_palette_bank_1(record->palette);
        } else {
            for (i = 17; i >= 0; i--) {
                record->sprites[i].clut = 0x7C3C;
            }
            record->sprites[10].clut = 0x7D7C;
            record->sprites[14].clut = 0x7FFC;
            record->sprites[15].clut = 0x7FFC;
            record->sprites[16].clut = 0x7FFC;
            world_menu_init_primitive_colors_palette_bank_0(record->palette);
        }
        world_gfx_draw_or_append_gpu_primitive((s32*)&record->draw_areas.areas[1]);
        for (i = 0; i < 14; i++) {
            world_gfx_draw_or_append_gpu_primitive((s32*)&record->sprites[i]);
        }
        world_gfx_draw_or_append_gpu_primitive((s32*)&record->sprites[17]);
        world_gfx_draw_or_append_gpu_primitive((s32*)&record->mode0);
        for (; i < 17; i++) {
            world_gfx_draw_or_append_gpu_primitive((s32*)&record->sprites[i]);
        }
        world_gfx_draw_or_append_gpu_primitive((s32*)&record->mode1);
        world_gfx_submit_primitive_group((world_primitive_group_t*)record->palette);
        world_gfx_draw_or_append_gpu_primitive((s32*)&record->draw_areas.areas[0]);
    }
    world_thread_yield();
    world_thread_exit_current();
}
