#include "fft/battle.h"
#include "fft/battle_menu_window.h"
#include "fft/battle_text.h"
#include "fft/menu_types.h"
#include "psx/gpu.h"
#include "psx/types.h"

void battle_menu_render_text_image_at_record_origin(battle_menu_text_image_t* record) {
    if (record->box_type != 0) {
        if (record->box_type == DIALOG_BOX_TYPE_PORTRAIT) {
            record->buffer = battle_menu_build_window_image(
                record->width, record->height, &record->rect, record->dialog_type, record->tail_offset, 1);
        } else {
            record->buffer
                = battle_menu_build_and_upload_window_frame_image(record->width, record->height, &record->rect, 1);
        }
        g_menu_text_state.origin_x = record->origin_x;
        g_menu_text_state.origin_y = record->origin_y;
        g_menu_text_state.stride = record->rect.w * 4;
        g_menu_text_param_22 = record->first_line;
        g_menu_text_param_24 = record->last_line;
        battle_menu_display_text_entry((s32)record->text, record->buffer, &g_menu_text_state.origin_x);
        LoadImage(&record->rect, record->buffer);
        battle_thread_yield();
        battle_menu_free_memory(record->buffer);
    }
}
