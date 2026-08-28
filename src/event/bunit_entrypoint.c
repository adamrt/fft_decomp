/* Initialize the unit-list gfx capacities and run the BUNIT menu frame loop
 * until the list closes.
 *
 * The capacity stores follow the shared overlay gfx-init order (compare
 * jobstts_entrypoint): 0x1e is first stored just before the 0x3c store, which
 * puts its single-set `li` between the 0x1f4 `li` and its store, in $v1. The
 * globals must not be volatile here: sched2 hoists the 0x40/0x1f4/0x3c stores
 * above the zero stores, as in the target.
 */
#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_text.h"
#include "fft/bunit.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "psx/etc.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Plain view of the pool capacities; the packet-pool initializers read them volatile. */
extern u16 g_bunit_gfx_poly_f3_capacity;
extern u16 g_bunit_gfx_poly_ft3_capacity;
extern u16 g_bunit_gfx_poly_f4_capacity;
extern u16 g_bunit_gfx_poly_ft4_capacity;
extern u16 g_bunit_gfx_poly_g3_capacity;
extern u16 g_bunit_gfx_poly_gt3_capacity;
extern u16 g_bunit_gfx_poly_g4_capacity;
extern u16 g_bunit_gfx_poly_gt4_capacity;
extern u16 g_bunit_gfx_line_f2_capacity;
extern u16 g_bunit_gfx_line_f3_capacity;
extern u16 g_bunit_gfx_line_f4_capacity;
extern u16 g_bunit_gfx_line_g2_capacity;
extern u16 g_bunit_gfx_line_g3_capacity;
extern u16 g_bunit_gfx_line_g4_capacity;
extern u16 g_bunit_gfx_tile_capacity;
extern u16 g_bunit_gfx_tile_1_capacity;
extern u16 g_bunit_gfx_tile_8_capacity;
extern u16 g_bunit_gfx_tile_16_capacity;
extern u16 g_bunit_gfx_sprite_capacity;
extern u16 g_bunit_gfx_sprite_8_capacity;
extern u16 g_bunit_gfx_sprite_16_capacity;
extern u16 g_bunit_gfx_draw_move_capacity;
extern u16 g_bunit_gfx_draw_area_capacity;
extern u16 g_bunit_gfx_draw_mode_capacity;

typedef struct bunit_gfx_frame_buffer bunit_gfx_frame_buffer_t;

s32 bunit_entrypoint(void) {
    POLY_FT4 textured_quads[1000];
    POLY_G4 gouraud_quads[60];
    POLY_GT4 gouraud_textured_quads[120];
    TILE tiles[60];
    DR_MOVE draw_moves[60];
    DR_MODE draw_modes[60];
    DR_AREA draw_areas[60];
    u32 otags[128];
    u8 buffers[2][0xec];
    u8 previous_mode;
    s32 running;
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
    /* The target passes the selected index to this argument-less callee. */
    ((void (*)(s32))bunit_menu_init_unit_list)(g_bunit_unit_selected_index);
    do {
        bunit_gfx_submit_frame_and_swap_buffers(0, 0);
        previous_mode = g_event_mode;
        bunit_menu_update_event_thread_state();
        bunit_input_update_lock_timer();
        bunit_gfx_update_decreasing_fade();
        if (g_bunit_menu_input_mask & PSX_PAD_SELECT) {
            bunit_text_start_selection_thread((u8*)3);
        }
        if (g_bunit_help_screen_id != 0) {
            bunit_menu_update_node_cursor(g_bunit_unit_data[g_bunit_unit_selected_index]->uses_monster_skillset);
        }
        if (previous_mode != 0 && g_event_mode == 0) {
            battle_text_restore_pointer_table();
        }
        bunit_text_set_palette_and_metrics(g_event_mode);
        if (g_bunit_menu_current_menu == 2) {
            bunit_menu_update_job();
            /* The target also passes a1 = 0 to this one-argument callee. */
            ((void (*)(s32, s32))bunit_gfx_draw_falloff_shaded_rows)(
                g_bunit_unit_list_page_offset + g_bunit_unit_list_scroll_step, 0);
        } else {
            running = bunit_menu_run_unit_list_frame();
            /* The target passes the arguments with default promotions, as if
             * no prototype were in scope (the u16 prototype would zero-extend
             * the sixth argument), so call through an unprototyped view. */
            ((void (*)())bunit_menu_update_unit_grid)(1, 1, 1, 1, 0,
                g_bunit_help_screen_id == 0 ? (s16)g_bunit_input_secondary_repeat : 0,
                (s16)bunit_menu_is_active_or_transitioning());
        }
        bunit_menu_update_unit_browser();
        g_current_otag_entry = g_bunit_gfx_context->otag + 0x3d;
        battle_menu_draw_active_window_frames();
        /* The target zero-extends the y word; the definition's s16 parameter would sign-extend it. */
        ((void (*)(void*, u32, u16))bunit_input_update_event_state)(
            g_bunit_gfx_context->otag + 9, bunit_input_read_pad1_unless_locked(), g_bunit_gfx_drawenv_y);
        sound_request = g_sound_effect_id_to_play;
        if (sound_request == MAIN_SFX_CURSOR_MOVE || sound_request == MAIN_SFX_CANCEL) {
            g_bunit_sound_queued_effect_id = g_sound_effect_id_to_play;
        }
        if (sound_request < 0 || g_battle_sound_suppressed != 0) {
            g_sound_effect_id_to_play = 0;
        }
        sound_id = g_sound_effect_id_to_play;
        if (sound_id == 0) {
            sound_id = g_bunit_sound_queued_effect_id;
        }
        if (sound_id == MAIN_SFX_CARD_ERROR) {
            main_sound_play_sfx_find_channel(MAIN_SFX_CARD_ERROR);
        } else if (sound_id != 0) {
            main_sound_play_sfx(sound_id);
        }
        g_bunit_sound_queued_effect_id = 0;
        g_sound_effect_id_to_play = 0;
    } while ((u8)running != 0);
    bunit_thread_toggle_9_and_12(0);
    bunit_thread_toggle_8(0);
    while (DrawSync(1) != 0) { }
    VSync(0);
    return (s8)g_bunit_selected_roster_id;
}
