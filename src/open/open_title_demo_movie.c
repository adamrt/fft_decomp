/* Title demo movie and New Game exit controllers. */

#include "fft/open.h"
#include "psx/pad.h"

/* Start a title demo movie at the given CD mix level and push handler 11. The
 * first sector and length are stored in the controller record, which handler 11
 * does not read. */
void open_movie_stream_title_demo_and_push_controller(
    s32 start_sector, s32 stream_length, s32 end_frame, s32 cd_mix_level) {
    s32 controller;

    open_movie_start_stream(start_sector, end_frame, -1, cd_mix_level);
    g_open_system_runtime_flags &= ~OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
    controller = g_open_current_controller_index;
    g_open_controller_records_next[controller].movie.start_sector = start_sector;
    g_open_controller_records_next[controller].movie.stream_length = stream_length;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_TITLE_DEMO_MOVIE;
    g_open_current_controller_index = controller + 1;
}

/* Handler 11, a title demo movie: Start stops the stream. Once the stream has
 * stopped, by itself or on the update after Start, the display is blanked and the
 * controller is replaced by the FFTST.STR logo movie, which leads back to the
 * title menu. */
void open_movie_start_fftst_or_skip_controller(open_controller_record_t* unused_record) {
    if ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING) == 0) {
        SetDispMask(0);
        g_active_graphics_buffer_index = 1;
        g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
        g_open_current_controller_index--;
        open_movie_stream_fftst_and_push_controller(DISC_SECTOR_OPEN_FFTST_STR, 0x278a, 0x3df, 0x2de);
    } else if ((g_open_input_new_button_presses & PSX_PAD_START) != 0) {
        open_movie_stop_stream(1);
    }
}

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

/* Per-frame handler of the title-exit controller (handler 12), pushed when New
 * Game is chosen with no memory card inserted.
 *
 * Cross, or Circle or Start, records the choice and fades the screen to black.
 * When the fade has ended the notice record and the controller are released: on
 * Cross the frame image saved in g_open_work_buffer_0 is copied back and the
 * title menu is pushed again; on Circle or Start the new-game prologue starts. */
void open_title_update_exit_transition(open_controller_record_t* record) {
    s32 state;
    u32 buttons;

    if ((g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE) != 0) {
        return;
    }

    if (record->title_exit.state != OPEN_TITLE_EXIT_INPUT) {
        g_open_gfx_next_render_record_36 -= 1;
        g_open_gfx_render_record_pointer_count -= 1;
        g_open_current_controller_index -= 1;
        state = record->title_exit.state;
        if (state == OPEN_TITLE_EXIT_CANCEL) {
            s32* source;
            s32* destination;
            s32 index;

            SetDispMask(0);
            source = (s32*)g_open_work_buffer_0;
            g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
            destination = g_open_file_destination;
            for (index = 0; index < 0x7800; index++) {
                destination[index] = source[index];
            }
            open_title_push_menu_controller(0);
        } else if (state == OPEN_TITLE_EXIT_CONFIRM) {
            open_title_start_new_game_transition();
        }
        return;
    }

    buttons = g_open_input_new_button_presses;
    if ((buttons & PSX_PAD_CROSS) != 0) {
        main_sound_play_sfx(MAIN_SFX_CANCEL);
        open_gfx_start_overlay_fade_in(0x20);
        record->title_exit.state = OPEN_TITLE_EXIT_CANCEL;
    } else if ((buttons & (PSX_PAD_CIRCLE | PSX_PAD_START)) != 0) {
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        open_gfx_start_overlay_fade_in(0x20);
        record->title_exit.state = OPEN_TITLE_EXIT_CONFIRM;
    }
}
