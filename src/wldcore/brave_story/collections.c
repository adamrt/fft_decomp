/* Brave Story displays discovered lands and treasures. A treasure can open its sound novel, asking whether to resume a
 * saved reading while the treasure picture dissolves into view. */

#include "fft/brave_story.h"
#include "fft/calendar.h"
#include "fft/display_object.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/message_box.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/sound_novel.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u8 g_treasure_sound_novel_ids[];

/* Offsets of the 1 KiB bands of a picture, in the order its dissolving upload fills them. */
extern u16* g_picture_dissolve_band_order;

static void land_detail_open(s32 value);
static s32 picture_dissolve_update(GsIMAGE* image, s32 step);
static void reading_choice_open(s32 mode);

static u8 g_dissolve_progress[16] = { 0 };

/* Open the Brave Story list of unexplored lands found so far (their script flags set), up to eight
 * rows. */
void brave_story_unexplored_land_list_open(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s32 count = 0;
    s32 i; /* reused as the visible-row count, which the target keeps in $s0 */
    s32 record_index;

    for (i = 0; i < EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT; i++) {
        if (script_variable_get(i + EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST) != 0) {
            g_menu_stack_entry_values[count] = i + TEXT_ID_UNEXPLORED_LAND_NAME_BASE;
            g_menu_stack_row_text_colors[count] = 0;
            count++;
        }
    }
    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
    g_menu_stack_records[g_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x60;
    origin.x = 0x80;
    layout.visible_rows = i;
    origin.y = 0x50;
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_UNEXPLORED_LANDS, count, i);
    window_panel_thread_start(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_BRAVE_STORY_UNEXPLORED_LANDS;
    g_menu_stack_depth = record_index + 1;
}

/* Resume handler of the unexplored lands list: block Circle, Cross and Select for the
 * levels underneath and show TEXT_ID_BRAVE_STORY_INTRO again. */
void brave_story_unexplored_land_list_prompt_restore(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void brave_story_block_unexplored_land_list_buttons(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the unexplored lands list; twin of rumor_list_input_update
 * that closes the level inline, opens help message 0x1058 on Select, and hands
 * a confirmed entry to land_detail_open. Cross
 * is accepted even while a file is loading.
 *
 * The target passes the level to brave_story_block_unexplored_land_list_buttons,
 * which reads no argument. */
void brave_story_unexplored_land_list_input_update(menu_stack_record_t* level) {
    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED)
        && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_stack_ot_offset = 1;
        g_menu_blocked_buttons = 0;
        g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        if (level->panel.result != 0) {
            menu_stack_cursor_store(MENU_STACK_CURSOR_SLOT_UNEXPLORED_LANDS);
            land_detail_open(
                g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        menu_stack_cursor_clear(MENU_STACK_CURSOR_SLOT_UNEXPLORED_LANDS);
        menu_stack_dispatch_resume();
        return;
    }
    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0
        && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        brave_story_block_unexplored_land_list_buttons(level);
        menu_stack_message_open(0x1058, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0
        && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes MENU_STACK_LEVEL_BRAVE_STORY_UNEXPLORED_LAND, the picture and text of
 * one unexplored land. Records the land on the new level, loads its picture
 * data set, appends a hidden render record for the picture at (-116, -34),
 * reads the land's discovery date from g_brave_story_land_discovery_date_bits, and
 * sets up g_text_window on the land's text
 * (TEXT_ID_WORLD_MESSAGE_BASE + land) with the month name
 * (TEXT_ID_MONTH_NAME_BASE + month) and the day substituted. Then
 * hides the Brave Story menu's four windows and frame and says
 * TEXT_ID_BRAVE_STORY_ENTRY_INFO with the land's name.
 *
 * Matching constraint: the constant switch, with the selected case last,
 * keeps image_position.y at its absolute address while later stores reuse
 * the text_id base. This spelling does not establish the original source.
 *
 * The render record's position is written through a pointer to its base: the
 * target reaches base.x/base.y at 0 and 4 off one register holding
 * &g_display_object_gs_image_pool[index].position, while the flags update above
 * it still uses the $at absolute form. Spelling both coordinates as ordinary
 * indexed field stores recomputes the $at base twice and costs four
 * instructions.
 *
 * `unused` reproduces eight bytes of locals below the date bytes that the
 * target never touches. */
static void land_detail_open(s32 value) {
    s32 index;
    point32_t* base;
    s32 coordinate_mode = 1;
    u8 unused[8];
    calendar_date_t date;

    g_menu_stack_records[g_menu_stack_depth].detail.value = value;
    picture_load_set(g_unexplored_land_picture_sets[value]);
    index = display_object_image_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].detail.render_index = index;
    g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_HIDDEN;
    base = &g_display_object_gs_image_pool[index].position;
    base->x = -0x74;
    base->y = -0x22;
    calendar_date_read(g_brave_story_land_discovery_date_bits, value, &date);
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
    g_text_window.priority = 0xa;
    g_text_window.image_coordinate_mode = coordinate_mode;
    g_text_window.extra_render_index = -1;
    g_text_window.text_substitutions[0] = date.month + TEXT_ID_MONTH_NAME_BASE;
    g_text_window.text_substitutions[1] = date.day;
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
    g_text_substitution_values[0] = value + TEXT_ID_UNEXPLORED_LAND_NAME_BASE;
    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_menu_stack_records[g_menu_stack_depth].detail.phase = BRAVE_STORY_DETAIL_PHASE_LOAD;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_BRAVE_STORY_UNEXPLORED_LAND;
    g_menu_stack_depth = g_menu_stack_depth + 1;
}

/* Step of the unexplored-land detail level.
 *
 * LOAD waits until render record saved_record_index is not popping in; no
 * push sets that field (retail). BUILD, once file loading finishes, uploads
 * the land picture's TIM pixels and CLUT, binds the level's render record to
 * the VRAM image at (-116, -34) and makes it the scrollable text window's
 * extra render record (position and image rectangle). POP_IN waits for that
 * record's pop-in. SHOWN waits for the text window to finish, shows the
 * Brave Story menu's windows and frame again, pops the level and reopens the
 * unexplored-land list.
 *
 * The TIM widths are read unsigned (`lhu`), as Psy-Q's GsIMAGE declares
 * pw/ph. The text window's base and image rectangle are each copied as one
 * aggregate: the target loads both 32-bit values before either store. */
void brave_story_unexplored_land_level_update(menu_stack_record_t* level) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut_position;
    point32_t* base;
    s32* index;
    s32 render;
    s32 depth;

    switch (level->detail.phase) {
    case BRAVE_STORY_DETAIL_PHASE_LOAD:
        if (!(g_display_object_gs_image_pool[level->detail.saved_record_index].flags & DISPLAY_OBJECT_POP_IN)) {
            level->detail.phase = BRAVE_STORY_DETAIL_PHASE_BUILD;
        }
        break;
    case BRAVE_STORY_DETAIL_PHASE_BUILD:
        if (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
            break;
        }
        GsGetTimInfo(g_picture_buffer + 1, &image);
        LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
        LoadClut(image.clut, image.cx, image.cy);
        render = level->detail.render_index;
        bounds.x = image.px;
        bounds.y = image.py;
        bounds.w = image.pw;
        bounds.h = image.ph;
        clut_position.x = image.cx;
        clut_position.y = image.cy;
        display_object_gs_image_init(render, bounds, clut_position, DISPLAY_OBJECT_8BIT_CLUT);
        g_display_object_gs_image_pool[render].priority = 0xA;
        base = &g_display_object_gs_image_pool[render].position;
        base->x = -0x74;
        base->y = -0x22;
        g_display_object_gs_image_pool[render].flags
            = (g_display_object_gs_image_pool[render].flags & ~DISPLAY_OBJECT_HIDDEN) | DISPLAY_OBJECT_POP_IN;
        g_text_window.extra_render_index = render;
        g_text_window.extra_base = *base;
        g_text_window.extra_bounds = g_display_object_gs_image_pool[render].bounds;
        level->detail.phase = level->detail.phase + 1;
        break;
    case BRAVE_STORY_DETAIL_PHASE_POP_IN:
        if (!(g_display_object_gs_image_pool[level->detail.render_index].flags & DISPLAY_OBJECT_POP_IN)) {
            level->detail.phase = BRAVE_STORY_DETAIL_PHASE_SHOWN;
        }
        break;
    default:
        index = &g_text_window.render_record_index;
        if (g_display_object_gs_image_pool[*index].flags & DISPLAY_OBJECT_POP_IN) {
            break;
        }
        if (text_window_update(&g_text_window) != 0) {
            break;
        }
        depth = g_menu_stack_depth;
        g_display_object_animated_pool[g_menu_stack_records[depth - 2].brave_story.cursor_window].flags
            ^= DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[g_menu_stack_records[depth - 2].brave_story.label_window].flags
            ^= DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[g_menu_stack_records[depth - 2].brave_story.rows_image].flags
            ^= DISPLAY_OBJECT_HIDDEN;
        g_display_object_gs_image_pool[g_menu_stack_records[depth - 2].brave_story.rows_image].flags
            |= DISPLAY_OBJECT_POP_IN;
        g_display_object_animated_pool[g_menu_stack_records[depth - 2].brave_story.land_completion_star].flags
            ^= DISPLAY_OBJECT_HIDDEN;
        g_display_object_image_count--;
        g_display_object_queue_count--;
        g_display_object_animated_pool[g_menu_stack_records[depth - 2].brave_story.treasure_completion_star].flags
            ^= DISPLAY_OBJECT_HIDDEN;
        g_menu_stack_depth = depth - 1;
        brave_story_unexplored_land_list_open();
        break;
    }
}

/* Opens the Brave Story's treasure list: every treasure found
 * (EVENT_SCRIPT_VAR_TREASURE_FIRST + treasure set), by name
 * (TEXT_ID_TREASURE_NAME_BASE + treasure), up to eight rows at (0x88, 0x50),
 * 0x5a wide, and pushes MENU_STACK_LEVEL_BRAVE_STORY_TREASURES. Twin of
 * brave_story_unexplored_land_list_open. */
void brave_story_treasure_list_open(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s32 count = 0;
    s32 i; /* reused as the visible-row count, which the target keeps in $s0 */
    s32 record_index;

    for (i = 0; i < EVENT_SCRIPT_VAR_TREASURE_COUNT; i++) {
        if (script_variable_get(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) != 0) {
            g_menu_stack_entry_values[count] = i + TEXT_ID_TREASURE_NAME_BASE;
            g_menu_stack_row_text_colors[count] = 0;
            count++;
        }
    }
    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
    g_menu_stack_records[g_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x5A;
    origin.x = 0x88;
    layout.visible_rows = i;
    origin.y = 0x50;
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_TREASURES, count, i);
    window_panel_thread_start(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_BRAVE_STORY_TREASURES;
    g_menu_stack_depth = record_index + 1;
}

/* Resume handler of the treasures list: block Circle, Cross and Select for the
 * levels underneath and show TEXT_ID_BRAVE_STORY_INTRO again. */
void brave_story_treasure_list_prompt_restore(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
}

/* Withhold every button from the WORLD menu frame while a level opened from the treasures list
 * runs. */
void brave_story_block_treasure_list_buttons(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the treasure list; twin of brave_story_unexplored_land_list_input_update.
 * Cross and Circle wait for file loading, close the panel and record the
 * choice in result; Select on a non-empty list opens help message 0x101f.
 * Once the panel has closed the level pops, and a confirmed choice passes the
 * selected entry's value to brave_story_treasure_level_open.
 *
 * The target passes the level to brave_story_block_unexplored_land_list_buttons,
 * which reads no argument. */
void brave_story_treasure_list_input_update(menu_stack_record_t* level) {
    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED)
        && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_stack_ot_offset = 1;
        g_menu_blocked_buttons = 0;
        g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        if (level->panel.result != 0) {
            menu_stack_cursor_store(MENU_STACK_CURSOR_SLOT_TREASURES);
            brave_story_treasure_level_open(
                g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        menu_stack_cursor_clear(MENU_STACK_CURSOR_SLOT_TREASURES);
        menu_stack_dispatch_resume();
        return;
    }
    if ((g_world_map_input_new_button_presses & PSX_PAD_CROSS) && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        brave_story_block_unexplored_land_list_buttons(level);
        menu_stack_message_open(0x101F, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0
        && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes MENU_STACK_LEVEL_BRAVE_STORY_TREASURE, the picture and text of one
 * treasure; the twin of land_detail_open and
 * brave_story_event_level_open.
 *
 * Records the treasure on the new level, loads its picture data set from
 * g_treasure_picture_sets, appends a hidden render record for the
 * picture, reads the acquisition date from
 * g_brave_story_treasure_acquisition_date_bits and sets up
 * g_text_window on its text (TEXT_ID_WORLD_MESSAGE_BASE +
 * value) with the month name and the day substituted. It then hides the
 * Brave Story menu's four windows and frame and says
 * TEXT_ID_BRAVE_STORY_ENTRY_INFO with the treasure's name.
 *
 * Unlike land_detail_open this one does not
 * place the picture render record (the step does), and it clears the new
 * level's timer as well as starting it in LOAD.
 *
 * Matching constraint: the constant switch, with the selected case last,
 * keeps image_position.y at its absolute address while later stores reuse
 * the text_id base. This spelling does not establish the original source.
 *
 * `unused` reproduces eight bytes of locals below the date bytes that the
 * target never touches. */
void brave_story_treasure_level_open(s32 value) {
    s32 index;
    s32 coordinate_mode = 1;
    u8 unused[8];
    calendar_date_t date;

    g_menu_stack_records[g_menu_stack_depth].detail.value = value;
    picture_load_set(g_treasure_picture_sets[value]);
    index = display_object_image_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].detail.render_index = index;
    g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_HIDDEN;
    calendar_date_read(g_brave_story_treasure_acquisition_date_bits, value, &date);
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
    g_text_window.priority = 0xa;
    g_text_window.image_coordinate_mode = coordinate_mode;
    g_text_window.extra_render_index = -1;
    g_text_window.text_substitutions[0] = date.month + TEXT_ID_MONTH_NAME_BASE;
    g_text_window.text_substitutions[1] = date.day;
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
    g_text_substitution_values[0] = value + TEXT_ID_TREASURE_NAME_BASE;
    thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_menu_stack_records[g_menu_stack_depth].detail.phase = BRAVE_STORY_DETAIL_PHASE_LOAD;
    g_menu_stack_records[g_menu_stack_depth].detail.timer = 0;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_BRAVE_STORY_TREASURE;
    g_menu_stack_depth = g_menu_stack_depth + 1;
}

/* Handler of the treasure detail level (the treasure's picture and text): the
 * phases mirror brave_story_unexplored_land_level_update.
 *
 * LOAD waits until render record saved_record_index is not popping in; no
 * push sets that field (retail). BUILD, once file loading finishes, uploads
 * the picture TIM's CLUT, binds the level's render record to the VRAM image
 * with its right edge at x -0x3e and centered on y 0x10, starts the dissolve
 * upload (picture_dissolve_update) and publishes the record in
 * g_text_window (extra_render_index, extra_base,
 * extra_bounds). POP_IN advances that upload one step per frame. SHOWN:
 * Triangle on a treasure with a sound novel (g_treasure_sound_novel_ids)
 * picks it, blocks input and closes the message box, fading the screen out
 * unless a saved reading exists; once that is done the level is released
 * and the novel opens (reading_choice_open for a saved
 * reading, else sound_novel_reader_open). When the text window
 * finishes instead, the Brave Story menu comes back and the treasure list
 * reopens.
 *
 * The record index is staged in `saved` before the depth store: the target
 * reloads the full 32-bit value for the 16-bit store instead of narrowing the load.
 * The button path's byte uses its own local; sharing `slot` changes the
 * register allocation. */
void brave_story_treasure_level_update(menu_stack_record_t* level) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut_position;
    point32_t* base;
    s32 render;
    s32 slot;
    s32 depth;
    s32 saved;
    s32 entry;

    switch (level->detail.phase) {
    case BRAVE_STORY_DETAIL_PHASE_LOAD:
        if (!(g_display_object_gs_image_pool[level->detail.saved_record_index].flags & DISPLAY_OBJECT_POP_IN)) {
            level->detail.phase = BRAVE_STORY_DETAIL_PHASE_BUILD;
        }
        break;
    case BRAVE_STORY_DETAIL_PHASE_BUILD:
        if (g_file_cd_state.state != FILE_LOAD_STATE_IDLE) {
            break;
        }
        GsGetTimInfo(g_picture_buffer + 1, &image);
        LoadClut(image.clut, image.cx, image.cy);
        render = level->detail.render_index;
        bounds.x = image.px;
        bounds.y = image.py;
        bounds.w = image.pw;
        bounds.h = image.ph;
        clut_position.x = image.cx;
        clut_position.y = image.cy;
        display_object_gs_image_init(render, bounds, clut_position, DISPLAY_OBJECT_8BIT_CLUT);
        g_display_object_gs_image_pool[render].priority = 0xA;
        g_display_object_gs_image_pool[render].flags &= ~(DISPLAY_OBJECT_HIDDEN | DISPLAY_OBJECT_POP_IN);
        base = &g_display_object_gs_image_pool[render].position;
        base->x = -0x3E - image.pw;
        base->y = 0x10 - (image.ph >> 1);
        picture_dissolve_update(&image, 0);
        g_text_window.extra_render_index = render;
        g_text_window.extra_base = *base;
        g_text_window.extra_bounds = g_display_object_gs_image_pool[render].bounds;
        level->detail.phase = level->detail.phase + 1;
        break;
    case BRAVE_STORY_DETAIL_PHASE_POP_IN:
        level->detail.timer = level->detail.timer + 1;
        GsGetTimInfo(g_picture_buffer + 1, &image);
        if (picture_dissolve_update(&image, level->detail.timer) == 0) {
            level->detail.phase = BRAVE_STORY_DETAIL_PHASE_SHOWN;
        }
        break;
    default:
        if ((g_world_map_flags & (WORLD_MAP_FLAG_INPUT_BLOCKED | WORLD_MAP_FLAG_FADING)) == WORLD_MAP_FLAG_INPUT_BLOCKED
            && thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
            slot = level->detail.saved_record_index;
            g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
            g_display_object_animated_count -= 4;
            g_display_object_image_count -= 2;
            g_display_object_queue_count -= 6;
            saved = level->detail.saved_record_index;
            g_menu_stack_depth--;
            g_sound_novel_selected = saved;
            if (slot < 5 && g_sound_novel_saved_readings[slot].section != 0) {
                reading_choice_open(level->detail.value);
                break;
            }
            sound_novel_reader_open(level->detail.value);
            break;
        }
        if (text_window_update(&g_text_window) == 0) {
            depth = g_menu_stack_depth;
            g_display_object_animated_pool[g_menu_stack_records[depth - 2].brave_story.cursor_window].flags
                ^= DISPLAY_OBJECT_HIDDEN;
            g_display_object_animated_pool[g_menu_stack_records[depth - 2].brave_story.label_window].flags
                ^= DISPLAY_OBJECT_HIDDEN;
            g_display_object_gs_image_pool[g_menu_stack_records[depth - 2].brave_story.rows_image].flags
                ^= DISPLAY_OBJECT_HIDDEN;
            g_display_object_gs_image_pool[g_menu_stack_records[depth - 2].brave_story.rows_image].flags
                |= DISPLAY_OBJECT_POP_IN;
            g_display_object_animated_pool[g_menu_stack_records[depth - 2].brave_story.land_completion_star].flags
                ^= DISPLAY_OBJECT_HIDDEN;
            g_display_object_image_count--;
            g_display_object_queue_count--;
            g_display_object_animated_pool[g_menu_stack_records[depth - 2].brave_story.treasure_completion_star].flags
                ^= DISPLAY_OBJECT_HIDDEN;
            g_menu_stack_depth = depth - 1;
            brave_story_treasure_list_open();
            break;
        }
        if (g_world_map_input_new_button_presses & PSX_PAD_TRIANGLE) {
            entry = g_treasure_sound_novel_ids[level->detail.value];
            if (entry != 0) {
                level->detail.saved_record_index = entry - 1;
                if (entry - 1 >= 5 || g_sound_novel_saved_readings[entry - 1].section == 0) {
                    world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
                }
                g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
                thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
                sound_play_effect(SOUND_SFX_CONFIRM);
            }
        }
        break;
    }
}

/* Free RAM above WLDCORE's data that the dissolve upload stages its image in. */
#define DISSOLVE_STAGING_BUFFER ((u8*)0x801EE000)

/*
 * Streams an image into VRAM through the 0x801EE000 staging buffer in 1 KiB
 * bands, copying 32 more bytes of each active band per step in the order
 * given by the g_picture_dissolve_band_order offset table.
 *
 * Step 0 clears the buffer to 0xFF, uploads it and resets the per-band
 * progress. Its result is unspecified, and initialization callers ignore it.
 * Later steps return 0 once step reaches 50, otherwise 1.
 */
static s32 picture_dissolve_update(GsIMAGE* image, s32 step) {
    const u8* pixels = (const u8*)image->pixel;
    const u16* order = g_picture_dissolve_band_order;
    u8* buffer = DISSOLVE_STAGING_BUFFER;
    s32 i;
    s32 j;
    s32 size;
    s32 base;
    s32 index;
    s32 start;
    s32 rows;
    u32 height;
    s32 count;
    s32 first;
    u8* state;

    if (step == 0) {
        for (i = 0; i < image->pw * (image->ph << 1); i++) {
            buffer[i] = 0xff;
        }
        LoadTPage((u32*)buffer, 0, 0, image->px, image->py, image->pw * 4, image->ph);
        state = &g_dissolve_progress[15];
        for (i = 15; i >= 0; i--) {
            *state-- = 0;
        }
        return;
    }

    if (step >= 50) {
        return 0;
    }

    step--;
    size = image->pw * image->ph * 2;
    height = image->ph;
    rows = height >> 3;
    if (height & 7) {
        start = 15 - rows;
    } else {
        start = 16 - rows;
    }

    for (i = start; i < 16; i++) {
        if (i <= step && (count = g_dissolve_progress[i]) < 32U) {
            base = (15 - i) << 10;
            /* Computed before the size check: the target copies it into j. */
            first = count * 32;
            if (base >= size) {
                continue;
            }
            for (j = first; j < first + 32; j++) {
                index = base + order[j];
                if (index < size) {
                    buffer[index] = pixels[index];
                }
            }
            g_dissolve_progress[i]++;
        }
        LoadTPage((u32*)buffer, 0, 0, image->px, image->py, image->pw * 4, image->ph);
    }

    return 1;
}

/* Pushes MENU_STACK_LEVEL_SOUND_NOVEL_RESTART (stepped by
 * sound_novel_reading_choice_update) for treasure `mode`, whose sound
 * novel has a saved reading: appends the cursor window, a VRAM render record
 * for the TIM at g_picture_buffer (uploaded progressively through
 * picture_dissolve_update, starting at step 0), and two text
 * records: the treasure's name (TEXT_ID_SUBSTITUTED_TEXT) and the
 * resume choices (TEXT_ID_SOUND_NOVEL_RESUME_CHOICES). The picture
 * record is placed against the first text record from the TIM's width and
 * half height, and the cursor window at the second text record's origin +
 * (0x18, 0x3c).
 *
 * The target reserves 0x10 frame bytes the function never uses. The
 * two offset temporaries keep GCC from folding base_x - (width - 0x3e) into
 * (base_x + 0x3e) - width, and each render-record base pointer is its own
 * local so the pointer shares the register of the constant it dies into. */
static void reading_choice_open(s32 mode) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut;
    s32 unused[4];
    s32 index;
    s32 depth;
    s32 offset_x;
    s32 offset_y;
    point32_t* base1;
    point32_t* base2;
    point32_t* base3;
    point32_t* base4;

    g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.mode = mode;
    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.cursor_window = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    g_display_object_animated_pool[index].priority = 0xA;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    GsGetTimInfo(g_picture_buffer + 1, &image);

    index = display_object_image_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.picture_render = index;
    bounds.x = image.px;
    bounds.y = image.py;
    bounds.w = image.pw;
    bounds.h = image.ph;
    clut.x = image.cx;
    clut.y = image.cy;
    display_object_gs_image_init(index, bounds, clut, DISPLAY_OBJECT_8BIT_CLUT);
    g_display_object_gs_image_pool[index].priority = 0xA;
    picture_dissolve_update(&image, 0);

    index = display_object_image_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.message_render = index;
    bounds.x = 0;
    bounds.y = 0x70;
    bounds.w = 0x7C;
    bounds.h = 0x80;
    g_text_substitution_values[0] = mode + TEXT_ID_TREASURE_NAME_BASE;
    window_text_record_init(index, bounds, 1, TEXT_ID_SUBSTITUTED_TEXT, g_window_image_buffer);
    g_display_object_gs_image_pool[index].priority = 0xA;
    base1 = &g_display_object_gs_image_pool[index].position;
    base1->x = -0x70;
    base1->y = -0x38;
    DrawSync(0);
    depth = g_menu_stack_depth;
    base2 = &g_display_object_gs_image_pool[g_menu_stack_records[depth].sound_novel_resume.picture_render].position;
    offset_x = image.pw - 0x3E;
    base2->x = g_display_object_gs_image_pool[index].position.x - offset_x;
    base3 = &g_display_object_gs_image_pool[g_menu_stack_records[depth].sound_novel_resume.picture_render].position;
    offset_y = (image.ph >> 1) - 0x4C;
    base3->y = g_display_object_gs_image_pool[index].position.y - offset_y;

    index = display_object_image_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.option_render = index;
    bounds.x = 0x80;
    bounds.y = 0x70;
    bounds.w = 0x70;
    bounds.h = 0x70;
    window_text_record_init(index, bounds, 1, TEXT_ID_SOUND_NOVEL_RESUME_CHOICES, g_window_image_buffer + 0x400);
    g_display_object_gs_image_pool[index].priority = 0xA;
    base4 = &g_display_object_gs_image_pool[index].position;
    base4->x = 8;
    base4->y = -0x38;
    g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.cursor_window].position.x
        = g_display_object_gs_image_pool[index].position.x + 0x18;
    g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.cursor_window].position.y
        = g_display_object_gs_image_pool[index].position.y + 0x3C;
    g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.dissolve_step = 1;
    g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.cursor_row = 0;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_SOUND_NOVEL_RESTART;
    g_menu_stack_depth++;
}

/* Input step of the sound novel resume level.
 *
 * While dissolve_step is set the level feeds the dissolve upload
 * (picture_dissolve_update), one step per frame. Cross
 * releases the level, restarts the message box and returns to the treasure
 * detail; Circle fades the screen out, and once the fade is over the level is
 * released and the novel opens, its saved reading discarded when the second
 * row was chosen. Up and down toggle cursor_row, moving the cursor by 16.
 *
 * sound_novel_selected_reading_reset ignores its argument; retail still
 * passes g_sound_novel_selected. */
void sound_novel_reading_choice_update(menu_stack_record_t* level) {
    GsIMAGE image;
    s32 buttons;

    if (g_display_object_gs_image_pool[level->sound_novel_resume.message_render].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    if (level->sound_novel_resume.dissolve_step != 0) {
        GsGetTimInfo(g_picture_buffer + 1, &image);
        if (picture_dissolve_update(&image, level->sound_novel_resume.dissolve_step) == 0) {
            level->sound_novel_resume.dissolve_step = 0;
            return;
        }
        level->sound_novel_resume.dissolve_step++;
        return;
    }

    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED) && !(g_world_map_flags & WORLD_MAP_FLAG_FADING)) {
        g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
        g_display_object_animated_count--;
        g_display_object_image_count -= 3;
        g_display_object_queue_count -= 4;
        g_menu_stack_depth--;
        if (level->sound_novel_resume.cursor_row != 0) {
            sound_novel_selected_reading_reset(g_sound_novel_selected);
        }
        sound_novel_reader_open(level->sound_novel_resume.mode);
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        g_display_object_animated_count--;
        g_display_object_image_count -= 3;
        g_display_object_queue_count -= 4;
        g_menu_stack_depth--;
        thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
        sound_play_effect(SOUND_SFX_CANCEL);
        brave_story_treasure_level_open(level->sound_novel_resume.mode);
        return;
    }

    if (buttons & PSX_PAD_CIRCLE) {
        g_display_object_animated_pool[level->sound_novel_resume.cursor_window].sequence
            = DISPLAY_OBJECT_ANIM_CURSOR_PRESS;
        g_display_object_animated_pool[level->sound_novel_resume.cursor_window].frame_index
            = g_display_object_animated_pool[level->sound_novel_resume.cursor_window].anim_counter = 0;
        world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
        sound_play_effect(SOUND_SFX_CONFIRM);
        return;
    }

    if (world_map_input_repeat_update(PSX_PAD_UP) == 0 && world_map_input_repeat_update(PSX_PAD_DOWN) == 0) {
        return;
    }

    level->sound_novel_resume.cursor_row ^= 1;
    g_display_object_animated_pool[level->sound_novel_resume.cursor_window].position.y
        = g_display_object_gs_image_pool[level->sound_novel_resume.option_render].position.y + 0x3c
        + level->sound_novel_resume.cursor_row * 0x10;
    sound_play_effect(SOUND_SFX_CURSOR_MOVE);
}
