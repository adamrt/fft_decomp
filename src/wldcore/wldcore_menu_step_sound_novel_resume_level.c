#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/gs.h"
#include "psx/pad.h"

/* Second link name for g_main_system_flags (0x8004d950), as in
 * wldcore_list_handle_tutorial_mask_1_entries_input.c. */
extern s32 g_main_system_flags_alias;

void world_gs_gettiminfo(u32* tim, GsIMAGE* im);
s32 wldcore_gfx_step_dissolve_image_upload(GsIMAGE* im, s32 step);
void wldcore_menu_push_sound_novel_level(s32 mode);
void wldcore_menu_push_treasure_detail_level(s32 mode);
void world_text_message_box_thread(void);

/* Input step for the type-0x14 sound-novel resume level.
 *
 * While dissolve_step is set the level is feeding the progressive texture upload at
 * 0x8007f998, one step per frame. Cross tears the level down through thread
 * 14; Circle fades the screen out and defers the teardown to the
 * system-flag-4 path; up and down toggle cursor_row, which moves the owned
 * window record down by 16.
 *
 * wldcore_reset_selected_saved_record takes no argument but the target still
 * loads g_wldcore_saved_record_index into a0, so the call is made through a
 * cast prototype.
 *
 * The system-flag test reads g_main_system_flags_alias, a second link name for
 * g_main_system_flags: the target re-loads the word for the `^= 4` instead of
 * reusing the tested value, and one symbol cannot produce both loads (a
 * `volatile` cast forces an unfolded `addiu` for the address and costs an
 * instruction).
 *
 * The window pointer and the base_y local give the final `addu` its operand
 * order (base first) while keeping the window index computed before the
 * render-record index. */
void wldcore_menu_step_sound_novel_resume_level(wldcore_menu_sound_novel_resume_level_t* level) {
    GsIMAGE image;
    s32 toggle;
    s32 index;
    s32 buttons;
    s32 base;
    wldcore_window_record_t* window;

    if (g_wldcore_window_render_records[level->message_render].flags & 0x100) {
        return;
    }
    if (level->dissolve_step != 0) {
        world_gs_gettiminfo(g_wldcore_picture_buffer + 1, &image);
        if (wldcore_gfx_step_dissolve_image_upload(&image, level->dissolve_step) == 0) {
            level->dissolve_step = 0;
            return;
        }
        level->dissolve_step = level->dissolve_step + 1;
        return;
    }
    if ((g_main_system_flags_alias & 0xc) == 4) {
        g_main_system_flags ^= 4;
        g_wldcore_window_record_count--;
        g_wldcore_window_render_record_count -= 3;
        g_wldcore_window_render_object_count -= 4;
        g_wldcore_menu_stack_depth--;
        if (level->cursor_row != 0) {
            ((void (*)(s16))wldcore_reset_selected_saved_record)(g_wldcore_saved_record_index);
        }
        wldcore_menu_push_sound_novel_level(level->mode);
        return;
    }
    buttons = g_wldcore_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        g_wldcore_window_record_count--;
        g_wldcore_window_render_record_count -= 3;
        g_wldcore_window_render_object_count -= 4;
        g_wldcore_menu_stack_depth--;
        world_thread_start(0xe, world_text_message_box_thread);
        world_thread_set_parameters(0xe, 0x19, 0xb848, 0);
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        wldcore_menu_push_treasure_detail_level(level->mode);
        return;
    }
    if (buttons & PSX_PAD_CIRCLE) {
        g_wldcore_window_records[level->cursor_window].sequence = 1;
        index = level->cursor_window;
        g_wldcore_window_records[index].anim_counter = 0;
        g_wldcore_window_records[index].frame_index = 0;
        wldcore_fade_start_screen(2, 0x10);
        g_main_system_flags |= 4;
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        return;
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_UP) == 0
        && wldcore_input_check_repeating_directional(PSX_PAD_DOWN) == 0) {
        return;
    }
    toggle = level->cursor_row ^ 1;
    level->cursor_row = toggle;
    window = &g_wldcore_window_records[level->cursor_window];
    base = g_wldcore_window_render_records[level->option_render].base_y;
    window->y = base + 0x3c + toggle * 0x10;
    wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
}
