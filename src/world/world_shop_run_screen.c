#include "fft/data.h"
#include "fft/main_heap.h"
#include "fft/main_sound.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef void (*world_shop_menu_handler_t)(void);

/* Stack-resident primitive pools for both WORLD packet buffers; each pool
 * holds two frames of the per-type counts this routine publishes. */
typedef struct world_shop_screen_packets {
    POLY_FT4 textured_quads[2 * 500];         /* 0x0000: g_world_gfx_textured_quad_capacity = 500 */
    POLY_G4 gradient_quads[2 * 90];           /* 0x9c40: g_world_gfx_gradient_quad_capacity = 90 */
    POLY_GT4 textured_gradient_quads[2 * 60]; /* 0xb590: g_world_gfx_textured_gradient_quad_capacity = 60 */
    TILE tiles[2 * 30];                       /* 0xcdf0: g_world_gfx_tile_capacity = 30 */
    TILE tiles_24[2 * 10];                    /* 0xd1b0: g_world_gfx_tiles_24_capacity = 10 */
    DR_MOVE draw_moves[2 * 30];               /* 0xd2f0: g_world_gfx_draw_move_capacity = 30 */
    DR_MODE draw_modes[2 * 30];               /* 0xd890: g_world_gfx_draw_mode_capacity = 30 */
    DR_AREA draw_areas[2 * 10];               /* 0xdb60: g_world_gfx_draw_area_capacity = 10 */
    u32 otags[2 * 64];                        /* 0xdc50: g_world_ot_length = 64 */
    world_gfx_packet_buffer_t buffers[2];     /* 0xde50 */
} world_shop_screen_packets_t;

extern world_shop_menu_handler_t g_world_shop_menu_step_handlers[];

/*
 * Run the WORLD shop screen until g_world_shop_menu_step becomes -1.
 *
 * shop_type 0x64 and 0x65 select fixed shop variants; any other value takes
 * the shop from script variable 0x31. The routine saves the VRAM area under
 * the shop TIM, binds both packet buffers onto a stack-resident pool block,
 * then steps the shop menu handler once per frame and restores the VRAM area
 * on exit.
 *
 * buttons, ot_index and disabled also carry the TIM table base, music
 * scenario and thread parameter before the loop; the shared pseudos give the
 * target's $s1/$s3/$s4 allocation. The ot_index choice must be an if/else
 * (a then-block of its own keeps sched from hoisting the 0x3c store above
 * world_menu_draw_active_window_frames), and tile_box is a never-modified
 * local rematerialised at its use; without it the loop is one insn too long
 * for loop.c to hoist the constant 6 into $s7.
 */
