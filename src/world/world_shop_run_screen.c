#include "fft/gfx.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

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
 * menu_draw_active_window_frames), and tile_box is a never-modified
 * local rematerialised at its use; without it the loop is one insn too long
 * for loop.c to hoist the constant 6 into $s7.
 */
void world_shop_run_screen(s32 shop_type) {
    POLY_FT4 textured_quads[2 * 500];
    POLY_G4 gradient_quads[2 * 90];
    POLY_GT4 textured_gradient_quads[2 * 60];
    TILE tiles[2 * 30];
    LINE_F2 flat_lines[2 * 10];
    DR_MOVE draw_moves[2 * 30];
    DR_MODE draw_modes[2 * 30];
    DR_AREA draw_areas[2 * 10];
    u32 otags[2 * 64];
    menu_gfx_context_t buffers[2];
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
    g_world_gfx_flat_line_capacity = 0xA;
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
    g_world_menu_context_id = shop_type;
    buttons = script_get_variable(EVENT_SCRIPT_VAR_TOWN_BACKGROUND);
    if (shop_type == WORLD_MENU_CONTEXT_FUR_SHOP) {
        ot_index = MAIN_MUSIC_FUR_SHOP;
        g_world_shop_menu_step = WORLD_SHOP_STEP_FUR_MENU;
        disabled = 0xF82F;
        tim_index = 3;
    } else if (shop_type == WORLD_MENU_CONTEXT_SOLDIER_OFFICE) {
        ot_index = MAIN_MUSIC_SOLDIER_OFFICE;
        g_world_shop_menu_step = WORLD_SHOP_STEP_SOLDIER_OFFICE_MENU;
        disabled = 0xF820;
        tim_index = 6;
    } else {
        g_world_menu_context_id = script_get_variable(EVENT_SCRIPT_VAR_LOCATION);
        ot_index = MAIN_MUSIC_SHOP;
        disabled = 0xF801;
        tim_index = 0;
        g_world_shop_menu_step = WORLD_SHOP_STEP_MAIN_MENU;
    }
    SetDispMask(0);
    while (DrawSync(1) != 0) { }
    gfx_move_image_and_wait(&g_world_shop_background_vram_rect, 0, 0);
    tim = world_shop_load_background_tim(tim_index + buttons);
    gfx_load_image_and_wait(&g_world_shop_background_vram_rect, tim + 5);
    gfx_store_image_and_wait(&g_world_shop_vram_backup_rect, tim);
    world_gfx_init_double_packet_buffers(buffers, otags, 0, 0, 0, textured_quads, 0, 0, gradient_quads,
        textured_gradient_quads, flat_lines, 0, 0, 0, 0, 0, tiles, 0, 0, 0, 0, 0, 0, draw_moves, draw_areas,
        draw_modes);
    last_step = -1;
    world_formation_init_menu_state();
    g_world_shop_main_menu_thread_data.selected_index = 0;
    g_world_shop_fur_menu_thread_data.selected_index = 0;
    g_world_shop_hire_menu_thread_data.selected_index = 0;
    world_text_bind_section_data(WORLD_TEXT_DATA_SHOP_DIALOGUE);
    g_text_section_pointers[WORLD_TEXT_SECTION_SHOP_MENU_LABELS] = g_world_text_shop_menu_labels_data;
    thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x19, disabled, 0);
    g_world_menu_music_slot = main_sound_open_music_into_free_slot(ot_index);
    main_sound_switch_music_track(g_world_menu_music_slot, 0x7F, 0);
    g_world_shop_background_visible = 1;
    g_world_formation_unit_banner_enabled = 0;
    while (g_world_shop_menu_step != WORLD_SHOP_STEP_EXIT) {
        world_gfx_present_frame_and_swap_packet_buffer(0, 0);
        world_input_update_with_message_state();
        world_menu_update_thread_7_idle_countdown();
        world_gfx_update_fade_in_tile();
        if (g_world_input_primary_repeat & PSX_PAD_SELECT) {
            world_menu_start_description_text_thread(DIALOG_POSITION_CENTER);
        }
        if (g_world_grid_menu_id != MENU_GRID_HELP_NONE) {
            world_menu_update_grid_cursor(
                g_world_unit_pointers[g_world_formation_selected_unit_index]->uses_monster_skillset);
        }
        world_menu_set_palette_mode(g_menu_input_lock_mode);
        if (g_world_shop_background_visible != 0) {
            world_gfx_enqueue_textured_quad(&g_world_shop_background_rect, 0, 0, 0, 0, 0x14C, 0, 1);
            world_gfx_enqueue_tile(&g_world_shop_backdrop_tile_rect, &g_world_gfx_black_tile_color, 0, 0);
            world_gfx_enqueue_tile(tile_box, &g_world_gfx_black_tile_color, 0, g_world_ot_length - 1);
        }
        if ((g_world_formation_unit_cycle_mode != 0 || g_world_formation_unit_browse_enabled != 0)
            && g_world_shop_menu_step < 0x14) {
            disabled = 0;
            if (g_menu_input_lock_mode != 0 || g_world_grid_menu_id != MENU_GRID_HELP_NONE
                || g_world_formation_unit_cycle_mode == 1) {
                disabled = 1;
            }
            buttons = 0;
            if (disabled == 0) {
                buttons = g_world_input_secondary_repeat;
            }
            if (thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) == 0) {
                if (buttons & PSX_PAD_L1) {
                    g_world_formation_selected_unit_index = g_world_formation_selected_unit_index == 0
                        ? g_world_formation_unit_count - 1
                        : g_world_formation_selected_unit_index - 1;
                    buttons &= ~PSX_PAD_R1;
                } else if (buttons & PSX_PAD_R1) {
                    g_world_formation_selected_unit_index
                        = g_world_formation_selected_unit_index == g_world_formation_unit_count - 1
                        ? 0
                        : g_world_formation_selected_unit_index + 1;
                }
            } else {
                buttons = 0;
            }
            world_menu_draw_pressable_button(
                MENU_PRESSABLE_BUTTON_L1, 0xC, 0xC, buttons & PSX_PAD_L1, disabled, MAIN_SFX_PAGE_SWITCH);
            world_menu_draw_pressable_button(
                MENU_PRESSABLE_BUTTON_R1, 0xE0, 0xC, buttons & PSX_PAD_R1, disabled, MAIN_SFX_PAGE_SWITCH);
        }
        g_world_shop_menu_step_handlers[g_world_shop_menu_step]();
        if (g_world_shop_menu_step != WORLD_SHOP_STEP_RENAME_NAME_ENTRY
            && g_world_shop_menu_step != WORLD_SHOP_STEP_HIRE_NAME_ENTRY
            && g_world_shop_menu_step != WORLD_SHOP_STEP_FITTING_ROOM_REMOVE) {
            world_shop_update_unit_selection();
        }
        g_gfx_current_otag_entry = g_menu_gfx_context->otag + 63;
        menu_draw_active_window_frames();
        if (g_world_shop_menu_step == WORLD_SHOP_STEP_FUR_BUY_CONFIRM
            || g_world_shop_menu_step == WORLD_SHOP_STEP_FUR_SELL_CONFIRM
            || g_world_shop_menu_step == WORLD_SHOP_STEP_FITTING_ROOM_SELL
            || g_world_shop_menu_step == WORLD_SHOP_STEP_BUY_CONFIRM
            || g_world_shop_menu_step == WORLD_SHOP_STEP_SELL_CONFIRM || g_world_shop_cost_window_visible != 0
            || shop_type == WORLD_MENU_CONTEXT_SOLDIER_OFFICE) {
            ot_index = 0x3C;
        } else {
            ot_index = 9;
        }
        world_input_update_event_state(g_menu_gfx_context->otag + ot_index, world_input_filter_menu(),
            g_world_gfx_draw_buffer_clip_y == 0 ? 0xF0 : 0);
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
    world_formation_save_records_to_party();
    world_script_set_vsync_mode_and_event_speed(0);
    world_formation_reset_menu_context();
    gfx_load_image_and_wait(&g_world_shop_background_vram_rect, tim);
    main_heap_free(tim);
}
