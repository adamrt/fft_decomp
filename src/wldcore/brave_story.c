/* Brave Story levels, including story events and their text, script variable lists and details, and tutorial topics and
 * their text. */

#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_save.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_panel.h"
#include "fft/script_variable.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/window.h"
#include "fft/wldcore.h"
#include "fft/wldcore_gfx.h"
#include "fft/wldcore_map.h"
#include "fft/wldcore_menu.h"
#include "fft/wldcore_menu_level.h"
#include "fft/wldcore_proposition.h"
#include "fft/wldcore_sound.h"
#include "fft/wldcore_text.h"
#include "fft/wldcore_window.h"
#include "fft/world_menu.h"
#include "fft/world_sprite.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

static void wldcore_list_open_persons(void);
static void wldcore_list_open_story_events(void);
static void wldcore_menu_build_brave_story_entries(void);
static void wldcore_menu_push_proposition_category_level(void);
static void wldcore_menu_push_tutorial_topic_text_level(s32 topic);
static void wldcore_menu_push_variable_detail_level(s32 value);

/* Draws the event transition's spinning sprite while that level is on top of
 * the menu stack, unless it is a plain fade.
 *
 * The level is spelled g_wldcore_menu_stack_records_next[current - 1]; the
 * equivalent g_wldcore_menu_stack_records[current] changes the address
 * arithmetic. */
void wldcore_draw_close_indicator_if_pending(GsOT* ordering_table) {
    s32 current = g_wldcore_menu_stack_depth;

    if (g_wldcore_menu_stack_types[current] == WLDCORE_MENU_LEVEL_EVENT_TRANSITION
        && g_wldcore_menu_stack_records_next[current - 1].event_transition.mode != WLDCORE_EVENT_TRANSITION_MODE_FADE) {
        GsSortSprite(&g_wldcore_menu_indicator_sprite, ordering_table, 2);
    }
}

/* The byte color triple of the screen-wide fade overlay. It
 * overlaps g_wldcore_screen_fade_state.boxes[0].r, but the target addresses
 * it separately from the 32-bit flags: reaching it through the fade-state
 * struct makes the flags update use absolute addressing instead of a base
 * register. */
extern CVECTOR g_wldcore_screen_fade_color;

/* Per-frame step of the event-transition level (WLDCORE_MENU_LEVEL_EVENT_TRANSITION)
 * pushed by wldcore_menu_push_event_transition_level: the screen effect that
 * leads from the map into an event or battle.
 *
 * In FADE mode it only waits for the screen fade and then ends the world loop
 * (toggling MAIN_SYSTEM_FLAG_RUNNING off). Otherwise frame 1 copies the screen
 * to VRAM (0x100, 0) and hides the map and root windows; the sprite drifts a
 * pixel a frame and, every third frame, spins and shrinks in SPIN_IN, then in
 * SPIN mode spins back out (SPIN_OUT) textured with the captured screen. The
 * fade box over g_wldcore_screen_fade_color flashes up by 0x10 a frame
 * to 0x80 (FLASH), then whites out by 8 a frame (WHITE_OUT); at full white the
 * world loop ends and the picture at VRAM (0x200, 0x100) is moved to
 * (0x100, 0). */
