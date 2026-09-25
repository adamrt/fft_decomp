#include "fft/open.h"
#include "psx/cd.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef struct open_title_controller {
    /* 0x00 */ s32 header_record_36;
    /* 0x04 */ s32 option_records_36[4];
    /* 0x14 */ s32 idle_timer;
    /* 0x18 */ s32 cd_end_position;
    /* 0x1c */ s32 cursor;
    /* 0x20 */ s32 state;
    /* 0x24 */ s32 field_24[3];
    /* 0x30 */ s32 exit_timer;
    /* 0x34 */ s32 exiting;
    /* 0x38 */ u8 padding_38[0x64 - 0x38];
} open_title_controller_t;

/* Per-frame handler for the OPEN title menu controller.
 *
 * While the attract CD track plays it watches the current sector and stops
 * the drive at the record's end position. Once the menu is idle for 601
 * frames it fades out and starts the next demo movie; a confirm press fades
 * out and dispatches on the cursor (new game, continue, options), and the
 * directional helpers move the highlight between the four entries.
 */
void open_title_update_menu(open_title_controller_t* state) {
    u8 loc[8];
    s32 status;
    s32 remaining;
    s32 flags;
    s32 demo;
    s32 start_sector;
    s32 stream_length;
    s32 end_frame;
    s32 frame_rate;

    if (g_open_gfx_overlay_fade.flags & 4) {
        return;
    }

    if (state->exiting != 0) {
        if (g_open_system_runtime_flags & 4) {
            state->exit_timer--;
            if (state->exit_timer == 3) {
                CdControl(9, 0, 0);
            }
            remaining = state->exit_timer;
            if (remaining == 2) {
                status = CdSync(1, loc);
                if (status == 5) {
                    CdControl(9, 0, 0);
                } else if (status != remaining) {
                    state->exit_timer++;
                }
            }
            if (state->exit_timer != 0) {
                return;
            }
            state->exiting = 0;
            g_open_system_runtime_flags ^= 4;
            return;
        }
        state->exiting = 0;
        return;
    }

    if (g_open_system_runtime_flags & 4) {
        CdControlb(0x10, 0, loc);
        if (CdPosToInt(loc) >= state->cd_end_position) {
            open_sound_set_type_and_volume(0, 0x20);
            state->exit_timer = 0x20;
            state->exiting = 1;
            return;
        }
    }

    if (state->state == 3) {
        if (g_open_gfx_overlay_fade.flags & 4) {
            return;
        }
        g_active_graphics_buffer_index = 1;
        g_open_system_runtime_flags ^= 0x10;
        g_open_gfx_next_render_record_36 -= 6;
        g_open_gfx_render_record_pointer_count -= 6;
        demo = g_open_title_demo_movie_index;
        g_open_current_controller_index--;
        switch (demo) {
        case 1:
            start_sector = 0x25160;
            stream_length = 0x1d60;
            end_frame = 0x2b7;
            frame_rate = 0x7a;
            break;
        case 2:
            start_sector = 0x26ec0;
            stream_length = 0x8691;
            end_frame = 0xd4d;
            frame_rate = 0x73;
            break;
        default:
            start_sector = 0x1e038;
            stream_length = 0x38d0;
            end_frame = 0x54e;
            frame_rate = 0x85;
            break;
        }
        open_movie_stream_other_str_and_push_controller_11(start_sector, stream_length, end_frame, frame_rate);
        g_open_title_demo_movie_index++;
        if (g_open_title_demo_movie_index >= 3) {
            g_open_title_demo_movie_index = 0;
        }
        return;
    }

    if (state->state == 2) {
        state->state = 0;
        open_gfx_show_four_render_records((const open_birthday_render_record_indices_t*)state);
        open_menu_push_sound_type_controller();
        return;
    }

    flags = g_open_system_runtime_flags;
    if (flags & 0x10) {
        s32 cursor;

        if (g_open_gfx_overlay_fade.flags & 4) {
            return;
        }
        g_open_system_runtime_flags = flags ^ 0x10;
        open_movie_pause_cd_audio();
        g_open_system_runtime_flags &= ~OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN;
        open_gfx_init_screen_environments(1);
        cursor = state->cursor;
        switch (cursor) {
        case 0:
            g_main_debug_display_enabled = 0;
            g_open_system_runtime_flags &= ~0x180;
            g_open_gfx_next_render_record_36 -= 6;
            g_open_gfx_render_record_pointer_count -= 6;
            g_open_current_controller_index--;
            open_title_start_new_game_or_clear_file_buffer();
            return;
        case 1:
            g_open_system_runtime_flags |= 0x40;
            open_title_init_new_game_party(1, 1);
            DrawSync(0);
            VSync(0);
            SetDispMask(1);
            if (world_card_run_menu_screen(0) == 0) {
                g_open_file_current_openbk_image_id = -1;
                open_file_build_openbk_header(0, g_open_file_destination);
                open_file_wait_for_pending();
                g_open_gfx_next_render_record_36 -= 6;
                g_open_gfx_render_record_pointer_count -= 6;
                g_open_current_controller_index--;
                SetDispMask(0);
                g_open_system_runtime_flags |= 0x40;
                open_title_push_menu_controller(0);
                return;
            }
            g_open_system_runtime_flags ^= 1;
            if (world_script_get_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT) != 0
                || world_script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) == 0) {
                g_open_system_result = cursor;
                return;
            }
            g_open_system_result = 0;
            return;
        case 2:
            open_title_init_new_game_party(0, 1);
            g_open_system_result = 2;
            g_open_gfx_next_render_record_36 -= 6;
            g_open_gfx_render_record_pointer_count -= 6;
            g_open_current_controller_index--;
            g_main_system_flags |= 0x80000;
            g_open_system_runtime_flags ^= 1;
            return;
        }
        return;
    }

    state->idle_timer++;
    if (!(flags & 4) && state->idle_timer >= 0x259) {
        state->state = 3;
        g_open_gfx_overlay_fade.ot_index = 2;
        open_gfx_start_overlay_fade_in(0x20);
        g_open_system_runtime_flags |= 0x10;
        return;
    }

    if (g_open_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_START)) {
        open_sound_set_type_and_volume(0, 0x64);
        state->exit_timer = 0x32;
        state->exiting = 1;
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        switch (state->cursor) {
        case 0:
        case 1:
        case 2:
            g_open_gfx_overlay_fade.ot_index = 2;
            open_gfx_start_overlay_fade_in(0x20);
            g_open_system_runtime_flags |= 0x10;
            return;
        case 3:
            state->state = 2;
            return;
        }
        return;
    }

    if (open_input_check_repeating_directional(PSX_PAD_UP) != 0) {
        state->idle_timer = 0;
        g_open_gfx_render_records_36[state->option_records_36[state->cursor]].palette = 0;
        state->cursor--;
        if (state->cursor < 0) {
            state->cursor = 3;
        }
        g_open_gfx_render_records_36[state->option_records_36[state->cursor]].palette = 9;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }
    if (open_input_check_repeating_directional(PSX_PAD_DOWN) != 0
        || (g_open_input_new_button_presses & PSX_PAD_SELECT)) {
        state->idle_timer = 0;
        g_open_gfx_render_records_36[state->option_records_36[state->cursor]].palette = 0;
        state->cursor++;
        if (state->cursor >= 4) {
            state->cursor = 0;
        }
        g_open_gfx_render_records_36[state->option_records_36[state->cursor]].palette = 9;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }
}
