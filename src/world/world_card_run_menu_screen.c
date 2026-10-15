#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Run the WORLD memory-card screen until its fade-out completes.
 *
 * Publishes the packet-pool counts for this screen, binds both packet buffers
 * onto a stack-resident pool block, plays the card-screen music and steps the
 * card menu state machine once per frame. Returns g_world_card_menu_result.
 *
 * The count stores keep their source order: the 30 and 10 counts are shared
 * constants whose overlapping lifetimes put the 10 in $t0, and sched2 then
 * hoists only the $v0 stores.
 */
s32 world_card_run_menu_screen(s32 mode_select_enabled) {
    POLY_FT4 textured_quads[2 * 600];
    TILE tiles[2 * 10];
    DR_MOVE draw_moves[2 * 30];
    DR_MODE draw_modes[2 * 30];
    DR_AREA draw_areas[2 * 10];
    u32 otags[2 * 64];
    menu_gfx_context_t buffers[2];
    u8 unused_packet_gap[0x10]; /* Retains the original unused stack space. */
    DIRENTRY directory_buffer[CARD_SAVE_SLOT_COUNT];
    s16 music_handle;
    u16 input;
    s32 buttons;
    s32 step;

    g_world_ot_length = 0x40;
    g_world_gfx_textured_quad_capacity = 0x258;
    g_gfx_poly_f3_capacity = 0;
    g_gfx_poly_ft3_capacity = 0;
    g_gfx_poly_f4_capacity = 0;
    g_gfx_poly_g3_capacity = 0;
    g_gfx_poly_gt3_capacity = 0;
    g_world_gfx_gradient_quad_capacity = 0;
    g_world_gfx_textured_gradient_quad_capacity = 0;
    g_world_gfx_flat_line_capacity = 0;
    g_gfx_line_f3_capacity = 0;
    g_gfx_line_f4_capacity = 0;
    g_world_gfx_gradient_line_capacity = 0;
    g_world_gfx_line_g3_capacity = 0;
    g_gfx_line_g4_capacity = 0;
    g_gfx_tile_capacity = 10;
    g_gfx_tile_1_capacity = 0;
    g_gfx_tile_8_capacity = 0;
    g_gfx_tile_16_capacity = 0;
    g_gfx_sprite_capacity = 0;
    g_gfx_sprite_8_capacity = 0;
    g_gfx_sprite_16_capacity = 0;
    g_world_gfx_draw_move_capacity = 30;
    g_world_gfx_draw_area_capacity = 10;
    g_gfx_draw_mode_capacity = 30;
    world_gfx_init_double_packet_buffers(buffers, otags, 0, 0, 0, textured_quads, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, tiles,
        0, 0, 0, 0, 0, 0, draw_moves, draw_areas, draw_modes);
    world_formation_init_menu_display();
    g_world_formation_unit_count
        = world_formation_build_record_list(0, g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_ALL);
    g_text_section_pointers[TEXT_SECTION_OVERLAY_SCREEN] = g_world_card_slot_text_table;
    world_text_bind_section_data(WORLD_TEXT_DATA_FORMATION_MESSAGES);
    g_world_card_menu_step = WORLD_CARD_MENU_STEP_SELECT_CARD;
    /* 1 from the world menu, 0 from the title screen's Continue (load only). */
    g_world_card_mode_select_enabled = mode_select_enabled;
    music_handle = main_sound_open_music_into_free_slot(MAIN_MUSIC_SAVE);
    while (main_sound_is_music_loading_stub() != 0) { }
    main_sound_switch_music_track(music_handle, 0x7f, 0);
    card_io_consume_all_events();
    g_world_card_directory_buffer = directory_buffer;
    world_card_init_menu_state();
    thread_start(THREAD_BORROWED_SLOT_CARD_BACKDROP, world_formation_view_zoom_thread);
    thread_set_parameters(THREAD_BORROWED_SLOT_CARD_BACKDROP, 0, 0, 0);
    while (world_gfx_update_fade_out_tile() != 0) {
        g_world_script_tutorial_id = 0;
        world_gfx_present_frame_and_swap_packet_buffer(0, 0);
        world_card_input_update_with_message_state();
        world_gfx_update_fade_in_tile();
        gfx_enqueue_tile(&g_world_card_screen_background_tile, &g_world_gfx_black_tile_color, 0, 0);
        if (g_world_card_menu_step == WORLD_CARD_MENU_STEP_SELECT_SAVE
            && thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) == 0) {
            input = g_menu_input_primary_repeat;
        } else {
            input = 0;
        }
        if (g_world_card_menu_step >= 3 && g_world_card_menu_step <= 5) {
            world_card_update_and_draw_slot_list(input);
        }
        step = g_world_card_menu_step;
        if (step == WORLD_CARD_MENU_STEP_SELECT_CARD) {
            world_card_run_slot_select_step();
        } else if (step == WORLD_CARD_MENU_STEP_SCANNING) {
            world_card_update_slot_scan();
        } else if (step == WORLD_CARD_MENU_STEP_SELECT_MODE) {
            world_card_run_mode_select_step();
        } else if (step == WORLD_CARD_MENU_STEP_LOADING) {
            world_card_run_load_step(g_world_card_active_slot);
        } else if (step == WORLD_CARD_MENU_STEP_SAVING) {
            world_card_run_save_step(g_world_card_active_slot);
        } else if (step == WORLD_CARD_MENU_STEP_LEAVE) {
            main_sound_set_current_music_target(0, 0xf0);
            while (card_poll_info_result_variant() == CARD_IO_RESULT_PENDING) { }
            world_gfx_start_increasing_fade();
            g_world_card_menu_step = WORLD_CARD_MENU_STEP_LEFT;
        }
        world_formation_set_ordering_table_pointer(g_menu_gfx_context->otag + 1);
        g_gfx_current_otag_entry = g_menu_gfx_context->otag + 60;
        menu_draw_active_window_frames_variant();
        buttons = PadRead(0) & PSX_PAD_DPAD_MASK;
        buttons |= g_menu_input_newly_pressed & 0xfff;
        if (thread_is_running(THREAD_SCREEN_SLOT_TEXT) == 0 && thread_is_running(THREAD_SCREEN_SLOT_MENU_CHILD) == 0
            && thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU) == 0) {
            buttons &= 0xff00;
        }
        menu_input_update_event_state_variant(
            g_menu_gfx_context->otag + 60, buttons, g_world_gfx_draw_buffer_clip_y == 0 ? 0xf0 : 0);
        world_sound_dispatch_effect();
    }
    thread_set_parameters(THREAD_BORROWED_SLOT_CARD_BACKDROP, 0, -1, 0);
    world_free_work_buffer();
    world_card_close_stub();
    main_sound_unload_scenario_mus(music_handle);
    world_formation_reset_menu_context();
    world_text_generate_formation_unit_name_string();
    return g_world_card_menu_result;
}