void wldcore_menu_step_event_transition_level(wldcore_menu_event_transition_level_t* level) {
    RECT rect;
    s32 step;
    s32 shade;
    u8 red_sum;
    s32 angle;

    if (level->mode == WLDCORE_EVENT_TRANSITION_MODE_FADE) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            g_main_system_flags = g_main_system_flags ^ MAIN_SYSTEM_FLAG_RUNNING;
        }
        return;
    }

    /* Capture the screen into VRAM at (0x100, 0) and hide the map and root windows. */
    if (level->frame == 1) {
        rect.w = 0xFF;
        rect.x = 0;
        rect.y = 0;
        rect.h = 0xF0;
        MoveImage(&rect, 0x100, 0);
        DrawSync(0);
        g_wldcore_window_records[g_wldcore_map_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_root_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    }

    /* White-out: brighten the overlay by 8 a frame; at full white, toggle the running flag
     * and move the image at VRAM (0x200, 0x100) back to (0x100, 0). */
    if (level->fade_phase == WLDCORE_EVENT_TRANSITION_FADE_WHITE_OUT) {
        red_sum = g_wldcore_screen_fade_color.r + 8;
        g_wldcore_screen_fade_color.r = red_sum;
        g_wldcore_screen_fade_color.g = g_wldcore_screen_fade_color.g + 8;
        g_wldcore_screen_fade_color.b = g_wldcore_screen_fade_color.b + 8;
        if (red_sum >= 0xF8) {
            g_wldcore_screen_fade_color.b = 0xFF;
            g_wldcore_screen_fade_color.g = 0xFF;
            g_wldcore_screen_fade_color.r = 0xFF;
            rect.x = 0x200;
            rect.y = 0x100;
            rect.w = 0x80;
            rect.h = 0x100;
            g_main_system_flags = g_main_system_flags ^ MAIN_SYSTEM_FLAG_RUNNING;
            MoveImage(&rect, 0x100, 0);
            DrawSync(0);
        }
        return;
    }

    /* Flash: brighten by 0x10 a frame to 0x80, then cut to black and start the white-out. */
    if (level->fade_phase == WLDCORE_EVENT_TRANSITION_FADE_FLASH) {
        g_wldcore_screen_fade_color.r = g_wldcore_screen_fade_color.r + 0x10;
        g_wldcore_screen_fade_color.g = g_wldcore_screen_fade_color.g + 0x10;
        g_wldcore_screen_fade_color.b = g_wldcore_screen_fade_color.b + 0x10;
        if (g_wldcore_screen_fade_color.g >= 0x80) {
            g_wldcore_screen_fade_color.r = 0;
            g_wldcore_screen_fade_color.g = 0;
            g_wldcore_screen_fade_color.b = 0;
            level->fade_phase = WLDCORE_EVENT_TRANSITION_FADE_WHITE_OUT;
        }
    }

    /* The indicator drifts a pixel a frame on each axis; spin, scale and shade step every third frame. */
    switch (level->phase) {
    case WLDCORE_EVENT_TRANSITION_PHASE_SPIN_IN:
        if (level->drift_direction & WLDCORE_EVENT_TRANSITION_DRIFT_RIGHT) {
            g_wldcore_menu_indicator_sprite.x = g_wldcore_menu_indicator_sprite.x + 1;
        } else {
            g_wldcore_menu_indicator_sprite.x = g_wldcore_menu_indicator_sprite.x - 1;
        }
        if (level->drift_direction & WLDCORE_EVENT_TRANSITION_DRIFT_DOWN) {
            g_wldcore_menu_indicator_sprite.y = g_wldcore_menu_indicator_sprite.y + 1;
        } else {
            g_wldcore_menu_indicator_sprite.y = g_wldcore_menu_indicator_sprite.y - 1;
        }

        if (level->frame == ((level->frame / 3) * 3)) {
            step = level->anim_step + 2;
            level->anim_step = step;
            g_wldcore_menu_indicator_sprite.scalex = 0x1100 - (step << 7);
            g_wldcore_menu_indicator_sprite.scaley = 0x1100 - (level->anim_step << 7);
            if (level->spin_direction != 0) {
                angle = g_wldcore_menu_indicator_sprite.rotate + 0x2000;
            } else {
                angle = g_wldcore_menu_indicator_sprite.rotate - 0x2000;
            }
            g_wldcore_menu_indicator_sprite.rotate = angle;

            /* Stored as a byte, -0x80 - 4 * step darkens the sprite from 0x80. */
            shade = -0x80 - (level->anim_step * 4);
            g_wldcore_menu_indicator_sprite.b = shade;
            g_wldcore_menu_indicator_sprite.g = shade;
            g_wldcore_menu_indicator_sprite.r = shade;

            if (level->mode == WLDCORE_EVENT_TRANSITION_MODE_SPIN) {
                if (level->anim_step == 4) {
                    wldcore_sound_play_effect(MAIN_SFX_EVENT_TRANSITION_MODE_0);
                }

                /* Spin back out at full size, textured from the 16-bit page holding the capture. */
                if (level->anim_step >= 0x20) {
                    level->phase = WLDCORE_EVENT_TRANSITION_PHASE_SPIN_OUT;
                    g_wldcore_menu_indicator_sprite.attribute = 0x52000000;
                    g_wldcore_menu_indicator_sprite.tpage = GetTPage(2, 1, 0x100, 0);
                    g_wldcore_menu_indicator_sprite.scaley = ONE;
                    g_wldcore_menu_indicator_sprite.scalex = ONE;
                }
            } else {
                if (level->anim_step == 4) {
                    wldcore_sound_play_effect(MAIN_SFX_EVENT_TRANSITION_MODE_1);
                }
                if (level->anim_step >= 0x20) {
                    level->phase = WLDCORE_EVENT_TRANSITION_PHASE_DONE;
                }
                if (level->anim_step == 0x18) {
                    g_wldcore_screen_fade_color.r = 0;
                    g_wldcore_screen_fade_color.g = 0;
                    g_wldcore_screen_fade_color.b = 0;
                    g_wldcore_screen_fade_state.flags[0] |= WLDCORE_SCREEN_FADE_BOX_VISIBLE;
                    level->fade_phase = WLDCORE_EVENT_TRANSITION_FADE_WHITE_OUT;
                }
            }
        }
        break;

    case WLDCORE_EVENT_TRANSITION_PHASE_SPIN_OUT:
        if (level->drift_direction & WLDCORE_EVENT_TRANSITION_DRIFT_RIGHT) {
            g_wldcore_menu_indicator_sprite.x = g_wldcore_menu_indicator_sprite.x - 1;
        } else {
            g_wldcore_menu_indicator_sprite.x = g_wldcore_menu_indicator_sprite.x + 1;
        }
        if (level->drift_direction & WLDCORE_EVENT_TRANSITION_DRIFT_DOWN) {
            g_wldcore_menu_indicator_sprite.y = g_wldcore_menu_indicator_sprite.y - 1;
        } else {
            g_wldcore_menu_indicator_sprite.y = g_wldcore_menu_indicator_sprite.y + 1;
        }

        if (level->frame == ((level->frame / 3) * 3)) {
            step = level->anim_step + 4;
            level->anim_step = step;
            g_wldcore_menu_indicator_sprite.scalex = 0x1100 - ((0x40 - step) << 7);
            g_wldcore_menu_indicator_sprite.scaley = 0x1100 - ((0x40 - level->anim_step) << 7);
            g_wldcore_menu_indicator_sprite.b = (level->anim_step - 0x20) * 4;
            if (level->spin_direction != 0) {
                angle = g_wldcore_menu_indicator_sprite.rotate - 0x2000;
            } else {
                angle = g_wldcore_menu_indicator_sprite.rotate + 0x2000;
            }
            g_wldcore_menu_indicator_sprite.rotate = angle;

            if (level->anim_step >= 0x38) {
                g_wldcore_screen_fade_color.r = 0;
                g_wldcore_screen_fade_color.g = 0;
                g_wldcore_screen_fade_color.b = 0;
                g_wldcore_screen_fade_state.flags[0] |= WLDCORE_SCREEN_FADE_BOX_VISIBLE;
                level->fade_phase = WLDCORE_EVENT_TRANSITION_FADE_FLASH;
            }
            if (level->anim_step >= 0x50) {
                g_wldcore_menu_indicator_sprite.r = 0;
                g_wldcore_menu_indicator_sprite.g = 0;
                g_wldcore_menu_indicator_sprite.b = 0;
                level->phase = WLDCORE_EVENT_TRANSITION_PHASE_DONE;
            }
        }
        break;
    }

    level->frame = level->frame + 1;
}

/* Pushes the Brave Story menu (WLDCORE_MENU_LEVEL_BRAVE_STORY).
 *
 * Appends the cursor, the two completion stars and the side label, draws one
 * row per entry (WLDCORE_TEXT_ID_BRAVE_STORY_ROW_FIRST + row) into a new render
 * record, draws the injured and casualty counts (each capped at 9999) into a
 * second record with WLDCORE_TEXT_ID_INJURED_CASUALTIES, selects the
 * play-time display, clamps the saved cursor row in
 * g_wldcore_brave_story_saved_cursor to the entry count and positions the windows relative to the list
 * record before committing the level at depth + 1.
 *
 * The unused 48-byte local reproduces the target's frame size. */
