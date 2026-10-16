/* BUNIT entry, including VRAM init and fade, the input lock timer, pad reading, menu results and the unit browser
 * frame. */

#include "fft/event_bunit.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_panel.h"
#include "fft/menu_text.h"
#include "fft/sound.h"
#include "fft/status.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static s32 bunit_menu_run_unit_list_frame(void);
static void bunit_menu_update_unit_browser(void);
static void bunit_unit_copy_selected_data(void);

/*
 * Load the BUNIT screen's VRAM images and start the fade-in.
 *
 * Binds the overlay's text sections, uploads the menu glyph image to
 * (0x100, 0) and derives four palette rows at y 0xfe from the CLUT rows at
 * (0x3c0, 0x1f0) and (0x3c0, 0x1f4): each row is copied with entry 4 made
 * transparent, and its entries 4..15 are copied again as a 12-color row.
 * The 16-entry row is read into buf1, which holds four entries; the other
 * twelve land in buf2, which follows it on the stack. The 0x200-byte menu
 * palette image is copied to the stack before it is uploaded to
 * (0x100, 0xfa).
 */
static void bunit_gfx_init_vram_and_start_fade(void) {
    RECT rect;
    u16 buf1[4];
    u16 buf2[12];
    clut_t block;

    memcpy(block, g_bunit_menu_clut, sizeof(block));

    menu_set_vsync_mode_and_event_speed(0);
    g_text_section_pointers[TEXT_SECTION_EVENT_MESSAGES] = g_bunit_text_menu_section;
    g_text_section_pointers[TEXT_SECTION_COMPOSED_TEXT] = g_bunit_text_reorder_list_buffer;
    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    gfx_load_image_and_wait(&rect, g_menu_glyph_sheet->pixels);
    rect.x = 0x3C0;
    rect.y = 0x1F0;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, buf1);
    buf2[0] = 0;
    rect.x = 0x120;
    rect.y = 0xFE;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, buf1);
    rect.x = 0x100;
    rect.y = 0xFE;
    rect.w = 0xC;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, buf2);
    rect.x = 0x3C0;
    rect.y = 0x1F4;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, buf1);
    buf2[0] = 0;
    rect.x = 0x130;
    rect.y = 0xFE;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, buf1);
    rect.x = 0x110;
    rect.y = 0xFE;
    rect.w = 0xC;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, buf2);
    rect.x = 0x100;
    rect.y = 0xFA;
    rect.w = 0x40;
    rect.h = 4;
    gfx_load_image_and_wait(&rect, block);
    g_text_selection_id = 0;
    g_bunit_help_screen_id = MENU_GRID_HELP_NONE;
    gfx_start_decreasing_fade();
}

/* Hold the menu input masks at zero while the comparison banner thread runs,
 * a status banner slide is requested or the unit list has to scroll, and for
 * nine further calls after the last of those. */
static void bunit_input_update_lock_timer(void) {
    s32 timer;

    if (thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) != 0) {
        timer = 0xA;
    } else if ((g_bunit_status_display_flags & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP)) != 0) {
        timer = 0xA;
    } else if (bunit_menu_is_unit_list_scroll_needed() != 0) {
        timer = 0xA;
    } else {
        timer = g_input_lock_timer;
        if (timer == 0) {
            return;
        }
        timer -= 1;
    }
    g_input_lock_timer = timer;
    if (g_input_lock_timer != 0) {
        g_menu_input_newly_pressed = 0;
        g_menu_input_secondary_repeat = 0;
        g_menu_input_primary_repeat = 0;
    }
}

s32 bunit_input_get_lock_timer(void) {
    return g_input_lock_timer;
}

/* BUNIT.OUT 001c3150 - Return the pad-1 input, or 0 while the input lock timer is active. */
static s32 bunit_input_read_pad1_unless_locked(void) {
    u32 input = PadRead(0);

    if (bunit_input_get_lock_timer() != 0) {
        input = 0;
    }
    return input;
}

