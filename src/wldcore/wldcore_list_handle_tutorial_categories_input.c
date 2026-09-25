#include "fft/wldcore.h"
#include "psx/pad.h"

/* Per-frame step of the tutorial menu list level.
 *
 * Mode 0 runs the cursor and the four entries: formation tutorials, the
 * wldcore_list_open_tutorial_mask_1_entries list, the chapter-5 tutorial topics, and entry 3 (or cancel),
 * which starts WORLD thread 14 on text 0xb8de and moves to mode 1. Mode 1
 * waits for confirm/cancel and starts the fade out, mode 2 clears VRAM once
 * the fade flag is set and mode 3 pops the level: with system flag 0x80000 it
 * hands back to the caller through 0x10000000, otherwise it fades in and calls
 * wldcore_menu_push_entry_window_level.
 *
 * The selected-entry reload and fixed-register wrap update preserve the
 * original instruction schedule. Names beyond the shared headers remain
 * provisional. */
void wldcore_list_handle_tutorial_categories_input(wldcore_menu_list_window_level_t* level) {
    u32 buttons;
    volatile s32* selected_entry;

    if (g_wldcore_window_render_records[level->frame_render].flags & 0x100) {
        return;
    }
    if (g_main_system_flags & 4) {
        if (level->mode == 2 && !(g_main_system_flags & 8)) {
            level->mode = 3;
            wldcore_gfx_clear_vram_and_scratch(1);
        }
        if (level->mode == 3 && world_thread_is_running(0xE) == 0) {
            if ((g_main_system_flags & 0x80000) == 0 || g_wldcore_audio_queue.count == 0) {
                g_main_system_flags ^= 4;
                wldcore_window_set_render_objects_visible(1);
                g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth - 1;
                g_main_system_flags = g_main_system_flags & ~0x3800;
                wldcore_menu_dispatch_publish_handler();
                if (g_main_system_flags & 0x80000) {
                    g_main_system_flags = (g_main_system_flags ^ 0x80001) | 0x10000000;
                    return;
                }
                wldcore_fade_start_screen(4, 0x10);
                wldcore_menu_push_entry_window_level();
            }
        }
        return;
    }
    if (level->mode == 1) {
        if (g_wldcore_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            level->mode = 2;
            world_thread_set_parameters(0xE, 0, -1, 0);
            g_main_system_flags = g_main_system_flags | 4;
            wldcore_fade_start_screen(2, 0x10);
            if (g_main_system_flags & 0x80000) {
                wldcore_sound_enqueue_audio_command(2, 0x10);
                wldcore_sound_enqueue_audio_command(4, 0);
            } else {
                wldcore_sound_enqueue_music_start(0x11B);
            }
            g_wldcore_tutorial_category_saved_cursor = 0;
        }
        return;
    }
    if (g_wldcore_audio_queue.count != 0) {
        return;
    }
    buttons = g_wldcore_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        world_thread_set_parameters(0xE, 0x19, 0xB8DE, 0);
        level->mode = 1;
        g_wldcore_window_record_count = g_wldcore_window_record_count - 2;
        g_wldcore_window_render_record_count = g_wldcore_window_render_record_count - 1;
        g_wldcore_window_render_object_count = g_wldcore_window_render_object_count - 3;
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        wldcore_window_publish_slot_b8dd(level);
        wldcore_menu_push_message_level(level->entries[level->selected_entry] + 0x108B, 1);
        return;
    }
    if (buttons & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        selected_entry = &level->selected_entry;
        g_wldcore_tutorial_category_saved_cursor = *selected_entry;
        switch (level->entries[*selected_entry]) {
        case 0:
            wldcore_window_publish_slot_b8dd(level);
            g_wldcore_window_records[level->main_window].sequence = 1;
            wldcore_list_open_formation_tutorials();
            return;
        case 1:
            wldcore_window_publish_slot_b8dd(level);
            g_wldcore_window_records[level->main_window].sequence = 1;
            wldcore_list_open_tutorial_mask_1_entries();
            return;
        case 2:
            wldcore_load_message_block_if_changed(5);
            wldcore_window_publish_slot_b8dd(level);
            g_wldcore_window_records[level->main_window].sequence = 1;
            wldcore_list_open_tutorial_topics();
            return;
        case 3:
            world_thread_set_parameters(0xE, 0x19, 0xB8DE, 0);
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
            g_wldcore_window_records[level->main_window].y = g_wldcore_window_records[level->main_window].y - 0x10;
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
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
    if (wldcore_input_check_repeating_directional(PSX_PAD_DOWN) != 0) {
        if (level->selected_entry < level->entry_count - 1) {
            g_wldcore_window_records[level->main_window].y = g_wldcore_window_records[level->main_window].y + 0x10;
            level->selected_entry = level->selected_entry + 1;
        } else {
            g_wldcore_window_records[level->main_window].y
                = g_wldcore_window_records[level->main_window].y - ((level->entry_count - 1) * 0x10);
            level->selected_entry = 0;
        }
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
}
