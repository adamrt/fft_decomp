#include "fft/data.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef void (*world_formation_menu_handler_t)(void);

/* Stack-resident primitive pools for both WORLD packet buffers; each pool
 * holds two frames of the per-type counts this routine publishes. */
typedef struct world_formation_screen_packets {
    POLY_FT4 textured_quads[2 * 500];         /* 0x0000: g_world_gfx_textured_quad_capacity = 500 */
    POLY_G4 gradient_quads[2 * 90];           /* 0x9c40: g_world_gfx_gradient_quad_capacity = 90 */
    LINE_G2 gradient_lines[2 * 130];          /* 0xb590: g_world_gfx_gradient_line_capacity = 130 */
    TILE tiles_24[2 * 10];                    /* 0xc9e0: g_world_gfx_tiles_24_capacity = 10 */
    POLY_GT4 textured_gradient_quads[2 * 60]; /* 0xcb20: g_world_gfx_textured_gradient_quad_capacity = 60 */
    TILE tiles[2 * 30];                       /* 0xe380: g_world_gfx_tile_capacity = 30 */
    DR_MOVE draw_moves[2 * 30];               /* 0xe740: g_world_gfx_draw_move_capacity = 30 */
    DR_MODE draw_modes[2 * 30];               /* 0xece0: g_world_gfx_draw_mode_capacity = 30 */
    DR_AREA draw_areas[2 * 30];               /* 0xefb0: g_world_gfx_draw_area_capacity = 30 */
    u32 otags[2 * 64];                        /* 0xf280: g_world_ot_length = 64 */
    world_gfx_packet_buffer_t buffers[2];     /* 0xf480 */
    u8 unknown_f658[0x18];
} world_formation_screen_packets_t;

extern world_formation_menu_handler_t g_world_formation_menu_handlers[];

extern s32 world_formation_update_and_draw_unit_grid(s16, s16, s16, s16, s16, s32 (*)(), s32 (*)());

/*
 * Run the WORLD formation screen until g_world_formation_screen_running clears or the tutorial ends.
 *
 * Binds both packet buffers onto a stack-resident pool block, starts the
 * formation music unless a tutorial (1-10) is running, then steps the current
 * formation menu handler once per frame. On exit it waits for menu threads 1-16
 * to finish and saves the edited records back to party data.
 *
 * Compiled with the -O1 profile: the target leaves the packet-pool argument
 * stores unscheduled and reloads g_world_formation_unit_cycle_mode where -O2's cse-follow-jumps would
 * reuse it. i is one local reused for the button disabled flag, the otag
 * index and the thread loop counter, which keeps the (s16) extension.
 */
