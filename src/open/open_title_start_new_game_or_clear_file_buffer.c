#include "fft/open.h"

/* New Game on the title menu. With a memory card in either slot the new-game
 * prologue starts at once. With none the frame image (30,720 32-bit values) is saved to
 * g_open_work_buffer_0 and blanked, animation 0x12 is shown on the black screen
 * and the title-exit controller (handler 12) is pushed: Cross returns to the
 * title menu, Circle or Start starts the prologue anyway. */
void open_title_start_new_game_or_clear_file_buffer(void) {
    s32* source;
    s32* destination;
    s32 index;
    s32 record;
    point32_t* position;
    s32 controller;
    s32 _card_status;
    s32 frame_padding[2];

    _card_status = open_card_check_slots();
    index = 0;
    if (_card_status != 0) {
        open_title_start_new_game_transition();
        return;
    }

    source = g_open_file_destination;
    destination = (s32*)g_open_work_buffer_0;
    for (; index < 30720; index++) {
        *destination = *source;
        *source = 0;
        source++;
        destination++;
    }

    open_gfx_start_overlay_fade_out(0x10);
    record = window_append_animated_record_variant(
        g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_gfx_render_records_36[record].sequence = 0x12;
    g_open_gfx_render_records_36[record].anim_counter = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;
    position = &g_open_gfx_render_records_36[record].position;
    controller = g_open_current_controller_index;
    position->x = -4;
    position->y = -16;
    g_open_controller_records_next[controller].title_exit.state = 0;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_TITLE_EXIT;
    g_open_current_controller_index = controller + 1;
}
