#include "fft/event_bunit.h"
#include "psx/gpu.h"
#include "psx/types.h"

struct menu_number_entry;
struct menu_number_position;

/* Thread task 0x3b: numeric editor panel; twin of equip_editor_run_numeric_thread;
 * like WORLD, BUNIT reads the 0/0xf0 draw offset from g_bunit_frame_arg.
 * The primitive block is battle_menu_status_panel_numeric_buffer_t, double buffered at
 * g_bunit_editor_numeric_state_a (thread 12) or _b; the thread parameter block is
 * battle_menu_status_panel_frame_config_t. */

void bunit_run_numeric_editor_thread(void) {
    battle_menu_status_panel_frame_config_t* thread;
    battle_menu_status_panel_numeric_buffer_t* buffer;
    battle_menu_status_panel_numeric_buffer_t* buffers;
    u8* text_pixels;
    u8* entries;
    RECT* upload_a;
    RECT* upload_b;
    RECT* upload_c;
    const world_gfx_image_load_parameters_t* descriptor;
    s32 i;
    s32 frame;
    s32 field_y;
    s32 use_offset;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_STATUS_PANEL);
    thread
        = (battle_menu_status_panel_frame_config_t*)g_battle_threads[g_battle_current_thread_id].function_parameter_1;
    g_bunit_input_controller = battle_script_get_controller_input_pointer(0) + 1;
    if (g_battle_current_thread_id == 12) {
        buffer = g_bunit_editor_numeric_state_a;
        buffers = buffer;
        text_pixels = g_bunit_editor_numeric_text_a;
        upload_a = &g_bunit_editor_numeric_entries_a;
        upload_b = &g_bunit_editor_numeric_entries_b;
        upload_c = &g_bunit_editor_numeric_entries_c;
        entries = g_bunit_editor_numeric_descriptor_a;
    } else {
        buffer = g_bunit_editor_numeric_state_b;
        buffers = buffer;
        text_pixels = g_bunit_editor_numeric_text_b;
        upload_a = &g_bunit_editor_numeric_entries_d;
        upload_b = &g_bunit_editor_numeric_entries_e;
        upload_c = &g_bunit_editor_numeric_entries_f;
        entries = g_bunit_editor_numeric_descriptor_b;
    }
    battle_gfx_set_draw_mode_for_texture_page(&buffer->draw_modes[0], 0);
    battle_gfx_set_draw_mode_for_texture_page(&buffer->draw_modes[1], 2);
    battle_gfx_set_draw_mode_for_texture_page(&buffer->draw_modes[2], 1);
    battle_menu_init_numeric_display_frame_primitives(
        &g_bunit_editor_numeric_table.source.texture_rect, &buffer->numeric_frame);
    bunit_gfx_init_scaled_draw_area_packets((u8*)&buffer->portrait);
    i = 0;
    descriptor = (const world_gfx_image_load_parameters_t*)g_bunit_editor_numeric_texture;
    for (; i < 18; i++) {
        battle_menu_init_semitransparent_sprt(&buffer->sprites[i]);
        battle_gfx_init_image_loading((POLY_FT4*)&buffer->sprites[i], g_bunit_editor_numeric_geometry,
            &g_bunit_editor_numeric_table.source.texture_origin, descriptor);
        descriptor++;
    }
    if (g_battle_current_thread_id != 12) {
        buffer->sprites[14].v0 += 0x4C;
        buffer->sprites[15].v0 += 0x4C;
        buffer->sprites[16].v0 += 0x4C;
    }
    buffer->sprites[10].clut = 0x7D7C;
    battle_copy_bytes(&buffer[1], buffer, sizeof(battle_menu_status_panel_numeric_buffer_t));

    for (frame = 0;; frame++) {
        buffer = &buffers[frame & 1];
        if (frame == 0 || thread->redraw_request != 0) {
            battle_clear_menu_render_buffer(text_pixels, 0x918);
            g_menu_text_state.stride = 0x14;
            battle_menu_set_text_origin(0, 0);
            if (g_battle_current_thread_id != 12) {
                bunit_text_render_signed_decimal_entries((s32)text_pixels,
                    (battle_menu_status_panel_gauge_entry_t*)entries,
                    (battle_menu_status_panel_text_position_t*)&g_menu_text_state.origin_x, 3);
            } else {
                battle_menu_draw_numeric_display_entries((s32)text_pixels, (struct menu_number_entry*)entries,
                    (struct menu_number_position*)&g_menu_text_state.origin_x, 3);
            }
            g_menu_text_state.stride = 0x40;
            battle_menu_set_text_origin(0, 0);
            if (g_battle_current_thread_id != 12) {
                bunit_text_render_signed_decimal_entries((s32)(text_pixels + 0x168),
                    (battle_menu_status_panel_gauge_entry_t*)(entries + 0x24),
                    (battle_menu_status_panel_text_position_t*)&g_menu_text_state.origin_x, 4);
            } else {
                battle_menu_draw_numeric_display_entries((s32)(text_pixels + 0x168),
                    (struct menu_number_entry*)(entries + 0x24),
                    (struct menu_number_position*)&g_menu_text_state.origin_x, 4);
            }
            g_menu_text_state.stride = 0x64;
            battle_menu_set_text_origin(0, 0);
            if (g_battle_current_thread_id != 12) {
                bunit_text_render_signed_decimal_entries((s32)(text_pixels + 0x468),
                    (battle_menu_status_panel_gauge_entry_t*)(entries + 0x54),
                    (battle_menu_status_panel_text_position_t*)&g_menu_text_state.origin_x, 8);
            } else {
                battle_menu_draw_numeric_display_entries((s32)(text_pixels + 0x468),
                    (struct menu_number_entry*)(entries + 0x54),
                    (struct menu_number_position*)&g_menu_text_state.origin_x, 8);
            }
            LoadImage(upload_a, (u32*)text_pixels);
            LoadImage(upload_b, (u32*)(text_pixels + 0x168));
            LoadImage(upload_c, (u32*)(text_pixels + 0x468));
        }
        field_y = (u16)g_bunit_frame_arg;
        use_offset = field_y != 0xF0;
        g_bunit_gfx_draw_offset_y = -use_offset & 0xF0;
        bunit_gfx_build_scaled_draw_area_packets((bunit_gfx_scaled_draw_area_pair_t*)&buffer->portrait,
            &g_bunit_editor_numeric_table.draw_area_rect, frame, field_y, (s16*)thread);
        buffer->draw_offsets[0].x = thread->origin_x - 0x80;
        buffer->draw_offsets[0].y = thread->origin_y + g_bunit_gfx_draw_offset_y;
        SetDrawOffset(&buffer->draw_offsets[0], &buffer->draw_offsets[0].x);
        buffer->draw_offsets[1].x = -0x80;
        buffer->draw_offsets[1].y = g_bunit_gfx_draw_offset_y;
        SetDrawOffset(&buffer->draw_offsets[1], &buffer->draw_offsets[1].x);
        if (thread->style == 1 || g_event_mode == 1) {
            for (i = 17; i >= 0; i--) {
                buffer->sprites[i].clut = 0x7D3C;
            }
            buffer->sprites[10].clut = 0x7EBC;
            buffer->sprites[14].clut = 0x7F7D;
            buffer->sprites[15].clut = 0x7F7D;
            buffer->sprites[16].clut = 0x7F7D;
        } else {
            for (i = 17; i >= 0; i--) {
                buffer->sprites[i].clut = 0x7C3C;
            }
            buffer->sprites[10].clut = 0x7D7C;
            buffer->sprites[14].clut = 0x7FFC;
            buffer->sprites[15].clut = 0x7FFC;
            buffer->sprites[16].clut = 0x7FFC;
        }
        bunit_gfx_apply_menu_palette_for_mode(&buffer->numeric_frame, thread);
        if (frame < 13) {
            battle_gfx_draw_or_append_gpu_primitive(&buffer->portrait.areas[1]);
        }
        battle_gfx_draw_or_append_gpu_primitive(&buffer->draw_offsets[1]);
        for (i = 0; i < 14; i++) {
            battle_gfx_draw_or_append_gpu_primitive(&buffer->sprites[i]);
        }
        battle_gfx_draw_or_append_gpu_primitive(&buffer->sprites[17]);
        battle_gfx_draw_or_append_gpu_primitive(&buffer->draw_modes[0]);
        for (; i < 17; i++) {
            battle_gfx_draw_or_append_gpu_primitive(&buffer->sprites[i]);
        }
        if (g_battle_current_thread_id == 12) {
            battle_gfx_draw_or_append_gpu_primitive(&buffer->draw_modes[1]);
        } else {
            battle_gfx_draw_or_append_gpu_primitive(&buffer->draw_modes[2]);
        }
        battle_menu_submit_numeric_display_frame_primitives(&buffer->numeric_frame);
        battle_gfx_draw_or_append_gpu_primitive(&buffer->draw_offsets[0]);
        if (frame < 13) {
            battle_gfx_draw_or_append_gpu_primitive(&buffer->portrait);
        }
        battle_thread_yield();
        if (battle_thread_get_current_parameter_3() != 0) {
            break;
        }
    }
    battle_thread_yield();
    battle_thread_exit_current();
}
