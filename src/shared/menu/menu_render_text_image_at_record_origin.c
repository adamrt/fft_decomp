#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/libgpu.h"

/* Render a text record into a freshly built window image at the record's own
 * origin, upload it to the record's rectangle and free the buffer.
 *
 * A portrait dialog gets the window with its tail, every other type the plain
 * frame; a centered box is skipped. The thread yields once between the upload and
 * the free. */
void menu_render_text_image_at_record_origin(text_dialog_record_t* record) {
    if (record->box_type != DIALOG_BOX_TYPE_CENTERED) {
        if (record->box_type == DIALOG_BOX_TYPE_PORTRAIT) {
            record->buffer = menu_build_window_image(
                record->width, record->height, &record->rect, record->dialog_type, record->tail_offset, 1);
        } else {
            record->buffer = menu_build_and_upload_window_frame_image_variant(
                record->width, record->height, &record->rect, MENU_WINDOW_FRAME_BUILD);
        }
        g_menu_text_state.pen.rect.x = record->origin_x;
        g_menu_text_state.pen.rect.y = record->origin_y;
        g_menu_text_state.pen.stride = record->rect.w * 4;
        g_menu_text_param_22 = record->first_line;
        g_menu_text_param_24 = record->last_line;
        menu_display_text_entry(record->text, record->buffer, &g_menu_text_state.pen);
        LoadImage(&record->rect, record->buffer);
        thread_yield();
        menu_free_memory(record->buffer);
    }
}
