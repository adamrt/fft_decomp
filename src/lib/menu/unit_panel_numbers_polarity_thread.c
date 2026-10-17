/* The thread drawing the numeric panel until its task requests a stop. */

#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/input.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/script.h"
#include "fft/status_screen.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* The numeric stats panel as unit_panel_numbers_thread draws it, except that the
 * draw offset follows the screen polarity and both slots use g_unit_panel_editor_numeric_state, so a
 * preview panel shares the main panel's primitive buffers (retail). Runs until a stop
 * request. ATTACK and REQUIRE only start slot 12. Their retained preview branch's
 * text_b image range overlaps later cursor/menu state. */
void unit_panel_numbers_polarity_thread(void) {
    unit_panel_thread_t* thread;
    menu_status_panel_numeric_buffer_t* buffer;
    menu_status_panel_numeric_buffer_t* buffers;
    menu_unit_stat_number_images_t* number_images;
    menu_number_entry_t* entries;
    RECT* upload_a;
    RECT* upload_b;
    RECT* upload_c;
    const image_layout_t* descriptor;
    s32 i;
    s32 frame;
    s32 field_y;
    s32 use_offset;

    thread_set_current_task_id(THREAD_TASK_STATUS_PANEL);
    thread = (unit_panel_thread_t*)g_threads[g_thread_current_id].function_parameter_1;
    g_input_controller = script_get_controller_input_pointer(0) + 1;

    /* Slot 12 draws the unit's stats; the other slot (a preview panel) has its own number
     * images, VRAM rects and entries. */
    if (g_thread_current_id == THREAD_SCREEN_SLOT_NUMERIC_PANEL) {
        buffer = g_unit_panel_editor_numeric_state;
        buffers = buffer;
        number_images = &g_unit_panel_editor_numeric_text_a;
        upload_a = &g_unit_panel_editor_numeric_entries_a;
        upload_b = &g_unit_panel_editor_numeric_entries_b;
        upload_c = &g_unit_panel_editor_numeric_entries_c;
        entries = g_unit_panel_editor_numeric_descriptor_a;
    } else {
        buffer = g_unit_panel_editor_numeric_state;
        buffers = buffer;
        number_images = &g_unit_panel_editor_numeric_text_b;
        upload_a = &g_unit_panel_editor_numeric_entries_d;
        upload_b = &g_unit_panel_editor_numeric_entries_e;
        upload_c = &g_unit_panel_editor_numeric_entries_f;
        entries = g_unit_panel_editor_numeric_descriptor_b;
    }

    gfx_set_draw_mode_for_texture_page(&buffer->frame_draw_mode, MENU_TEXTURE_PAGE_FRAME);
    gfx_set_draw_mode_for_texture_page(&buffer->text_draw_mode, MENU_TEXTURE_PAGE_TEXT_IMAGES);
    gfx_set_draw_mode_for_texture_page(&buffer->image_draw_mode, MENU_TEXTURE_PAGE_ALLOCATED_IMAGES);
    status_screen_init_numbers_frame(&g_unit_panel_editor_numeric_table.texture_rect, &buffer->numeric_frame);
    menu_init_scaled_draw_area_packets(&buffer->portrait);

    i = 0;
    descriptor = g_unit_panel_editor_numeric_texture;
    for (; i < 18; i++) {
        menu_init_sprt(&buffer->sprites[i]);
        gfx_set_image_primitive_layout(&buffer->sprites[i], &g_unit_panel_texture_origin,
            &g_unit_panel_editor_numeric_table.texture_rect, descriptor);
        descriptor++;
    }

    /* The preview panel uploads its number images 0x4C rows lower (y 204), so its number
     * sprites sample there. */
    if (g_thread_current_id != THREAD_SCREEN_SLOT_NUMERIC_PANEL) {
        buffer->sprites[14].v0 += 0x4C;
        buffer->sprites[15].v0 += 0x4C;
        buffer->sprites[16].v0 += 0x4C;
    }
    buffer->sprites[10].clut = 0x7D7C;
    mem_copy_bytes_nonempty(&buffer[1], buffer, sizeof(menu_status_panel_numeric_buffer_t));

    for (frame = 0;; frame++) {
        buffer = &buffers[frame & 1];

        /* Redraw and upload the number images on the first frame or on request; the preview
         * panel draws its values signed. */
        if (frame == 0 || thread->redraw_request != 0) {
            menu_clear_render_buffer(number_images, sizeof(*number_images));
            /* Each pen stride is its image's width in 4-bit pixels (four per 16-bit VRAM unit). */
            g_menu_window_text_pen.stride = 0x14;
            menu_set_text_origin(0, 0);
            if (g_thread_current_id != THREAD_SCREEN_SLOT_NUMERIC_PANEL) {
                menu_text_render_signed_decimal_entries(number_images->mobility, entries, &g_menu_window_text_pen, 3);
            } else {
                menu_draw_numeric_display_entries(number_images->mobility, entries, &g_menu_window_text_pen, 3);
            }

            g_menu_window_text_pen.stride = 0x40;
            menu_set_text_origin(0, 0);
            if (g_thread_current_id != THREAD_SCREEN_SLOT_NUMERIC_PANEL) {
                menu_text_render_signed_decimal_entries(
                    number_images->weapons, entries + 3, &g_menu_window_text_pen, 4);
            } else {
                menu_draw_numeric_display_entries(number_images->weapons, entries + 3, &g_menu_window_text_pen, 4);
            }

            g_menu_window_text_pen.stride = 0x64;
            menu_set_text_origin(0, 0);
            if (g_thread_current_id != THREAD_SCREEN_SLOT_NUMERIC_PANEL) {
                menu_text_render_signed_decimal_entries(number_images->combat, entries + 7, &g_menu_window_text_pen, 8);
            } else {
                menu_draw_numeric_display_entries(number_images->combat, entries + 7, &g_menu_window_text_pen, 8);
            }

            LoadImage(upload_a, (u32*)number_images->mobility);
            LoadImage(upload_b, (u32*)number_images->weapons);
            LoadImage(upload_c, (u32*)number_images->combat);
        }

        /* A field_y of 0 draws into the lower frame-buffer half (offset 0xF0), 0xF0 into the upper half. */
        field_y = g_gfx_screen_polarity * 0xF0;
        use_offset = field_y != 0xF0;
        g_gfx_draw_offset_y = -use_offset & 0xF0;
        menu_build_scaled_draw_area_packets(
            &buffer->portrait, &g_unit_panel_editor_numeric_table.draw_area_rect, frame, field_y, &thread->position);
        buffer->draw_offsets[0].position.x = thread->position.x - 0x80;
        buffer->draw_offsets[0].position.y = thread->position.y + g_gfx_draw_offset_y;
        SetDrawOffset(&buffer->draw_offsets[0].packet, (u16*)&buffer->draw_offsets[0].position);
        buffer->draw_offsets[1].position.x = -0x80;
        buffer->draw_offsets[1].position.y = g_gfx_draw_offset_y;
        SetDrawOffset(&buffer->draw_offsets[1].packet, (u16*)&buffer->draw_offsets[1].position);

        if (thread->style == 1 || g_menu_input_lock_mode == 1) {
            for (i = 17; i >= 0; i--) {
                buffer->sprites[i].clut = MENU_CLUT_DIMMED;
            }
            buffer->sprites[10].clut = 0x7EBC;
            buffer->sprites[14].clut = 0x7F7D;
            buffer->sprites[15].clut = 0x7F7D;
            buffer->sprites[16].clut = 0x7F7D;
        } else {
            for (i = 17; i >= 0; i--) {
                buffer->sprites[i].clut = MENU_CLUT_NORMAL;
            }
            buffer->sprites[10].clut = 0x7D7C;
            buffer->sprites[14].clut = 0x7FFC;
            buffer->sprites[15].clut = 0x7FFC;
            buffer->sprites[16].clut = 0x7FFC;
        }

        unit_panel_select_palette(&buffer->numeric_frame, thread);

        /* Primitives link in at the head of the ordering-table entry, so they draw in reverse:
         * each draw mode or offset added below applies to the sprites added above it. */
        if (frame < 13) {
            gfx_draw_or_append_gpu_primitive(&buffer->portrait.areas[1]);
        }
        gfx_draw_or_append_gpu_primitive(&buffer->draw_offsets[1]);
        for (i = 0; i < 14; i++) {
            gfx_draw_or_append_gpu_primitive(&buffer->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&buffer->sprites[17]);
        gfx_draw_or_append_gpu_primitive(&buffer->frame_draw_mode);
        for (; i < 17; i++) {
            gfx_draw_or_append_gpu_primitive(&buffer->sprites[i]);
        }
        if (g_thread_current_id == THREAD_SCREEN_SLOT_NUMERIC_PANEL) {
            gfx_draw_or_append_gpu_primitive(&buffer->text_draw_mode);
        } else {
            gfx_draw_or_append_gpu_primitive(&buffer->image_draw_mode);
        }
        status_screen_submit_numbers_frame(&buffer->numeric_frame);
        gfx_draw_or_append_gpu_primitive(&buffer->draw_offsets[0]);
        /* During the opening zoom the panel draws clipped to the scaled area; portrait.areas[1],
         * drawn last, restores the full area. */
        if (frame < 13) {
            gfx_draw_or_append_gpu_primitive(&buffer->portrait);
        }

        thread_yield();
        if (thread_get_current_parameter_3() != 0) {
            break;
        }
    }

    thread_yield();
    thread_exit_current();
}
