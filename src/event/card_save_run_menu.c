#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/card.h"
#include "fft/main_sound.h"
#include "psx/pad.h"
#include "psx/types.h"

void card_save_run_menu(s32 unused) {
    u8 tile_storage[0x140];
    u8 otag_storage[0x200];
    u8 context_storage[0x1f8];
    volatile s32 leading_alignment;
    s32 input;
    s32 unused_local;
    s32 buttons;
    u16 menu_input;
    volatile u16 alignment;
    s16 music_slot;
    u8 save_buffer[0x258];

    g_card_gfx_otag_length = 0x40;
    g_card_gfx_poly_ft4_capacity = 0x258;
    g_card_gfx_tile_capacity = 10;
    /* The definition takes 17 parameters; this call site passes 25. The extra
     * eight trailing words are not read by the routine, so card.h declares the
     * definition's prototype and the call keeps its original 25-argument codegen
     * through the function-pointer cast. */
    ((void (*)(void*, void*, s32, s32, s32, void*, s32, s32, s32, s32, s32, s32, s32, s32, s32, s32, void*, s32, s32,
        s32, s32, s32, s32, s32, s32))card_gfx_init_contexts)(context_storage, otag_storage, 0, 0, 0,
        g_card_gfx_poly_ft4_buffers, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, tile_storage, 0, 0, 0, 0, 0, 0, 0, 0);
    card_save_init_menu_graphics();
    music_slot = main_sound_open_music_into_free_slot(0x29);
    while (main_return_zero_80043708() != 0) { }
    main_sound_switch_music_track(music_slot, 0x7f, 0);
    card_io_consume_all_events();
    g_card_save_menu_work_buffer = (card_directory_entry_t*)save_buffer;
    card_save_init_menu_state();
    battle_thread_start(2, card_save_run_menu_graphic_thread);
    battle_thread_set_parameters(2, 0, 0, 0);
    g_card_save_initial_prompt_timer = 0;

    while (card_gfx_update_increasing_fade() != 0) {
        card_gfx_submit_frame_and_swap_buffers(0, 0);
        card_input_update_suppressed_during_fade();
        card_gfx_update_decreasing_fade();
        card_gfx_enqueue_translucent_tile(&g_card_gfx_fullscreen_fade_rect, g_card_gfx_fade_overlay_color, 0, 0);

        menu_input = g_card_save_menu_state == CARD_SAVE_MENU_SELECT_SAVE && battle_thread_is_running(5) == 0
            ? *(u16*)&g_card_input_primary_repeat
            : 0;
        if (g_card_save_menu_state >= CARD_SAVE_MENU_SELECT_SAVE
            && g_card_save_menu_state < CARD_SAVE_MENU_WRITING + 1) {
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
        } else if (g_card_save_menu_state == 10 || g_card_save_menu_state == 9) {
            main_sound_set_current_music_target(0, 0xf0);
            while (card_io_poll_operation_result() == CARD_IO_RESULT_PENDING) { }
            card_gfx_start_increasing_fade();
            g_card_save_menu_state = 0xff;
        }

        card_gfx_set_render_otag(g_card_gfx_context->otag + 1);
        g_current_otag_entry = g_card_gfx_context->otag + 0x3c;
        battle_menu_draw_active_window_frames();
        input = PadRead(0) & (PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_DOWN | PSX_PAD_LEFT);
        buttons = g_card_input_newly_pressed & 0x0fff;
        input |= buttons;
        if (battle_thread_is_running(1) == 0 && battle_thread_is_running(5) == 0 && battle_thread_is_running(14) == 0) {
            input &= 0xff00;
        }
        card_input_update_event_state(g_card_gfx_context->otag + 0x3c, input, g_card_gfx_drawenv_y == 0 ? 0xf0 : 0);

        if (g_sound_effect_id_to_play == MAIN_SFX_CURSOR_MOVE || g_sound_effect_id_to_play == MAIN_SFX_CANCEL) {
            g_card_sound_queued_effect_id = g_sound_effect_id_to_play;
        }
        if (g_sound_effect_id_to_play < 0 || g_battle_sound_suppressed != 0) {
            g_sound_effect_id_to_play = 0;
        }
        input = g_sound_effect_id_to_play != 0 ? g_sound_effect_id_to_play : g_card_sound_queued_effect_id;
        if (input != 0) {
            main_sound_play_sfx(input);
        }
        g_card_sound_queued_effect_id = 0;
        g_sound_effect_id_to_play = 0;
    }

    battle_thread_set_parameters(2, 0, -1, 0);
    main_sound_unload_scenario_mus(music_slot);
}
