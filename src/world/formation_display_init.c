/* Formation menu display init, unit list rebuilds, the map busy check and unit messages with sound. */

#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/item.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_help.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "fft/world_formation.h"
#include "fft/world_gfx.h"
#include "fft/world_menu.h"
#include "fft/world_script.h"
#include "fft/world_text.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static u32 g_world_formation_status_label_image[352] = { 0x111, 0, 0x111, 0x11111110, 0, 0x10000000, 0x111110,
    0x11100000, 0, 0x10000000, 17, 0x111111, 0x11100000, 0x10000000, 17, 0, 0x11100011, 1, 0x111, 0, 0x11100, 0x111110,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x121, 0, 0x121, 0x12222210, 0, 0, 0x152251, 0x12100000, 17, 0x10000000, 18, 0x122221,
    0x12100000, 0x10000000, 18, 0, 0x21210121, 1, 0x121, 0, 0x1f2e10, 0x162210, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x11110121, 0x11111111, 0x11111111, 0x11121111, 0x11111111, 0x1111, 0x11123321, 0x12111111, 0x11111112, 0x11111111,
    0x111111, 0x11111121, 0x11111111, 0x11111111, 0x11111111, 17, 0x11221221, 0x11111111, 0x11111111, 0x111, 0x1def2e1,
    0x156210, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x52510121, 0x21235221, 0x61522123, 0x25121122, 0x15252215, 0x1522,
    0x22111121, 0x21152215, 0x15251122, 0x12121226, 0x152212, 0x26112221, 0x12121212, 0x15221522, 0x26152212, 18,
    0x21232321, 0x22512251, 0x61522121, 0x122, 0x1edef21, 0x145610, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x21210121,
    0x63616541, 0x21232121, 0x12121121, 0x12323212, 0x1212, 0x54111121, 0x11123216, 0x12121112, 0x12121212, 0x1121212,
    0x12111121, 0x12121212, 0x12121212, 0x12123212, 18, 0x21213121, 0x16211621, 0x21232121, 0x121, 0x1fedee1, 0x154510,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x46211121, 0x56512121, 0x21212121, 0x62121121, 0x12121214, 0x1212, 0x12123321,
    0x10121212, 0x14621112, 0x12121212, 0x1121212, 0x12111121, 0x12121212, 0x12121212, 0x12121212, 18, 0x21211121,
    0x26112611, 0x21212121, 0x121, 0x1fed10, 0x165410, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x22512221, 0x32356261, 0x61212121,
    0x25121122, 0x12121212, 0x1522, 0x26152251, 0x10121256, 0x12251126, 0x16261226, 0x1152212, 0x26122221, 0x12162612,
    0x15221522, 0x26121212, 18, 0x21210121, 0x52215221, 0x61212121, 0x122, 0x11100, 0x111110, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0x11111111, 0x11111111, 0x11111111, 0x11111123, 0x11111111, 0x1112, 0x11111110, 0x10111111, 0x11111111,
    0x11111231, 0x111211, 0x31111111, 0x11111112, 0x11121112, 0x31111111, 18, 0x11110111, 0x11111111, 0x11111111, 0x123,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x61000000, 0x152, 0x10000000, 18, 0, 0, 0, 0x1210, 0x1001210, 0x10000000,
    0x10000012, 0x121012, 0x26100000, 21, 0, 0, 0x61000000, 0x152, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x11000000,
    17, 0x10000000, 17, 0, 0, 0, 0x1110, 0x1001110, 0x10000000, 0x10000011, 0x111011, 0x11100000, 1, 0, 0, 0x11000000,
    17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xc850000, 0x1109435d, 0x1df2156d, 0x18ee2eb9, 0x151610d2, 0xcec157c,
    0x19f71174, 0x3b3c227a, 0x14830000, 0x29067775, 0x4e2c3988, 0x18ee62d2, 0x151610d2, 0xcec157c, 0x19f71174,
    0x3b3c227a, 0x4430000, 0x10c725d0, 0x192a1509, 0x18ee1d8e, 0x151610d2, 0xcec157c, 0x19f71174, 0x3b3c227a,
    0x14610000, 0x20c545ed, 0x31482906, 0x18ee3dab, 0x151610d2, 0xcec157c, 0x19f71174, 0x3b3c227a };
static RECT g_world_formation_status_label_image_rect = { 384, 245, 64, 11 };

/* Set up the formation screen's display: reset the menu context, clear the screen (except when
 * entering from name entry), upload the menu glyph sheet, palettes and status label image, and
 * copy two window-frame palettes into VRAM row 510 with colors 4, 8 and 12 cleared. */
