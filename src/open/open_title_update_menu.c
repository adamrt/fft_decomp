#include "fft/open.h"
#include "fft/script.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Per-frame handler for the OPEN title menu controller.
 *
 * While the attract CD track plays it watches the current sector and stops
 * the drive at the record's end position. Once the menu is idle for 601
 * frames it fades out and starts the next demo movie; a confirm press fades
 * out and dispatches on the cursor (new game, continue, options), and the
 * directional helpers move the highlight between the four entries.
 */
void open_title_update_menu(open_controller_record_t* record) {
    open_title_controller_t* state = &record->title;
    u8 loc[8];
    s32 status;
    s32 remaining;
    s32 flags;
    s32 demo;
    s32 start_sector;
    s32 stream_length;
    s32 end_frame;
    s32 cd_mix_level;

    if (g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE) {
        return;
    }

    if (state->exiting != 0) {
        if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING) {
            state->exit_timer--;
            if (state->exit_timer == 3) {
                CdControl(CdlPause, 0, 0);
            }
            remaining = state->exit_timer;
            if (remaining == 2) {
                status = CdSync(1, loc);
                if (status == 5) {
                    CdControl(CdlPause, 0, 0);
                } else if (status != remaining) {
                    state->exit_timer++;
                }
            }
            if (state->exit_timer != 0) {
                return;
            }
            state->exiting = 0;
            g_open_system_runtime_flags ^= OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING;
            return;
        }
        state->exiting = 0;
        return;
    }

    if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING) {
        CdControlB(CdlGetlocL, 0, loc);
        if (CdPosToInt((CdlLOC*)loc) >= state->cd_end_position) {
            open_sound_set_cd_mix_and_volume(0, 0x20);
            state->exit_timer = 0x20;
            state->exiting = 1;
            return;
        }
    }

    if (state->state == OPEN_TITLE_STATE_DEMO_MOVIE) {
        if (g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE) {
            return;
        }
        g_active_graphics_buffer_index = 1;
        g_open_system_runtime_flags ^= OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED;
        g_open_gfx_next_render_record_36 -= 6;
        g_open_gfx_render_record_pointer_count -= 6;
        demo = g_open_title_demo_movie_index;
        g_open_current_controller_index--;
        switch (demo) {
        case 1:
            start_sector = DISC_SECTOR_OPEN_FFTPR_STR;
            stream_length = 0x1d60;
            end_frame = 0x2b7;
            cd_mix_level = 0x7a;
            break;
        case 2:
            start_sector = DISC_SECTOR_OPEN_FFTUNIT_STR;
            stream_length = 0x8691;
            end_frame = 0xd4d;
            cd_mix_level = 0x73;
            break;
        default:
            start_sector = DISC_SECTOR_OPEN_FFTPRE_STR;
            stream_length = 0x38d0;
            end_frame = 0x54e;
            cd_mix_level = 0x85;
            break;
        }
        open_movie_stream_title_demo_and_push_controller(start_sector, stream_length, end_frame, cd_mix_level);
        g_open_title_demo_movie_index++;
        if (g_open_title_demo_movie_index >= 3) {
            g_open_title_demo_movie_index = 0;
        }
        return;
    }

    if (state->state == OPEN_TITLE_STATE_SOUND_MENU) {
        state->state = OPEN_TITLE_STATE_MENU;
        open_gfx_hide_four_render_records(state);
        open_menu_push_sound_type_controller();
        return;
    }

    flags = g_open_system_runtime_flags;
    if (flags & OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED) {
        s32 cursor;

        if (g_open_gfx_overlay_fade.flags & OPEN_OVERLAY_FADE_ACTIVE) {
            return;
        }
        g_open_system_runtime_flags = flags ^ OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED;
        open_movie_pause_cd_audio();
        g_open_system_runtime_flags &= ~OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN;
        open_gfx_init_screen_environments(1);
        cursor = state->cursor;
        switch (cursor) {
        case OPEN_TITLE_MENU_NEW_GAME:
            g_main_debug_display_enabled = 0;
            g_open_system_runtime_flags
                &= ~(OPEN_SYSTEM_RUNTIME_FLAG_WORLD_OVERLAY_LOADED | OPEN_SYSTEM_RUNTIME_FLAG_PARTY_CHOSEN);
            g_open_gfx_next_render_record_36 -= 6;
            g_open_gfx_render_record_pointer_count -= 6;
            g_open_current_controller_index--;
            open_title_start_new_game_or_clear_file_buffer();
            return;
        case OPEN_TITLE_MENU_CONTINUE:
            g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
            open_title_init_new_game_party(1, 1);
            DrawSync(0);
            VSync(0);
            SetDispMask(1);
            if (world_card_run_menu_screen(0) == 0) {
                g_open_file_current_openbk_image_id = -1;
                open_file_build_openbk_header(0, g_open_file_destination);
                file_wait_for_pending_variant();
                g_open_gfx_next_render_record_36 -= 6;
                g_open_gfx_render_record_pointer_count -= 6;
                g_open_current_controller_index--;
                SetDispMask(0);
                g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED;
                open_title_push_menu_controller(0);
                return;
            }
            g_open_system_runtime_flags ^= OPEN_SYSTEM_RUNTIME_FLAG_RUNNING;
            if (script_get_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT) != 0
                || script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) == 0) {
                g_open_system_result = cursor;
                return;
            }
            g_open_system_result = 0;
            return;
        case OPEN_TITLE_MENU_TUTORIAL:
            open_title_init_new_game_party(0, 1);
            g_open_system_result = 2;
            g_open_gfx_next_render_record_36 -= 6;
            g_open_gfx_render_record_pointer_count -= 6;
            g_open_current_controller_index--;
            g_main_system_flags |= MAIN_SYSTEM_FLAG_TITLE_TUTORIAL;
            g_open_system_runtime_flags ^= OPEN_SYSTEM_RUNTIME_FLAG_RUNNING;
            return;
        }
        return;
    }

    state->idle_timer++;
    if (!(flags & OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING) && state->idle_timer >= 0x259) {
        state->state = OPEN_TITLE_STATE_DEMO_MOVIE;
        g_open_gfx_overlay_fade.ot_index = 2;
        open_gfx_start_overlay_fade_in(0x20);
        g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED;
        return;
    }

    if (g_open_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_START)) {
        open_sound_set_cd_mix_and_volume(0, 0x64);
        state->exit_timer = 0x32;
        state->exiting = 1;
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
        switch (state->cursor) {
        case OPEN_TITLE_MENU_NEW_GAME:
        case OPEN_TITLE_MENU_CONTINUE:
        case OPEN_TITLE_MENU_TUTORIAL:
            g_open_gfx_overlay_fade.ot_index = 2;
            open_gfx_start_overlay_fade_in(0x20);
            g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED;
            return;
        case OPEN_TITLE_MENU_SOUND:
            state->state = OPEN_TITLE_STATE_SOUND_MENU;
            return;
        }
        return;
    }

    if (open_input_update_repeat(PSX_PAD_UP) != 0) {
        state->idle_timer = 0;
        g_open_gfx_render_records_36[state->records_36[state->cursor + 1]].palette = 0;
        state->cursor--;
        if (state->cursor < 0) {
            state->cursor = OPEN_TITLE_MENU_SOUND;
        }
        g_open_gfx_render_records_36[state->records_36[state->cursor + 1]].palette
            = OPEN_MENU_ENTRY_HIGHLIGHTED_PALETTE;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }
    if (open_input_update_repeat(PSX_PAD_DOWN) != 0 || (g_open_input_new_button_presses & PSX_PAD_SELECT)) {
        state->idle_timer = 0;
        g_open_gfx_render_records_36[state->records_36[state->cursor + 1]].palette = 0;
        state->cursor++;
        if (state->cursor >= 4) {
            state->cursor = OPEN_TITLE_MENU_NEW_GAME;
        }
        g_open_gfx_render_records_36[state->records_36[state->cursor + 1]].palette
            = OPEN_MENU_ENTRY_HIGHLIGHTED_PALETTE;
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
    }
}
