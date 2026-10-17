/* BUNIT entry, including VRAM init and fade, the input lock timer, pad reading, menu results and the unit browser
 * frame. */

#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/input.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_window.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_list.h"
#include "fft/unit_panel.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static s32 unit_list_run_frame(void);
static void unit_list_update_browser(void);
static void unit_list_copy_selected(void);

/*
 * Load the BUNIT screen's VRAM images and start the fade-in.
 *
 * Binds the overlay's text sections, uploads the menu glyph image to
 * (0x100, 0) and derives four palette rows at y 0xfe from the CLUT rows at
 * (0x3c0, 0x1f0) and (0x3c0, 0x1f4): each row is copied with entry 4 made
 * transparent, and its entries 4..15 are copied again as a 12-color row.
 * The complete 16-entry row is read into palette; entry four is then made
 * transparent, and the 12-entry suffix is uploaded independently. The
 * 0x200-byte menu palette image is copied to the stack before it is uploaded to
 * (0x100, 0xfa).
 */
static void unit_list_init_vram_and_fade(void) {
    RECT rect;
    u16 palette[16];
    clut_t block;

    memcpy(block, g_unit_list_clut, sizeof(block));

    menu_set_vsync_mode_and_event_speed(0);
    g_text_section_pointers[TEXT_SECTION_EVENT_MESSAGES] = g_unit_list_text_menu_section;
    g_text_section_pointers[TEXT_SECTION_COMPOSED_TEXT] = g_unit_list_text_reorder_list_buffer;
    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    gfx_load_image_and_wait(&rect, g_menu_glyph_sheet->pixels);
    rect.x = 0x3C0;
    rect.y = 0x1F0;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, palette);
    palette[4] = 0;
    rect.x = 0x120;
    rect.y = 0xFE;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette);
    rect.x = 0x100;
    rect.y = 0xFE;
    rect.w = 0xC;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette + 4);
    rect.x = 0x3C0;
    rect.y = 0x1F4;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, palette);
    palette[4] = 0;
    rect.x = 0x130;
    rect.y = 0xFE;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette);
    rect.x = 0x110;
    rect.y = 0xFE;
    rect.w = 0xC;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette + 4);
    rect.x = 0x100;
    rect.y = 0xFA;
    rect.w = 0x40;
    rect.h = 4;
    gfx_load_image_and_wait(&rect, block);
    g_text_selection_id = 0;
    g_unit_list_help_screen_id = MENU_GRID_HELP_NONE;
    gfx_start_decreasing_fade();
}

/* Hold the menu input masks at zero while the comparison banner thread runs,
 * a status banner slide is requested or the unit list has to scroll, and for
 * nine further calls after the last of those. */