void world_formation_init_menu_display(void) {
    RECT rect;
    palette_t palette;

    world_formation_reset_menu_context();
    menu_set_vsync_mode_and_event_speed(0);
    if (g_world_menu_context_id != WORLD_MENU_CONTEXT_NAME_ENTRY) {
        rect.x = 0;
        rect.y = 0;
        rect.w = 0x100;
        rect.h = 0x1E0;
        ClearImage(&rect, 0, 0, 0);
        SetDispMask(1);
    }
    rect.x = 0x240;
    rect.y = 0x100;
    rect.w = 0x40;
    rect.h = 0x30;
    gfx_load_image_and_wait(&rect, g_menu_glyph_sheet->pixels);
    rect.x = 0x240;
    rect.y = 0x1FA;
    rect.w = 0x40;
    rect.h = 4;
    gfx_load_image_and_wait(&rect, g_world_formation_menu_palettes);
    rect.x = 0x3C0;
    rect.y = 0x1F0;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, palette);

    /* Each window-frame palette is copied whole to x 0x260/0x270 of row 510,
     * and its colors 4..15 again to x 0x240/0x250, after clearing colors 4, 8
     * and 12. */
    palette[4].red = palette[4].green = palette[4].blue = palette[4].semi_transparent = 0;
    palette[8].red = palette[8].green = palette[8].blue = palette[8].semi_transparent = 0;
    palette[12].red = palette[12].green = palette[12].blue = palette[12].semi_transparent = 0;
    rect.x = 0x260;
    rect.y = 0x1FE;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette);
    rect.x = 0x240;
    rect.y = 0x1FE;
    rect.w = 0xC;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &palette[4]);
    rect.x = 0x3C0;
    rect.y = 0x1F4;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, palette);
    palette[4].red = palette[4].green = palette[4].blue = palette[4].semi_transparent = 0;
    palette[8].red = palette[8].green = palette[8].blue = palette[8].semi_transparent = 0;
    palette[12].red = palette[12].green = palette[12].blue = palette[12].semi_transparent = 0;
    rect.x = 0x270;
    rect.y = 0x1FE;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette);
    rect.x = 0x250;
    rect.y = 0x1FE;
    rect.w = 0xC;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &palette[4]);
    gfx_load_image_and_wait(&g_world_formation_status_label_image_rect, g_world_formation_status_label_image);
    g_menu_help_message_text_id = 0;
    menu_init_scrollable_list_core(0, 0, 0);
    world_gfx_start_decreasing_fade();
    menu_clear_selection_values();
    g_world_formation_unit_cycle_mode = 0;
    world_menu_reset_selection_results();
    world_text_bind_section_data(WORLD_TEXT_DATA_SKILLSET_NAMES);
    world_text_bind_section_data(WORLD_TEXT_DATA_JOB_NAMES);
    world_text_bind_section_data(WORLD_TEXT_DATA_ABILITY_NAMES);
    world_text_bind_section_data(WORLD_TEXT_DATA_FORMATION_MESSAGES);
    g_text_section_pointers[TEXT_SECTION_COMPOSED_TEXT] = g_world_formation_menu_text_buffer;
    g_text_section_pointers[TEXT_SECTION_OVERLAY_SCREEN] = g_world_formation_menu_text_table;
    g_world_formation_cursor_ot_override = 0;
    g_world_tutorial_id = 0;
}

/* Rebuild the formation unit list for the screen in use: every unit on the
 * formation screen, the monsters at the soldier office and the available
 * humans in the other shops. Also reconciles the five item category lists
 * and resets the cursor and scroll. */
void world_formation_rebuild_unit_list(void) {
    s32 mode;

    if (g_world_menu_context_id == WORLD_MENU_CONTEXT_FORMATION) {
        mode = WORLD_FORMATION_RECORD_LIST_ALL;
    } else {
        mode = WORLD_FORMATION_RECORD_LIST_MONSTERS;
        if (g_world_menu_context_id != WORLD_MENU_CONTEXT_SOLDIER_OFFICE) {
            mode = WORLD_FORMATION_RECORD_LIST_AVAILABLE_HUMANS;
        }
    }
    g_world_formation_unit_count = world_formation_build_record_list(0, g_world_unit_pointers, mode);
    item_rebuild_category_list(0);
    item_rebuild_category_list(1);
    item_rebuild_category_list(2);
    item_rebuild_category_list(3);
    item_rebuild_category_list(4);
    world_formation_init_cursor_and_scroll_state(0);
    g_world_formation_screen_running = 1;
}

/* Reports whether the screen is busy: the grid help is up (g_world_grid_menu_id),
 * a menu runs in the upper menu or screen menu thread slot, or menu input is
 * locked (g_menu_input_lock_mode). */
s32 world_map_is_busy(void) {
    s32 result = 0;

    if (g_world_grid_menu_id != MENU_GRID_HELP_NONE || thread_is_running(WORLD_THREAD_SLOT_UPPER_MENU) != 0
        || g_menu_input_lock_mode != 0 || thread_is_running(THREAD_SCREEN_SLOT_MENU) != 0) {
        result = 1;
    }
    return result;
}

/* Show message `message_id` in the text thread (dialog type 0x2B), play
 * `sound_id` at once and lock menu input while the message is up. */
void world_text_show_message_and_play_sound(s32 message_id, s32 sound_id) {
    menu_text_start_thread_if_idle(THREAD_SCREEN_SLOT_TEXT, 0x2B, message_id, 0);
    g_world_sound_release_held_loop = 1;
    main_sound_play_sfx_find_channel(sound_id);
    g_menu_input_lock_mode = 1;
}

/* Show `message_id` in the text thread (dialog type 0x1B) with the formation
 * unit's roster slot as speaker, unless the thread is busy. */
void world_formation_show_unit_message(s16 unit_index, s32 message_id) {
    menu_text_start_thread_if_idle(
        THREAD_SCREEN_SLOT_TEXT, 0x1B, message_id, g_world_unit_pointers[unit_index]->identity.roster_slot);
}
