/* World-map menu transitions hide the parent list, prepare the destination screen and restore its presentation. */

#include "fft/gfx/display_object.h"
#include "fft/gfx/zodiac.h"
#include "fft/menu/menu_screen.h"
#include "fft/party/formation.h"
#include "fft/script/script_variable.h"
#include "fft/sys/file.h"
#include "fft/sys/music.h"
#include "fft/sys/suzuki.h"
#include "fft/sys/thread.h"
#include "fft/text/message.h"
#include "fft/world/brave_story.h"
#include "fft/world/menu_stack.h"
#include "fft/world/menu_stack_records.h"
#include "fft/world/tutorial.h"
#include "fft/world/world_map.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static void hide_parent_windows(void);
static void hide_parent_tutorial_menu(void);

/* Push a screen transition into `destination_level` (a menu level type): clear the map, hide date, location label and
 * value, fade in, do that screen's setup (music, list, message block), clear the scratch buffer
 * and start the zoom thread; param_a and param_b are kept for the new screen. */
void world_map_open_screen_transition(menu_stack_level_e destination_level, s32 param_a, s32 param_b) {
    u32* clear;
    s32 count;
    s32 i;
    s32 depth;

    world_map_clear_screen(1);
    g_world_map_flags
        |= WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE | WORLD_MAP_FLAG_HIDE_CALENDAR_DATE | WORLD_MAP_FLAG_HIDE_LOCATION_LABEL;
    world_map_start_fade(0, 0x10);
    switch (destination_level) {
    case MENU_STACK_LEVEL_BRAVE_STORY:
        display_object_set_queues_enabled(0);
        music_queue_wait_until_empty();
        music_queue_start_track(MUSIC_QUEUE_TRACK(2, MUSIC_BRAVE_STORY));
        break;
    case MENU_STACK_LEVEL_BRAVE_STORY_EVENTS:
        display_object_set_queues_enabled(0);
        menu_screen_set_world_menu_cursor(2);
        count = 0;
        music_queue_wait_until_empty();
        music_queue_start_track(MUSIC_QUEUE_TRACK(2, MUSIC_BRAVE_STORY));
        world_map_suspend_cursor_level(&g_menu_stack_records[g_menu_stack_depth - 1]);
        if (script_variable_get(EVENT_SCRIPT_VAR_FACTS_ENABLED) != 0) {
            for (i = 0; i < EVENT_SCRIPT_VAR_FACT_COUNT; i++) {
                if (script_variable_get(i + EVENT_SCRIPT_VAR_FACT_FIRST) != 0) {
                    count++;
                }
            }
        }
        if (count != 0) {
            g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_BRAVE_STORY].selected_index = 1;
        } else {
            g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_BRAVE_STORY].selected_index = 0;
        }
        brave_story_open_menu();
        g_world_map_flags |= WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE;
        hide_parent_windows();
        message_load_block_if_changed(MESSAGE_BLOCK_STORY_EVENTS);
        file_wait_pending();
        g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_EVENTS] = g_brave_story_saved_list_cursor_state;
        break;
    case MENU_STACK_LEVEL_TUTORIAL:
        display_object_set_queues_enabled(0);
        music_queue_wait_until_empty();
        music_queue_start_track(MUSIC_QUEUE_TRACK(2, MUSIC_TUTORIAL));
        break;
    case MENU_STACK_LEVEL_TUTORIAL_FORMATION_LIST:
        tutorial_build_menu_panel(&g_menu_stack_records[g_menu_stack_depth - 1].tutorial_menu);
        hide_parent_tutorial_menu();
        break;
    case MENU_STACK_LEVEL_TUTORIAL_BATTLE_LIST:
        display_object_set_queues_enabled(0);
        menu_screen_set_world_menu_cursor(3);
        world_map_suspend_cursor_level(&g_menu_stack_records[g_menu_stack_depth - 1]);
        tutorial_open_menu(1, 0);
        hide_parent_tutorial_menu();
        g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_BATTLE_TUTORIALS] = g_brave_story_saved_list_cursor_state;
        break;
    case MENU_STACK_LEVEL_BRAVE_STORY_TREASURE:
        hide_parent_windows();
        message_load_block_if_changed(MESSAGE_BLOCK_TREASURES);
        file_wait_pending();
        music_queue_wait_until_empty();
        suzuki_sound_set_master_volume(0x3FFF, 1);
        music_queue_start_track(MUSIC_QUEUE_TRACK(2, MUSIC_BRAVE_STORY));
        break;
    default:
        break;
    }

    /* Blank the backdrop image from the end. */
    i = WORLD_MAP_BACKDROP_WORDS - 1;
    clear = &g_world_map_scratch_buffer[WORLD_MAP_BACKDROP_WORDS - 1];
    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].menu_screen_transition.destination_level = destination_level;
    g_menu_stack_records[depth].menu_screen_transition.param_a = param_a;
    g_menu_stack_records[depth].menu_screen_transition.param_b = param_b;
    for (; i >= 0; i--) {
        *clear = 0;
        clear--;
    }

    menu_screen_set_brightness(0, 0, 0);
    thread_start(WORLD_MAP_THREAD_SLOT_TRANSITION, zodiac_backdrop_thread);
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_TRANSITION, 0, 0, 0);
    g_menu_stack_records[g_menu_stack_depth].menu_screen_transition.delay = 0x14;
    g_menu_stack_records[g_menu_stack_depth].menu_screen_transition.phase = 0;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_MENU_SCREEN_TRANSITION;
    g_menu_stack_depth++;
}