void wldcore_menu_push_brave_story_level(void) {
    RECT bounds;
    point32_t point;
    s32 index;
    s32 value;
    s32 i;
    s32 depth;
    s32 temp;
    u8 unused[0x30];

    g_main_system_flags = g_main_system_flags & ~MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;

    index = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.main_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.upper_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_BRAVE_STORY_MARKER;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.lower_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_BRAVE_STORY_MARKER;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.side_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_LIST_SIDE_LABEL;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    i = 0;
    wldcore_menu_build_brave_story_entries();

    index = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.frame_render = index;
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = 0x5C;
    bounds.h = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 0xA;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = -0x78;
        base->y = -0x28;
    }

    point.x = 8;
    point.y = 8;
    for (; i < g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count; i++) {
        window_display_text_entry(index,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[i]
                + WLDCORE_TEXT_ID_BRAVE_STORY_ROW_FIRST,
            point, g_wldcore_window_image_buffer);
        point.y += 0x10;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);

    index = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.content_render = index;
    value = script_get_variable(EVENT_SCRIPT_VAR_INJURED);
    if (value >= 10000) {
        value = 9999;
    }
    g_text_substitution_values[0] = value;
    value = script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES);
    if (value >= 10000) {
        value = 9999;
    }
    g_text_substitution_values[1] = value;
    bounds.x = 0x80;
    bounds.y = 0x40;
    bounds.w = 0x5C;
    bounds.h = 0x30;
    window_init_text_record(index, bounds, 0, WLDCORE_TEXT_ID_INJURED_CASUALTIES, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 0xA;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0x1C;
        base->y = -0x28;
    }

    g_wldcore_context_value_display_mode = WLDCORE_VALUE_DISPLAY_PLAY_TIME;
    g_wldcore_displayed_numeric_value = g_main_system_play_time_hours;
    g_wldcore_displayed_numeric_value_secondary = g_main_system_play_time_minutes;
    g_wldcore_displayed_numeric_value_tertiary = g_main_system_play_time_seconds;

    depth = g_wldcore_menu_stack_depth;
    /* temp spans two blocks without crossing a call, so global allocation
     * gives the clamp flag and the upper window's y sum the target's v1.
     * A saved cursor past the last entry restarts at row 0. */
    temp = g_wldcore_menu_stack_records_next[depth].list_window.entry_count < g_wldcore_brave_story_saved_cursor[0] + 1;
    if (temp) {
        g_wldcore_brave_story_saved_cursor[0] = 0;
    }
    g_wldcore_menu_stack_records_next[depth].list_window.selected_entry = g_wldcore_brave_story_saved_cursor[0];
    g_wldcore_menu_stack_records_next[depth].list_window.mode = WLDCORE_LIST_WINDOW_MODE_BROWSE;

    /* Place the windows on the list record: the cursor 16 pixels per saved row, and the two
     * stars by adding the row offsets wldcore_menu_build_brave_story_entries stored in their y. */
    index = g_wldcore_menu_stack_records_next[depth].list_window.frame_render;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.main_window].position.x
        = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.main_window].position.y
        = g_wldcore_window_render_records[index].base.y - g_wldcore_brave_story_saved_cursor[0] * -0x10 + 0xE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.side_window].position.x
        = g_wldcore_window_render_records[index].base.x + 3;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.side_window].position.y
        = g_wldcore_window_render_records[index].base.y - 2;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.upper_window].position.x
        = g_wldcore_window_render_records[index].base.x + 0x4C;
    {
        s32 base_y = g_wldcore_window_render_records[index].base.y;

        temp = g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.upper_window].position.y
            + base_y;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.upper_window].position.y = temp;
    }
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.lower_window].position.x
        = g_wldcore_window_render_records[index].base.x + 0x30;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.lower_window].position.y
        += g_wldcore_window_render_records[index].base.y;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_BRAVE_STORY;
}

/* Build the Brave Story rows of the level wldcore_menu_push_brave_story_level
 * pushes. The facts row is listed when EVENT_SCRIPT_VAR_FACTS_ENABLED and any
 * fact flag are set; the story events and persons rows always; the
 * propositions row while any job is active or completed; the unexplored
 * lands and treasures rows once one is found; then Leave. The upper and lower
 * windows are the completion stars of the unexplored lands and treasures
 * rows: they show (WORLD_RENDER_RECORD_BLANK cleared) once all 16 lands or
 * the first 31 treasures are found.
 */
static void wldcore_menu_build_brave_story_entries(void) {
    s32 count;
    s32 i;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count = 0;
    if (script_get_variable(EVENT_SCRIPT_VAR_FACTS_ENABLED) != 0) {
        count = 0;
        for (i = 0; i < EVENT_SCRIPT_VAR_FACT_COUNT; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_FACT_FIRST) != 0) {
                count++;
            }
        }
        if (count != 0) {
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[0]
                = WLDCORE_BRAVE_STORY_ROW_FACTS;
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count = 1;
        }
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
        .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
        = WLDCORE_BRAVE_STORY_ROW_STORY_EVENTS;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
        .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count + 1]
        = WLDCORE_BRAVE_STORY_ROW_PERSONS;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count += 2;

    count = g_main_save_proposition_count;
    for (i = 0; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_PROPOSITION_FIRST) & WLDCORE_PROPOSITION_VARIABLE_COMPLETED) {
            count++;
        }
    }
    if (count != 0) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
            .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
            = WLDCORE_BRAVE_STORY_ROW_PROPOSITIONS;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
    }

    count = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.upper_window]
        .flags |= WORLD_RENDER_RECORD_BLANK;
    for (i = 0; i < EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST) != 0) {
            count++;
        }
    }
    if (count != 0) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.upper_window]
            .position.y
            = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count * 0x10 + 0xE;
        if (count >= 0x10) {
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                         .list_window.upper_window]
                .flags &= ~WORLD_RENDER_RECORD_BLANK;
        }
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
            .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
            = WLDCORE_BRAVE_STORY_ROW_UNEXPLORED_LANDS;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
    }

    count = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.lower_window]
        .flags |= WORLD_RENDER_RECORD_BLANK;
    for (i = 0; i < EVENT_SCRIPT_VAR_TREASURE_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) != 0) {
            count++;
        }
    }
    if (count != 0) {
        count = 0;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.lower_window]
            .position.y
            = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count * 0x10 + 0xE;
        for (i = 0; i < 31; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) != 0) {
                count++;
            }
        }
        if (count >= 0x1F) {
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                         .list_window.lower_window]
                .flags &= ~WORLD_RENDER_RECORD_BLANK;
        }
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
            .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
            = WLDCORE_BRAVE_STORY_ROW_TREASURES;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
        .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
        = WLDCORE_BRAVE_STORY_ROW_LEAVE;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
}

/* Return the Brave Story menu to browsing when a sub-level closes: restore
 * its palettes and the idle cursor, show WLDCORE_TEXT_ID_BRAVE_STORY_INTRO
 * again and show (with their pop-in) whichever of its records a sub-level
 * hid. */