static void unit_list_update_input_lock(void) {
    s32 timer;

    if (thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) != 0) {
        timer = 0xA;
    } else if ((g_unit_panel_status_display_flags & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP)) != 0) {
        timer = 0xA;
    } else if (unit_list_is_scroll_needed() != 0) {
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

/* Return the input lock timer; nonzero means input is still ignored. */
s32 unit_list_get_input_lock(void) {
    return g_input_lock_timer;
}

/* Return the pad-1 input, or 0 while the input lock timer is active. */
static s32 unit_list_read_pad(void) {
    u32 input = PadRead(0);

    if (unit_list_get_input_lock() != 0) {
        input = 0;
    }
    return input;
}

/* The double-buffer binder uses a volatile view of these two capacities. */
extern u16 g_unit_list_gfx_line_g2_capacity;
extern u16 g_unit_list_gfx_line_g3_capacity;

/* BUNIT.OUT entry: set up stack-allocated primitive pools and double packet buffers, then run the
 * unit list (or job menu) frame by frame, playing the queued menu sound each frame, until the list
 * closes. Returns the chosen battle unit.
 *
 * The capacity stores follow the shared overlay gfx-init order (compare
 * jobstts_entrypoint): 0x1e is first stored just before the 0x3c store, which
 * puts its single-set `li` between the 0x1f4 `li` and its store, in $v1. The
 * globals must not be volatile here: sched2 hoists the 0x40/0x1f4/0x3c stores
 * above the zero stores, as in the target. */
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
    g_unit_list_gfx_line_g2_capacity = 0;
    g_unit_list_gfx_line_g3_capacity = 0;
    g_gfx_line_g4_capacity = 0;
    g_gfx_tile_capacity = 0x1e;
    g_gfx_tile_1_capacity = 0;
    g_gfx_tile_8_capacity = 0;
    g_gfx_tile_16_capacity = 0;
    g_gfx_sprite_capacity = 0;
    g_gfx_sprite_8_capacity = 0;
    g_gfx_sprite_16_capacity = 0;
    g_unit_list_gfx_draw_move_capacity = 0x1e;
    g_unit_list_gfx_draw_area_capacity = 0x1e;
    g_gfx_draw_mode_capacity = 0x1e;
    unit_list_gfx_init_buffers(buffers, otags, 0, 0, 0, textured_quads, 0, 0, gouraud_quads, gouraud_textured_quads, 0,
        0, 0, 0, 0, 0, tiles, 0, 0, 0, 0, 0, 0, draw_moves, draw_areas, draw_modes);
    running = 1;
    unit_list_init_vram_and_fade();
    unit_list_init(g_unit_selected_index);
    do {
        unit_list_gfx_submit_frame(0, 0);
        previous_mode = g_menu_input_lock_mode;
        unit_list_update_input();
        unit_list_update_input_lock();
        gfx_update_decreasing_fade();
        if (g_menu_input_newly_pressed & PSX_PAD_SELECT) {
            unit_list_start_text_thread(3);
        }
        if (g_unit_list_help_screen_id != MENU_GRID_HELP_NONE) {
            unit_list_update_help_cursor(g_unit_list_pointers[g_unit_selected_index]->uses_monster_skillset);
        }
        if (previous_mode != 0 && g_menu_input_lock_mode == 0) {
            text_restore_section_pointers();
        }
        menu_set_palette_mode(g_menu_input_lock_mode);
        if (g_unit_list_current_menu == UNIT_LIST_MENU_JOB) {
            unit_list_update_job_page();
            /* The target also passes a1 = 0 to this one-argument callee. */
            ((void (*)(s32, s32))unit_list_draw_shaded_rows)(g_unit_list_page_offset + g_unit_list_scroll_step, 0);
        } else {
            running = unit_list_run_frame();
            unit_list_update_grid(1, 1, 1, 1, 0,
                g_unit_list_help_screen_id == MENU_GRID_HELP_NONE ? g_unit_list_input_secondary_repeat : 0,
                unit_list_is_busy());
        }
        unit_list_update_browser();

        /* Window frames, event input, then this frame's sound: a cursor or cancel sound is kept
         * as the queued effect, and the error message sound takes a free channel. */
        g_gfx_current_otag_entry = g_menu_gfx_context->otag + 0x3d;
        menu_draw_active_window_frames();
        menu_input_update_event_state(g_menu_gfx_context->otag + 9, unit_list_read_pad(), g_menu_gfx_drawenv_y);
        sound_request = g_sound_pending_sfx_id;
        if (sound_request == SOUND_SFX_CURSOR_MOVE || sound_request == SOUND_SFX_CANCEL) {
            g_sound_queued_effect_id = g_sound_pending_sfx_id;
        }
        if (sound_request < 0 || g_sound_suppressed != 0) {
            g_sound_pending_sfx_id = SOUND_SFX_NONE;
        }
        sound_id = g_sound_pending_sfx_id;
        if (sound_id == SOUND_SFX_NONE) {
            sound_id = g_sound_queued_effect_id;
        }
        if (sound_id == SOUND_SFX_ERROR_MESSAGE) {
            sound_play_sfx_on_free_channel(SOUND_SFX_ERROR_MESSAGE);
        } else if (sound_id != SOUND_SFX_NONE) {
            sound_play_sfx(sound_id);
        }
        g_sound_queued_effect_id = SOUND_SFX_NONE;
        g_sound_pending_sfx_id = SOUND_SFX_NONE;
    } while (running != 0);
    unit_panel_toggle_equipment_and_numbers(0);
    unit_panel_toggle_banner_thread(0);
    while (DrawSync(1) != 0) { }
    VSync(0);
    return g_unit_list_selected_battle_id;
}

