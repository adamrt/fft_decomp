#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Pushes the type-0x31 screen-transition level for screen `screen`.
 *
 * Clears the screen, sets system flags 0x3800 and starts a 16-step fade,
 * then runs the per-screen teardown for screens 0x1b..0x2d (jump table at
 * 0x80067210; 0x1e..0x2a do nothing):
 *   0x1b  closes the parent list, loads chapter 3, waits for the file, drains
 *         the sound queue, rebalances the volume and restarts music 0x220;
 *   0x1c  hides the render objects and restarts music 0x220;
 *   0x1d  hides the render objects, sets menu slot 4's cursor to 2, restarts
 *         music 0x220, resets the level's window, records in g_wldcore_brave_story_saved_cursor
 *         whether script variable 0x92 and any of variables 0x1a4..0x1bb are
 *         set, closes the parent list, loads chapter 4 and restores the
 *         saved list cursor;
 *   0x2b  hides the render objects and restarts music 0x21a;
 *   0x2c  rebuilds the tutorial category panel image and closes the parent
 *         list frame windows;
 *   0x2d  hides the render objects, sets menu slot 4's cursor to 3, resets
 *         the level's window, reopens the tutorial categories and restores the
 *         saved list cursor.
 * It then records the three arguments on the new level, zeroes the 0x1e000
 * scratch buffer, blacks the menu brightness out, starts WORLD thread 4 and
 * pushes the level as type 0x31 with delay = 0x14.
 *
 * The new level's fields go through the stack-record union, whose field
 * access is what keeps the target's per-field `$at` bases. */
void wldcore_menu_push_screen_transition_level(s32 screen, s32 param_a, s32 param_b) {
    u32* clear;
    s32 count;
    s32 i;
    s32 depth;

    wldcore_clear_screen_and_set_map_clip_rect(1);
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE | MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE
        | MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
    wldcore_fade_start_screen(0, 0x10);
    switch (screen) {
    case WLDCORE_MENU_LEVEL_BRAVE_STORY:
        wldcore_window_set_render_objects_visible(0);
        wldcore_sound_wait_for_queue_drain();
        wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_BRAVE_STORY));
        break;
    case WLDCORE_MENU_LEVEL_STORY_EVENTS:
        wldcore_window_set_render_objects_visible(0);
        world_menu_set_slot_4_cursor_index(2);
        count = 0;
        wldcore_sound_wait_for_queue_drain();
        wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_BRAVE_STORY));
        wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth]);
        if (script_get_variable(EVENT_SCRIPT_VAR_FACTS_ENABLED) != 0) {
            for (i = 0; i < EVENT_SCRIPT_VAR_FACT_COUNT; i++) {
                if (script_get_variable(i + EVENT_SCRIPT_VAR_FACT_FIRST) != 0) {
                    count++;
                }
            }
        }
        if (count != 0) {
            g_wldcore_brave_story_saved_cursor[0] = 1;
        } else {
            g_wldcore_brave_story_saved_cursor[0] = 0;
        }
        wldcore_menu_push_brave_story_level();
        g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        wldcore_window_close_parent_list_all_windows();
        wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_STORY_EVENTS);
        file_wait_for_pending_variant();
        g_wldcore_story_events_cursor_state = g_main_saved_list_cursor_state;
        break;
    case WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES:
        wldcore_window_set_render_objects_visible(0);
        wldcore_sound_wait_for_queue_drain();
        wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_TUTORIAL));
        break;
    case WLDCORE_MENU_LEVEL_FORMATION_TUTORIALS:
        wldcore_list_build_tutorial_category_panel_image(
            &g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth].text_list);
        wldcore_window_close_parent_list_frame_windows();
        break;
    case WLDCORE_MENU_LEVEL_TUTORIAL_MASK_1_ENTRIES:
        wldcore_window_set_render_objects_visible(0);
        world_menu_set_slot_4_cursor_index(3);
        wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth]);
        wldcore_list_open_tutorial_categories(1, 0);
        wldcore_window_close_parent_list_frame_windows();
        g_wldcore_tutorial_entries_cursor_state = g_main_saved_list_cursor_state;
        break;
    case WLDCORE_MENU_LEVEL_TREASURE_DETAIL:
        wldcore_window_close_parent_list_all_windows();
        wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_TREASURES);
        file_wait_for_pending_variant();
        wldcore_sound_wait_for_queue_drain();
        suzuki_sound_set_master_volume(0x3FFF, 1);
        wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_BRAVE_STORY));
        break;
    default:
        break;
    }
    i = 0x77FF;
    clear = &g_wldcore_scratch_buffer[0x77FF];
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].screen_transition.screen = screen;
    g_wldcore_menu_stack_records_next[depth].screen_transition.param_a = param_a;
    g_wldcore_menu_stack_records_next[depth].screen_transition.param_b = param_b;
    for (; i >= 0; i--) {
        *clear = 0;
        clear--;
    }
    world_menu_set_brightness(0, 0, 0);
    thread_start(WLDCORE_THREAD_SLOT_TRANSITION, world_formation_view_zoom_thread);
    thread_set_parameters(WLDCORE_THREAD_SLOT_TRANSITION, 0, 0, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].screen_transition.delay = 0x14;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].screen_transition.phase = 0;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_SCREEN_TRANSITION;
    g_wldcore_menu_stack_depth++;
}

/* Closes every window of the list-window level beneath the current one: the
 * main window goes to palette 10 (sequence, frame_index and anim_counter
 * cleared), the side window to palette 6, the frame and content render
 * records to palette 2, and the upper/lower windows to palette 5, each hidden with flag 0x10. */
void wldcore_window_close_parent_list_all_windows(void) {
    s32 index;

    index = g_wldcore_menu_stack_depth - 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].palette = 10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].sequence
        = WLDCORE_ANIM_SEQUENCE_CURSOR_STILL;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].frame_index
        = g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].anim_counter = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.side_window].palette = 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.side_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].palette = 2;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.content_render].palette = 2;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.content_render].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.upper_window].palette = 5;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.upper_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.lower_window].palette = 5;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.lower_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
}

/* Closes the list window of the level beneath the current one: the main
 * window goes to palette 10 (sequence, frame_index and anim_counter cleared),
 * the side window to palette 6 and the frame render record to palette 2, each hidden with
 * flag 0x10. */
void wldcore_window_close_parent_list_frame_windows(void) {
    s32 index;

    index = g_wldcore_menu_stack_depth - 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].palette = 10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].sequence
        = WLDCORE_ANIM_SEQUENCE_CURSOR_STILL;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].frame_index
        = g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].anim_counter = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.side_window].palette = 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.side_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].palette = 2;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
}

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