void wldcore_menu_resume_brave_story_level(wldcore_menu_stack_record_t* level) {
    g_wldcore_window_records[level->list_window.main_window].palette
        = g_wldcore_window_records[level->list_window.side_window].palette
        = g_wldcore_window_render_records[level->list_window.frame_render].palette
        = g_wldcore_window_render_records[level->list_window.content_render].palette = 0;
    g_wldcore_window_records[level->list_window.upper_window].palette
        = g_wldcore_window_records[level->list_window.lower_window].palette = 0;
    g_wldcore_window_records[level->list_window.main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[level->list_window.main_window].frame_index
        = g_wldcore_window_records[level->list_window.main_window].anim_counter = 0;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
    if (g_wldcore_window_render_records[level->list_window.frame_render].flags & WORLD_RENDER_RECORD_HIDDEN) {
        g_wldcore_window_records[level->list_window.main_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->list_window.side_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->list_window.frame_render].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->list_window.frame_render].flags |= WORLD_RENDER_RECORD_POP_IN;
        g_wldcore_window_records[level->list_window.upper_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->list_window.lower_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    }
    if (g_wldcore_window_render_records[level->list_window.content_render].flags & WORLD_RENDER_RECORD_HIDDEN) {
        g_wldcore_window_render_records[level->list_window.content_render].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_main_system_flags &= ~MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        g_wldcore_window_render_records[level->list_window.content_render].flags |= WORLD_RENDER_RECORD_POP_IN;
    }
    level->list_window.mode = WLDCORE_LIST_WINDOW_MODE_BROWSE;
}

/* Gray out the Brave Story menu while a sub-level opens over it: the cursor
 * takes palette 10 and restarts its press animation, the side label 6, the
 * frame 2 and both stars 5. Under a help message (FADE_OR_HELP) the
 * injured/casualties record is grayed too; otherwise any row but Persons hides
 * it and the play-time display (MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE).
 *
 * The chained store keeps one main-window index for both cleared fields. */
void wldcore_menu_suspend_brave_story_level(wldcore_menu_list_window_level_t* state) {
    g_wldcore_window_records[state->main_window].palette = 10;
    g_wldcore_window_records[state->side_window].palette = 6;
    g_wldcore_window_render_records[state->frame_render].palette = 2;
    g_wldcore_window_records[state->upper_window].palette = 5;
    g_wldcore_window_records[state->lower_window].palette = 5;
    g_wldcore_window_records[state->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
    g_wldcore_window_records[state->main_window].frame_index = g_wldcore_window_records[state->main_window].anim_counter
        = 0;
    if (state->mode == WLDCORE_LIST_WINDOW_MODE_FADE_OR_HELP) {
        g_wldcore_window_render_records[state->content_render].palette = 2;
        return;
    }
    if (state->entries[state->selected_entry] != WLDCORE_BRAVE_STORY_ROW_PERSONS) {
        g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        g_wldcore_window_render_records[state->content_render].flags |= WORLD_RENDER_RECORD_HIDDEN;
    }
}

/* Input step of the Brave Story list level.
 *
 * Copies the play time into the displayed numeric values, then:
 *  - while input is blocked: once the fade out ends, wait for the message
 *    thread (and, when opened from the title, the audio queue), pop the level
 *    and either return to the title or fade the map back in and push the
 *    entry window level;
 *  - after the goodbye message: Circle or Cross starts the fade out and the
 *    world-map music (from the title, fades out and unloads the music);
 *  - otherwise: Cross and the Leave row show the goodbye message and release
 *    the level's windows; Select shows the highlighted row's help; Circle
 *    opens the highlighted row's list; Up and Down move the cursor with
 *    wrap-around, stepping the window record's y by 16.
 *
 * The cancel path and the Leave row each spell out the window-count release;
 * jump2 cross-jumps them after the two count loads. The target reloads the
 * cursor after storing it to g_wldcore_brave_story_saved_cursor, which needs g_wldcore_brave_story_saved_cursor
 * accessed as an array element (a scalar store does not alias the level field). The wrap-to-bottom step is written `y
 * -= n * -16` to keep y as the first addu operand.
 */
void wldcore_menu_handle_brave_story_input(wldcore_menu_list_window_level_t* level) {
    u32 flags;
    u32 buttons;

    g_wldcore_displayed_numeric_value = g_main_system_play_time_hours;
    g_wldcore_displayed_numeric_value_secondary = g_main_system_play_time_minutes;
    g_wldcore_displayed_numeric_value_tertiary = g_main_system_play_time_seconds;

    if (g_wldcore_window_render_records[level->content_render].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }

    flags = g_main_system_flags;
    if (flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) {
        if (level->mode == WLDCORE_LIST_WINDOW_MODE_FADE_OR_HELP && !(flags & MAIN_SYSTEM_FLAG_FADING)) {
            level->mode = WLDCORE_LIST_WINDOW_MODE_LEAVE_WAIT;
            g_main_system_flags = flags | MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
            wldcore_gfx_clear_vram_and_scratch(1);
        }
        if (level->mode != WLDCORE_LIST_WINDOW_MODE_LEAVE_WAIT) {
            return;
        }
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
            return;
        }
        if ((g_main_system_flags & MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY) && g_wldcore_audio_queue.count != 0) {
            return;
        }

        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_window_set_render_objects_visible(1);
        g_wldcore_menu_stack_depth--;
        g_main_system_flags &= ~(MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE | MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE
            | MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER);
        wldcore_menu_dispatch_suspend_handler();

        /* Opened from the title screen: go back there instead of to the map. */
        if (g_main_system_flags & MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY) {
            g_main_system_flags
                = (g_main_system_flags ^ (MAIN_SYSTEM_FLAG_RUNNING | MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY))
                | MAIN_SYSTEM_FLAG_RETURN_TO_TITLE;
            return;
        }
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_REDRAW_MAP, 0x10);
        wldcore_menu_push_entry_window_level();
        return;
    }

    if (level->mode == WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE) {
        if (!(g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
            return;
        }
        level->mode = WLDCORE_LIST_WINDOW_MODE_FADE_OR_HELP;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
        if (g_main_system_flags & MAIN_SYSTEM_FLAG_TITLE_BRAVE_STORY) {
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_UNLOAD, 0);
        } else {
            wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(1, MAIN_MUSIC_WORLD_MAP));
        }
        g_wldcore_brave_story_saved_cursor[0] = 0;
        return;
    }

    if (g_wldcore_audio_queue.count != 0) {
        return;
    }

    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_GOODBYE, 0);
        level->mode = WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE;

        /* Release the level's four windows and two render records: six queue entries. */
        g_wldcore_window_record_count -= 4;
        g_wldcore_window_render_record_count -= 2;
        g_wldcore_window_render_object_count -= 6;
        return;
    }

    if (buttons & PSX_PAD_SELECT) {
        level->mode = WLDCORE_LIST_WINDOW_MODE_FADE_OR_HELP;
        wldcore_menu_suspend_brave_story_level(level);
        wldcore_menu_push_message_level(
            g_wldcore_brave_story_help_text_ids[level->entries[level->selected_entry]] | 0x1000, 1);
        return;
    }

    if (buttons & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        g_wldcore_brave_story_saved_cursor[0] = level->selected_entry;
        switch (level->entries[level->selected_entry]) {
        case WLDCORE_BRAVE_STORY_ROW_FACTS:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_FACTS);
            wldcore_menu_suspend_brave_story_level(level);
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
            wldcore_list_open_facts();
            return;
        case WLDCORE_BRAVE_STORY_ROW_STORY_EVENTS:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_STORY_EVENTS);
            wldcore_menu_suspend_brave_story_level(level);
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
            wldcore_list_open_story_events();
            return;
        case WLDCORE_BRAVE_STORY_ROW_PERSONS:
            wldcore_menu_suspend_brave_story_level(level);
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
            wldcore_list_open_persons();
            return;
        case WLDCORE_BRAVE_STORY_ROW_PROPOSITIONS:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_BRAVE_STORY_PROPOSITIONS);
            wldcore_menu_suspend_brave_story_level(level);
            wldcore_menu_push_proposition_category_level();
            return;
        case WLDCORE_BRAVE_STORY_ROW_UNEXPLORED_LANDS:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_UNEXPLORED_LANDS);
            wldcore_menu_suspend_brave_story_level(level);
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
            wldcore_list_open_unexplored_lands();
            return;
        case WLDCORE_BRAVE_STORY_ROW_TREASURES:
            wldcore_load_message_block_if_changed(WLDCORE_MESSAGE_BLOCK_TREASURES);
            wldcore_menu_suspend_brave_story_level(level);
            g_wldcore_window_records[level->main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
            wldcore_list_open_treasures();
            return;
        case WLDCORE_BRAVE_STORY_ROW_LEAVE:
            /* The same goodbye and release as the cancel path above. */
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_GOODBYE, 0);
            level->mode = WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE;
            g_wldcore_window_record_count -= 4;
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 6;
            return;
        }
        return;
    }

    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->selected_entry != 0) {
            g_wldcore_window_records[level->main_window].position.y -= 0x10;
            level->selected_entry = level->selected_entry - 1;
        } else {
            g_wldcore_window_records[level->main_window].position.y -= (level->entry_count - 1) * -16;
            level->selected_entry = level->entry_count - 1;
        }
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }

    if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        if (level->selected_entry < level->entry_count - 1) {
            g_wldcore_window_records[level->main_window].position.y += 0x10;
            level->selected_entry = level->selected_entry + 1;
        } else {
            g_wldcore_window_records[level->main_window].position.y -= (level->entry_count - 1) * 16;
            level->selected_entry = 0;
        }
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
}

