/* The Formation screen: its display setup and list rebuilds, the input lockout and button filter, the screen loop and
 * unit grid frame, and selecting and staging a unit. The shops, the Soldier Office, name entry and the save screen
 * reuse its display setup, busy check and lockout. One -O1 object in WORLD. */

#include "fft/game/battle_unit.h"
#include "fft/game/item.h"
#include "fft/game/party.h"
#include "fft/gfx/fade.h"
#include "fft/gfx/gfx.h"
#include "fft/gfx/gfx_image.h"
#include "fft/gfx/gfx_primitive.h"
#include "fft/menu/help.h"
#include "fft/menu/menu.h"
#include "fft/menu/menu_gfx.h"
#include "fft/menu/menu_input.h"
#include "fft/menu/menu_list.h"
#include "fft/menu/menu_screen.h"
#include "fft/menu/menu_script.h"
#include "fft/menu/menu_selection.h"
#include "fft/menu/menu_text.h"
#include "fft/menu/menu_window.h"
#include "fft/party/equipment.h"
#include "fft/party/formation.h"
#include "fft/party/formation_ability.h"
#include "fft/party/formation_item.h"
#include "fft/party/formation_job.h"
#include "fft/party/item_list.h"
#include "fft/party/shop.h"
#include "fft/party/unit_grid.h"
#include "fft/party/unit_list.h"
#include "fft/party/unit_panel.h"
#include "fft/sys/music.h"
#include "fft/sys/sound.h"
#include "fft/sys/thread.h"
#include "fft/text/message.h"
#include "fft/text/text.h"
#include "fft/world/tutorial.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern clut_t g_formation_menu_palettes;

static u32 s_status_label_image[352] = { 0x111, 0, 0x111, 0x11111110, 0, 0x10000000, 0x111110, 0x11100000, 0,
    0x10000000, 17, 0x111111, 0x11100000, 0x10000000, 17, 0, 0x11100011, 1, 0x111, 0, 0x11100, 0x111110, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0x121, 0, 0x121, 0x12222210, 0, 0, 0x152251, 0x12100000, 17, 0x10000000, 18, 0x122221, 0x12100000,
    0x10000000, 18, 0, 0x21210121, 1, 0x121, 0, 0x1f2e10, 0x162210, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x11110121,
    0x11111111, 0x11111111, 0x11121111, 0x11111111, 0x1111, 0x11123321, 0x12111111, 0x11111112, 0x11111111, 0x111111,
    0x11111121, 0x11111111, 0x11111111, 0x11111111, 17, 0x11221221, 0x11111111, 0x11111111, 0x111, 0x1def2e1, 0x156210,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x52510121, 0x21235221, 0x61522123, 0x25121122, 0x15252215, 0x1522, 0x22111121,
    0x21152215, 0x15251122, 0x12121226, 0x152212, 0x26112221, 0x12121212, 0x15221522, 0x26152212, 18, 0x21232321,
    0x22512251, 0x61522121, 0x122, 0x1edef21, 0x145610, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x21210121, 0x63616541,
    0x21232121, 0x12121121, 0x12323212, 0x1212, 0x54111121, 0x11123216, 0x12121112, 0x12121212, 0x1121212, 0x12111121,
    0x12121212, 0x12121212, 0x12123212, 18, 0x21213121, 0x16211621, 0x21232121, 0x121, 0x1fedee1, 0x154510, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0x46211121, 0x56512121, 0x21212121, 0x62121121, 0x12121214, 0x1212, 0x12123321, 0x10121212,
    0x14621112, 0x12121212, 0x1121212, 0x12111121, 0x12121212, 0x12121212, 0x12121212, 18, 0x21211121, 0x26112611,
    0x21212121, 0x121, 0x1fed10, 0x165410, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x22512221, 0x32356261, 0x61212121, 0x25121122,
    0x12121212, 0x1522, 0x26152251, 0x10121256, 0x12251126, 0x16261226, 0x1152212, 0x26122221, 0x12162612, 0x15221522,
    0x26121212, 18, 0x21210121, 0x52215221, 0x61212121, 0x122, 0x11100, 0x111110, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x11111111, 0x11111111, 0x11111111, 0x11111123, 0x11111111, 0x1112, 0x11111110, 0x10111111, 0x11111111, 0x11111231,
    0x111211, 0x31111111, 0x11111112, 0x11121112, 0x31111111, 18, 0x11110111, 0x11111111, 0x11111111, 0x123, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x61000000, 0x152, 0x10000000, 18, 0, 0, 0, 0x1210, 0x1001210, 0x10000000, 0x10000012,
    0x121012, 0x26100000, 21, 0, 0, 0x61000000, 0x152, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x11000000, 17,
    0x10000000, 17, 0, 0, 0, 0x1110, 0x1001110, 0x10000000, 0x10000011, 0x111011, 0x11100000, 1, 0, 0, 0x11000000, 17,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xc850000, 0x1109435d, 0x1df2156d, 0x18ee2eb9, 0x151610d2, 0xcec157c,
    0x19f71174, 0x3b3c227a, 0x14830000, 0x29067775, 0x4e2c3988, 0x18ee62d2, 0x151610d2, 0xcec157c, 0x19f71174,
    0x3b3c227a, 0x4430000, 0x10c725d0, 0x192a1509, 0x18ee1d8e, 0x151610d2, 0xcec157c, 0x19f71174, 0x3b3c227a,
    0x14610000, 0x20c545ed, 0x31482906, 0x18ee3dab, 0x151610d2, 0xcec157c, 0x19f71174, 0x3b3c227a };
static RECT s_status_label_rect = { 384, 245, 64, 11 };

/* Set up the formation screen's display: reset the menu context, clear the screen (except when
 * entering from name entry), upload the menu glyph sheet, palettes and status label image, and
 * copy two window-frame palettes into VRAM row 510 with colors 4, 8 and 12 cleared. */