/* Initialize the unit-list gfx capacities and run the BUNIT menu frame loop
 * until the list closes.
 *
 * The capacity stores follow the shared overlay gfx-init order (compare
 * jobstts_entrypoint): 0x1e is first stored just before the 0x3c store, which
 * puts its single-set `li` between the 0x1f4 `li` and its store, in $v1. The
 * globals must not be volatile here: sched2 hoists the 0x40/0x1f4/0x3c stores
 * above the zero stores, as in the target.
 */

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

    g_gfx_otag_count = 0x40;
    g_gfx_poly_ft4_capacity = 0x1f4;
    g_gfx_poly_f3_capacity = 0;
    g_gfx_poly_ft3_capacity = 0;
    g_gfx_poly_f4_capacity = 0;
    g_gfx_poly_g3_capacity = 0;
    g_gfx_poly_gt3_capacity = 0;
    g_gfx_poly_g4_capacity = 0x1e;
    g_gfx_poly_gt4_capacity = 0x3c;
    g_gfx_line_f2_capacity = 0;
    g_gfx_line_f3_capacity = 0;
    g_gfx_line_f4_capacity = 0;
    g_bunit_gfx_line_g2_capacity = 0;
    g_bunit_gfx_line_g3_capacity = 0;
    g_gfx_line_g4_capacity = 0;
    g_gfx_tile_capacity = 0x1e;
    g_gfx_tile_1_capacity = 0;
    g_gfx_tile_8_capacity = 0;
    g_gfx_tile_16_capacity = 0;
    g_gfx_sprite_capacity = 0;
    g_gfx_sprite_8_capacity = 0;
    g_gfx_sprite_16_capacity = 0;
    g_bunit_gfx_draw_move_capacity = 0x1e;
    g_bunit_gfx_draw_area_capacity = 0x1e;
    g_gfx_draw_mode_capacity = 0x1e;
    bunit_gfx_init_double_packet_buffers(buffers, otags, 0, 0, 0, textured_quads, 0, 0, gouraud_quads,
        gouraud_textured_quads, 0, 0, 0, 0, 0, 0, tiles, 0, 0, 0, 0, 0, 0, draw_moves, draw_areas, draw_modes);
    running = 1;
    bunit_gfx_init_vram_and_start_fade();
    bunit_menu_init_unit_list(g_unit_selected_index);
    do {
        bunit_gfx_submit_frame_and_swap_buffers(0, 0);
        previous_mode = g_menu_input_lock_mode;
        bunit_menu_update_event_thread_state();
        bunit_input_update_lock_timer();
        gfx_update_decreasing_fade();
        if (g_menu_input_newly_pressed & PSX_PAD_SELECT) {
            bunit_text_start_selection_thread(3);
        }
        if (g_bunit_help_screen_id != MENU_GRID_HELP_NONE) {
            bunit_menu_update_node_cursor(g_bunit_unit_pointers[g_unit_selected_index]->uses_monster_skillset);
        }
        if (previous_mode != 0 && g_menu_input_lock_mode == 0) {
            text_restore_section_pointers();
        }
        menu_set_palette_mode(g_menu_input_lock_mode);
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
        menu_input_update_event_state_variant(
            g_menu_gfx_context->otag + 9, bunit_input_read_pad1_unless_locked(), g_menu_gfx_drawenv_y);
        sound_request = g_sound_pending_sfx_id;
        if (sound_request == MAIN_SFX_CURSOR_MOVE || sound_request == MAIN_SFX_CANCEL) {
            g_sound_queued_effect_id = g_sound_pending_sfx_id;
        }
        if (sound_request < 0 || g_sound_suppressed != 0) {
            g_sound_pending_sfx_id = MAIN_SFX_NONE;
        }
        sound_id = g_sound_pending_sfx_id;
        if (sound_id == MAIN_SFX_NONE) {
            sound_id = g_sound_queued_effect_id;
        }
        if (sound_id == MAIN_SFX_ERROR_MESSAGE) {
            main_sound_play_sfx_find_channel(MAIN_SFX_ERROR_MESSAGE);
        } else if (sound_id != MAIN_SFX_NONE) {
            main_sound_play_sfx(sound_id);
        }
        g_sound_queued_effect_id = MAIN_SFX_NONE;
        g_sound_pending_sfx_id = MAIN_SFX_NONE;
    } while (running != 0);
    bunit_panel_toggle_equipment_and_numeric_editor_threads(0);
    menu_toggle_unit_status_thread(0);
    while (DrawSync(1) != 0) { }
    VSync(0);
    return g_bunit_selected_battle_id;
}