/* Clear the unit list's primary and reorder results to -1 (nothing chosen). */
void unit_list_reset_results(void) {
    g_unit_list_primary_result = -1;
    g_unit_list_reorder_result = -1;
}

/* Returns 1 while the reorder list or the triangle menu thread runs, or input is locked
 * for a message; the unit grid takes no input meanwhile. */
s32 unit_list_is_busy(void) {
    s32 result = 0;
    if (thread_is_running(UNIT_LIST_THREAD_SLOT_REORDER_MENU) || g_menu_input_lock_mode != 0
        || (result = 0, thread_is_running(UNIT_LIST_THREAD_SLOT_MENU))) {
        result = 1;
    }
    return result;
}

/* One frame of the unit list (or the reorder list when that is the current menu).
 * Returns 0 once the closing fade has finished, else 1.
 *
 * The first frame rebuilds the unit tables and the panels' data. TRIANGLE or START
 * opens the triangle menu, whose rows choose the reorder list (0) or the highlighted
 * unit (1, which fades out and returns its battle id). CIRCLE while browsing opens the
 * job menu, refused for a unit on a monster skillset or off the player's team; CROSS
 * outside browsing fades the screen out. */
static s32 unit_list_run_frame(void) {
    s32 input = g_menu_input_primary_repeat;
    unit_list_record_t* unit;
    u8 banner_enabled;
    u8 browse_enabled;

    if (g_unit_list_current_menu == UNIT_LIST_MENU_UNIT_LIST) {
        if (g_unit_list_initialized == 0) {
            g_unit_list_status_banner_enabled = g_unit_list_saved_status_banner_enabled;
            g_unit_list_browse_enabled = g_unit_list_saved_unit_browse_enabled;
            unit_list_init(g_unit_selected_index);
            g_unit_list_visible_count = unit_list_build_records(-1, (s32)g_unit_list_pointers, 1);
            g_unit_list_reorderable_count = unit_list_build_sortable();
            unit_list_build_index_map();
            g_unit_list_grid_selected_cell = g_unit_list_index_map[g_unit_selected_index];
            unit_list_copy_selected();
            g_unit_panel_status_display_dimmed = 0;
            g_unit_panel_numbers_thread_params.style = 0;
            g_unit_panel_character_status_frame_config.style = 0;
            unit_list_reset_results();
            g_unit_list_initialized = 1;
        }
        if (gfx_update_increasing_fade() == 0) {
            g_unit_list_initialized = 0;
            menu_request_thread_stop(UNIT_LIST_THREAD_SLOT_MENU);
            return 0;
        }
        if (g_unit_list_triangle_menu_open != 0) {
            /* Help for the highlighted row, from HELP_TEXT_SECTION_MENU_COMMANDS (0x1000). */
            g_text_selection_id = g_unit_list_triangle_menu.selected_index != 0 ? 0x1001 : 0x1000;
            if ((g_unit_list_triangle_menu_open
                    = unit_list_run_menu_thread(UNIT_LIST_THREAD_SLOT_MENU, &g_unit_list_triangle_menu))
                == 0) {
                if (g_unit_list_primary_result == 0) {
                    banner_enabled = g_unit_list_status_banner_enabled;
                    browse_enabled = g_unit_list_browse_enabled;
                    g_unit_list_current_menu = UNIT_LIST_MENU_REORDER_LIST;
                    g_unit_list_status_banner_enabled = 0;
                    g_unit_list_browse_enabled = 0;
                    g_unit_list_saved_status_banner_enabled = banner_enabled;
                    g_unit_list_saved_unit_browse_enabled = browse_enabled;
                    unit_list_reset_results();
                    g_unit_list_initialized = 0;
                } else if (g_unit_list_primary_result == 1) {
                    gfx_start_increasing_fade();
                    g_unit_list_selected_battle_id = g_unit_list_pointers[g_unit_selected_index]->battle_id;
                }
                g_unit_panel_character_status_frame_config.style = 0;
                g_unit_panel_numbers_thread_params.style = 0;
                return 1;
            }
            return 1;
        }
        if (g_unit_list_browse_enabled != 0) {
            g_text_selection_id = MENU_GRID_HELP_UNIT_BROWSE;
        } else if (g_unit_list_status_banner_enabled != 0) {
            g_text_selection_id
                = g_unit_list_status_banner_at_bottom != 0 ? MENU_GRID_HELP_BANNER_BOTTOM : MENU_GRID_HELP_BANNER_TOP;
        } else {
            g_text_selection_id = 0;
        }
        if (unit_list_get_input_lock() != 0) {
            return 1;
        }
        if ((input & PSX_PAD_TRIANGLE) || (input & PSX_PAD_START)) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            g_unit_panel_character_status_frame_config.style = 1;
            g_unit_panel_numbers_thread_params.style = 1;
            g_unit_list_triangle_menu_open = 1;
        } else if ((input & PSX_PAD_CIRCLE) && g_unit_list_browse_enabled != 0) {
            unit = g_unit_list_pointers[g_unit_selected_index];
            if (unit->uses_monster_skillset != 0 || (unit->initial_team_flags & UNIT_TEAM_MASK)) {
                g_sound_queued_effect_id = SOUND_SFX_INVALID;
            } else {
                g_unit_panel_status_display_dimmed = 1;
                g_unit_panel_numbers_thread_params.style = 1;
                g_unit_panel_character_status_frame_config.style = 1;
                g_unit_list_initialized = 0;
                g_unit_list_saved_unit_browse_enabled = g_unit_list_browse_enabled;
                g_unit_list_current_menu = UNIT_LIST_MENU_JOB;
                g_unit_list_saved_status_banner_enabled = g_unit_list_status_banner_enabled;
            }
        } else if (g_unit_list_browse_enabled == 0 && (input & PSX_PAD_CROSS)) {
            gfx_start_increasing_fade();
        }
    } else if (g_unit_list_current_menu == UNIT_LIST_MENU_REORDER_LIST) {
        unit_list_run_sort_menu();
    }
    return 1;
}

