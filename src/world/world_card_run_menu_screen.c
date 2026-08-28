#include "fft/data.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Stack-resident primitive pools for both WORLD packet buffers. Each pool
 * holds two frames of the per-type counts this routine publishes, which is
 * what fixes the offsets of the pool arguments passed to
 * world_gfx_init_double_packet_buffers. */
typedef struct world_card_screen_packets {
    POLY_FT4 textured_quads[2 * 600];     /* 0x0000: g_world_gfx_textured_quad_capacity = 600 */
    TILE tiles[2 * 10];                   /* 0xbb80: g_world_gfx_tile_capacity = 10 */
    DR_MOVE draw_moves[2 * 30];           /* 0xbcc0: g_world_gfx_draw_move_capacity = 30 */
    DR_MODE draw_modes[2 * 30];           /* 0xc260: g_world_gfx_draw_mode_capacity = 30 */
    DR_AREA draw_areas[2 * 10];           /* 0xc530: g_world_gfx_draw_area_capacity = 10 */
    u32 otags[2 * 64];                    /* 0xc620: g_world_ot_length = 64 */
    world_gfx_packet_buffer_t buffers[2]; /* 0xc820: the two packet-buffer headers */
    u8 unknown_c9f8[0x10];                /* 0xc9f8: gap before the published work area */
    u8 directory_buffer[0x258];           /* 0xca08: published through g_world_card_directory_buffer */
} world_card_screen_packets_t;

extern s8 g_card_save_occupied_slot_count;

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
s32 world_card_run_menu_screen(s32 slot_count) {
    world_card_screen_packets_t packets;
    s32 music_handle;
    s32 input;
    s32 buttons;
    s32 step;

    g_world_ot_length = 0x40;
    g_world_gfx_textured_quad_capacity = 0x258;
    g_world_gfx_poly_f3_capacity = 0;
    g_world_gfx_poly_ft3_capacity = 0;
    g_world_gfx_poly_f4_capacity = 0;
    g_world_gfx_poly_g3_capacity = 0;
    g_world_gfx_poly_gt3_capacity = 0;
    g_world_gfx_gradient_quad_capacity = 0;
    g_world_gfx_textured_gradient_quad_capacity = 0;
    g_world_gfx_tiles_24_capacity = 0;
    g_world_gfx_line_f3_capacity = 0;
    g_world_gfx_line_f4_capacity = 0;
    g_world_gfx_gradient_line_capacity = 0;
    g_world_gfx_line_g3_capacity = 0;
    g_world_gfx_line_g4_capacity = 0;
    g_world_gfx_tile_capacity = 10;
    g_world_gfx_tile_1_capacity = 0;
    g_world_gfx_tile_8_capacity = 0;
    g_world_gfx_tile_16_capacity = 0;
    g_world_gfx_sprite_capacity = 0;
    g_world_gfx_sprite_8_capacity = 0;
    g_world_gfx_sprite_16_capacity = 0;
    g_world_gfx_draw_move_capacity = 30;
    g_world_gfx_draw_area_capacity = 10;
    g_world_gfx_draw_mode_capacity = 30;
    world_gfx_init_double_packet_buffers(packets.buffers, packets.otags, 0, 0, 0, packets.textured_quads, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, packets.tiles, 0, 0, 0, 0, 0, 0, packets.draw_moves, packets.draw_areas, packets.draw_modes);
    world_formation_init_menu_display();
    g_world_formation_unit_count = world_formation_build_record_list(0, g_world_formation_unit_pointers, 0);
    g_world_text_section_pointers[27] = g_world_card_slot_text_table;
    world_gfx_bind_data_pointer(3);
    g_world_card_menu_step = 0;
    /* WORLD stores the CARD slot count as a halfword. */
    *(s16*)&g_card_save_occupied_slot_count = slot_count;
    music_handle = main_sound_open_music_into_free_slot(0x29);
    while (main_return_zero_80043708() != 0) { }
    main_sound_switch_music_track((s16)music_handle, 0x7f, 0);
    world_card_clear_events();
    g_world_card_directory_buffer = packets.directory_buffer;
    world_card_init_menu_state();
    world_thread_start(2, world_formation_view_zoom_thread);
    world_thread_set_parameters(2, 0, 0, 0);
    while (world_gfx_update_fade_out_tile() != 0) {
        g_world_script_tutorial_id = 0;
        world_gfx_present_frame_and_swap_packet_buffer(0, 0);
        world_thread_update_task_state_2();
        world_gfx_update_fade_in_tile();
        world_gfx_append_tile_to_otag(&g_world_card_screen_background_tile, g_world_gfx_black_tile_color, 0, 0);
        if (g_world_card_menu_step == 3 && world_thread_is_running(5) == 0) {
            input = (u16)g_world_input_primary_repeat;
        } else {
            input = 0;
        }
        if ((u32)(g_world_card_menu_step - 3) < 3) {
            world_card_update_and_draw_slot_list(input);
        }
        step = g_world_card_menu_step;
        if (step == 0) {
            world_card_run_slot_select_step();
        } else if (step == 2) {
            world_card_update_slot_scan();
        } else if (step == 1) {
            world_card_run_mode_select_step();
        } else if (step == 4) {
            world_card_run_load_step(g_world_card_active_slot);
        } else if (step == 5) {
            world_card_run_save_step(g_world_card_active_slot);
        } else if (step == 9) {
            main_sound_set_current_music_target(0, 0xf0);
            while (world_card_check_selected() == -1) { }
            world_gfx_start_increasing_fade();
            g_world_card_menu_step = 0xff;
        }
        world_formation_set_ordering_table_pointer(g_world_gfx_active_packet_buffer->otag + 1);
        g_world_gfx_active_otag_entries = g_world_gfx_active_packet_buffer->otag + 60;
        world_menu_draw_active_window_frames();
        buttons = PadRead(0) & 0xf000;
        buttons |= g_world_input_newly_pressed & 0xfff;
        if (world_thread_is_running(1) == 0 && world_thread_is_running(5) == 0 && world_thread_is_running(0xf) == 0) {
            buttons &= 0xff00;
        }
        world_script_update_event_frame_input(
            g_world_gfx_active_packet_buffer->otag + 60, buttons, g_world_gfx_draw_buffer_clip_y == 0 ? 0xf0 : 0);
        world_sound_dispatch_effect();
    }
    world_thread_set_parameters(2, 0, -1, 0);
    world_free_work_buffer();
    world_noop_801325d4();
    main_sound_unload_scenario_mus((s16)music_handle);
    world_formation_reset_menu_context();
    world_text_generate_formation_unit_name_string();
    return g_world_card_menu_result;
}