void formation_init_display(void) {
    RECT rect;
    palette_t palette;

    menu_screen_reset_context();
    menu_set_event_speed(0);
    if (g_menu_screen_context_id != MENU_SCREEN_CONTEXT_NAME_ENTRY) {
        rect.x = 0;
        rect.y = 0;
        rect.w = 256;
        rect.h = 480;
        ClearImage(&rect, 0, 0, 0);
        SetDispMask(1);
    }
    rect.x = 576;
    rect.y = 256;
    rect.w = 64;
    rect.h = 48;
    gfx_load_image_and_wait(&rect, g_menu_glyph_sheet->pixels);
    rect.x = 576;
    rect.y = 506;
    rect.w = 64;
    rect.h = 4;
    gfx_load_image_and_wait(&rect, g_formation_menu_palettes);
    rect.x = 960;
    rect.y = 496;
    rect.w = 16;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, palette);

    /* Each window-frame palette is copied whole to x 0x260/0x270 of row 510,
     * and its colors 4..15 again to x 0x240/0x250, after clearing colors 4, 8
     * and 12. */
    palette[4].red = palette[4].green = palette[4].blue = palette[4].semi_transparent = 0;
    palette[8].red = palette[8].green = palette[8].blue = palette[8].semi_transparent = 0;
    palette[12].red = palette[12].green = palette[12].blue = palette[12].semi_transparent = 0;
    rect.x = 608;
    rect.y = 510;
    rect.w = 16;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette);
    rect.x = 576;
    rect.y = 510;
    rect.w = 12;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &palette[4]);
    rect.x = 960;
    rect.y = 500;
    rect.w = 16;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, palette);
    palette[4].red = palette[4].green = palette[4].blue = palette[4].semi_transparent = 0;
    palette[8].red = palette[8].green = palette[8].blue = palette[8].semi_transparent = 0;
    palette[12].red = palette[12].green = palette[12].blue = palette[12].semi_transparent = 0;
    rect.x = 624;
    rect.y = 510;
    rect.w = 16;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette);
    rect.x = 592;
    rect.y = 510;
    rect.w = 12;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &palette[4]);
    gfx_load_image_and_wait(&s_status_label_rect, s_status_label_image);
    g_help_message_text_id = 0;
    menu_scrollable_list_init_core(0, 0, 0);
    fade_menu_start_in();
    menu_selection_clear_values();
    g_formation_unit_cycle_mode = 0;
    menu_screen_reset_selection_results();
    menu_screen_text_bind_section(MENU_SCREEN_TEXT_DATA_SKILLSET_NAMES);
    menu_screen_text_bind_section(MENU_SCREEN_TEXT_DATA_JOB_NAMES);
    menu_screen_text_bind_section(MENU_SCREEN_TEXT_DATA_ABILITY_NAMES);
    menu_screen_text_bind_section(MENU_SCREEN_TEXT_DATA_FORMATION_MESSAGES);
    g_text_section_pointers[TEXT_SECTION_COMPOSED_TEXT] = g_formation_menu_text_buffer;
    g_text_section_pointers[TEXT_SECTION_OVERLAY_SCREEN] = g_formation_menu_text_table;
    g_formation_cursor_ot_override = 0;
    g_tutorial_id = 0;
}

/* Rebuild the formation unit list for the screen in use: every unit on the
 * formation screen, the monsters at the soldier office and the available
 * humans in the other shops. Also reconciles the five item category lists
 * and resets the cursor and scroll. */
static void rebuild_lists(void) {
    formation_record_list_mode_e mode;

    if (g_menu_screen_context_id == MENU_SCREEN_CONTEXT_FORMATION) {
        mode = FORMATION_RECORD_LIST_ALL;
    } else {
        mode = FORMATION_RECORD_LIST_MONSTERS;
        if (g_menu_screen_context_id != MENU_SCREEN_CONTEXT_SOLDIER_OFFICE) {
            mode = FORMATION_RECORD_LIST_AVAILABLE_HUMANS;
        }
    }
    g_formation_unit_count = formation_build_record_list(0, g_formation_unit_pointers, mode);
    item_list_rebuild_category(0);
    item_list_rebuild_category(1);
    item_list_rebuild_category(2);
    item_list_rebuild_category(3);
    item_list_rebuild_category(4);
    formation_grid_init_cursor(0);
    g_formation_screen_running = 1;
}

/* Reports whether the screen is busy: the grid help is up (g_menu_screen_grid_id),
 * a menu runs in the upper menu or screen menu thread slot, or menu input is
 * locked (g_menu_input_lock_mode). */
s32 menu_screen_check_busy(void) {
    return g_menu_screen_grid_id != MENU_GRID_HELP_NONE || thread_is_running(MENU_SCREEN_THREAD_SLOT_UPPER_MENU) != 0
        || g_menu_input_lock_mode != 0 || thread_is_running(THREAD_SCREEN_SLOT_MENU) != 0;
}

/* Show message `message_id` in the text thread (dialog type 0x2B), play
 * `sound_id` at once and lock menu input while the message is up. */
void message_box_show_with_sound(s32 message_id, s32 sound_id) {
    menu_text_thread_start_if_idle(THREAD_SCREEN_SLOT_TEXT, 0x2B, message_id, 0);
    g_menu_screen_sound_release_held_loop = 1;
    sound_play_sfx_on_free_channel(sound_id);
    g_menu_input_lock_mode = 1;
}

/* Show `message_id` in the text thread (dialog type 0x1B) with the formation
 * unit's roster slot as speaker, unless the thread is busy. */
void formation_unit_show_message(s16 unit_index, s32 message_id) {
    menu_text_thread_start_if_idle(
        THREAD_SCREEN_SLOT_TEXT, 0x1B, message_id, g_formation_unit_pointers[unit_index]->identity.roster_slot);
}

/* Countdown maintained by menu_screen_update_input_lockout. */
extern u8 g_menu_screen_input_lockout_timer;

/* Count g_menu_screen_input_lockout_timer down while thread 7 (the comparison status panel) is idle
 * and no status panel slides, and reset it to 10 otherwise. While it is nonzero, the menu input
 * masks are cleared (menu_input_clear_state). Compiled at -O1 with the rest of the Formation
 * screen: the reloads of the timer are that level's code, not a volatile variable. */
void menu_screen_update_input_lockout(void) {
    if (thread_is_running(THREAD_SCREEN_SLOT_COMPARISON_STATUS) != 0) {
        g_menu_screen_input_lockout_timer = 10;
    } else if (g_unit_panel_status_display_thread_params.flags
        & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP)) {
        g_menu_screen_input_lockout_timer = 10;
    } else if (g_menu_screen_input_lockout_timer != 0) {
        g_menu_screen_input_lockout_timer--;
    }
    if (g_menu_screen_input_lockout_timer != 0) {
        menu_input_clear_state();
    }
}

/* Countdown maintained by menu_screen_update_input_lockout; while it is nonzero the formation
 * screen's per-frame handler returns before reading controller input. */
s32 menu_screen_get_input_lockout(void) {
    return g_menu_screen_input_lockout_timer;
}

