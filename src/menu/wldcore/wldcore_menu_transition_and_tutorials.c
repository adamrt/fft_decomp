/* Screen transition level, parent list window closes and the tutorial category list. */

#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

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

/* Pushes the tutorial-category list level (type 0x2b).
 *
 * Appends a cursor window, a header window and the frame render record, seeds
 * the four category entries, and positions both windows relative to the render
 * record's origin. With `reset` set it also clears the two 32-bit tutorial
 * progress masks at g_main_tutorial_progress_masks.
 *
 * The 40 bytes of `dead_locals` reproduce the target's 0x50 frame: the
 * original declares locals it never reads, and GCC still reserves their slots.
 * The `- -(...)` in the cursor-row Y expression only flips the commutative
 * operand order of the final `addu`; it is arithmetically identical. */
void wldcore_list_open_tutorial_categories(s32 selected_entry, s32 reset) {
    s32 dead_locals[10];
    s32 window_index;
    s32 record_index;

    if (reset != 0) {
        g_main_tutorial_progress_masks[1] = 0;
        g_main_tutorial_progress_masks[0] = 0;
    }

    window_index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.main_window = window_index;
    g_wldcore_window_records[window_index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[window_index].priority = 0xA;
    g_wldcore_window_records[window_index].anim_counter = 0;
    g_wldcore_window_records[window_index].frame_index = 0;

    window_index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.side_window = window_index;
    g_wldcore_window_records[window_index].sequence = WLDCORE_ANIM_SEQUENCE_LIST_SIDE_LABEL;
    g_wldcore_window_records[window_index].priority = 0xA;
    g_wldcore_window_records[window_index].anim_counter = 0;
    g_wldcore_window_records[window_index].frame_index = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[0]
        = WLDCORE_TUTORIAL_CATEGORY_ROW_FORMATION;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[1]
        = WLDCORE_TUTORIAL_CATEGORY_ROW_MASK_1_ENTRIES;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[2]
        = WLDCORE_TUTORIAL_CATEGORY_ROW_TOPICS;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[3]
        = WLDCORE_TUTORIAL_CATEGORY_ROW_LEAVE;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count = 4;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.frame_render
        = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    wldcore_list_build_tutorial_category_panel_image(
        &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].text_list);

    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].list_window.selected_entry = selected_entry;
    g_wldcore_menu_stack_records_next[record_index].list_window.mode = WLDCORE_LIST_WINDOW_MODE_BROWSE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].list_window.main_window].position.x
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].list_window.frame_render]
              .base.x
        + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].list_window.main_window].position.y
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].list_window.frame_render]
              .base.y
        - -(g_wldcore_menu_stack_records_next[record_index].list_window.selected_entry * 0x10) + 0xE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].list_window.side_window].position.x
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].list_window.frame_render]
              .base.x
        + 3;
    g_wldcore_menu_stack_depth = record_index + 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].list_window.side_window].position.y
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].list_window.frame_render]
              .base.y
        - 2;
    g_wldcore_menu_stack_records_next[record_index].list_window.content_render = 0x18;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES;
}

/* Build the image of a 0x44-wide text list panel: one 16-pixel row per text id
 * (text 0xb8d9 + id), anchored at (-0x78, -0x28) in coordinate mode 0xa.
 *
 * The base coordinates are written through a point view of base_x/base_y; the
 * target addresses them from &base_x, not from the record start. */
void wldcore_list_build_tutorial_category_panel_image(wldcore_text_list_panel_t* panel) {
    RECT bounds;
    point32_t point;
    s32 index;
    point32_t* base;
    s32 i;

    index = panel->render_index;
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = 0x44;
    bounds.h = panel->row_count * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 0xA;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x78;
    base->y = -0x28;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < panel->row_count; i++) {
        window_display_text_entry_variant(index, panel->text_ids[i] + WLDCORE_TEXT_ID_TUTORIAL_CATEGORY_ROW_FIRST,
            point, g_wldcore_window_image_buffer);
        point.y += 0x10;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);
}

/* Restore the tutorial list's palettes and cursor animation, then show its welcome text. */
void wldcore_window_clear_layout_states_and_queue_msg_b8dd(wldcore_menu_stack_record_t* level) {
    s32 main_window;
    s32 side_window;
    s32 slot;
    /* The original frame reserves eight unused bytes. */
    s32 pad[2];

    main_window = level->list_window.main_window;
    side_window = level->list_window.side_window;
    g_wldcore_window_render_records[level->list_window.frame_render].palette = 0;
    g_wldcore_window_records[side_window].palette = 0;
    g_wldcore_window_records[main_window].palette = 0;
    slot = level->list_window.main_window;
    g_wldcore_window_records[slot].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    slot = level->list_window.main_window;
    g_wldcore_window_records[slot].anim_counter = 0;
    g_wldcore_window_records[slot].frame_index = 0;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_TUTORIAL_WELCOME, 0);
}
