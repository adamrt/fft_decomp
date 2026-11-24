/* The Tutorial's topic list and text pages. Selecting a topic temporarily hides
 * the category menu; closing its text window restores the menu and reopens the
 * topic list at the saved cursor. Scripted Formation and battle lessons use the
 * separate Tutorial lists and player. */

#include "fft/gfx/display_object.h"
#include "fft/menu/menu_input.h"
#include "fft/sys/file.h"
#include "fft/sys/sound.h"
#include "fft/sys/thread.h"
#include "fft/text/text.h"
#include "fft/world/menu_stack.h"
#include "fft/world/menu_stack_records.h"
#include "fft/world/tutorial.h"
#include "fft/world/window.h"
#include "fft/world/world_map.h"
#include "psx/pad.h"

static void open_topic(s32 topic);

#define append_panel_list_entry(count, value)                                                                          \
    do {                                                                                                               \
        g_menu_stack_entry_values[count] = value;                                                                      \
        g_menu_stack_row_text_colors[count] = 0;                                                                       \
        count = (count) + 1;                                                                                           \
    } while (0)

/* Opens the list of the 41 tutorial topics (TEXT_ID_TIP_NAME_BASE + topic),
 * up to eight rows at (0x54, 0x50), 0x8c wide, and pushes
 * MENU_STACK_LEVEL_TUTORIAL_TOPICS.
 *
 * The list is indexed by `count` rather than `i`; indexing by `i` lets loop.c
 * fold the final count to the constant 41. */
void tutorial_open_topic_list(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s32 count = 0;
    s32 i;
    s32 visible;
    s32 record_index;

    for (i = 0; i < 41; i++) {
        append_panel_list_entry(count, i + TEXT_ID_TIP_NAME_BASE);
    }
    g_menu_stack_records[g_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        visible = count;
    } else {
        visible = 8;
    }
    layout.text_width = 0x8c;
    origin.x = 84;
    layout.visible_rows = visible;
    origin.y = 80;
    menu_stack_restore_cursor(MENU_STACK_CURSOR_SLOT_TUTORIAL_TOPICS, count, visible);
    window_start_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_ot_offset = 9;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_TUTORIAL_TOPICS;
    g_menu_stack_depth = record_index + 1;
}

/* Resume handler of the tutorial topics list: block Circle, Cross and Select from the WORLD menu
 * frame again. */
void tutorial_resume_topic_list(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void tutorial_suspend_topic_list(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the tutorial topics list; twin of
 * brave_story_update_treasure_list_input with help message 0x108d. A confirmed
 * topic's value is passed to open_topic. */
void tutorial_update_topic_list_input(menu_stack_record_t* level) {
    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED) && !thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL)) {
        g_menu_blocked_buttons = 0;
        g_menu_stack_ot_offset = 1;
        g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        if (level->panel.result != 0) {
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_TUTORIAL_TOPICS);
            open_topic(g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_TUTORIAL_TOPICS);
        menu_stack_dispatch_resume();
        return;
    }
    if ((g_world_map_input_new_button_presses & PSX_PAD_CROSS) && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        tutorial_suspend_topic_list(level);
        menu_stack_open_message(0x108D, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0
        && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes MENU_STACK_LEVEL_TUTORIAL_TOPIC: records the topic on the new
 * level, sets up g_text_window on the topic's text
 * (TEXT_ID_WORLD_MESSAGE_BASE + topic), and hides the tutorial menu's two
 * windows and frame. tutorial_update_topic
 * steps the window and pops the level, showing all three again.
 *
 * Matching constraint: the constant switch, with the selected case last,
 * keeps image_position.y at its absolute address while later stores reuse
 * the text_id base. This spelling does not establish the original source.
 *
 * The menu-stack type store comes last so the
 * compiler scales depth by four late and in place, forcing the copy into v0
 * for depth + 1. */
static void open_topic(s32 topic) {
    s32 coordinate_mode = 1;
    s32 depth;

    g_menu_stack_records[g_menu_stack_depth].tutorial_topic_index = topic;
    g_text_window.text_id = topic + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 0:
        g_text_window.image_position.x = 0;
        g_text_window.image_position.y = 96;
        break;
    case 1:
        g_text_window.image_position.x = 0;
        g_text_window.image_position.y = 112;
        break;
    }
    g_text_window.base.x = -122;
    g_text_window.base.y = -40;
    g_text_window.text_width = 0xdc;
    g_text_window.rows_per_page = 8;
    g_text_window.priority = 9;
    g_text_window.image_coordinate_mode = coordinate_mode;
    g_text_window.extra_render_index = -1;
    g_text_window.text_substitutions[1] = -1;
    g_text_window.text_substitutions[0] = -1;
    window_open_paged_text(&g_text_window);
    depth = g_menu_stack_depth;
    g_display_object_animated_pool[g_menu_stack_records[depth - 1].tutorial_menu.cursor_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[g_menu_stack_records[depth - 1].tutorial_menu.label_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth = depth + 1;
    g_display_object_gs_image_pool[g_menu_stack_records[depth - 1].tutorial_menu.rows_image].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_TUTORIAL_TOPIC;
}

/* Step of the tutorial topic text level pushed by
 * open_topic: once the text window has
 * closed (window_update_paged_text), pop the level, show the
 * tutorial menu's windows and frame again and reopen the topic list. */
void tutorial_update_topic(menu_stack_record_t* unused_level) {
    s32 depth;

    if (!(g_display_object_gs_image_pool[g_text_window.render_record_index].flags & DISPLAY_OBJECT_POP_IN)
        && window_update_paged_text(&g_text_window) == 0) {
        sound_play_effect(SOUND_SFX_CANCEL);
        depth = g_menu_stack_depth;
        g_display_object_gs_image_pool[g_menu_stack_records[depth - 2].tutorial_menu.rows_image].flags
            |= DISPLAY_OBJECT_POP_IN;
        g_display_object_animated_pool[g_menu_stack_records[depth - 2].tutorial_menu.cursor_window].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[g_menu_stack_records[depth - 2].tutorial_menu.label_window].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_menu_stack_depth = depth - 1;
        g_display_object_gs_image_pool[g_menu_stack_records[depth - 2].tutorial_menu.rows_image].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        tutorial_open_topic_list();
    }
}
