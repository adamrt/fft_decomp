#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"

/*
 * Steps the menu level that swaps the world-map screen for a full-screen
 * menu level.
 *
 * After `delay` frames it stores the 256x240 frame buffer into the scratch
 * buffer, asks the transition thread to finish and forgets the loaded
 * background set. Once that thread has ended it pops itself, restarts the
 * message box (line 0xB848, or 0xB8DD for the tutorial lists) and pushes the
 * level named by `screen`: Brave Story, a story event text, the tutorial
 * categories, the formation tutorials, the mask-1 tutorial entries or a
 * treasure detail. The two tutorial entry lists first un-hide the parent
 * list's windows and frame.
 */
void wldcore_menu_step_screen_transition_level(wldcore_menu_screen_transition_level_t* level) {
    RECT rect;

    if (level->delay == 0) {
        if (level->phase == 0) {
            DrawSync(0);
            rect.x = 0;
            rect.y = 0;
            rect.w = 0x100;
            rect.h = 0xF0;
            StoreImage(&rect, g_wldcore_scratch_buffer);
            DrawSync(0);
            VSync(0);
            thread_set_parameters(WLDCORE_THREAD_SLOT_TRANSITION, 0, 0, 1);
            g_wldcore_loaded_background_set = -1;
            level->phase++;
            return;
        }
        if (thread_is_running(WLDCORE_THREAD_SLOT_TRANSITION) != 0) {
            return;
        }
        g_wldcore_menu_stack_depth--;
        switch (level->screen) {
        case WLDCORE_MENU_LEVEL_BRAVE_STORY:
            thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
            wldcore_menu_push_brave_story_level();
            break;
        case WLDCORE_MENU_LEVEL_STORY_EVENTS:
            thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
            wldcore_menu_push_story_event_text_level(g_main_replay_story_event_index);
            break;
        case WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES:
            thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_TUTORIAL_WELCOME, 0);
            wldcore_list_open_tutorial_categories(level->param_a, level->param_b);
            break;
        case WLDCORE_MENU_LEVEL_FORMATION_TUTORIALS:
            thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_TUTORIAL_WELCOME, 0);
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                         .list_window.main_window]
                .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                         .list_window.side_window]
                .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                                .list_window.frame_render]
                .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            wldcore_list_open_formation_tutorials();
            break;
        case WLDCORE_MENU_LEVEL_TUTORIAL_MASK_1_ENTRIES:
            thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_TUTORIAL_WELCOME, 0);
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                         .list_window.main_window]
                .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                         .list_window.side_window]
                .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                                .list_window.frame_render]
                .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            wldcore_list_open_tutorial_mask_1_entries();
            break;
        case WLDCORE_MENU_LEVEL_TREASURE_DETAIL:
            thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
            wldcore_menu_push_treasure_detail_level(level->param_a);
            break;
        }
    } else {
        level->delay--;
    }
}
