#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

void wldcore_bar_handle_menu_input(wldcore_menu_list_window_level_t* level) {
    s32 flags;
    u32 buttons;
    s32* selected_entry;

    if (g_wldcore_window_render_records[level->frame_render].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    flags = g_main_system_flags;
    if (flags & MAIN_SYSTEM_FLAG_FADING) {
        return;
    }
    if ((flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_menu_pop_level_and_rebuild_screen(level);
        return;
    }

    if (level->mode != WLDCORE_LIST_WINDOW_MODE_BROWSE) {
        if (!(g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
            return;
        }
        level->mode = WLDCORE_LIST_WINDOW_MODE_BROWSE;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
        wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(1, MAIN_MUSIC_WORLD_MAP));
        return;
    }

    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        /* Same exit as case 4 below; GCC cross-jumps the two copies. */
        g_wldcore_bar_menu_saved_cursor = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BAR_GOODBYE, 0);
        level->mode = WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE;
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 3;
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        wldcore_window_publish_slot_b806(level);
        wldcore_menu_push_message_level(
            g_wldcore_bar_row_help_text_ids[level->entries[level->selected_entry]] | 0x1000, 1);
        return;
    }
    if (buttons & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        selected_entry = &level->selected_entry;
        g_wldcore_bar_menu_saved_cursor = *selected_entry;
        if (level->entries[*selected_entry] != WLDCORE_BAR_MENU_ROW_LEAVE) {
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
        }
        switch (level->entries[level->selected_entry]) {
        case WLDCORE_BAR_MENU_ROW_RUMORS:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_RUMORS);
            wldcore_window_publish_slot_b806(level);
            wldcore_list_open_rumors();
            return;
        case WLDCORE_BAR_MENU_ROW_AVAILABLE_PROPOSITIONS:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_PROPOSITIONS);
            wldcore_window_publish_slot_b806(level);
            wldcore_list_open_available_propositions();
            return;
        case WLDCORE_BAR_MENU_ROW_COMPLETED_PROPOSITIONS:
            wldcore_window_publish_slot_b806(level);
            wldcore_list_open_completed_propositions();
            return;
        case WLDCORE_BAR_MENU_ROW_PROPOSITIONS:
            wldcore_window_publish_slot_b806(level);
            wldcore_list_open_propositions();
            return;
        case WLDCORE_BAR_MENU_ROW_LEAVE:
            g_wldcore_bar_menu_saved_cursor = 0;
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BAR_GOODBYE, 0);
            level->mode = WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE;
            g_wldcore_window_record_count -= 2;
            g_wldcore_window_render_record_count -= 1;
            g_wldcore_window_render_object_count -= 3;
            return;
        }
        return;
    }

    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->selected_entry != 0) {
            g_wldcore_window_records[level->main_window].position.y -= 0x10;
            level->selected_entry--;
        } else {
            g_wldcore_window_records[level->main_window].position.y += (level->entry_count - 1) * 0x10;
            level->selected_entry = level->entry_count - 1;
        }
    } else if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        if (level->selected_entry < level->entry_count - 1) {
            g_wldcore_window_records[level->main_window].position.y += 0x10;
            level->selected_entry++;
        } else {
            g_wldcore_window_records[level->main_window].position.y -= (level->entry_count - 1) * 0x10;
            level->selected_entry = 0;
        }
    } else {
        return;
    }
    wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    if (level->content_render != 0) {
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BAR_WELCOME, 0);
        level->content_render = 0;
    }
}