void bunit_menu_reset_results(void) {
    g_bunit_menu_primary_result = -1;
    g_bunit_menu_reorder_result = -1;
}

/* BUNIT.OUT 001c360c - True while a menu transition (thread 15), event mode, or
 * thread 5 is running; used to gate input processing. */
s32 bunit_menu_is_active_or_transitioning(void) {
    s32 result = 0;
    if (thread_is_running(BUNIT_THREAD_SLOT_REORDER_MENU) || g_menu_input_lock_mode != 0
        || (result = 0, thread_is_running(BUNIT_THREAD_SLOT_MENU))) {
        result = 1;
    }
    return result;
}

/* Unit-list menu frame handler (g_bunit_menu_current_menu state -1 / 0). */
static s32 bunit_menu_run_unit_list_frame(void) {
    s32 input = g_menu_input_primary_repeat;
    bunit_unit_t* unit;
    u8 mode_b3;
    u8 mode_b4;

    if (g_bunit_menu_current_menu == BUNIT_MENU_UNIT_LIST) {
        if (g_bunit_unit_list_initialized == 0) {
            g_bunit_status_banner_enabled = g_bunit_saved_status_banner_enabled;
            g_bunit_unit_browse_enabled = g_bunit_saved_unit_browse_enabled;
            bunit_menu_init_unit_list(g_unit_selected_index);
            g_bunit_unit_list_count = bunit_unit_build_data_list(-1, (s32)g_bunit_unit_pointers, 1);
            g_bunit_unit_reorderable_count = bunit_unit_build_reorderable_index_list();
            bunit_unit_build_list_index_map();
            g_bunit_unit_grid_selected_cell = g_bunit_unit_list_index_map[g_unit_selected_index];
            bunit_unit_copy_selected_data();
            g_bunit_status_display_dimmed = 0;
            g_bunit_numeric_editor_thread_params.style = 0;
            g_bunit_character_status_frame_config.style = 0;
            bunit_menu_reset_results();
            g_bunit_unit_list_initialized = 1;
        }
        if (gfx_update_increasing_fade() == 0) {
            g_bunit_unit_list_initialized = 0;
            menu_request_thread_stop(BUNIT_THREAD_SLOT_MENU);
            return 0;
        }
        if (g_bunit_unit_list_triangle_menu_open != 0) {
            g_text_selection_id = g_bunit_unit_list_triangle_menu.selected_index != 0 ? 0x1001 : 0x1000;
            if ((g_bunit_unit_list_triangle_menu_open
                    = bunit_menu_run_descriptor_thread(BUNIT_THREAD_SLOT_MENU, &g_bunit_unit_list_triangle_menu))
                == 0) {
                if (g_bunit_menu_primary_result == 0) {
                    mode_b3 = g_bunit_status_banner_enabled;
                    mode_b4 = g_bunit_unit_browse_enabled;
                    g_bunit_menu_current_menu = BUNIT_MENU_REORDER_LIST;
                    g_bunit_status_banner_enabled = 0;
                    g_bunit_unit_browse_enabled = 0;
                    g_bunit_saved_status_banner_enabled = mode_b3;
                    g_bunit_saved_unit_browse_enabled = mode_b4;
                    bunit_menu_reset_results();
                    g_bunit_unit_list_initialized = 0;
                } else if (g_bunit_menu_primary_result == 1) {
                    gfx_start_increasing_fade();
                    g_bunit_selected_battle_id = g_bunit_unit_pointers[g_unit_selected_index]->battle_id;
                }
                g_bunit_character_status_frame_config.style = 0;
                g_bunit_numeric_editor_thread_params.style = 0;
                return 1;
            }
            return 1;
        }
        if (g_bunit_unit_browse_enabled != 0) {
            g_text_selection_id = MENU_GRID_HELP_UNIT_BROWSE;
        } else if (g_bunit_status_banner_enabled != 0) {
            g_text_selection_id
                = g_bunit_status_banner_at_bottom != 0 ? MENU_GRID_HELP_BANNER_BOTTOM : MENU_GRID_HELP_BANNER_TOP;
        } else {
            g_text_selection_id = 0;
        }
        if (bunit_input_get_lock_timer() != 0) {
            return 1;
        }
        if ((input & PSX_PAD_TRIANGLE) || (input & PSX_PAD_START)) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            g_bunit_character_status_frame_config.style = 1;
            g_bunit_numeric_editor_thread_params.style = 1;
            g_bunit_unit_list_triangle_menu_open = 1;
        } else if ((input & PSX_PAD_CIRCLE) && g_bunit_unit_browse_enabled != 0) {
            unit = g_bunit_unit_pointers[g_unit_selected_index];
            if (unit->uses_monster_skillset != 0 || (unit->initial_team_flags & BATTLE_TEAM_MASK)) {
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
            } else {
                g_bunit_status_display_dimmed = 1;
                g_bunit_numeric_editor_thread_params.style = 1;
                g_bunit_character_status_frame_config.style = 1;
                g_bunit_unit_list_initialized = 0;
                g_bunit_saved_unit_browse_enabled = g_bunit_unit_browse_enabled;
                g_bunit_menu_current_menu = BUNIT_MENU_JOB;
                g_bunit_saved_status_banner_enabled = g_bunit_status_banner_enabled;
            }
        } else if (g_bunit_unit_browse_enabled == 0 && (input & PSX_PAD_CROSS)) {
            gfx_start_increasing_fade();
        }
    } else if (g_bunit_menu_current_menu == BUNIT_MENU_REORDER_LIST) {
        bunit_menu_run_reorder_list();
    }
    return 1;
}

