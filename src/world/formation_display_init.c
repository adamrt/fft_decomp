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
#include "fft/menu_text.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "fft/world_formation.h"
#include "fft/world_gfx.h"
#include "fft/world_menu.h"
#include "fft/world_script.h"
#include "fft/world_text.h"
#include "psx/libgpu.h"
#include "psx/types.h"

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