void world_formation_run_screen(s32 tutorial, s32 unused) {
    world_formation_screen_packets_t packets;
    s32 buttons;
    s32 i;
    u8 last_menu;

    g_world_ot_length = 0x40;
    g_world_gfx_textured_quad_capacity = 0x1F4;
    g_world_gfx_gradient_quad_capacity = 0x5A;
    g_world_gfx_textured_gradient_quad_capacity = 0x3C;
    g_world_gfx_tiles_24_capacity = 0xA;
    g_world_gfx_gradient_line_capacity = 0x82;
    g_world_gfx_tile_capacity = 0x1E;
    g_world_gfx_draw_move_capacity = 0x1E;
    g_world_gfx_draw_area_capacity = 0x1E;
    g_world_gfx_draw_mode_capacity = 0x1E;
    world_gfx_init_double_packet_buffers(packets.buffers, packets.otags, 0, 0, 0, packets.textured_quads, 0, 0,
        packets.gradient_quads, packets.textured_gradient_quads, packets.tiles_24, 0, 0, packets.gradient_lines, 0, 0,
        packets.tiles, 0, 0, 0, 0, 0, 0, packets.draw_moves, packets.draw_areas, packets.draw_modes);
    g_world_shop_id = -1;
    world_formation_init_menu_state();
    tutorial = -(tutorial < 11) & tutorial;
    world_script_start_tutorial(tutorial);
    if (tutorial == 0) {
        g_world_menu_music_slot = main_sound_open_music_into_free_slot(0x1F);
        while (main_return_zero_80043708() != 0) { }
        main_sound_switch_music_track(g_world_menu_music_slot, 0x7F, 0);
    }
    if (g_world_formation_screen_running != 0) {
        do {
            world_gfx_present_frame_and_swap_packet_buffer(0, 0);
            if (world_script_handle_tutorial_command() == 0) {
                break;
            }
            world_thread_update_task_state();
            if (g_world_unit_status_banner_active != 0) {
                world_menu_run_unit_status_banner_countdown(0, 0);
                world_input_clear_state();
            }
            world_menu_update_thread_7_idle_countdown();
            world_gfx_update_fade_in_tile();
            if (g_world_input_primary_repeat & PSX_PAD_SELECT) {
                world_menu_start_description_text_thread((world_menu_description_record_t*)3);
            }
            if (g_world_grid_menu_id != 0) {
                world_menu_update_grid_cursor(
                    g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->uses_monster_skillset);
            }
            world_menu_set_palette_mode(g_world_thread_task_active);
            g_world_formation_menu_handlers[g_world_formation_current_menu]();
            if (g_world_formation_current_menu == 0 || (u32)(g_world_formation_current_menu - 4) < 2) {
                world_formation_update_and_draw_unit_grid(1, g_world_formation_current_menu == 0,
                    g_world_formation_current_menu == 0,
                    g_world_formation_triangle_menu_open + g_world_grid_menu_id != 0
                        ? 0
                        : g_world_input_secondary_repeat_latched,
                    world_map_is_busy(), 0, 0);
            } else {
                /* The target also passes a1 = 0 to this one-argument callee. */
                ((void (*)(s32, s32))world_formation_draw_background_tiles)(
                    g_world_formation_scroll_position + g_world_formation_scroll_velocity, 0);
            }
            i = 0;
            if (g_world_formation_unit_cycle_mode != 0 || g_world_formation_unit_browse_enabled != 0) {
                if (g_world_thread_task_active != 0 || g_world_grid_menu_id != 0
                    || g_world_formation_unit_cycle_mode == 1 || g_world_formation_triangle_menu_running != 0) {
                    i = 1;
                }
                buttons = 0;
                if (g_world_formation_current_menu == 4) {
                    i = 1;
                }
                if (i == 0) {
                    buttons = g_world_input_secondary_repeat;
                }
                world_formation_cycle_selected_unit((s16)buttons);
                /* The definition's s16/u16 parameter conversions would change this call's codegen. */
                ((void (*)(s32, s32, s32, s32, s16, s32))world_menu_draw_pressable_button)(
                    0, 0xC, 0xE, buttons & PSX_PAD_L1, i, 6);
                /* The definition's s16/u16 parameter conversions would change this call's codegen. */
                ((void (*)(s32, s32, s32, s32, s16, s32))world_menu_draw_pressable_button)(
                    1, 0xE0, 0xE, buttons & PSX_PAD_R1, i, 6);
            }
            if (g_world_formation_current_menu != 8) {
                world_formation_update_unit_selection();
            }
            if (g_world_formation_unit_slide_pending != 0) {
                world_formation_step_unit_selection_slide();
            }
            g_world_gfx_active_otag_entries = g_world_gfx_active_packet_buffer->otag + 63;
            world_menu_draw_active_window_frames();
            if (g_world_formation_current_menu == 9 || g_world_formation_current_menu >= 14
                || g_world_formation_current_menu == 3) {
                i = 0x3E;
            } else {
                i = 9;
            }
            if (g_world_grid_menu_id != 0) {
                i = 9;
            }
            if (g_world_formation_cursor_ot_override != 0) {
                i = g_world_formation_cursor_ot_override;
            }
            world_script_update_event_frame_input(g_world_gfx_active_packet_buffer->otag + i, world_input_filter_menu(),
                g_world_gfx_draw_buffer_clip_y == 0 ? 0xF0 : 0);
            if (g_world_formation_current_menu != last_menu) {
                g_world_formation_cursor_ot_override = 0;
                g_world_formation_unit_cycle_mode = 0;
                world_script_set_vsync_mode_and_event_speed(0);
                g_world_menu_description_text_id = 0;
                world_menu_reset_selection_results();
                last_menu = g_world_formation_current_menu;
            }
            world_sound_dispatch_effect();
            main_sound_update_tunes();
        } while (g_world_formation_screen_running != 0);
    }
    do {
        buttons = 0;
        for (i = 1; i < 16; i++) {
            if (world_thread_is_running(i) != 0) {
                buttons = 1;
                world_menu_stop_unit_status_banner_thread(i);
            }
        }
        if (world_thread_is_running(16) != 0) {
            buttons = 1;
            world_text_resume_printing(16);
        }
        world_script_update_event_frame_input(g_world_gfx_active_packet_buffer->otag, 0, 0);
    } while (buttons != 0);
    if (tutorial == 0) {
        main_sound_unload_scenario_mus(g_world_menu_music_slot);
    }
    world_formation_save_records_to_party_data();
    world_formation_stop_menu_threads();
    world_script_set_vsync_mode_and_event_speed(0);
    world_formation_reset_menu_context();
}