/* The target reaches g_unit_panel_status_display_flags through an address register (lui/addiu, then
 * 0(reg)) at each access site, which is what GCC emits for a dereferenced
 * constant address; a plain global access folds the address into lui/%lo. */
#define UNIT_LIST_REQUEST_FLAGS (*(s32*)&g_unit_panel_status_display_flags)

/* Per-frame step of the unit browser.
 *
 * When the selected unit differs from g_unit_list_comparison_index it republishes that unit's
 * record, restarts the slide animation, and reports the scroll direction in
 * g_unit_panel_status_display_redraw_request; otherwise it advances the g_unit_list_status_banner_slide_step slide and
 * stops the comparison banner thread once the slide completes. */
static void unit_list_update_browser(void) {
    s32 index;
    s32 enable;
    s8 slide;
    u8 value;

    if (g_unit_selected_index != g_unit_list_comparison_index) {
        if (g_unit_list_status_banner_enabled != 0) {
            g_unit_panel_comparison_display_flags = UNIT_LIST_REQUEST_FLAGS;
            UNIT_LIST_REQUEST_FLAGS = 0;
            if (g_unit_list_status_banner_at_bottom_latch != g_unit_list_status_banner_at_bottom) {
                bcopy(g_unit_list_pointers[g_unit_list_comparison_index], &g_unit_panel_comparison_unit_status,
                    sizeof(g_unit_panel_comparison_unit_status));
                bcopy(&g_unit_list_pointers[g_unit_list_comparison_index]->name_index,
                    &g_unit_panel_panel_comparison_unit_data, sizeof(g_unit_panel_panel_comparison_unit_data));
                unit_panel_toggle_comparison_thread(1);
                g_unit_list_status_banner_at_bottom_latch = g_unit_list_status_banner_at_bottom;
                if (g_unit_list_status_banner_slide_step < 0) {
                    g_unit_list_status_banner_slide_step += 0x10;
                } else if (g_unit_list_status_banner_slide_step > 0) {
                    g_unit_list_status_banner_slide_step -= 0x10;
                } else if (g_unit_list_status_banner_at_bottom != 0) {
                    g_unit_list_status_banner_slide_step = -1;
                } else {
                    g_unit_list_status_banner_slide_step = 1;
                }
            }
        } else {
            value = g_unit_list_status_banner_at_bottom;
            g_unit_list_status_banner_at_bottom_latch = value;
            if (value != 0) {
                g_unit_panel_status_display_offset_y = 0x90;
            } else {
                g_unit_panel_status_display_offset_y = 0;
            }
        }
        index = g_unit_list_comparison_index;
        unit_list_copy_selected();
        if ((g_menu_input_primary_repeat & PSX_PAD_RIGHT) || (g_menu_input_secondary_repeat & PSX_PAD_R1)) {
            g_unit_panel_status_display_redraw_request = 1;
        } else if ((g_menu_input_primary_repeat & PSX_PAD_LEFT) || (g_menu_input_secondary_repeat & PSX_PAD_L1)) {
            g_unit_panel_status_display_redraw_request = 2;
        } else {
            g_unit_panel_status_display_redraw_request = index < g_unit_selected_index ? 1 : 2;
        }
        return;
    }
    if (g_unit_list_status_banner_at_bottom_latch != g_unit_list_status_banner_at_bottom
        && g_unit_list_status_banner_slide_step == 0) {
        g_unit_list_status_banner_at_bottom_latch = g_unit_list_status_banner_at_bottom;
        if (g_unit_list_status_banner_enabled != 0) {
            if (g_unit_list_status_banner_at_bottom != 0) {
                UNIT_LIST_REQUEST_FLAGS |= MENU_STATUS_PANEL_SLIDE_DOWN;
            } else {
                UNIT_LIST_REQUEST_FLAGS |= MENU_STATUS_PANEL_SLIDE_UP;
            }
        } else if (g_unit_list_status_banner_at_bottom != 0) {
            g_unit_panel_status_display_offset_y = 0x90;
        } else {
            g_unit_panel_status_display_offset_y = 0;
        }
    }
    unit_panel_toggle_equipment_and_numbers(g_unit_list_browse_enabled);
    enable = 0;
    if (g_unit_list_status_banner_enabled != 0 || g_unit_list_browse_enabled != 0) {
        enable = 1;
    }
    unit_panel_toggle_banner_thread(enable);
    slide = g_unit_list_status_banner_slide_step;
    if (slide < 0) {
        g_unit_panel_status_display_offset_y = slide * 8 + 0x108;
        g_unit_panel_comparison_display_offset_y = slide * 8;
        if (--g_unit_list_status_banner_slide_step < -0xF) {
            g_unit_list_status_banner_slide_step = 0;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    } else if (slide > 0) {
        g_unit_panel_status_display_offset_y = slide * 8 - 0x78;
        g_unit_panel_comparison_display_offset_y = slide * 8 + 0x90;
        if (++g_unit_list_status_banner_slide_step >= 0x10) {
            g_unit_list_status_banner_slide_step = 0;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    }
}

/* Copy the selected unit's stat summary, identity and stat detail blocks
 * into the panel records and request a redraw of all four panels. */
static void unit_list_copy_selected(void) {
    unit_list_record_t* comparison;

    g_unit_list_comparison_index = g_unit_selected_index;
    comparison = g_unit_list_pointers[g_unit_list_comparison_index];
    bcopy(comparison, &g_unit_panel_selected_unit_status, sizeof(g_unit_panel_selected_unit_status));
    bcopy(&g_unit_list_pointers[g_unit_selected_index]->name_index, &g_unit_panel_panel_selected_unit_data,
        sizeof(g_unit_panel_panel_selected_unit_data));
    bcopy(&g_unit_list_pointers[g_unit_selected_index]->move, &g_unit_panel_selected_unit_fields,
        sizeof(g_unit_panel_selected_unit_fields));
    g_unit_panel_status_display_redraw_request = 1;
    g_unit_panel_comparison_display_redraw_request = 1;
    g_unit_panel_character_status_frame_config.redraw_request = 1;
    g_unit_panel_numbers_thread_params.redraw_request = 1;
}