/* Opens the Brave Story's Record list: every story event whose script
 * variable (EVENT_SCRIPT_VAR_STORY_EVENT_FIRST + index) is set, by name
 * (TEXT_ID_STORY_EVENT_NAME_BASE + index), up to eight rows at (0x4c, 0x4f),
 * 0x94 wide, and pushes WLDCORE_MENU_LEVEL_STORY_EVENTS. Twin of
 * wldcore_list_open_treasures, but walks the output arrays by pointer. */
static void wldcore_list_open_story_events(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* flags;
    s32 count;
    s32 i;
    s32 record_index;

    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
    count = 0;
    i = 0;
    flags = g_wldcore_list_row_flags;
    values = g_wldcore_list_entry_values;
    for (; i < EVENT_SCRIPT_VAR_STORY_EVENT_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_STORY_EVENT_FIRST) != 0) {
            *values = i + TEXT_ID_STORY_EVENT_NAME_BASE;
            *flags = 0;
            flags++;
            values++;
            count++;
        }
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x94;
    origin.x = 0x4C;
    layout.visible_rows = i;
    origin.y = 0x4F;
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_STORY_EVENTS, count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 9;
    record_index = g_wldcore_menu_stack_depth;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_STORY_EVENTS;
    g_wldcore_menu_stack_depth = record_index + 1;
}

/* Resume handler of the story events list: block Circle, Cross and Select for the
 * levels underneath and show WLDCORE_TEXT_ID_BRAVE_STORY_INTRO again. */
void wldcore_list_story_events_restore_prompt(void) {
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void wldcore_list_story_events_block_all_buttons(wldcore_menu_panel_level_t* level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the story events list; twin of wldcore_list_handle_persons_input with help
 * message 0x1055. Cancel and confirm wait for file loading to finish; a
 * confirmed entry's value is passed to wldcore_menu_push_story_event_text_level. */
void wldcore_list_handle_story_events_input(wldcore_menu_panel_level_t* level) {
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        if (level->result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_STORY_EVENTS);
            wldcore_menu_push_story_event_text_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .list_window.content_render]
            .flags |= WORLD_RENDER_RECORD_POP_IN;
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_STORY_EVENTS);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((g_wldcore_input_new_button_presses & PSX_PAD_CROSS) && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_story_events_block_all_buttons(level);
        wldcore_menu_push_message_level(0x1055, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes WLDCORE_MENU_LEVEL_STORY_EVENT_TEXT for story event `value`: records
 * the entry on the new level, sets up g_wldcore_scrollable_text_window on its
 * text (TEXT_ID_WORLD_MESSAGE_BASE + value, from the story events message
 * block) with no substitutions, hides the Brave Story menu's four windows and
 * frame, and says WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO with the event's name.
 *
 * Masked-identical to wldcore_menu_push_fact_detail_level across all 165
 * instructions. The two differ only in the name base substituted
 * (TEXT_ID_FACT_NAME_BASE there), the menu level type and the frame size.
 *
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one.
 *
 * The depth is spelled as the global at every use rather than cached in a
 * local: the target re-reads g_wldcore_menu_stack_depth after each call.
 *
 * The frame is 32 bytes against the twin's 24: the target reserves eight bytes
 * below its two saved registers that it never touches, reproduced by the
 * unused local. */
void wldcore_menu_push_story_event_text_level(s32 value) {
    s32 unused[2];
    s32 coordinate_mode = 1;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].story_event_index = value;
    g_wldcore_scrollable_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 0:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x60;
        break;
    case 1:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x70;
        break;
    }
    g_wldcore_scrollable_text_window.base.x = -122;
    g_wldcore_scrollable_text_window.base.y = -40;
    g_wldcore_scrollable_text_window.text_width = 0xdc;
    g_wldcore_scrollable_text_window.rows_per_page = 8;
    g_wldcore_scrollable_text_window.priority = 9;
    g_wldcore_scrollable_text_window.image_coordinate_mode = coordinate_mode;
    g_wldcore_scrollable_text_window.extra_render_index = -1;
    g_wldcore_scrollable_text_window.text_substitutions[1] = -1;
    g_wldcore_scrollable_text_window.text_substitutions[0] = -1;
    wldcore_text_init_scrollable_window(&g_wldcore_scrollable_text_window);
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.main_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.side_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .list_window.frame_render]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.upper_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.lower_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_text_substitution_values[0] = value + TEXT_ID_STORY_EVENT_NAME_BASE;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_STORY_EVENT_TEXT;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}