/* screen dispatch state */
extern void (*g_formation_menu_handlers[])(void);
static s8 s_formation_main_menu_initialized;
extern menu_entry_t g_formation_egg_triangle_menu;

/* Chained after the egg triangle menu; update_unit_grid_menu only resets it. */
extern menu_entry_t g_formation_egg_submenu;
extern s16 g_formation_selected_unit_index_latch;
extern u8 g_formation_unit_slide_pending;
extern u8 g_formation_unit_slide_frame;

/* Read the menu input (tutorial or pad) and filter it: nothing while the
 * item category change, the status banner or the lockout timer is busy, and
 * no Select or Start (mask 0xF0FF) while a help text is requested or
 * showing, or while the dismiss menu's message runs. */
s32 menu_screen_filter_buttons(void) {
    s32 input = menu_screen_read_buttons(0);

    if (g_formation_item_category_input_lock != 0 || g_formation_banner_active != 0
        || menu_screen_get_input_lockout() != 0) {
        input = 0;
    } else if (thread_is_running(THREAD_SCREEN_SLOT_TEXT) != 0) {
        if (g_formation_current_menu == FORMATION_MENU_DISMISS_UNIT) {
            input &= 0xF0FF;
        }
    } else if (g_help_message_text_id != 0 || g_menu_screen_grid_id != MENU_GRID_HELP_NONE) {
        input &= 0xF0FF;
    }
    return input;
}

/* Steps the formation unit-selection slide; once its frame counter reaches 9 it
 * ends the slide, sets the status display redraw_request to 1 and clears flags.
 *
 * Compiled at -O1 like its neighbors: the final arm's two field stores share
 * one `la` of redraw_request, with flags at -4 from it. At -O2, cse2 folds that
 * -4 store back into an absolute address and the scheduler moves the first
 * arm's store into the frame load's delay slot.
 */
static void update_selection_slide(void) {
    if (g_formation_unit_slide_frame < 9) {
        g_formation_unit_slide_frame++;
        g_unit_panel_status_display_thread_params.redraw_request = 0;
    } else {
        g_formation_unit_slide_frame = 0;
        g_formation_unit_slide_pending = 0;
        g_unit_panel_status_display_thread_params.redraw_request = 1;
        g_unit_panel_status_display_thread_params.flags = 0;
    }
}

/*
 * Run the WORLD formation screen until g_formation_screen_running clears or the tutorial ends.
 *
 * Binds both packet buffers onto a stack-resident pool block, starts the
 * formation music unless a tutorial (1-10) is running, then steps the current
 * formation menu handler once per frame. On exit it waits for menu threads 1-16
 * to finish and saves the edited records back to party data.
 *
 * Compiled with the -O1 profile: the target leaves the packet-pool argument
 * stores unscheduled and reloads g_formation_unit_cycle_mode where -O2's cse-follow-jumps would
 * reuse it. i is one local reused for the button disabled flag, the otag
 * index and the thread loop counter, which keeps the (s16) extension.
 */
