#include "fft/open.h"

void open_title_start_new_game_or_clear_file_buffer(void) {
    s32* source;
    s32* destination;
    s32 index;
    s32 record;
    s32* position;
    s32 controller;
    s32 card_status;
    s32 frame_padding[2];

    card_status = open_card_check_slots();
    index = 0;
    if (card_status != 0) {
        open_title_start_new_game_transition();
        return;
    }

    source = (s32*)g_open_file_destination;
    destination = (s32*)g_open_work_buffer_0;
    do {
        *destination = *source;
        index++;
        *source = 0;
        source++;
        destination++;
    } while (index < 0x7800);

    open_gfx_start_overlay_fade_out(0x10);
    record
        = open_gfx_append_render_record_36(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_gfx_render_records_36[record].anim_id = 0x12;
    g_open_gfx_render_records_36[record].frame_timer = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;
    position = &g_open_gfx_render_records_36[record].x;
    controller = g_open_current_controller_index;
    position[0] = -4;
    position[1] = -16;
    g_open_controller_stream_start[controller].stream_start = 0;
    g_open_controller_handler_indices[controller] = 12;
    g_open_current_controller_index = controller + 1;
}
