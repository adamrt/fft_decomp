/* Input step for the world-map town menu list level.
 *
 * Waits out the screen transition (system flags 4/8), then: cancel or the
 * "leave" row (entry 4) starts the fade back to the map and announces text
 * 0xb807; L1 opens the help message for the highlighted row from
 * g_wldcore_bar_row_help_text_ids; confirm dispatches on the row value through a five-entry jump
 * table at 0x80067034 - 0 rumors, 1 the proposition-location list
 * (wldcore_list_open_available_propositions), 2 wldcore_list_open_completed_propositions, 3 the proposition list, 4
 * leave; and up/down wrap the cursor, moving the cursor window 16 pixels a row and re-announcing text 0xb806 while mode
 * is set.
 *
 * The selected-entry reload and fixed-register final row update preserve the
 * original instruction schedule. Names beyond the shared headers remain
 * provisional.
 */

#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/pad.h"

void wldcore_list_open_available_propositions(void);
void wldcore_list_open_completed_propositions(void);
void wldcore_list_open_propositions(void);
void wldcore_load_message_block_if_changed(s32 which);
void wldcore_menu_pop_level_and_rebuild_screen();
void wldcore_window_publish_slot_b806(s32* level);

void wldcore_bar_handle_menu_input(wldcore_menu_list_window_level_t* level) {
    s32 flags;
    u32 buttons;
    volatile s32* selected_entry;

    if (g_wldcore_window_render_records[level->frame_render].flags & 0x100) {
        return;
    }
    flags = g_main_system_flags;
    if (flags & 8) {
        return;
    }
    if ((flags & 4) && world_thread_is_running(0xE) == 0) {
        g_main_system_flags ^= 4;
        wldcore_menu_pop_level_and_rebuild_screen(level);
        return;
    }

    if (level->mode != 0) {
        if (!(g_wldcore_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
            return;
        }
        level->mode = 0;
        world_thread_set_parameters(0xE, 0, -1, 0);
        g_main_system_flags |= 4;
        wldcore_fade_start_screen(2, 0x10);
        wldcore_sound_enqueue_music_start(0x11B);
        return;
    }

    buttons = g_wldcore_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        /* Same exit as case 4 below; GCC cross-jumps the two copies. */
        g_wldcore_bar_menu_saved_cursor = 0;
        world_thread_set_parameters(0xE, 0x19, 0xB807, 0);
        level->mode = 1;
        g_wldcore_window_record_count = g_wldcore_window_record_count - 2;
        g_wldcore_window_render_record_count = g_wldcore_window_render_record_count - 1;
        g_wldcore_window_render_object_count = g_wldcore_window_render_object_count - 3;
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        wldcore_window_publish_slot_b806((s32*)level);
        wldcore_menu_push_message_level(
            g_wldcore_bar_row_help_text_ids[level->entries[level->selected_entry]] | 0x1000, 1);
        return;
    }
    if (buttons & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        selected_entry = &level->selected_entry;
        g_wldcore_bar_menu_saved_cursor = *selected_entry;
        if (level->entries[*selected_entry] != 4) {
            g_wldcore_window_records[level->main_window].sequence = 1;
        }
        switch (level->entries[level->selected_entry]) {
        case 0:
            wldcore_load_message_block_if_changed(0);
            wldcore_window_publish_slot_b806((s32*)level);
            wldcore_list_open_rumors();
            return;
        case 1:
            wldcore_load_message_block_if_changed(1);
            wldcore_window_publish_slot_b806((s32*)level);
            wldcore_list_open_available_propositions();
            return;
        case 2:
            wldcore_window_publish_slot_b806((s32*)level);
            wldcore_list_open_completed_propositions();
            return;
        case 3:
            wldcore_window_publish_slot_b806((s32*)level);
            wldcore_list_open_propositions();
            return;
        case 4:
            g_wldcore_bar_menu_saved_cursor = 0;
            world_thread_set_parameters(0xE, 0x19, 0xB807, 0);
            level->mode = 1;
            g_wldcore_window_record_count = g_wldcore_window_record_count - 2;
            g_wldcore_window_render_record_count = g_wldcore_window_render_record_count - 1;
            g_wldcore_window_render_object_count = g_wldcore_window_render_object_count - 3;
            return;
        }
        return;
    }

    if (wldcore_input_check_repeating_directional(PSX_PAD_UP) != 0) {
        if (level->selected_entry != 0) {
            g_wldcore_window_records[level->main_window].y -= 0x10;
            level->selected_entry = level->selected_entry - 1;
        } else {
            register s32 main_window asm("$2");
            register s32 record_offset asm("$4");
            register s32 entry_count asm("$3");

            main_window = level->main_window;
            entry_count = level->entry_count;
            record_offset = main_window;
            main_window = g_wldcore_window_records[record_offset].y;
            main_window = main_window + (entry_count - 1) * 0x10;
            g_wldcore_window_records[record_offset].y = main_window;
            level->selected_entry = level->entry_count - 1;
        }
    } else if (wldcore_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
        if (level->selected_entry < level->entry_count - 1) {
            g_wldcore_window_records[level->main_window].y += 0x10;
            level->selected_entry = level->selected_entry + 1;
        } else {
            g_wldcore_window_records[level->main_window].y -= (level->entry_count - 1) * 0x10;
            level->selected_entry = 0;
        }
    } else {
        return;
    }
    wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    if (level->content_render != 0) {
        world_thread_set_parameters(0xE, 0x19, 0xB806, 0);
        level->content_render = 0;
    }
}