void formation_run_screen(s32 tutorial, s32 unused) {
    POLY_FT4 textured_quads[2 * 500];
    POLY_G4 gradient_quads[2 * 90];
    LINE_G2 gradient_lines[2 * 130];
    LINE_F2 flat_lines[2 * 10];
    POLY_GT4 textured_gradient_quads[2 * 60];
    TILE tiles[2 * 30];
    DR_MOVE draw_moves[2 * 30];
    DR_MODE draw_modes[2 * 30];
    DR_AREA draw_areas[2 * 30];
    u32 otags[2 * 64];
    menu_gfx_context_t buffers[2];
    u8 unused_packet_gap[0x18]; /* Retains the original unused stack space. */
    s32 buttons;
    s32 i;
    u8 last_menu;

    g_menu_screen_ot_length = 64;
    g_menu_screen_textured_quad_capacity = 500;
    g_menu_screen_gradient_quad_capacity = 90;
    g_menu_screen_textured_gradient_quad_capacity = 60;
    g_menu_screen_flat_line_capacity = 10;
    g_menu_screen_gradient_line_capacity = 130;
    g_gfx_tile_capacity = 30;
    g_menu_screen_draw_move_capacity = 30;
    g_menu_screen_draw_area_capacity = 30;
    g_gfx_draw_mode_capacity = 30;

    menu_gfx_init_contexts(buffers, otags, 0, 0, 0, textured_quads, 0, 0, gradient_quads, textured_gradient_quads,
        flat_lines, 0, 0, gradient_lines, 0, 0, tiles, 0, 0, 0, 0, 0, 0, draw_moves, draw_areas, draw_modes);
    g_menu_screen_context_id = MENU_SCREEN_CONTEXT_FORMATION;
    formation_init_menu_state();

    /* Values above 10 are not tutorials and become 0, the plain screen. */
    tutorial = tutorial < 11 ? tutorial : 0;
    tutorial_start(tutorial);
    if (tutorial == 0) {
        g_menu_screen_music_slot = music_load_into_free_slot(MUSIC_FORMATION);
        while (music_is_loading() != 0) { }
        music_switch_track(g_menu_screen_music_slot, 0x7F, 0);
    }

    while (g_formation_screen_running != 0) {
        menu_gfx_present_frame(0, 0);
        if (tutorial_update() == 0) {
            break;
        }
        menu_input_update_message_state();
        if (g_formation_banner_active != 0) {
            formation_banner_countdown(0, 0);
            menu_input_clear_state();
        }
        menu_screen_update_input_lockout();
        fade_menu_update_in();
        if (g_menu_input_primary_repeat & PSX_PAD_SELECT) {
            help_start_text(DIALOG_POSITION_CENTER);
        }
        if (g_menu_screen_grid_id != MENU_GRID_HELP_NONE) {
            menu_screen_update_grid_cursor(
                g_formation_unit_pointers[g_unit_selected_index]->detail.uses_monster_skillset);
        }
        menu_palette_set_mode(g_menu_input_lock_mode);

        g_formation_menu_handlers[g_formation_current_menu]();
        if (g_formation_current_menu == FORMATION_MENU_UNIT_GRID
            || g_formation_current_menu == FORMATION_MENU_DISMISS_UNIT
            || g_formation_current_menu == FORMATION_MENU_ORDER_UNIT) {
            unit_grid_update(1, g_formation_current_menu == FORMATION_MENU_UNIT_GRID,
                g_formation_current_menu == FORMATION_MENU_UNIT_GRID,
                g_formation_triangle_menu_open + g_menu_screen_grid_id != 0
                    ? 0
                    : g_menu_screen_input_secondary_repeat_latched,
                menu_screen_check_busy(), 0, 0);
        } else {
            unit_grid_draw_background(g_formation_scroll_position + g_formation_scroll_velocity, 0);
        }

        /* L1/R1 cycle units; i is the buttons' disabled flag here. */
        i = 0;
        if (g_formation_unit_cycle_mode != 0 || g_formation_unit_browse_enabled != 0) {
            if (g_menu_input_lock_mode != 0 || g_menu_screen_grid_id != MENU_GRID_HELP_NONE
                || g_formation_unit_cycle_mode == 1 || g_formation_triangle_menu_running != 0) {
                i = 1;
            }
            buttons = 0;
            if (g_formation_current_menu == FORMATION_MENU_DISMISS_UNIT) {
                i = 1;
            }
            if (i == 0) {
                buttons = g_menu_input_secondary_repeat;
            }
            formation_cycle_selected_unit(buttons);
            menu_draw_pressable_button(
                MENU_PRESSABLE_BUTTON_L1, 0xC, 0xE, buttons & PSX_PAD_L1, i, SOUND_SFX_PAGE_SWITCH);
            menu_draw_pressable_button(
                MENU_PRESSABLE_BUTTON_R1, 0xE0, 0xE, buttons & PSX_PAD_R1, i, SOUND_SFX_PAGE_SWITCH);
        }

        if (g_formation_current_menu != FORMATION_MENU_ITEM_REMOVE) {
            unit_grid_update_selection();
        }
        if (g_formation_unit_slide_pending != 0) {
            update_selection_slide();
        }

        /* The event layer goes to OT 0x3E over the item list, the job wheel and the transitions (14 and up). */
        g_gfx_current_otag_entry = g_menu_gfx_context->otag + 63;
        menu_draw_active_window_frames();
        if (g_formation_current_menu == FORMATION_MENU_ITEM_LIST
            || g_formation_current_menu >= FORMATION_MENU_JOB_WHEEL_ENTER
            || g_formation_current_menu == FORMATION_MENU_JOB_WHEEL) {
            i = 0x3E;
        } else {
            i = 9;
        }
        if (g_menu_screen_grid_id != MENU_GRID_HELP_NONE) {
            i = 9;
        }
        if (g_formation_cursor_ot_override != 0) {
            i = g_formation_cursor_ot_override;
        }
        menu_input_update_event_state(g_menu_gfx_context->otag + i, menu_screen_filter_buttons(),
            g_menu_screen_draw_buffer_clip_y == 0 ? 0xF0 : 0);

        /* Entering a new menu resets the per-menu state. */
        if (g_formation_current_menu != last_menu) {
            g_formation_cursor_ot_override = 0;
            g_formation_unit_cycle_mode = 0;
            menu_set_event_speed(0);
            g_help_message_text_id = 0;
            menu_screen_reset_selection_results();
            last_menu = g_formation_current_menu;
        }
        menu_screen_update_sound_effect();
        music_update_tunes();
    }

    /* Stop every menu thread and let the tutorial text finish; buttons flags one still running. */
    do {
        buttons = 0;
        for (i = 1; i < THREAD_ID_COUNT; i++) {
            if (thread_is_running(i) != 0) {
                buttons = 1;
                menu_thread_request_stop(i);
            }
        }
        if (thread_is_running(MENU_SCREEN_THREAD_SLOT_TUTORIAL_TEXT) != 0) {
            buttons = 1;
            menu_screen_text_resume_printing(MENU_SCREEN_THREAD_SLOT_TUTORIAL_TEXT);
        }
        menu_input_update_event_state(g_menu_gfx_context->otag, 0, 0);
    } while (buttons != 0);

    if (tutorial == 0) {
        music_unload_scenario_slot(g_menu_screen_music_slot);
    }
    formation_save_records();
    unit_panel_stop_all();
    menu_set_event_speed(0);
    menu_screen_reset_context();
}

/* Per-frame WORLD sound effect dispatch. A queued text glyph or page sound
 * plays at once on a free channel pair, as does an error message sound in
 * the menu queue. Then the id in g_sound_pending_sfx_id (dropped
 * when negative or while the menu sounds are muted), or failing that the one
 * in g_sound_queued_effect_id, plays on the sound_play_sfx channels
 * and both queues are cleared. Cancel and Cursor Move are copied to the menu
 * queue before the mute test, so they still play while muted. */
void menu_screen_update_sound_effect(void) {
    s32 sound_id;

    if (g_menu_screen_sound_release_held_loop != 0) {
        g_sound_queued_effect_id = SOUND_SFX_NONE;
    }
    if (g_sound_pending_sfx_id == SOUND_SFX_TEXT_GLYPH || g_sound_pending_sfx_id == SOUND_SFX_TEXT_PAGE) {
        sound_play_sfx_on_free_channel(g_sound_pending_sfx_id);
        g_sound_pending_sfx_id = SOUND_SFX_NONE;
    } else if (g_sound_queued_effect_id == SOUND_SFX_ERROR_MESSAGE) {
        sound_play_sfx_on_free_channel(SOUND_SFX_ERROR_MESSAGE);
        g_sound_queued_effect_id = SOUND_SFX_NONE;
    }
    if (g_sound_pending_sfx_id == SOUND_SFX_CANCEL || g_sound_pending_sfx_id == SOUND_SFX_CURSOR_MOVE) {
        g_sound_queued_effect_id = g_sound_pending_sfx_id;
    }
    if (g_sound_pending_sfx_id < 0 || g_sound_suppressed != 0) {
        g_sound_pending_sfx_id = SOUND_SFX_NONE;
    }
    sound_id = g_sound_pending_sfx_id;
    if (sound_id == SOUND_SFX_NONE) {
        sound_id = g_sound_queued_effect_id;
    }
    g_sound_queued_effect_id = SOUND_SFX_NONE;
    g_sound_pending_sfx_id = SOUND_SFX_NONE;
    if (sound_id != SOUND_SFX_NONE) {
        sound_play_sfx(sound_id);
    }
}

/* Reset the formation screen to its initial state: every submenu's
 * "initialized" latch, the panel thread styles, the selected unit and the
 * banner and browse settings, then the menu display and the unit list. */
