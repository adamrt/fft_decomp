#include "fft/battle_gfx.h"
#include "fft/gfx.h"
#include "fft/sound.h"
/* Initialize the unit-list gfx capacities and run the BUNIT menu frame loop
 * until the list closes.
 *
 * The capacity stores follow the shared overlay gfx-init order (compare
 * jobstts_entrypoint): 0x1e is first stored just before the 0x3c store, which
 * puts its single-set `li` between the 0x1f4 `li` and its store, in $v1. The
 * globals must not be volatile here: sched2 hoists the 0x40/0x1f4/0x3c stores
 * above the zero stores, as in the target.
 */
#include "fft/event_bunit.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* The double-buffer binder uses a volatile view of these two capacities. */
extern u16 g_bunit_gfx_line_g2_capacity;
extern u16 g_bunit_gfx_line_g3_capacity;

s32 bunit_entrypoint(s32 unused_unit, void* unused_otag) {
    POLY_FT4 textured_quads[1000];
    POLY_G4 gouraud_quads[60];
    POLY_GT4 gouraud_textured_quads[120];
    TILE tiles[60];
    DR_MOVE draw_moves[60];
    DR_MODE draw_modes[60];
    DR_AREA draw_areas[60];
    u32 otags[128];
    menu_gfx_context_t buffers[2];
    u8 previous_mode;
    u8 running;
    s32 sound_id;
    s32 sound_request;

    g_bunit_gfx_otag_count = 0x40;
    g_bunit_gfx_poly_ft4_capacity = 0x1f4;
    g_bunit_gfx_poly_f3_capacity = 0;
    g_bunit_gfx_poly_ft3_capacity = 0;
    g_bunit_gfx_poly_f4_capacity = 0;
    g_bunit_gfx_poly_g3_capacity = 0;
    g_bunit_gfx_poly_gt3_capacity = 0;
    g_bunit_gfx_poly_g4_capacity = 0x1e;
    g_bunit_gfx_poly_gt4_capacity = 0x3c;
    g_bunit_gfx_line_f2_capacity = 0;
    g_bunit_gfx_line_f3_capacity = 0;
    g_bunit_gfx_line_f4_capacity = 0;
    g_bunit_gfx_line_g2_capacity = 0;
    g_bunit_gfx_line_g3_capacity = 0;
    g_bunit_gfx_line_g4_capacity = 0;
    g_bunit_gfx_tile_capacity = 0x1e;
    g_bunit_gfx_tile_1_capacity = 0;
    g_bunit_gfx_tile_8_capacity = 0;
    g_bunit_gfx_tile_16_capacity = 0;
    g_bunit_gfx_sprite_capacity = 0;
    g_bunit_gfx_sprite_8_capacity = 0;
    g_bunit_gfx_sprite_16_capacity = 0;
    g_bunit_gfx_draw_move_capacity = 0x1e;
    g_bunit_gfx_draw_area_capacity = 0x1e;
    g_bunit_gfx_draw_mode_capacity = 0x1e;
    bunit_gfx_init_double_packet_buffers(buffers, otags, 0, 0, 0, textured_quads, 0, 0, gouraud_quads,
        gouraud_textured_quads, 0, 0, 0, 0, 0, 0, tiles, 0, 0, 0, 0, 0, 0, draw_moves, draw_areas, draw_modes);
    running = 1;
    bunit_gfx_init_vram_and_start_fade();
    bunit_menu_init_unit_list(g_bunit_unit_selected_index);
    do {
        bunit_gfx_submit_frame_and_swap_buffers(0, 0);
        previous_mode = g_menu_input_lock_mode;
        bunit_menu_update_event_thread_state();
        bunit_input_update_lock_timer();
        bunit_gfx_update_decreasing_fade();
        if (g_bunit_menu_input_mask & PSX_PAD_SELECT) {
            bunit_text_start_selection_thread(3);
        }
        if (g_bunit_help_screen_id != MENU_GRID_HELP_NONE) {
            bunit_menu_update_node_cursor(g_bunit_unit_pointers[g_bunit_unit_selected_index]->uses_monster_skillset);
        }
        if (previous_mode != 0 && g_menu_input_lock_mode == 0) {
            battle_text_restore_section_pointers();
        }
        bunit_menu_set_palette_mode(g_menu_input_lock_mode);
        if (g_bunit_menu_current_menu == BUNIT_MENU_JOB) {
            bunit_menu_update_job();
            /* The target also passes a1 = 0 to this one-argument callee. */
            ((void (*)(s32, s32))bunit_gfx_draw_falloff_shaded_rows)(
                g_bunit_unit_list_page_offset + g_bunit_unit_list_scroll_step, 0);
        } else {
            running = bunit_menu_run_unit_list_frame();
            bunit_menu_update_unit_grid(1, 1, 1, 1, 0,
                g_bunit_help_screen_id == MENU_GRID_HELP_NONE ? g_bunit_input_secondary_repeat : 0,
                bunit_menu_is_active_or_transitioning());
        }
        bunit_menu_update_unit_browser();
        g_gfx_current_otag_entry = g_menu_gfx_context->otag + 0x3d;
        menu_draw_active_window_frames();
        bunit_input_update_event_state(
            g_menu_gfx_context->otag + 9, bunit_input_read_pad1_unless_locked(), g_menu_gfx_drawenv_y);
        sound_request = g_sound_pending_sfx_id;
        if (sound_request == MAIN_SFX_CURSOR_MOVE || sound_request == MAIN_SFX_CANCEL) {
            g_bunit_sound_queued_effect_id = g_sound_pending_sfx_id;
        }
        if (sound_request < 0 || g_sound_suppressed != 0) {
            g_sound_pending_sfx_id = MAIN_SFX_NONE;
        }
        sound_id = g_sound_pending_sfx_id;
        if (sound_id == MAIN_SFX_NONE) {
            sound_id = g_bunit_sound_queued_effect_id;
        }
        if (sound_id == MAIN_SFX_ERROR_MESSAGE) {
            main_sound_play_sfx_find_channel(MAIN_SFX_ERROR_MESSAGE);
        } else if (sound_id != MAIN_SFX_NONE) {
            main_sound_play_sfx(sound_id);
        }
        g_bunit_sound_queued_effect_id = MAIN_SFX_NONE;
        g_sound_pending_sfx_id = MAIN_SFX_NONE;
    } while (running != 0);
    bunit_panel_toggle_equipment_and_numeric_editor_threads(0);
    bunit_panel_toggle_unit_status_thread(0);
    while (DrawSync(1) != 0) { }
    VSync(0);
    return g_bunit_selected_battle_id;
}