/* The target reaches g_bunit_status_display_flags through an address register (lui/addiu, then
 * 0(reg)) at each access site, which is what GCC emits for a dereferenced
 * constant address; a plain global access folds the address into lui/%lo. */
#define BUNIT_REQUEST_FLAGS (*(s32*)&g_bunit_status_display_flags)

/* Per-frame step of the unit browser.
 *
 * When the selected unit differs from g_bunit_unit_comparison_index it republishes that unit's
 * record, restarts the slide animation, and reports the scroll direction in
 * g_status_display_redraw_request; otherwise it advances the g_bunit_status_banner_slide_step slide and stops
 * thread 7 once the slide completes. */
static void bunit_menu_update_unit_browser(void) {
    s32 index;
    s32 enable;
    s8 slide;
    u8 value;

    if (g_unit_selected_index != g_bunit_unit_comparison_index) {
        if (g_bunit_status_banner_enabled != 0) {
            g_bunit_comparison_display_flags = BUNIT_REQUEST_FLAGS;
            BUNIT_REQUEST_FLAGS = 0;
            if (g_bunit_status_banner_at_bottom_latch != g_bunit_status_banner_at_bottom) {
                bcopy(g_bunit_unit_pointers[g_bunit_unit_comparison_index], &g_panel_comparison_billboard, 0x22);
                bcopy(&g_bunit_unit_pointers[g_bunit_unit_comparison_index]->name_index,
                    g_bunit_panel_comparison_unit_data, 0xE);
                menu_toggle_comparison_status_thread(1);
                g_bunit_status_banner_at_bottom_latch = g_bunit_status_banner_at_bottom;
                if (g_bunit_status_banner_slide_step < 0) {
                    g_bunit_status_banner_slide_step += 0x10;
                } else if (g_bunit_status_banner_slide_step > 0) {
                    g_bunit_status_banner_slide_step -= 0x10;
                } else if (g_bunit_status_banner_at_bottom != 0) {
                    g_bunit_status_banner_slide_step = -1;
                } else {
                    g_bunit_status_banner_slide_step = 1;
                }
            }
        } else {
            value = g_bunit_status_banner_at_bottom;
            g_bunit_status_banner_at_bottom_latch = value;
            if (value != 0) {
                g_bunit_status_display_offset_y = 0x90;
            } else {
                g_bunit_status_display_offset_y = 0;
            }
        }
        index = g_bunit_unit_comparison_index;
        bunit_unit_copy_selected_data();
        if ((g_menu_input_primary_repeat & PSX_PAD_RIGHT) || (g_menu_input_secondary_repeat & PSX_PAD_R1)) {
            g_status_display_redraw_request = 1;
        } else if ((g_menu_input_primary_repeat & PSX_PAD_LEFT) || (g_menu_input_secondary_repeat & PSX_PAD_L1)) {
            g_status_display_redraw_request = 2;
        } else {
            g_status_display_redraw_request = index < g_unit_selected_index ? 1 : 2;
        }
        return;
    }
    if (g_bunit_status_banner_at_bottom_latch != g_bunit_status_banner_at_bottom
        && g_bunit_status_banner_slide_step == 0) {
        g_bunit_status_banner_at_bottom_latch = g_bunit_status_banner_at_bottom;
        if (g_bunit_status_banner_enabled != 0) {
            if (g_bunit_status_banner_at_bottom != 0) {
                BUNIT_REQUEST_FLAGS |= 0x20;
            } else {
                BUNIT_REQUEST_FLAGS |= 0x40;
            }
        } else if (g_bunit_status_banner_at_bottom != 0) {
            g_bunit_status_display_offset_y = 0x90;
        } else {
            g_bunit_status_display_offset_y = 0;
        }
    }
    bunit_panel_toggle_equipment_and_numeric_editor_threads(g_bunit_unit_browse_enabled);
    enable = 0;
    if (g_bunit_status_banner_enabled != 0 || g_bunit_unit_browse_enabled != 0) {
        enable = 1;
    }
    menu_toggle_unit_status_thread(enable);
    slide = g_bunit_status_banner_slide_step;
    if (slide < 0) {
        g_bunit_status_display_offset_y = slide * 8 + 0x108;
        g_bunit_comparison_display_offset_y = slide * 8;
        if (--g_bunit_status_banner_slide_step < -0xF) {
            g_bunit_status_banner_slide_step = 0;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    } else if (slide > 0) {
        g_bunit_status_display_offset_y = slide * 8 - 0x78;
        g_bunit_comparison_display_offset_y = slide * 8 + 0x90;
        if (++g_bunit_status_banner_slide_step >= 0x10) {
            g_bunit_status_banner_slide_step = 0;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    }
}

/* Copy the selected unit's stat summary, identity and stat detail blocks
 * into the panel records and request a redraw of all four panels. */
static void bunit_unit_copy_selected_data(void) {
    bunit_unit_t* comparison;

    g_bunit_unit_comparison_index = g_unit_selected_index;
    comparison = g_bunit_unit_pointers[g_bunit_unit_comparison_index];
    bcopy(comparison, &g_panel_selected_billboard, 0x22);
    bcopy(&g_bunit_unit_pointers[g_unit_selected_index]->name_index, g_bunit_panel_selected_unit_data, 0xE);
    bcopy(&g_bunit_unit_pointers[g_unit_selected_index]->move, &g_menu_selected_unit_fields, 0x40);
    g_status_display_redraw_request = 1;
    g_bunit_comparison_display_redraw_request = 1;
    g_bunit_character_status_frame_config.redraw_request = 1;
    g_bunit_numeric_editor_thread_params.redraw_request = 1;
}