void formation_init_menu_state(void) {
    s_formation_main_menu_initialized = 0;
    g_formation_item_menu_initialized = 0;
    g_formation_item_equip_menu_initialized = 0;
    g_formation_item_best_remove_initialized = 0;
    g_formation_item_submenu_initialized = 0;
    g_formation_item_category_menu_initialized = 0;
    g_item_rearrange_initialized = 0;
    g_formation_item_action_menu_initialized = 0;
    g_formation_item_sort_order_initialized = 0;
    g_formation_item_equipping_units_initialized = 0;
    g_formation_ability_menu_initialized = 0;
    g_formation_ability_remove_initialized = 0;
    g_formation_ability_set_initialized = 0;
    g_formation_ability_learn_menu_initialized = 0;
    g_formation_job_wheel_initialized = 0;
    g_formation_ability_learn_job_list_initialized = 0;
    g_formation_ability_learn_ability_list_open = 0;
    g_formation_ability_view_initialized = 0;
    g_formation_ability_view_list_initialized = 0;
    g_formation_dismiss_prompt_running = 0;
    g_formation_order_open = 0;
    /* Through the records rather than the member scalars (offset_y, flags,
     * highlight): the target addresses these stores from one base each. */
    g_unit_panel_status_display_thread_params.position.y = 144;
    g_unit_panel_status_display_thread_params.flags = 0;
    g_unit_panel_status_display_thread_params.style = 0;
    g_unit_panel_comparison_display_thread_params.style = 0;
    g_formation_change_banner_thread_params.style = 0;
    g_formation_stat_preview_panel_thread_params.style = 0;
    g_formation_ability_panel_thread_params.style = 0;
    g_formation_equipment_panel_thread_params.style = 0;
    g_unit_panel_numeric_panel_params.style = 0;
    g_unit_panel_preview_stats_thread_params.style = 0;
    g_unit_panel_preview_stats_thread_params.position.y = 2;
    g_formation_triangle_menu.selected_index = 0;
    g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
    g_unit_selected_index = 0;
    g_formation_selected_unit_index_latch = 0;
    g_formation_unit_cycle_mode = 1;
    g_formation_unit_banner_enabled = 1;
    g_formation_unit_banner_at_bottom_latch = 1;
    g_formation_unit_banner_at_bottom = 1;
    g_formation_unit_browse_enabled = 0;
    g_formation_cursor_ot_override = 0;
    g_formation_triangle_menu_open = 0;
    formation_init_display();
    rebuild_lists();
}

/* Clear the 16 menu selection results to -1 (nothing chosen).
 *
 * Built at -O1: -O1 does no strength reduction, which is why the target
 * recomputes the index inside the loop. Same profile as the adjacent
 * formation_init_menu_state. */
void menu_screen_reset_selection_results(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        g_menu_screen_selection_results[i] = -1;
    }
}

/*
 * Run one frame of the formation screen's unit grid, order and dismiss menus.
 *
 * On the unit grid, the first call rebuilds the unit list and clamps the
 * selection. Triangle or Start opens the unit's menu (Item, Ability, Job,
 * Dismiss, Order; an egg's menu only offers dismissal) and the choice moves
 * to that menu's state; Circle while browsing opens View Abilities; Cross
 * fades out, and the screen ends once the fade completes. The order and
 * dismiss menus run their own steps.
 */
void update_unit_grid_menu(void) {
    u32 input = g_menu_input_primary_repeat;

    if (g_formation_current_menu == FORMATION_MENU_UNIT_GRID) {
        if (s_formation_main_menu_initialized == 0) {
            g_formation_unit_banner_enabled = g_formation_saved_banner_enabled;
            g_formation_dismiss_anim_timer = 0;
            g_formation_unit_count
                = formation_build_record_list(0, g_formation_unit_pointers, FORMATION_RECORD_LIST_ALL);
            if (g_unit_selected_index > g_formation_unit_count - 1) {
                g_formation_selected_unit_index_latch = g_unit_selected_index = g_formation_unit_count - 1;
            }
            menu_screen_reset_selection_results();
            g_formation_unit_cycle_mode = 0;
            formation_init_selection_cursor(g_unit_selected_index);
            if (g_formation_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG) {
                g_formation_unit_browse_enabled = 0;
                g_unit_panel_numeric_panel_params.style = 0;
                g_formation_equipment_panel_thread_params.style = 0;
                menu_thread_request_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
                menu_thread_request_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
                g_formation_triangle_menu_open = 0;
            }
            s_formation_main_menu_initialized = 1;
        }

        /* Leave once the fade-out that Cross starts below completes. */
        if (g_tutorial_id == 0 && fade_menu_update_out() == 0) {
            s_formation_main_menu_initialized = 0;
            thread_suspend(THREAD_SCREEN_SLOT_MENU);
            g_formation_screen_running = 0;
            return;
        }

        if (g_formation_triangle_menu_running != 0) {
            g_help_message_text_id = 0;
            /* An egg gets its own triangle menu, whose first option dismisses it. */
            if (g_formation_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG) {
                if ((g_formation_triangle_menu_running
                        = menu_managed_thread_start(THREAD_SCREEN_SLOT_MENU, &g_formation_egg_triangle_menu))
                    == 0) {
                    if (g_menu_screen_selection_results[0] == 0) {
                        formation_dismiss(g_unit_selected_index);
                        s_formation_main_menu_initialized = 0;
                        g_sound_queued_effect_id = SOUND_SFX_DISMISS;
                    }
                    g_formation_egg_submenu.selected_index = 0;
                    menu_screen_reset_selection_results();
                    g_formation_equipment_panel_thread_params.style = 0;
                    g_unit_panel_numeric_panel_params.style = 0;
                    g_formation_triangle_menu_open = 0;
                }
            } else {
                if ((g_formation_triangle_menu_running
                        = menu_managed_thread_start(THREAD_SCREEN_SLOT_MENU, &g_formation_triangle_menu))
                    == 0) {
                    if (g_menu_screen_selection_results[0] == 0) {
                        g_formation_current_menu = FORMATION_MENU_ITEM_ENTER;
                        formation_item_update_menu();
                    } else if (g_menu_screen_selection_results[0] == 1) {
                        g_formation_current_menu = FORMATION_MENU_ABILITY_ENTER;
                        formation_ability_update_menu();
                    } else if (g_menu_screen_selection_results[0] == 2) {
                        g_formation_current_menu = FORMATION_MENU_JOB_WHEEL_ENTER;
                        formation_job_update_menu();
                    } else if (g_menu_screen_selection_results[0] == 3) {
                        g_formation_current_menu = FORMATION_MENU_DISMISS_UNIT;
                    } else if (g_menu_screen_selection_results[0] == 4) {
                        g_formation_current_menu = FORMATION_MENU_ORDER_UNIT;
                    }
                    g_formation_equipment_panel_thread_params.style = 0;
                    g_unit_panel_numeric_panel_params.style = 0;

                    if (g_menu_screen_selection_results[0] != -1) {
                        g_formation_saved_banner_enabled = g_formation_unit_banner_enabled;
                        if (g_formation_current_menu != FORMATION_MENU_DISMISS_UNIT) {
                            menu_thread_request_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
                            menu_thread_request_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
                        } else {
                            g_formation_equipment_panel_thread_params.style = 1;
                            g_unit_panel_numeric_panel_params.style = 1;
                        }
                        g_formation_selection_cursor_ready = 0;
                        s_formation_main_menu_initialized = 0;
                        return;
                    }
                    g_formation_triangle_menu_open = 0;
                }
            }
        } else {
            g_help_message_text_id = -1;
            if (g_formation_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG) {
                g_help_message_text_id = 0x101C;
            } else if (g_formation_triangle_menu_open == 0) {
                if (g_formation_unit_browse_enabled != 0) {
                    g_help_message_text_id = MENU_GRID_HELP_UNIT_BROWSE;
                } else if (g_formation_unit_banner_enabled != 0) {
                    g_help_message_text_id = g_formation_unit_banner_at_bottom != 0 ? MENU_GRID_HELP_BANNER_BOTTOM
                                                                                    : MENU_GRID_HELP_BANNER_TOP;
                }
            }

            if (menu_screen_get_input_lockout() == 0 && unit_grid_check_scroll() == 0) {
                if ((input & PSX_PAD_TRIANGLE) || (input & PSX_PAD_START) || g_formation_triangle_menu_open != 0) {
                    /* Keep the menu clear of the banner and of the cursor's half of the grid. */
                    if (g_formation_unit_banner_at_bottom != 0) {
                        g_formation_triangle_menu.window.y = 32;
                    } else {
                        g_formation_triangle_menu.window.y = 120;
                    }
                    g_formation_triangle_menu.window.x
                        = ((g_unit_selected_index % 4) < 2 || g_formation_unit_browse_enabled != 0) ? 0x12C : 0x8A;
                    g_formation_equipment_panel_thread_params.style = 1;
                    g_unit_panel_numeric_panel_params.style = 1;
                    g_formation_triangle_menu_running = 1;
                    g_formation_triangle_menu_open = 1;
                    if ((input & PSX_PAD_TRIANGLE) || (input & PSX_PAD_START)) {
                        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
                    }
                } else if (input & PSX_PAD_CIRCLE) {
                    if (g_formation_unit_browse_enabled != 0) {
                        if (g_formation_unit_pointers[g_unit_selected_index]->detail.uses_monster_skillset != 0) {
                            g_sound_queued_effect_id = SOUND_SFX_INVALID;
                        } else {
                            s_formation_main_menu_initialized = 0;
                            g_formation_current_menu = FORMATION_MENU_VIEW_ABILITIES;
                        }
                    }
                }

                if (g_formation_unit_browse_enabled == 0 && (input & PSX_PAD_CROSS) && g_tutorial_id == 0) {
                    fade_menu_start_out();
                    music_set_target_volume(0, 0xF0);
                    g_sound_queued_effect_id = SOUND_SFX_CANCEL;
                }
            }
        }
    } else if (g_formation_current_menu == FORMATION_MENU_ORDER_UNIT) {
        formation_update_order_menu();
    } else if (g_formation_current_menu == FORMATION_MENU_DISMISS_UNIT) {
        formation_update_dismiss_menu();
    }
}

