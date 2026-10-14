#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/libgpu.h"

/* Render three numeric images and redraw the double-buffered panel until stopped. */
void menu_run_numeric_display_panel_thread(void) {
    menu_panel_record_t* records;
    menu_panel_record_t* record;
    menu_unit_stat_number_images_t* number_images;
    s32 i;
    s32 frame;

    record = g_menu_numeric_display_panel_records;
    records = record;
    thread_set_current_task_id(THREAD_TASK_NUMERIC_DISPLAY_PANEL);
    gfx_set_draw_mode_for_texture_page(&record->mode0, MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&record->mode1, MENU_TEXTURE_PAGE_TEXT_IMAGES);
    menu_init_scaled_draw_area_packets(&record->draw_areas);
    number_images = menu_alloc_memory(sizeof(*number_images));
    menu_clear_render_buffer(number_images, sizeof(*number_images));
    g_menu_text_state.pen.stride = 0x14;
    menu_set_text_origin(0, 0);
    menu_draw_numeric_display_entries(
        number_images->mobility, g_menu_numeric_display_entries, &g_menu_text_state.pen, 3);
    g_menu_text_state.pen.stride = 0x40;
    menu_set_text_origin(0, 0);
    menu_draw_numeric_display_entries(
        number_images->weapons, &g_menu_numeric_display_entries[3], &g_menu_text_state.pen, 4);
    g_menu_text_state.pen.stride = 0x64;
    menu_set_text_origin(0, 0);
    menu_draw_numeric_display_entries(
        number_images->combat, &g_menu_numeric_display_entries[7], &g_menu_text_state.pen, 8);
    LoadImage(&g_menu_numeric_display_text_rect_a, (u32*)number_images->mobility);
    LoadImage(&g_menu_numeric_display_text_rect_b, (u32*)number_images->weapons);
    LoadImage(&g_menu_numeric_display_text_rect_c, (u32*)number_images->combat);
    thread_wait_frames(1);
    menu_free_memory(number_images);
    for (i = 0; i < 18; i++) {
        battle_menu_init_semitransparent_sprt(&record->sprites[i]);
        gfx_init_image_loading(&record->sprites[i], &g_menu_texture_origin, &g_menu_numeric_display_frame_rect,
            &g_menu_numeric_display_image_params[i]);
    }
    mem_copy_bytes_nonempty(&record[1], record, sizeof(*record));
    record[0].palette = &g_menu_numeric_display_palettes[0].primitives;
    record[1].palette = &g_menu_numeric_display_palettes[1].primitives;
    menu_init_numeric_display_frame_primitives(&g_menu_numeric_display_frame_rect, record[0].palette);
    menu_init_numeric_display_frame_primitives(&g_menu_numeric_display_frame_rect, record[1].palette);
    for (frame = 0;; frame++) {
        thread_yield();
        record = &records[frame & 1];
        gfx_build_scaled_draw_area_pair_swapped(
            &record->draw_areas, &g_menu_numeric_display_panel_texture_window, frame, g_main_gfx_screen_polarity);
        if (battle_thread_get_current_parameter_3() != 0) {
            break;
        }
        if (battle_thread_is_running_asm(THREAD_SLOT_MENU_GRANDCHILD) != 0) {
            for (i = 17; i >= 0; i--) {
                record->sprites[i].clut = 0x7D3C;
            }
            record->sprites[10].clut = 0x7EBC;
            menu_init_primitive_colors_dimmed(record->palette);
        } else {
            for (i = 17; i >= 0; i--) {
                record->sprites[i].clut = 0x7C3C;
            }
            record->sprites[10].clut = 0x7D7C;
            record->sprites[14].clut = 0x7FFC;
            record->sprites[15].clut = 0x7FFC;
            record->sprites[16].clut = 0x7FFC;
            menu_init_primitive_colors_normal(record->palette);
        }
        gfx_draw_or_append_gpu_primitive(&record->draw_areas.areas[1]);
        for (i = 0; i < 14; i++) {
            gfx_draw_or_append_gpu_primitive(&record->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&record->sprites[17]);
        gfx_draw_or_append_gpu_primitive(&record->mode0);
        for (; i < 17; i++) {
            gfx_draw_or_append_gpu_primitive(&record->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&record->mode1);
        menu_submit_numeric_display_frame_primitives(record->palette);
        gfx_draw_or_append_gpu_primitive(&record->draw_areas.areas[0]);
    }
    thread_yield();
    thread_exit_current();
}