/* Input step of a story event's text page. Triangle replays the event: it
 * sets EVENT_SCRIPT_VAR_CURRENT_EVENT, suppresses progress effects and fades
 * out, and once the fade ends the world loop stops with
 * MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY, saving the list cursor and the weather
 * variable. When the text window closes, the story event list reopens. */
void wldcore_menu_handle_story_event_text_input(wldcore_menu_stack_record_t* level) {
    s32 depth;
    s32 index;
    s32 value;
    u32 flags;

    if ((g_wldcore_window_render_records[g_wldcore_scrollable_text_window.render_record_index].flags
            & WORLD_RENDER_RECORD_POP_IN)
        != 0) {
        return;
    }

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        flags = g_main_system_flags;
        if ((flags & MAIN_SYSTEM_FLAG_FADING) == 0) {
            value = level->story_event_index;
            g_main_saved_list_cursor_state = g_wldcore_story_events_cursor_state;
            g_main_system_flags = (flags ^ MAIN_SYSTEM_FLAG_RUNNING) | MAIN_SYSTEM_FLAG_STORY_EVENT_REPLAY;
            g_main_replay_story_event_index = value;
            g_main_saved_weather_variable = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
            return;
        }
    }

    if ((g_wldcore_input_new_button_presses & PSX_PAD_TRIANGLE) != 0) {
        script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 1);
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_story_event_ids[level->story_event_index]);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
        return;
    }

    if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) != 0) {
        return;
    }

    wldcore_sound_play_effect(MAIN_SFX_CANCEL);
    depth = g_wldcore_menu_stack_depth;
    index = depth - 2;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].flags
        |= WORLD_RENDER_RECORD_POP_IN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.main_window].flags
        &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.side_window].flags
        &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[index].list_window.frame_render].flags
        &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.upper_window].flags
        &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth = depth - 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[index].list_window.lower_window].flags
        &= ~WORLD_RENDER_RECORD_HIDDEN;
    wldcore_list_open_story_events();
}

/* Opens the Brave Story's Persons list: every person whose profile variable
 * (EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST + person) is set, by the name of that
 * profile (TEXT_ID_PERSON_PROFILE_NAME_BASE + person * 16 + profile), up to
 * eight rows at (0x80, 0x50), 0x60 wide; hides the injured/casualties record
 * and pushes WLDCORE_MENU_LEVEL_PERSONS. Twin of
 * wldcore_list_open_brave_story_propositions. */
static void wldcore_list_open_persons(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* flags;
    s32 count;
    s32 i;
    s32 value;
    s32 record_index;

    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
    count = 0;
    i = 0;
    flags = g_wldcore_list_row_flags;
    values = g_wldcore_list_entry_values;
    for (; i < EVENT_SCRIPT_VAR_PERSON_PROFILE_COUNT; i++) {
        value = script_get_variable(i + EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST);
        if (value != 0) {
            *values = TEXT_ID_PERSON_PROFILE_NAME_BASE + (i << 4) + value;
            *flags = 0;
            flags++;
            values++;
            count++;
        }
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x60;
    origin.x = 0x80;
    layout.visible_rows = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_PERSONS, count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 9;
    record_index = g_wldcore_menu_stack_depth;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index - 1].list_window.content_render]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_types_next[record_index] = WLDCORE_MENU_LEVEL_PERSONS;
    g_wldcore_menu_stack_depth = record_index + 1;
}

/* Resume handler of the persons list: block Circle, Cross and Select for the
 * levels underneath and show WLDCORE_TEXT_ID_BRAVE_STORY_INTRO again. */
void wldcore_list_persons_restore_prompt(void) {
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void wldcore_list_persons_block_all_buttons(wldcore_menu_panel_level_t* level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the persons list opened by wldcore_list_open_persons; twin of
 * wldcore_list_handle_unexplored_lands_input with help message 0x1056.
 * Canceling pops the injured/casualties record back in; a confirmed entry's
 * value is passed to wldcore_menu_push_variable_detail_level. */
void wldcore_list_handle_persons_input(wldcore_menu_panel_level_t* level) {
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        if (level->result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_PERSONS);
            wldcore_menu_push_variable_detail_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .list_window.content_render]
            .flags |= WORLD_RENDER_RECORD_POP_IN;
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_PERSONS);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_persons_block_all_buttons(level);
        wldcore_menu_push_message_level(0x1056, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Opens the detail view (WLDCORE_MENU_LEVEL_VARIABLE_DETAIL) of a person
 * chosen in wldcore_list_handle_persons_input: value is person * 16 + profile.
 * It loads the message block of the person's group of 16 (blocks 6 on),
 * appends a hidden render record for the portrait, hides the Brave Story menu
 * and says WLDCORE_TEXT_ID_RAMZA_INFO with his title (person 0) or
 * WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO with the profile's name.
 *
 * The unused 48-byte local reproduces the target's frame size. */
static void wldcore_menu_push_variable_detail_level(s32 value) {
    u8 unused[0x30];
    s32 variable;
    s32 index;
    s32 depth;

    variable = value >> 4;
    wldcore_load_message_block_if_changed(variable / 16 + 6);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.value = value;
    index = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.render_index = index;
    g_wldcore_window_render_records[index].flags
        = (g_wldcore_window_render_records[index].flags & ~WORLD_RENDER_RECORD_8BIT_CLUT) | WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.main_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.side_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .list_window.frame_render]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.upper_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.lower_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    if (variable == 0) {
        g_text_substitution_values[0] = (value & 0xF) + WLDCORE_TEXT_ID_RAMZA_TITLE_FIRST;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_RAMZA_INFO, 0);
    } else {
        g_text_substitution_values[0] = value + TEXT_ID_PERSON_PROFILE_NAME_BASE;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    }
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].variable_detail.phase = WLDCORE_DETAIL_PHASE_LOAD;
    g_wldcore_menu_stack_records_next[depth].variable_detail.timer = 0;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_VARIABLE_DETAIL;
    g_wldcore_menu_stack_depth = depth + 1;
}