/*
 * Refresh formation-screen state after the selected unit may have changed.
 *
 * On a new selection this snapshots the unit's display data, starts the
 * status-banner slide (g_formation_banner_slide_step), and records the scroll direction in
 * g_unit_panel_status_display_thread_params.redraw_request; it then toggles the per-menu windows and advances the
 * slide. Compiled at O1 like its formation_stage_selected_unit neighbor.
 */
void unit_grid_update_selection(void) {
    s32 saved;
    s32 previous;
    s32* flags;

    if (g_unit_selected_index != g_formation_selected_unit_index_latch) {
        if (g_formation_unit_banner_enabled != 0) {
            saved = g_unit_panel_status_display_thread_params.flags;
            if (g_formation_unit_banner_at_bottom_latch != g_formation_unit_banner_at_bottom) {
                bcopy(&g_formation_unit_pointers[g_formation_selected_unit_index_latch]->status,
                    &g_formation_comparison_unit_stat_summary, sizeof(unit_panel_summary_t));
                bcopy(&g_formation_unit_pointers[g_formation_selected_unit_index_latch]->identity,
                    &g_formation_comparison_unit_identity, sizeof(menu_unit_status_identity_t));
                unit_panel_toggle_comparison_thread(1);
                g_unit_panel_comparison_display_thread_params.flags = saved;
                g_formation_unit_banner_at_bottom_latch = g_formation_unit_banner_at_bottom;
                if (g_formation_banner_slide_step < 0) {
                    g_formation_banner_slide_step += 16;
                } else if (g_formation_banner_slide_step > 0) {
                    g_formation_banner_slide_step -= 16;
                } else if (g_formation_unit_banner_at_bottom != 0) {
                    g_formation_banner_slide_step = -1;
                } else {
                    g_formation_banner_slide_step = 1;
                }
            }
        } else {
            if ((g_formation_unit_banner_at_bottom_latch = g_formation_unit_banner_at_bottom) != 0) {
                g_unit_panel_status_display_thread_params.position.y = 144;
            } else {
                g_unit_panel_status_display_thread_params.position.y = 0;
            }
        }
        previous = g_formation_selected_unit_index_latch;
        formation_stage_selected_unit();
        if ((g_menu_input_primary_repeat & PSX_PAD_RIGHT) || (g_menu_input_secondary_repeat & PSX_PAD_R1)) {
            g_unit_panel_status_display_thread_params.redraw_request = 1;
        } else if ((g_menu_input_primary_repeat & PSX_PAD_LEFT) || (g_menu_input_secondary_repeat & PSX_PAD_L1)) {
            g_unit_panel_status_display_thread_params.redraw_request = 2;
        } else {
            g_unit_panel_status_display_thread_params.redraw_request = previous < g_unit_selected_index ? 1 : 2;
        }
    } else if (g_formation_unit_banner_at_bottom_latch != g_formation_unit_banner_at_bottom
        && g_formation_banner_slide_step == 0) {
        g_formation_unit_banner_at_bottom_latch = g_formation_unit_banner_at_bottom;
        if (g_formation_unit_banner_enabled != 0) {
            if (g_formation_unit_banner_at_bottom != 0) {
                flags = &g_unit_panel_status_display_thread_params.flags;
                *flags |= MENU_STATUS_PANEL_SLIDE_DOWN;
            } else {
                flags = &g_unit_panel_status_display_thread_params.flags;
                *flags |= MENU_STATUS_PANEL_SLIDE_UP;
            }
        } else if (g_formation_unit_banner_at_bottom != 0) {
            g_unit_panel_status_display_thread_params.position.y = 144;
        } else {
            g_unit_panel_status_display_thread_params.position.y = 0;
        }
    }
    if (g_formation_current_menu != FORMATION_MENU_ABILITY_LEAVE
        && g_formation_current_menu != FORMATION_MENU_ITEM_LEAVE
        && g_formation_current_menu != FORMATION_MENU_JOB_WHEEL_LEAVE) {
        formation_open_equipment_panel(g_formation_unit_browse_enabled);
    }
    if (g_formation_current_menu == FORMATION_MENU_ITEM || g_formation_current_menu == FORMATION_MENU_ITEM_EQUIP
        || g_formation_current_menu == FORMATION_MENU_ITEM_BEST_FIT) {
        unit_panel_toggle_equipment_threads(1);
    } else if (g_formation_current_menu == FORMATION_MENU_ABILITY) {
        formation_toggle_ability_panel(1);
    }
    unit_panel_toggle_banner_thread(g_formation_unit_banner_enabled != 0 || g_formation_unit_browse_enabled != 0);
    if (g_formation_banner_slide_step < 0) {
        g_unit_panel_status_display_thread_params.position.y = g_formation_banner_slide_step * 8 + 0x108;
        g_unit_panel_comparison_display_thread_params.position.y = g_formation_banner_slide_step * 8;
        g_formation_banner_slide_step--;
        if (g_formation_banner_slide_step < -15) {
            g_formation_banner_slide_step = 0;
            menu_thread_request_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    } else if (g_formation_banner_slide_step > 0) {
        g_unit_panel_status_display_thread_params.position.y = g_formation_banner_slide_step * 8 - 0x78;
        g_unit_panel_comparison_display_thread_params.position.y = g_formation_banner_slide_step * 8 + 0x90;
        g_formation_banner_slide_step++;
        if (g_formation_banner_slide_step >= 16) {
            g_formation_banner_slide_step = 0;
            menu_thread_request_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    }
}

/*
 * Snapshot the selected formation unit after restoring its HP and MP.
 *
 * The three display-data groups are copied separately before the full unit
 * snapshot. O1 preserves the target's load/store order and direct array
 * reference in the final branch; O2 emits 364 bytes instead of 388.
 */
void formation_stage_selected_unit(void) {
    /* Preserve the target's unused 128-byte stack area. */
    u8 unused[128];
    formation_unit_t** units;
    s32 index;

    g_formation_selected_unit_index_latch = g_unit_selected_index;
    index = g_unit_selected_index;
    units = g_formation_unit_pointers;
    units[index]->status.gauges[MENU_UNIT_STATUS_GAUGE_HP].value
        = units[index]->status.gauges[MENU_UNIT_STATUS_GAUGE_HP].max;
    units[index]->status.gauges[MENU_UNIT_STATUS_GAUGE_MP].value
        = units[index]->status.gauges[MENU_UNIT_STATUS_GAUGE_MP].max;
    bcopy(&units[index]->status, &g_unit_panel_selected_unit_stat_summary, sizeof(unit_panel_summary_t));
    bcopy(&units[g_unit_selected_index]->identity, &g_formation_selected_unit_identity,
        sizeof(menu_unit_status_identity_t));
    bcopy(
        &units[g_unit_selected_index]->detail, &g_formation_selected_unit_stat_detail, sizeof(formation_unit_detail_t));
    formation_unit_copy_to_preview(g_unit_selected_index, FORMATION_PREVIEW_UNIT_INDEX);
    if (g_unit_panel_status_display_thread_params.flags == MENU_STATUS_PANEL_HIDDEN) {
        g_formation_unit_slide_pending = !(units[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG);
    } else {
        g_unit_panel_status_display_thread_params.flags
            = (g_formation_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG)
            ? MENU_STATUS_PANEL_HIDDEN
            : 0;
    }
    unit_panel_redraw_all();
}

/*
 * Restage the formation unit when the selection changes and step the unit
 * banner slide.
 *
 * On a change, the previous unit's display snapshot is kept for the slide
 * (g_formation_comparison_unit_stat_summary/g_formation_comparison_unit_identity, the second slot after
 * formation_stage_selected_unit's copies) and g_unit_panel_status_display_thread_params.redraw_request records the
 * direction: 1 for Right or R1 or a forward move, 2 otherwise. g_formation_banner_slide_step counts the
 * 16-frame banner slide in either direction, driving the banner y positions in
 * g_unit_panel_status_display_thread_params.position.y/g_unit_panel_comparison_display_thread_params.position.y.
 * Compiled at -O1; the flag update goes through a pointer so each branch materializes the address like the target.
 */
void formation_update_selection_stage(void) {
    s32 previous;
    s32* flags;

    if (g_unit_selected_index != g_formation_selected_unit_index_latch) {
        if (g_formation_unit_banner_enabled != 0) {
            if (g_formation_unit_banner_at_bottom_latch != g_formation_unit_banner_at_bottom) {
                bcopy(&g_formation_unit_pointers[g_formation_selected_unit_index_latch]->status,
                    &g_formation_comparison_unit_stat_summary, sizeof(unit_panel_summary_t));
                bcopy(&g_formation_unit_pointers[g_formation_selected_unit_index_latch]->identity,
                    &g_formation_comparison_unit_identity, sizeof(menu_unit_status_identity_t));
                unit_panel_toggle_comparison_thread(1);
                g_formation_unit_banner_at_bottom_latch = g_formation_unit_banner_at_bottom;
                if (g_formation_banner_slide_step < 0) {
                    g_formation_banner_slide_step += 16;
                } else if (g_formation_banner_slide_step > 0) {
                    g_formation_banner_slide_step -= 16;
                } else if (g_formation_unit_banner_at_bottom != 0) {
                    g_formation_banner_slide_step = -1;
                } else {
                    g_formation_banner_slide_step = 1;
                }
            }
        } else {
            if ((g_formation_unit_banner_at_bottom_latch = g_formation_unit_banner_at_bottom) != 0) {
                g_unit_panel_status_display_thread_params.position.y = 144;
            } else {
                g_unit_panel_status_display_thread_params.position.y = 0;
            }
        }
        previous = g_formation_selected_unit_index_latch;
        /* The fitting room shows stats for the equipment being tried on. */
        if (g_shop_menu_step >= SHOP_STEP_FITTING_ROOM && g_shop_menu_step <= SHOP_STEP_FITTING_ROOM_CHECKOUT) {
            formation_refresh_selected_stats();
        } else {
            formation_stage_selected_unit();
        }
        if ((g_menu_input_primary_repeat & PSX_PAD_RIGHT) || (g_menu_input_secondary_repeat & PSX_PAD_R1)) {
            g_unit_panel_status_display_thread_params.redraw_request = 1;
        } else if ((g_menu_input_primary_repeat & PSX_PAD_LEFT) || (g_menu_input_secondary_repeat & PSX_PAD_L1)) {
            g_unit_panel_status_display_thread_params.redraw_request = 2;
        } else {
            g_unit_panel_status_display_thread_params.redraw_request = previous < g_unit_selected_index ? 1 : 2;
        }
    } else if (g_formation_unit_banner_at_bottom_latch != g_formation_unit_banner_at_bottom
        && g_formation_banner_slide_step == 0) {
        g_formation_unit_banner_at_bottom_latch = g_formation_unit_banner_at_bottom;
        if (g_formation_unit_banner_enabled != 0) {
            if (g_formation_unit_banner_at_bottom != 0) {
                flags = &g_unit_panel_status_display_thread_params.flags;
                *flags |= MENU_STATUS_PANEL_SLIDE_DOWN;
            } else {
                flags = &g_unit_panel_status_display_thread_params.flags;
                *flags |= MENU_STATUS_PANEL_SLIDE_UP;
            }
        } else if (g_formation_unit_banner_at_bottom != 0) {
            g_unit_panel_status_display_thread_params.position.y = 144;
        } else {
            g_unit_panel_status_display_thread_params.position.y = 0;
        }
    }
    if (g_shop_menu_step >= SHOP_STEP_SOLDIER_MENU) {
        formation_open_equipment_panel(g_formation_unit_browse_enabled);
    } else {
        unit_panel_toggle_equipment_threads(g_formation_unit_browse_enabled);
    }
    unit_panel_toggle_banner_thread(g_formation_unit_banner_enabled != 0 || g_formation_unit_browse_enabled != 0);
    if (g_formation_banner_slide_step < 0) {
        g_unit_panel_status_display_thread_params.position.y = g_formation_banner_slide_step * 8 + 0x108;
        g_unit_panel_comparison_display_thread_params.position.y = g_formation_banner_slide_step * 8;
        g_formation_banner_slide_step--;
        if (g_formation_banner_slide_step < -15) {
            g_formation_banner_slide_step = 0;
            menu_thread_request_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    } else if (g_formation_banner_slide_step > 0) {
        g_unit_panel_status_display_thread_params.position.y = g_formation_banner_slide_step * 8 - 0x78;
        g_unit_panel_comparison_display_thread_params.position.y = g_formation_banner_slide_step * 8 + 0x90;
        g_formation_banner_slide_step++;
        if (g_formation_banner_slide_step >= 16) {
            g_formation_banner_slide_step = 0;
            menu_thread_request_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    }
}

/*
 * Rebuild the selected formation unit's derived stats from its job and
 * equipment, then refresh the three display snapshots.
 *
 * Shields in either hand go to the battle record's shield slots. Like
 * formation_stage_selected_unit, this needs O1 for the
 * target's store order and unreduced loop addressing.
 */
void formation_refresh_selected_stats(void) {
    /* Preserve the target's unused 128-byte stack area. */
    u8 unused[128];
    formation_unit_t record;
    battle_unit_t unit;
    s32 roster_slot;
    s32 i;
    s32 item;

    g_formation_selected_unit_index_latch = g_unit_selected_index;
    roster_slot = g_formation_unit_pointers[g_unit_selected_index]->identity.roster_slot;
    battle_unit_init_from_party_slot(&unit, roster_slot, 1);
    for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        unit.equipment[i] = ITEM_ID_NONE;
    }
    item = g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND]
        & ITEM_ENTRY_ID_MASK;
    if (item != ITEM_ID_NOTHING) {
        if (g_item_primary_data[item].type == ITEM_TYPE_SHIELD) {
            unit.equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = item;
        } else {
            unit.equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = item;
        }
    }
    item = g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND]
        & ITEM_ENTRY_ID_MASK;
    if (item != ITEM_ID_NOTHING) {
        if (g_item_primary_data[item].type == ITEM_TYPE_SHIELD) {
            unit.equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = item;
        } else {
            unit.equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = item;
        }
    }
    unit.equipment[UNIT_EQUIPMENT_SLOT_HEAD]
        = g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[MENU_EQUIPMENT_SLOT_HEAD];
    unit.equipment[UNIT_EQUIPMENT_SLOT_BODY]
        = g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[MENU_EQUIPMENT_SLOT_BODY];
    unit.equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]
        = g_formation_unit_pointers[g_unit_selected_index]->detail.equipment[MENU_EQUIPMENT_SLOT_ACCESSORY];
    for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        item = unit.equipment[i];
        if (item == ITEM_ID_NOTHING || item >= ITEM_ID_END) {
            unit.equipment[i] = ITEM_ID_NONE;
        }
    }
    battle_unit_refresh_formation_stats(&unit);
    formation_unit_build_record(&unit, &record, party_unit_get(roster_slot));
    bcopy(g_formation_unit_pointers[g_unit_selected_index]->detail.equipment, record.detail.equipment,
        sizeof(record.detail.equipment));
    record.detail.two_handed = equipment_is_two_handing_weapon(
        (s16*)record.detail.equipment, record.support_abilities[2] & UNIT_SUPPORT_SET_3_TWO_HANDS);
    bcopy(&record.status, &g_unit_panel_selected_unit_stat_summary, sizeof(unit_panel_summary_t));
    bcopy(&record.identity, &g_formation_selected_unit_identity, sizeof(menu_unit_status_identity_t));
    bcopy(&record.detail, &g_formation_selected_unit_stat_detail, sizeof(formation_unit_detail_t));
    equipment_build_weapon_evade_totals(
        &g_formation_selected_unit_stat_detail, g_formation_unit_pointers[g_unit_selected_index]->detail.equipment);
    formation_unit_copy_to_preview(g_unit_selected_index, FORMATION_PREVIEW_UNIT_INDEX);
    unit_panel_redraw_all();
}
