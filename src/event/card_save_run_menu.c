#include "fft/battle_gfx.h"
#include "fft/event_card.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * The save screen: the CARD overlay's entry for companion overlay state 5,
 * called from the event frame loops of BATTLE and WORLD.
 *
 * Builds its graphics context on its own stack frame, starts the save music
 * and the backdrop thread, and runs one frame per iteration until the closing
 * fade completes: the handler of g_card_save_menu_state (initial prompt, card
 * selection, scan, scan result, slot list, write), the window frames, the
 * event input update and the frame's sound effect. A LEAVE state fades the
 * music, waits for the pending card operation and starts the closing fade.
 * The event input is the held d-pad plus the newly pressed buttons; the face
 * and shoulder buttons are passed on only while a text, child-menu or
 * slot-14 thread runs. The save-file directory buffer also lives on this frame.
 */
void card_save_run_menu(s32 unused) {
    TILE tile_storage[20];
    u32 otag_storage[0x80];
    card_graphics_context_t context_storage[2];
    /* The retail frame reserves 16 bytes beyond the two contexts. */
    u8 unused_context_tail[0x10];
    s32 leading_alignment;
    s32 input;
    s32 unused_local;
    s32 buttons;
    u16 menu_input;
    u16 alignment;
    s16 music_slot;
    DIRENTRY save_buffer[15];

    g_gfx_otag_count = 0x40;
    g_card_gfx_poly_ft4_capacity = 0x258;
    g_card_gfx_tile_capacity = 10;
    card_gfx_init_contexts(context_storage, otag_storage, 0, 0, 0, g_card_gfx_poly_ft4_buffers, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, tile_storage, 0, 0, 0, 0, 0, 0, 0, 0);
    card_save_init_menu_graphics();
    music_slot = main_sound_open_music_into_free_slot(MAIN_MUSIC_SAVE);
    while (main_sound_is_music_loading_stub() != 0) { }
    main_sound_switch_music_track(music_slot, 0x7f, 0);
    card_io_consume_all_events();
    g_card_save_menu_work_buffer = save_buffer;
    card_save_init_menu_state();
    thread_start(THREAD_BORROWED_SLOT_CARD_BACKDROP, card_save_run_menu_graphic_thread);
    thread_set_parameters(THREAD_BORROWED_SLOT_CARD_BACKDROP, 0, 0, 0);
    g_card_save_initial_prompt_timer = 0;

    while (gfx_update_increasing_fade() != 0) {
        card_gfx_submit_frame_and_swap_buffers(0, 0);
        card_input_update_suppressed_during_fade();
        gfx_update_decreasing_fade();
        gfx_enqueue_tile(&g_card_gfx_fullscreen_fade_rect, &g_card_gfx_fade_overlay_color, 0, 0);

        menu_input = g_card_save_menu_state == CARD_SAVE_MENU_SELECT_SAVE
                && thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) == 0
            ? g_menu_input_primary_repeat
            : 0;
        if (g_card_save_menu_state >= CARD_SAVE_MENU_SELECT_SAVE && g_card_save_menu_state <= CARD_SAVE_MENU_WRITING) {
            card_save_update_and_draw_slot_list(menu_input);
        }

        if (g_card_save_menu_state == CARD_SAVE_MENU_INITIAL_PROMPT) {
            card_save_update_initial_prompt();
        } else if (g_card_save_menu_state == CARD_SAVE_MENU_SELECT_CARD) {
            card_save_update_card_slot_selection();
        } else if (g_card_save_menu_state == CARD_SAVE_MENU_SCANNING) {
            card_save_update_slot_scan();
        } else if (g_card_save_menu_state == CARD_SAVE_MENU_SCAN_RESULT) {
            card_save_update_slot_scan_result();
        } else if (g_card_save_menu_state == CARD_SAVE_MENU_WRITING) {
            card_save_update_slot_write(g_card_save_list_selected_slot);
        } else if (g_card_save_menu_state == CARD_SAVE_MENU_LEAVE
            || g_card_save_menu_state == CARD_SAVE_MENU_LEAVE_ALT) {
            main_sound_set_current_music_target(0, 0xf0);
            while (card_io_poll_operation_result() == CARD_IO_RESULT_PENDING) { }
            card_gfx_start_increasing_fade();
            g_card_save_menu_state = CARD_SAVE_MENU_CLOSING;
        }

        card_gfx_set_render_otag(g_card_gfx_context->otag + 1);
        g_gfx_current_otag_entry = g_card_gfx_context->otag + 0x3c;
        menu_draw_active_window_frames_variant();
        input = PadRead(0) & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT);
        buttons = g_menu_input_newly_pressed & 0x0fff;
        input |= buttons;
        if (thread_is_running(THREAD_SCREEN_SLOT_TEXT) == 0 && thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) == 0
            && thread_is_running(14) == 0) {
            input &= 0xff00;
        }
        menu_input_update_event_state_variant(
            g_card_gfx_context->otag + 0x3c, input, g_card_gfx_drawenv_y == 0 ? 0xf0 : 0);

        if (g_sound_pending_sfx_id == MAIN_SFX_CURSOR_MOVE || g_sound_pending_sfx_id == MAIN_SFX_CANCEL) {
            g_card_sound_queued_effect_id = g_sound_pending_sfx_id;
        }
        if (g_sound_pending_sfx_id < 0 || g_sound_suppressed != 0) {
            g_sound_pending_sfx_id = MAIN_SFX_NONE;
        }
        input = g_sound_pending_sfx_id != MAIN_SFX_NONE ? g_sound_pending_sfx_id : g_card_sound_queued_effect_id;
        if (input != MAIN_SFX_NONE) {
            main_sound_play_sfx(input);
        }
        g_card_sound_queued_effect_id = MAIN_SFX_NONE;
        g_sound_pending_sfx_id = MAIN_SFX_NONE;
    }

    thread_set_parameters(THREAD_BORROWED_SLOT_CARD_BACKDROP, 0, -1, 0);
    main_sound_unload_scenario_mus(music_slot);
}