void world_shop_run_screen(s32 shop_type) {
    world_shop_screen_packets_t packets;
    u32* tim;
    RECT* tile_box = &g_world_shop_edge_tile_rect;
    s32 buttons;
    s32 ot_index;
    s32 disabled;
    s32 tim_index;
    s16 last_step;

    g_world_ot_length = 0x40;
    g_world_gfx_textured_quad_capacity = 0x1F4;
    g_world_gfx_poly_f3_capacity = 0;
    g_world_gfx_poly_ft3_capacity = 0;
    g_world_gfx_poly_f4_capacity = 0;
    g_world_gfx_poly_g3_capacity = 0;
    g_world_gfx_poly_gt3_capacity = 0;
    g_world_gfx_gradient_quad_capacity = 0x5A;
    g_world_gfx_textured_gradient_quad_capacity = 0x3C;
    g_world_gfx_tiles_24_capacity = 0xA;
    g_world_gfx_line_f3_capacity = 0;
    g_world_gfx_line_f4_capacity = 0;
    g_world_gfx_gradient_line_capacity = 0;
    g_world_gfx_line_g3_capacity = 0;
    g_world_gfx_line_g4_capacity = 0;
    g_world_gfx_tile_capacity = 0x1E;
    g_world_gfx_tile_1_capacity = 0;
    g_world_gfx_tile_8_capacity = 0;
    g_world_gfx_tile_16_capacity = 0;
    g_world_gfx_sprite_capacity = 0;
    g_world_gfx_sprite_8_capacity = 0;
    g_world_gfx_sprite_16_capacity = 0;
    g_world_gfx_draw_move_capacity = 0x1E;
    g_world_gfx_draw_area_capacity = 0xA;
    g_world_gfx_draw_mode_capacity = 0x1E;
    g_world_shop_id = shop_type;
    buttons = world_script_get_variable(EVENT_SCRIPT_VAR_TOWN_BACKGROUND);
    if (shop_type == 0x64) {
        ot_index = 0x1E;
        g_world_shop_menu_step = 0xE;
        disabled = 0xF82F;
        tim_index = 3;
    } else if (shop_type == 0x65) {
        ot_index = 0x1D;
        g_world_shop_menu_step = 0x14;
        disabled = 0xF820;
        tim_index = 6;
    } else {
        g_world_shop_id = world_script_get_variable(EVENT_SCRIPT_VAR_LOCATION);
        ot_index = 0x1C;
        disabled = 0xF801;
        tim_index = 0;
        g_world_shop_menu_step = 0;
    }
    SetDispMask(0);
    while (DrawSync(1) != 0) { }
    world_gfx_move_image_and_wait(&g_world_shop_background_vram_rect, 0, 0);
    tim = ((u32 * (*)(s32)) world_gfx_load_tim_by_table_index)(tim_index + buttons);
    world_gfx_load_image_sync(&g_world_shop_background_vram_rect, tim + 5);
    world_gfx_move_image_to_ram_from_vram_and_wait(&g_world_shop_vram_backup_rect, tim);
    world_gfx_init_double_packet_buffers(packets.buffers, packets.otags, 0, 0, 0, packets.textured_quads, 0, 0,
        packets.gradient_quads, packets.textured_gradient_quads, packets.tiles_24, 0, 0, 0, 0, 0, packets.tiles, 0, 0,
        0, 0, 0, 0, packets.draw_moves, packets.draw_areas, packets.draw_modes);
    last_step = -1;
    world_formation_init_menu_state();
    g_world_shop_main_menu_thread_data.cursor = 0;
    g_world_shop_fur_menu_thread_data.cursor = 0;
    g_world_shop_hire_menu_thread_data.cursor = 0;
    world_gfx_bind_data_pointer(8);
    g_world_text_section_pointers[30] = g_world_text_shop_menu_labels_data;
    world_thread_start(2, world_text_message_box_thread);
    world_thread_set_parameters(2, 0x19, disabled, 0);
    g_world_menu_music_slot = main_sound_open_music_into_free_slot(ot_index);
    main_sound_switch_music_track(g_world_menu_music_slot, 0x7F, 0);
    g_world_shop_background_visible = 1;
    g_world_formation_unit_banner_enabled = 0;
    while (g_world_shop_menu_step != -1) {
        world_gfx_present_frame_and_swap_packet_buffer(0, 0);
        world_thread_update_task_state();
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
        if (g_world_shop_background_visible != 0) {
            world_gfx_enqueue_textured_quad(&g_world_shop_background_rect, 0, 0, 0, 0, 0x14C, 0, 1);
            world_gfx_append_tile_to_otag(&g_world_shop_backdrop_tile_rect, g_world_gfx_black_tile_color, 0, 0);
            world_gfx_append_tile_to_otag(tile_box, g_world_gfx_black_tile_color, 0, g_world_ot_length - 1);
        }
        if ((g_world_formation_unit_cycle_mode != 0 || g_world_formation_unit_browse_enabled != 0)
            && g_world_shop_menu_step < 0x14) {
            disabled = 0;
            if (g_world_thread_task_active != 0 || g_world_grid_menu_id != 0
                || g_world_formation_unit_cycle_mode == 1) {
                disabled = 1;
            }
            buttons = 0;
            if (disabled == 0) {
                buttons = g_world_input_secondary_repeat;
            }
            if (world_thread_is_running(7) == 0) {
                if (buttons & PSX_PAD_L1) {
                    g_world_formation_selected_unit_index = g_world_formation_selected_unit_index == 0
                        ? g_world_formation_unit_count - 1
                        : g_world_formation_selected_unit_index - 1;
                    buttons &= ~8;
                } else if (buttons & PSX_PAD_R1) {
                    g_world_formation_selected_unit_index
                        = g_world_formation_selected_unit_index == g_world_formation_unit_count - 1
                        ? 0
                        : g_world_formation_selected_unit_index + 1;
                }
            } else {
                buttons = 0;
            }
            /* The definition's s16/u16 parameter conversions would change this call's codegen. */
            ((void (*)(s32, s32, s32, s32, s16, s32))world_menu_draw_pressable_button)(
                0, 0xC, 0xC, buttons & PSX_PAD_L1, disabled, 6);
            /* The definition's s16/u16 parameter conversions would change this call's codegen. */
            ((void (*)(s32, s32, s32, s32, s16, s32))world_menu_draw_pressable_button)(
                1, 0xE0, 0xC, buttons & PSX_PAD_R1, disabled, 6);
        }
        g_world_shop_menu_step_handlers[g_world_shop_menu_step]();
        if (g_world_shop_menu_step != 0x1B && g_world_shop_menu_step != 0x17 && g_world_shop_menu_step != 9) {
            world_shop_update_unit_selection();
        }
        g_world_gfx_active_otag_entries = g_world_gfx_active_packet_buffer->otag + 63;
        world_menu_draw_active_window_frames();
        if (g_world_shop_menu_step == 0x10 || g_world_shop_menu_step == 0x13 || g_world_shop_menu_step == 0xB
            || g_world_shop_menu_step == 2 || g_world_shop_menu_step == 5 || g_world_shop_cost_window_visible != 0
            || shop_type == 0x65) {
            ot_index = 0x3C;
        } else {
            ot_index = 9;
        }
        world_script_update_event_frame_input(g_world_gfx_active_packet_buffer->otag + ot_index,
            world_input_filter_menu(), g_world_gfx_draw_buffer_clip_y == 0 ? 0xF0 : 0);
        if (g_world_shop_menu_step != last_step) {
            g_world_formation_unit_cycle_mode = 0;
            world_script_set_vsync_mode_and_event_speed(0);
            g_world_menu_description_text_id = 0;
            world_menu_reset_selection_results();
            last_step = g_world_shop_menu_step;
        }
        world_sound_dispatch_effect();
    }
    main_sound_unload_scenario_mus(g_world_menu_music_slot);
    world_formation_save_records_to_party_data();
    world_script_set_vsync_mode_and_event_speed(0);
    world_formation_reset_menu_context();
    world_gfx_load_image_sync(&g_world_shop_background_vram_rect, tim);
    main_heap_free(tim);
}
