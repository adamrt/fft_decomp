/* Brave Story lists the unlocked Facts and opens each fact in a text window. */

#include "fft/brave_story.h"
#include "fft/display_object.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/pad.h"
#include "psx/types.h"

static void fact_detail_open(s32 value);

/* Open the Brave Story Facts list: one row per fact flag set (EVENT_SCRIPT_VAR_FACT_FIRST onward),
 * up to eight, hiding the parent's content panel. */
void brave_story_fact_list_open(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* colors;
    s32 count = 0;
    s32 i = 0;
    s32 record_index;

    colors = g_menu_stack_row_text_colors;
    values = g_menu_stack_entry_values;
    for (; i < EVENT_SCRIPT_VAR_FACT_COUNT; i++) {
        if (script_variable_get(i + EVENT_SCRIPT_VAR_FACT_FIRST) != 0) {
            *values = i + TEXT_ID_FACT_NAME_BASE;
            *colors = 0;
            colors++;
            values++;
            count++;
        }
    }
    g_menu_stack_records[g_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x8C;
    origin.x = 0x58;
    layout.visible_rows = i;
    origin.y = 0x50;
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_FACTS, count, i);
    window_panel_thread_start(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    g_world_map_flags |= WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE;
    g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.casualty_image].flags
        |= DISPLAY_OBJECT_HIDDEN;
    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
    record_index = g_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_ot_offset = 9;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_BRAVE_STORY_FACTS;
    g_menu_stack_depth = record_index + 1;
}

/* Resume handler of the Facts list: block Circle, Cross and Select from the WORLD menu frame
 * again. */
void brave_story_fact_list_buttons_restore(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void brave_story_block_fact_list_buttons(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the Facts list; twin of tutorial_topic_list_input_update
 * with help message 0x108f, handing a confirmed entry's value to
 * fact_detail_open. */
void brave_story_fact_list_input_update(menu_stack_record_t* level) {
    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED)
        && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_menu_stack_ot_offset = 1;
        g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        if (level->panel.result != 0) {
            menu_stack_cursor_store(MENU_STACK_CURSOR_SLOT_FACTS);
            fact_detail_open(
                g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        menu_stack_cursor_clear(MENU_STACK_CURSOR_SLOT_FACTS);
        menu_stack_dispatch_resume();
        return;
    }
    if ((g_world_map_input_new_button_presses & PSX_PAD_CROSS) && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        brave_story_block_fact_list_buttons(level);
        menu_stack_message_open(0x108F, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0
        && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes MENU_STACK_LEVEL_BRAVE_STORY_FACT, the text of one fact, which
 * brave_story_wait_fact_then_pop closes: records the entry on the new
 * level, sets up g_text_window on its text
 * (TEXT_ID_WORLD_MESSAGE_BASE + entry) with no substitutions, hides the Brave
 * Story menu's four windows and frame, and says
 * TEXT_ID_BRAVE_STORY_ENTRY_INFO with the fact's name
 * (TEXT_ID_FACT_NAME_BASE + entry).
 *
 * Matching constraint: the constant switch, with the selected case last,
 * keeps image_position.y at its absolute address while later stores reuse
 * the text_id base. This spelling does not establish the original source.
 *
 * The depth is spelled as the global at every use rather than cached in a
 * local: the target re-reads g_menu_stack_depth after each call. */
static void fact_detail_open(s32 value) {
    s32 coordinate_mode = 1;

    g_menu_stack_records[g_menu_stack_depth].window_index = value;
    g_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 0:
        g_text_window.image_position.x = 0;
        g_text_window.image_position.y = 0x60;
        break;
    case 1:
        g_text_window.image_position.x = 0;
        g_text_window.image_position.y = 0x70;
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
    text_window_open(&g_text_window);
    g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.cursor_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.label_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_gs_image_pool[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.rows_image].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.land_completion_star].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.treasure_completion_star]
        .flags |= DISPLAY_OBJECT_HIDDEN;
    g_text_substitution_values[0] = value + TEXT_ID_FACT_NAME_BASE;
    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_BRAVE_STORY_FACT;
    g_menu_stack_depth = g_menu_stack_depth + 1;
}

/* Steps a fact's text window until text_window_update reports it
 * closed, then pops the level, shows the Brave Story menu's windows and
 * frame again and reopens the Facts list (brave_story_fact_list_open). */
void brave_story_wait_fact_then_pop(menu_stack_record_t* unused_level) {
    s32 depth;

    if (!(g_display_object_gs_image_pool[g_text_window.render_record_index].flags & DISPLAY_OBJECT_POP_IN)
        && text_window_update(&g_text_window) == 0) {
        depth = g_menu_stack_depth;
        g_display_object_gs_image_pool[g_menu_stack_records[depth - 2].brave_story.rows_image].flags
            |= DISPLAY_OBJECT_POP_IN;
        g_display_object_animated_pool[g_menu_stack_records[depth - 2].brave_story.cursor_window].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[g_menu_stack_records[depth - 2].brave_story.label_window].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[g_menu_stack_records[depth - 2].brave_story.rows_image].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[g_menu_stack_records[depth - 2].brave_story.land_completion_star].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        g_menu_stack_depth = depth - 1;
        g_display_object_animated_pool[g_menu_stack_records[depth - 2].brave_story.treasure_completion_star].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        brave_story_fact_list_open();
    }
}
