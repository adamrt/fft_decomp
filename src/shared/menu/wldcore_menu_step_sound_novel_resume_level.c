#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/pad.h"

/* Input step for the type-0x14 sound-novel resume level.
 *
 * While dissolve_step is set the level is feeding the progressive texture upload at
 * 0x8007f998, one step per frame. Cross tears the level down through thread
 * 14; Circle fades the screen out and defers the teardown to the
 * system-flag-4 path; up and down toggle cursor_row, which moves the owned
 * window record down by 16.
 *
 * wldcore_reset_selected_saved_record ignores its argument; retail still
 * passes g_wldcore_saved_record_index. */
void wldcore_menu_step_sound_novel_resume_level(wldcore_menu_sound_novel_resume_level_t* level) {
    GsIMAGE image;
    s32 buttons;

    if (g_wldcore_window_render_records[level->message_render].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (level->dissolve_step != 0) {
        GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
        if (wldcore_gfx_step_dissolve_image_upload(&image, level->dissolve_step) == 0) {
            level->dissolve_step = 0;
            return;
        }
        level->dissolve_step++;
        return;
    }
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) && !(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_window_record_count--;
        g_wldcore_window_render_record_count -= 3;
        g_wldcore_window_render_object_count -= 4;
        g_wldcore_menu_stack_depth--;
        if (level->cursor_row != 0) {
            wldcore_reset_selected_saved_record(g_wldcore_saved_record_index);
        }
        wldcore_menu_push_sound_novel_level(level->mode);
        return;
    }
    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        g_wldcore_window_record_count--;
        g_wldcore_window_render_record_count -= 3;
        g_wldcore_window_render_object_count -= 4;
        g_wldcore_menu_stack_depth--;
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        wldcore_menu_push_treasure_detail_level(level->mode);
        return;
    }
    if (buttons & PSX_PAD_CIRCLE) {
        g_wldcore_window_records[level->cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
        g_wldcore_window_records[level->cursor_window].frame_index
            = g_wldcore_window_records[level->cursor_window].anim_counter = 0;
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) == 0 && wldcore_input_update_repeat(PSX_PAD_DOWN) == 0) {
        return;
    }
    level->cursor_row ^= 1;
    g_wldcore_window_records[level->cursor_window].position.y
        = g_wldcore_window_render_records[level->option_render].base.y + 0x3c + level->cursor_row * 0x10;
    wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
}
