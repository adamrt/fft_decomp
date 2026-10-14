#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"

/* Render a menu text record into a temporary image buffer at the fixed
 * origin (8, 8), upload it to VRAM and release the buffer. Twin of
 * menu_render_text_image_at_record_origin. */
void world_menu_render_text_image_at_fixed_origin(text_dialog_record_t* record) {
    if (record->box_type != DIALOG_BOX_TYPE_CENTERED) {
        if (record->box_type == DIALOG_BOX_TYPE_PORTRAIT) {
            record->buffer = menu_build_window_image(
                record->width, record->height, &record->rect, record->dialog_type, record->tail_offset, 1);
        } else {
            record->buffer = world_menu_build_and_upload_window_frame_image(
                record->width, record->height, &record->rect, MENU_WINDOW_FRAME_BUILD);
        }
        g_menu_text_state.pen.stride = record->rect.w * 4;
        g_menu_text_state.pen.rect.x = 8;
        g_menu_text_state.pen.rect.y = 8;
        g_world_menu_text_param_22 = record->first_line;
        g_world_menu_text_param_24 = record->last_line;
        menu_display_text_entry(record->text, record->buffer, &g_menu_text_state.pen);
        LoadImage(&record->rect, record->buffer);
        thread_yield();
        menu_free_memory(record->buffer);
    }
}