/* Per-frame step of a person's detail level.
 *
 * LOAD waits for the message block. BUILD (from the eighth frame, once the
 * message box is ready) sets up the shared scrollable text window on the
 * profile's text, with Ramza's title and age or the person's age substituted,
 * and binds the portrait render record to the texture rectangle of the
 * portrait POLY_FT4. The portrait id also selects its CLUT: four 16-color
 * CLUTs to a VRAM row from (0x280, 0x1f0), ten rows to a group of 40
 * portraits, each group 64 pixels further right. POP_IN shows the portrait
 * once the text window has opened; SHOWN pops the level back to the persons
 * list once the text window has finished.
 *
 * The loading test reads g_main_file_cd_state.state (the same 32-bit value as
 * g_main_file_still_loading): only a struct-member load stays behind the
 * delay store. The unused RECT reserves the 8 bytes that put the descriptor
 * at sp+0x18, and page_x keeps the target's (column + 0x280) association. */
void wldcore_menu_step_variable_detail_level(wldcore_menu_variable_detail_level_t* level) {
    RECT rect;
    POLY_FT4 portrait;
    s32 entry;
    s32 character;
    s32 render_index;
    s32 depth;
    s32 value;
    s32 page_x;
    RECT* bounds;
    CVECTOR* color;
    point32_t* base;

    switch (level->phase) {
    case WLDCORE_DETAIL_PHASE_LOAD:
        level->timer = level->timer + 1;
        if (g_main_file_cd_state.state == MAIN_FILE_LOAD_STATE_IDLE) {
            level->phase = level->phase + 1;
        }
        return;
    case WLDCORE_DETAIL_PHASE_BUILD:
        if (level->timer < 8 || world_menu_get_event_state_flag() == 0) {
            level->timer = level->timer + 1;
            return;
        }
        script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, 0xFF);
        entry = level->value;
        g_wldcore_scrollable_text_window.image_y = 0x70;
        g_wldcore_scrollable_text_window.base.x = -0x7A;
        g_wldcore_scrollable_text_window.base.y = -0x28;
        g_wldcore_scrollable_text_window.text_width = 0xDC;
        g_wldcore_scrollable_text_window.rows_per_page = 8;
        g_wldcore_scrollable_text_window.priority = 9;
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_coordinate_mode = 1;
        character = entry >> 4;
        g_wldcore_scrollable_text_window.text_id = (entry & 0xFF) | TEXT_ID_WORLD_MESSAGE_BASE;
        if (character == 0) {
            g_wldcore_scrollable_text_window.text_substitutions[0] = g_text_substitution_values[0];
            g_wldcore_scrollable_text_window.text_substitutions[1] = g_main_brave_story_character_ages[0];
        } else {
            g_wldcore_scrollable_text_window.text_substitutions[0] = g_main_brave_story_character_ages[character];
            g_wldcore_scrollable_text_window.text_substitutions[1] = -1;
        }
        g_wldcore_scrollable_text_window.extra_render_index = -1;
        wldcore_text_init_scrollable_window(&g_wldcore_scrollable_text_window);
        /* Rendering the text stores the portrait it names, if any, over the 0xff. */
        value = script_get_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
        render_index = level->render_index;
        if (value != 0xFF) {
            menu_build_unit_portrait_poly(&portrait, value);
            page_x = 0x280;
            g_wldcore_window_render_records[render_index].clut.x = ((value / 40) << 6) + ((value % 4) * 0x10 + page_x);
            g_wldcore_window_render_records[render_index].clut.y = ((value % 40) / 4) + 0x1F0;
            g_wldcore_window_render_records[render_index].tpage = portrait.tpage;
            /* The rectangle at +0x28 and the color triple at +0x30 are written
             * through their own addresses; the rectangle and the base point are
             * then copied as blocks into the text window's extra-record fields. */
            bounds = &g_wldcore_window_render_records[render_index].bounds;
            bounds->x = portrait.u0;
            bounds->y = portrait.v0;
            bounds->w = portrait.u1 - portrait.u0;
            bounds->h = portrait.v2 - portrait.v0;
            color = &g_wldcore_window_render_records[render_index].color;
            color->b = color->g = color->r = 0x80;
            g_wldcore_window_render_records[render_index].anim_counter = 0;
            base = &g_wldcore_window_render_records[render_index].base;
            base->x = -0x72;
            base->y = -0x22;
            g_wldcore_window_render_records[render_index].priority = 9;
            g_wldcore_scrollable_text_window.extra_render_index = render_index;
            g_wldcore_scrollable_text_window.extra_base = *base;
            g_wldcore_scrollable_text_window.extra_bounds = g_wldcore_window_render_records[render_index].bounds;
        } else {
            g_wldcore_window_render_records[render_index].flags |= WORLD_RENDER_RECORD_BLANK;
        }
        level->phase = level->phase + 1;
        return;
    case WLDCORE_DETAIL_PHASE_POP_IN:
        if (!(g_wldcore_window_render_records[g_wldcore_scrollable_text_window.render_record_index].flags
                & WORLD_RENDER_RECORD_POP_IN)) {
            g_wldcore_window_render_records[level->render_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            level->phase = WLDCORE_DETAIL_PHASE_SHOWN;
        }
        return;
    default:
        if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) == 0) {
            depth = g_wldcore_menu_stack_depth;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
                |= WORLD_RENDER_RECORD_POP_IN;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.main_window].flags
                &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.side_window].flags
                &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
                &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_menu_stack_depth = depth - 1;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.upper_window].flags
                &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_record_count = g_wldcore_window_render_record_count - 1;
            g_wldcore_window_render_object_count = g_wldcore_window_render_object_count - 1;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.lower_window].flags
                &= ~WORLD_RENDER_RECORD_HIDDEN;
            wldcore_list_open_persons();
        }
    }
}

#define append_panel_list_entry(count, value)                                                                          \
    do {                                                                                                               \
        g_wldcore_list_entry_values[count] = value;                                                                    \
        g_wldcore_list_row_flags[count] = 0;                                                                           \
        count = (count) + 1;                                                                                           \
    } while (0)

/* Opens the list of the 41 tutorial topics (TEXT_ID_TIP_NAME_BASE + topic),
 * up to eight rows at (0x54, 0x50), 0x8c wide, and pushes
 * WLDCORE_MENU_LEVEL_TUTORIAL_TOPICS.
 *
 * The list is indexed by `count` rather than `i`; indexing by `i` lets loop.c
 * fold the final count to the constant 41. */
