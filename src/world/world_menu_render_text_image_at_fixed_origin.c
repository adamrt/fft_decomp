#include "fft/world.h"
#include "psx/gpu.h"

/* Render a menu text record into a temporary image buffer at the fixed
 * origin (8, 8), upload it to VRAM and release the buffer. Twin of
 * world_menu_render_text_image_at_record_origin. */
void world_menu_render_text_image_at_fixed_origin(world_menu_text_image_t* record) {
    if (record->bits_per_pixel != 0) {
        if (record->bits_per_pixel == 0x10) {
            record->buffer = world_menu_build_window_image(
                record->width, record->height, &record->rect, record->dialog_type, record->tail_offset, 1);
        } else {
            record->buffer
                = world_menu_build_and_upload_window_frame_image(record->width, record->height, &record->rect, 1);
        }
        g_world_menu_text_state.stride = record->rect.w * 4;
        g_world_menu_text_state.origin_x = 8;
        g_world_menu_text_state.origin_y = 8;
        g_world_menu_text_param_22 = record->first_line;
        g_world_menu_text_param_24 = record->last_line;
        world_menu_display_text_entry((s32)record->text, record->buffer, &g_world_menu_text_state.origin_x);
        LoadImage(&record->rect, record->buffer);
        world_thread_yield();
        world_menu_free_memory(record->buffer);
    }
}
