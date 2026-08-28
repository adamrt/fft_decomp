/*
 * WORLD twin: world_menu_run_numeric_display_panel_thread
 * (same code, globals and callees rebound).
 */
#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_gfx.h"
#include "fft/main_heap.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Provisional 0x1AC-byte double-buffered panel record at 0x80172834; layout
 * of world_panel_record_t (WORLD 0x801a2bd0). */
typedef struct battle_panel_record {
    DR_MODE mode0;                                 /* 0x000 */
    DR_MODE mode1;                                 /* 0x00c */
    battle_gfx_scaled_draw_area_pair_t draw_areas; /* 0x018; battle_menu_set_disabled_texture_window */
    SPRT sprites[18];                              /* 0x040 */
    world_menu_palette_primitives_t* palette;      /* 0x1a8 */
} battle_panel_record_t;

extern battle_panel_record_t g_battle_menu_numeric_display_panel_records[2];
struct menu_number_entry;
struct menu_number_position;

/* Thread task 0x23: renders three text images into VRAM, builds two
 * alternating 18-sprite panel records with line boxes, then redraws the panel
 * every frame (highlighted palette while thread 6 runs) until the thread's
 * third parameter becomes non-zero. Started by
 * battle_menu_run_unit_editor_panel_thread on thread 0xA. */
void battle_menu_run_numeric_display_panel_thread(void) {
    battle_panel_record_t* records;
    battle_panel_record_t* record;
    u8* buffer;
    s32 i;
    s32 frame;

    record = g_battle_menu_numeric_display_panel_records;
    records = record;
    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_NUMERIC_DISPLAY_PANEL);
    battle_gfx_set_draw_mode_for_texture_page(&record->mode0, 0);
    battle_gfx_set_draw_mode_for_texture_page(&record->mode1, 2);
    battle_menu_set_disabled_texture_window((u8*)&record->draw_areas);
    buffer = battle_menu_alloc_memory(0x918);
    battle_clear_menu_render_buffer(buffer, 0x918);
    g_menu_text_state.stride = 0x14;
    battle_menu_set_text_origin(0, 0);
    battle_menu_draw_numeric_display_entries((s32)buffer, (struct menu_number_entry*)g_battle_numeric_display_entries,
        (struct menu_number_position*)&g_menu_text_state.origin_x, 3);
    g_menu_text_state.stride = 0x40;
    battle_menu_set_text_origin(0, 0);
    battle_menu_draw_numeric_display_entries((s32)(buffer + 0x168),
        (struct menu_number_entry*)(g_battle_numeric_display_entries + 0x24),
        (struct menu_number_position*)&g_menu_text_state.origin_x, 4);
    g_menu_text_state.stride = 0x64;
    battle_menu_set_text_origin(0, 0);
    battle_menu_draw_numeric_display_entries((s32)(buffer + 0x468),
        (struct menu_number_entry*)(g_battle_numeric_display_entries + 0x54),
        (struct menu_number_position*)&g_menu_text_state.origin_x, 8);
    LoadImage(&g_battle_numeric_display_text_rect_a, (u32*)buffer);
    LoadImage(&g_battle_numeric_display_text_rect_b, (u32*)(buffer + 0x168));
    LoadImage(&g_battle_numeric_display_text_rect_c, (u32*)(buffer + 0x468));
    battle_thread_wait_frames(1);
    battle_menu_free_memory(buffer);
    for (i = 0; i < 18; i++) {
        battle_menu_init_semitransparent_sprt(&record->sprites[i]);
        battle_gfx_init_image_loading((POLY_FT4*)&record->sprites[i], &g_battle_menu_texture_location,
            (const battle_image_location_t*)&g_battle_numeric_display_frame_rect,
            &g_battle_numeric_display_image_params[i]);
    }
    battle_copy_bytes(&record[1], record, 0x1AC);
    record[0].palette = &g_battle_numeric_display_palettes[0].primitives;
    record[1].palette = &g_battle_numeric_display_palettes[1].primitives;
    battle_menu_init_numeric_display_frame_primitives(&g_battle_numeric_display_frame_rect, record[0].palette);
    battle_menu_init_numeric_display_frame_primitives(&g_battle_numeric_display_frame_rect, record[1].palette);
    for (frame = 0;; frame++) {
        battle_thread_yield();
        record = &records[frame & 1];
        battle_menu_build_zoom_draw_area_pair(
            &record->draw_areas, &g_battle_numeric_display_texture_window, frame, g_main_gfx_screen_polarity);
        if (battle_thread_get_current_parameter_3() != 0) {
            break;
        }
        if (battle_thread_is_running_8014cc94(6) != 0) {
            for (i = 17; i >= 0; i--) {
                record->sprites[i].clut = 0x7D3C;
            }
            record->sprites[10].clut = 0x7EBC;
            battle_menu_init_primitive_colors_palette_bank_1(record->palette);
        } else {
            for (i = 17; i >= 0; i--) {
                record->sprites[i].clut = 0x7C3C;
            }
            record->sprites[10].clut = 0x7D7C;
            record->sprites[14].clut = 0x7FFC;
            record->sprites[15].clut = 0x7FFC;
            record->sprites[16].clut = 0x7FFC;
            battle_menu_init_primitive_colors_palette_bank_0(record->palette);
        }
        battle_gfx_draw_or_append_gpu_primitive((s32*)&record->draw_areas.areas[1]);
        for (i = 0; i < 14; i++) {
            battle_gfx_draw_or_append_gpu_primitive((s32*)&record->sprites[i]);
        }
        battle_gfx_draw_or_append_gpu_primitive((s32*)&record->sprites[17]);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&record->mode0);
        for (; i < 17; i++) {
            battle_gfx_draw_or_append_gpu_primitive((s32*)&record->sprites[i]);
        }
        battle_gfx_draw_or_append_gpu_primitive((s32*)&record->mode1);
        battle_menu_submit_numeric_display_frame_primitives(record->palette);
        battle_gfx_draw_or_append_gpu_primitive((s32*)&record->draw_areas.areas[0]);
    }
    battle_thread_yield();
    battle_thread_exit_current();
}