void wldcore_list_open_tutorial_topics(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s32 count = 0;
    s32 i;
    s32 visible;
    s32 record_index;

    for (i = 0; i < 41; i++) {
        append_panel_list_entry(count, i + TEXT_ID_TIP_NAME_BASE);
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        visible = count;
    } else {
        visible = 8;
    }
    layout.text_width = 0x8c;
    origin.x = 0x54;
    layout.visible_rows = visible;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_TUTORIAL_TOPICS, count, visible);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_TUTORIAL_TOPICS;
    g_wldcore_menu_stack_depth = record_index + 1;
}

/* Resume handler of the tutorial topics list: block Circle, Cross and Select from the WORLD menu
 * frame again. */
void wldcore_list_tutorial_topics_restore_blocked_buttons(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void wldcore_list_tutorial_topics_block_all_buttons(wldcore_menu_panel_level_t* level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the tutorial topics list; twin of
 * wldcore_list_handle_treasures_input with help message 0x108d. A confirmed
 * topic's value is passed to wldcore_menu_push_tutorial_topic_text_level. */
void wldcore_list_handle_tutorial_topics_input(wldcore_menu_panel_level_t* level) {
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        if (level->result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_TUTORIAL_TOPICS);
            wldcore_menu_push_tutorial_topic_text_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_TUTORIAL_TOPICS);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((g_wldcore_input_new_button_presses & PSX_PAD_CROSS) && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_tutorial_topics_block_all_buttons(level);
        wldcore_menu_push_message_level(0x108D, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes WLDCORE_MENU_LEVEL_TUTORIAL_TOPIC_TEXT: records the topic on the new
 * level, sets up g_wldcore_scrollable_text_window on the topic's text
 * (TEXT_ID_WORLD_MESSAGE_BASE + topic), and hides the tutorial menu's two
 * windows and frame. wldcore_menu_wait_tutorial_topic_text_then_pop_level
 * steps the window and pops the level, showing all three again.
 *
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one.
 *
 * The menu-stack type store comes last so the
 * compiler scales depth by four late and in place, forcing the copy into v0
 * for depth + 1. */
static void wldcore_menu_push_tutorial_topic_text_level(s32 topic) {
    s32 coordinate_mode = 1;
    s32 depth;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_index = topic;
    g_wldcore_scrollable_text_window.text_id = topic + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 0:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x60;
        break;
    case 1:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x70;
        break;
    }
    g_wldcore_scrollable_text_window.base.x = -122;
    g_wldcore_scrollable_text_window.base.y = -40;
    g_wldcore_scrollable_text_window.text_width = 0xdc;
    g_wldcore_scrollable_text_window.rows_per_page = 8;
    g_wldcore_scrollable_text_window.priority = 9;
    g_wldcore_scrollable_text_window.image_coordinate_mode = coordinate_mode;
    g_wldcore_scrollable_text_window.extra_render_index = -1;
    g_wldcore_scrollable_text_window.text_substitutions[1] = -1;
    g_wldcore_scrollable_text_window.text_substitutions[0] = -1;
    wldcore_text_init_scrollable_window(&g_wldcore_scrollable_text_window);
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.first_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.second_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.render_index].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_types_next[depth] = WLDCORE_MENU_LEVEL_TUTORIAL_TOPIC_TEXT;
}

/* Step of the tutorial topic text level pushed by
 * wldcore_menu_push_tutorial_topic_text_level: once the text window has
 * finished (wldcore_text_is_window_finished), pop the level, show the
 * tutorial menu's windows and frame again and reopen the topic list. */
void wldcore_menu_wait_tutorial_topic_text_then_pop_level(void) {
    s32 depth;

    if (!(g_wldcore_window_render_records[g_wldcore_scrollable_text_window.render_record_index].flags
            & WORLD_RENDER_RECORD_POP_IN)
        && wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.render_index]
            .flags |= WORLD_RENDER_RECORD_POP_IN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.first_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.second_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_menu_stack_depth = depth - 1;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.render_index]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        wldcore_list_open_tutorial_topics();
    }
}

/* Push the Brave Story's job category menu (WLDCORE_MENU_LEVEL_PROPOSITION_CATEGORY).
 *
 * The active-jobs row is listed while any proposition is dispatched and the
 * completed-jobs row once any proposition is completed; each draws
 * WLDCORE_TEXT_ID_PROPOSITION_CATEGORY_ROW_FIRST + row. The Brave Story menu
 * is hidden underneath. The cursor is restored from
 * g_wldcore_proposition_category_saved_cursor (reset when out of range).
 *
 * `value` holds the cursor and then the reloaded main-window index: as one
 * multi-set variable it is left to global allocation, which gives the
 * target's v1/v0 assignment in the tail. `unused` reproduces the 0x58 frame.
 */
static void wldcore_menu_push_proposition_category_level(void) {
    point32_t point;
    RECT bounds;
    s32 unused[4];
    point32_t* base;
    s32 index;
    s32 i;
    s32 completed;
    s32 depth;
    s32 value;

    index = window_append_animated_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.main_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 9;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count = 0;
    completed = g_main_save_proposition_count;
    if (completed != 0) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[0]
            = WLDCORE_PROPOSITION_CATEGORY_ROW_ACTIVE;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
    }
    completed = 0;
    for (i = 0; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_PROPOSITION_FIRST) & WLDCORE_PROPOSITION_VARIABLE_COMPLETED) {
            completed++;
        }
    }
    if (completed != 0) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
            .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
            = WLDCORE_PROPOSITION_CATEGORY_ROW_BRAVE_STORY;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
    }

    index = window_append_record(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.side_window = index;
    bounds.x = 0x80;
    bounds.y = 0x80;
    bounds.w = 0x4C;
    bounds.h = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x78;
    base->y = -0x28;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count; i++) {
        window_display_text_entry(index,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[i]
                + WLDCORE_TEXT_ID_PROPOSITION_CATEGORY_ROW_FIRST,
            point, g_wldcore_window_image_buffer);
        point.y += 0x10;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.main_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.side_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.frame_render].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.upper_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.lower_window].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
    if (g_wldcore_menu_stack_records_next[depth].list_window.entry_count
        < g_wldcore_proposition_category_saved_cursor[0] + 1) {
        g_wldcore_proposition_category_saved_cursor[0] = 0;
    }
    value = g_wldcore_proposition_category_saved_cursor[0];
    g_wldcore_menu_stack_records_next[depth].list_window.selected_entry = value;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.main_window].position.x
        = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_menu_stack_depth = depth + 1;
    value = g_wldcore_menu_stack_records_next[depth].list_window.main_window;
    g_wldcore_window_records[value].position.y = g_wldcore_window_render_records[index].base.y
        - -(g_wldcore_proposition_category_saved_cursor[0] * 0x10) + 0xE;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_CATEGORY;
}