/* Closes every window of the Brave Story menu beneath the current level: the
 * main window goes to palette 10 (sequence, frame_index and anim_counter
 * cleared), the side window to palette 6, the frame and content render
 * records to palette 2, and the completion stars to palette 5, each hidden. */
static void hide_parent_windows(void) {
    s32 index = g_menu_stack_depth - 1;

    g_display_object_animated_pool[g_menu_stack_records[index].brave_story.cursor_window].palette = 10;
    g_display_object_animated_pool[g_menu_stack_records[index].brave_story.cursor_window].sequence
        = DISPLAY_OBJECT_ANIM_CURSOR_STILL;
    g_display_object_animated_pool[g_menu_stack_records[index].brave_story.cursor_window].frame_index
        = g_display_object_animated_pool[g_menu_stack_records[index].brave_story.cursor_window].anim_counter = 0;
    g_display_object_animated_pool[g_menu_stack_records[index].brave_story.cursor_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[g_menu_stack_records[index].brave_story.label_window].palette = 6;
    g_display_object_animated_pool[g_menu_stack_records[index].brave_story.label_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_gs_image_pool[g_menu_stack_records[index].brave_story.rows_image].palette = 2;
    g_display_object_gs_image_pool[g_menu_stack_records[index].brave_story.rows_image].flags |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_gs_image_pool[g_menu_stack_records[index].brave_story.casualty_image].palette = 2;
    g_display_object_gs_image_pool[g_menu_stack_records[index].brave_story.casualty_image].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[g_menu_stack_records[index].brave_story.land_completion_star].palette = 5;
    g_display_object_animated_pool[g_menu_stack_records[index].brave_story.land_completion_star].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[g_menu_stack_records[index].brave_story.treasure_completion_star].palette = 5;
    g_display_object_animated_pool[g_menu_stack_records[index].brave_story.treasure_completion_star].flags
        |= DISPLAY_OBJECT_HIDDEN;
}

/* Closes the list window of the level beneath the current one: the main
 * window goes to palette 10 (sequence, frame_index and anim_counter cleared),
 * the side window to palette 6 and the frame render record to palette 2, each
 * hidden. */
static void hide_parent_tutorial_menu(void) {
    s32 index = g_menu_stack_depth - 1;

    g_display_object_animated_pool[g_menu_stack_records[index].tutorial_menu.cursor_window].palette = 10;
    g_display_object_animated_pool[g_menu_stack_records[index].tutorial_menu.cursor_window].sequence
        = DISPLAY_OBJECT_ANIM_CURSOR_STILL;
    g_display_object_animated_pool[g_menu_stack_records[index].tutorial_menu.cursor_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[g_menu_stack_records[index].tutorial_menu.cursor_window].frame_index
        = g_display_object_animated_pool[g_menu_stack_records[index].tutorial_menu.cursor_window].anim_counter = 0;
    g_display_object_animated_pool[g_menu_stack_records[index].tutorial_menu.label_window].palette = 6;
    g_display_object_animated_pool[g_menu_stack_records[index].tutorial_menu.label_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_gs_image_pool[g_menu_stack_records[index].tutorial_menu.rows_image].palette = 2;
    g_display_object_gs_image_pool[g_menu_stack_records[index].tutorial_menu.rows_image].flags |= DISPLAY_OBJECT_HIDDEN;
}

/*
 * Steps the menu level that swaps the world-map screen for a full-screen
 * menu level.
 *
 * After `delay` frames it stores the 256x240 frame buffer into the scratch
 * buffer, asks the transition thread to finish and forgets the loaded
 * background set. Once that thread has ended it pops itself, restarts the
 * message box (TEXT_ID_BRAVE_STORY_INTRO, or
 * TEXT_ID_TUTORIAL_WELCOME for the tutorial lists) and pushes the
 * level named by `destination_level`: Brave Story, a story event text, the tutorial
 * categories, the formation tutorials, the battle tutorials or a
 * treasure detail. The two tutorial entry lists first un-hide the parent
 * list's windows and frame.
 */
void world_map_update_screen_transition(menu_stack_record_t* level) {
    RECT rect;

    if (level->menu_screen_transition.delay == 0) {
        if (level->menu_screen_transition.phase == 0) {
            DrawSync(0);
            rect.x = 0;
            rect.y = 0;
            rect.w = 256;
            rect.h = 240;
            StoreImage(&rect, g_world_map_scratch_buffer);
            DrawSync(0);
            VSync(0);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_TRANSITION, 0, 0, 1);
            g_picture_background_loaded_set = -1;
            level->menu_screen_transition.phase++;
            return;
        }
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_TRANSITION)) {
            return;
        }
        g_menu_stack_depth--;
        switch (level->menu_screen_transition.destination_level) {
        case MENU_STACK_LEVEL_BRAVE_STORY:
            thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
            brave_story_open_menu();
            break;
        case MENU_STACK_LEVEL_BRAVE_STORY_EVENTS:
            thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
            brave_story_open_event_detail(g_brave_story_replay_story_event_index);
            break;
        case MENU_STACK_LEVEL_TUTORIAL:
            thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_TUTORIAL_WELCOME, 0);
            tutorial_open_menu(level->menu_screen_transition.param_a, level->menu_screen_transition.param_b);
            break;
        case MENU_STACK_LEVEL_TUTORIAL_FORMATION_LIST:
            thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_TUTORIAL_WELCOME, 0);
            g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].tutorial_menu.cursor_window]
                .flags &= ~DISPLAY_OBJECT_HIDDEN;
            g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].tutorial_menu.label_window]
                .flags &= ~DISPLAY_OBJECT_HIDDEN;
            g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 1].tutorial_menu.rows_image].flags
                &= ~DISPLAY_OBJECT_HIDDEN;
            tutorial_open_formation_list();
            break;
        case MENU_STACK_LEVEL_TUTORIAL_BATTLE_LIST:
            thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_TUTORIAL_WELCOME, 0);
            g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].tutorial_menu.cursor_window]
                .flags &= ~DISPLAY_OBJECT_HIDDEN;
            g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].tutorial_menu.label_window]
                .flags &= ~DISPLAY_OBJECT_HIDDEN;
            g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 1].tutorial_menu.rows_image].flags
                &= ~DISPLAY_OBJECT_HIDDEN;
            tutorial_open_battle_list();
            break;
        case MENU_STACK_LEVEL_BRAVE_STORY_TREASURE:
            thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
            brave_story_open_treasure_detail(level->menu_screen_transition.param_a);
            break;
        }
    } else {
        level->menu_screen_transition.delay--;
    }
}
