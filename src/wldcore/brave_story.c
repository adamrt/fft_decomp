/* Brave Story levels, including story events and their text, script variable lists and details, and tutorial topics and
 * their text. */

#include "fft/brave_story.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/message.h"
#include "fft/music.h"
#include "fft/portrait.h"
#include "fft/proposition.h"
#include "fft/save.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u8 g_brave_story_help_text_ids[];
extern u16* g_brave_story_event_ids;

static void brave_story_open_person_list(void);
static void brave_story_open_event_list(void);
static void brave_story_build_rows(void);
static void brave_story_push_propositions_level(void);
static void tutorial_push_topic_level(s32 topic);
static void brave_story_push_person_level(s32 value);

/* Draws the event transition's spinning sprite while that level is on top of
 * the menu stack, unless it is a plain fade. */
void event_transition_draw_sprite(GsOT* ordering_table) {
    s32 current = g_menu_stack_depth;

    if (g_menu_stack_types[current - 1] == MENU_STACK_LEVEL_EVENT_TRANSITION
        && g_menu_stack_records[current - 1].event_transition.mode != EVENT_TRANSITION_MODE_FADE) {
        GsSortSprite(&g_event_transition_sprite, ordering_table, 2);
    }
}

/* The byte color triple of the screen-wide fade overlay. It
 * overlaps g_world_map_fade.boxes[0].r, but the target addresses
 * it separately from the 32-bit flags: reaching it through the fade-state
 * struct makes the flags update use absolute addressing instead of a base
 * register. */
extern CVECTOR g_world_map_fade_color;

/* Per-frame step of the event-transition level (MENU_STACK_LEVEL_EVENT_TRANSITION)
 * pushed by event_transition_push_level: the screen effect that
 * leads from the map into an event or battle.
 *
 * In FADE mode it only waits for the screen fade and then ends the world loop
 * (toggling SYSTEM_FLAG_RUNNING off). Otherwise frame 1 copies the screen
 * to VRAM (0x100, 0) and hides the map and root windows; the sprite drifts a
 * pixel a frame and, every third frame, spins and shrinks in SPIN_IN, then in
 * SPIN mode spins back out (SPIN_OUT) textured with the captured screen. The
 * fade box over g_world_map_fade_color flashes up by 0x10 a frame
 * to 0x80 (FLASH), then whites out by 8 a frame (WHITE_OUT); at full white the
 * world loop ends and the picture at VRAM (0x200, 0x100) is moved to
 * (0x100, 0). */
void event_transition_step_level(menu_stack_record_t* level) {
    RECT rect;
    s32 step;
    s32 shade;
    u8 red_sum;
    s32 angle;

    if (level->event_transition.mode == EVENT_TRANSITION_MODE_FADE) {
        if (!(g_system_flags & SYSTEM_FLAG_FADING)) {
            g_system_flags = g_system_flags ^ SYSTEM_FLAG_RUNNING;
        }
        return;
    }

    /* Capture the screen into VRAM at (0x100, 0) and hide the map and root windows. */
    if (level->event_transition.frame == 1) {
        rect.w = 0xFF;
        rect.x = 0;
        rect.y = 0;
        rect.h = 0xF0;
        MoveImage(&rect, 0x100, 0);
        DrawSync(0);
        g_window_records[g_world_map_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        g_window_records[g_world_map_cursor_window].flags |= DISPLAY_OBJECT_HIDDEN;
    }

    /* White-out: brighten the overlay by 8 a frame; at full white, toggle the running flag
     * and move the image at VRAM (0x200, 0x100) back to (0x100, 0). */
    if (level->event_transition.fade_phase == EVENT_TRANSITION_FADE_WHITE_OUT) {
        red_sum = g_world_map_fade_color.r + 8;
        g_world_map_fade_color.r = red_sum;
        g_world_map_fade_color.g = g_world_map_fade_color.g + 8;
        g_world_map_fade_color.b = g_world_map_fade_color.b + 8;
        if (red_sum >= 0xF8) {
            g_world_map_fade_color.b = 0xFF;
            g_world_map_fade_color.g = 0xFF;
            g_world_map_fade_color.r = 0xFF;
            rect.x = 0x200;
            rect.y = 0x100;
            rect.w = 0x80;
            rect.h = 0x100;
            g_system_flags = g_system_flags ^ SYSTEM_FLAG_RUNNING;
            MoveImage(&rect, 0x100, 0);
            DrawSync(0);
        }
        return;
    }

    /* Flash: brighten by 0x10 a frame to 0x80, then cut to black and start the white-out. */
    if (level->event_transition.fade_phase == EVENT_TRANSITION_FADE_FLASH) {
        g_world_map_fade_color.r = g_world_map_fade_color.r + 0x10;
        g_world_map_fade_color.g = g_world_map_fade_color.g + 0x10;
        g_world_map_fade_color.b = g_world_map_fade_color.b + 0x10;
        if (g_world_map_fade_color.g >= 0x80) {
            g_world_map_fade_color.r = 0;
            g_world_map_fade_color.g = 0;
            g_world_map_fade_color.b = 0;
            level->event_transition.fade_phase = EVENT_TRANSITION_FADE_WHITE_OUT;
        }
    }

    /* The sprite drifts a pixel a frame on each axis; spin, scale and shade step every third frame. */
    switch (level->event_transition.phase) {
    case EVENT_TRANSITION_PHASE_SPIN_IN:
        if (level->event_transition.drift_direction & EVENT_TRANSITION_DRIFT_RIGHT) {
            g_event_transition_sprite.x = g_event_transition_sprite.x + 1;
        } else {
            g_event_transition_sprite.x = g_event_transition_sprite.x - 1;
        }
        if (level->event_transition.drift_direction & EVENT_TRANSITION_DRIFT_DOWN) {
            g_event_transition_sprite.y = g_event_transition_sprite.y + 1;
        } else {
            g_event_transition_sprite.y = g_event_transition_sprite.y - 1;
        }

        if (level->event_transition.frame == ((level->event_transition.frame / 3) * 3)) {
            step = level->event_transition.anim_step + 2;
            level->event_transition.anim_step = step;
            g_event_transition_sprite.scalex = 0x1100 - (step << 7);
            g_event_transition_sprite.scaley = 0x1100 - (level->event_transition.anim_step << 7);
            if (level->event_transition.spin_direction != 0) {
                angle = g_event_transition_sprite.rotate + 0x2000;
            } else {
                angle = g_event_transition_sprite.rotate - 0x2000;
            }
            g_event_transition_sprite.rotate = angle;

            /* Stored as a byte, -0x80 - 4 * step darkens the sprite from 0x80. */
            shade = -0x80 - (level->event_transition.anim_step * 4);
            g_event_transition_sprite.b = shade;
            g_event_transition_sprite.g = shade;
            g_event_transition_sprite.r = shade;

            if (level->event_transition.mode == EVENT_TRANSITION_MODE_SPIN) {
                if (level->event_transition.anim_step == 4) {
                    sound_play_effect(SOUND_SFX_EVENT_TRANSITION_MODE_0);
                }

                /* Spin back out at full size, textured from the 16-bit page holding the capture. */
                if (level->event_transition.anim_step >= 0x20) {
                    level->event_transition.phase = EVENT_TRANSITION_PHASE_SPIN_OUT;
                    g_event_transition_sprite.attribute = 0x52000000;
                    g_event_transition_sprite.tpage = GetTPage(2, 1, 0x100, 0);
                    g_event_transition_sprite.scaley = ONE;
                    g_event_transition_sprite.scalex = ONE;
                }
            } else {
                if (level->event_transition.anim_step == 4) {
                    sound_play_effect(SOUND_SFX_EVENT_TRANSITION_MODE_1);
                }
                if (level->event_transition.anim_step >= 0x20) {
                    level->event_transition.phase = EVENT_TRANSITION_PHASE_DONE;
                }
                if (level->event_transition.anim_step == 0x18) {
                    g_world_map_fade_color.r = 0;
                    g_world_map_fade_color.g = 0;
                    g_world_map_fade_color.b = 0;
                    g_world_map_fade.flags[0] |= WORLD_MAP_FADE_BOX_VISIBLE;
                    level->event_transition.fade_phase = EVENT_TRANSITION_FADE_WHITE_OUT;
                }
            }
        }
        break;

    case EVENT_TRANSITION_PHASE_SPIN_OUT:
        if (level->event_transition.drift_direction & EVENT_TRANSITION_DRIFT_RIGHT) {
            g_event_transition_sprite.x = g_event_transition_sprite.x - 1;
        } else {
            g_event_transition_sprite.x = g_event_transition_sprite.x + 1;
        }
        if (level->event_transition.drift_direction & EVENT_TRANSITION_DRIFT_DOWN) {
            g_event_transition_sprite.y = g_event_transition_sprite.y - 1;
        } else {
            g_event_transition_sprite.y = g_event_transition_sprite.y + 1;
        }

        if (level->event_transition.frame == ((level->event_transition.frame / 3) * 3)) {
            step = level->event_transition.anim_step + 4;
            level->event_transition.anim_step = step;
            g_event_transition_sprite.scalex = 0x1100 - ((0x40 - step) << 7);
            g_event_transition_sprite.scaley = 0x1100 - ((0x40 - level->event_transition.anim_step) << 7);
            g_event_transition_sprite.b = (level->event_transition.anim_step - 0x20) * 4;
            if (level->event_transition.spin_direction != 0) {
                angle = g_event_transition_sprite.rotate - 0x2000;
            } else {
                angle = g_event_transition_sprite.rotate + 0x2000;
            }
            g_event_transition_sprite.rotate = angle;

            if (level->event_transition.anim_step >= 0x38) {
                g_world_map_fade_color.r = 0;
                g_world_map_fade_color.g = 0;
                g_world_map_fade_color.b = 0;
                g_world_map_fade.flags[0] |= WORLD_MAP_FADE_BOX_VISIBLE;
                level->event_transition.fade_phase = EVENT_TRANSITION_FADE_FLASH;
            }
            if (level->event_transition.anim_step >= 0x50) {
                g_event_transition_sprite.r = 0;
                g_event_transition_sprite.g = 0;
                g_event_transition_sprite.b = 0;
                level->event_transition.phase = EVENT_TRANSITION_PHASE_DONE;
            }
        }
        break;
    }

    level->event_transition.frame = level->event_transition.frame + 1;
}

/* Pushes the Brave Story menu (MENU_STACK_LEVEL_BRAVE_STORY).
 *
 * Appends the cursor, the two completion stars and the side label, draws one
 * row per entry (TEXT_ID_BRAVE_STORY_ROW_FIRST + row) into a new render
 * record, draws the injured and casualty counts (each capped at 9999) into a
 * second record with TEXT_ID_INJURED_CASUALTIES, selects the
 * play-time display, clamps the saved cursor row in
 * g_brave_story_saved_cursor to the entry count and positions the windows relative to the list
 * record before committing the level at depth + 1.
 *
 * The unused 48-byte local reproduces the target's frame size. */
void brave_story_push_level(void) {
    RECT bounds;
    point32_t point;
    s32 index;
    s32 value;
    s32 i;
    s32 depth;
    s32 temp;
    u8 unused[0x30];

    g_system_flags = g_system_flags & ~SYSTEM_FLAG_HIDE_CONTEXT_VALUE;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].brave_story.cursor_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[index].priority = 0xA;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].brave_story.land_completion_star = index;
    g_window_records[index].sequence = WINDOW_ANIM_BRAVE_STORY_MARKER;
    g_window_records[index].priority = 0xA;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].brave_story.treasure_completion_star = index;
    g_window_records[index].sequence = WINDOW_ANIM_BRAVE_STORY_MARKER;
    g_window_records[index].priority = 0xA;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].brave_story.label_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_LIST_SIDE_LABEL;
    g_window_records[index].priority = 0xA;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    i = 0;
    brave_story_build_rows();

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].brave_story.rows_image = index;
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = 0x5C;
    bounds.h = g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count * 0x10 + 0x10;
    window_build_render_record_image(index, bounds, 0, g_window_image_buffer);
    g_window_gs_image_records[index].priority = 0xA;
    g_window_gs_image_records[index].flags |= DISPLAY_OBJECT_POP_IN;
    {
        point32_t* base = &g_window_gs_image_records[index].base;
        base->x = -0x78;
        base->y = -0x28;
    }

    point.x = 8;
    point.y = 8;
    for (; i < g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count; i++) {
        window_display_text_entry(index,
            g_menu_stack_records[g_menu_stack_depth].brave_story.entries[i] + TEXT_ID_BRAVE_STORY_ROW_FIRST, point,
            g_window_image_buffer);
        point.y += 0x10;
    }
    window_load_render_record_image(index, g_window_image_buffer);

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].brave_story.casualty_image = index;
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
    window_init_text_record(index, bounds, 0, TEXT_ID_INJURED_CASUALTIES, g_window_image_buffer);
    g_window_gs_image_records[index].priority = 0xA;
    {
        point32_t* base = &g_window_gs_image_records[index].base;
        base->x = 0x1C;
        base->y = -0x28;
    }

    g_hud_value_mode = HUD_VALUE_PLAY_TIME;
    g_hud_value = g_save_play_time_hours;
    g_hud_value_secondary = g_save_play_time_minutes;
    g_hud_value_tertiary = g_save_play_time_seconds;

    depth = g_menu_stack_depth;
    /* temp spans two blocks without crossing a call, so global allocation
     * gives the clamp flag and the land star's y sum the target's v1.
     * A saved cursor past the last entry restarts at row 0. */
    temp = g_menu_stack_records[depth].brave_story.entry_count < g_brave_story_saved_cursor[0] + 1;
    if (temp) {
        g_brave_story_saved_cursor[0] = 0;
    }
    g_menu_stack_records[depth].brave_story.selected_entry = g_brave_story_saved_cursor[0];
    g_menu_stack_records[depth].brave_story.mode = MENU_STACK_WINDOW_MODE_BROWSE;

    /* Place the windows on the list record: the cursor 16 pixels per saved row, and the two
     * stars by adding the row offsets brave_story_build_rows stored in their y. */
    index = g_menu_stack_records[depth].brave_story.rows_image;
    g_window_records[g_menu_stack_records[depth].brave_story.cursor_window].position.x
        = g_window_gs_image_records[index].base.x + 6;
    g_window_records[g_menu_stack_records[depth].brave_story.cursor_window].position.y
        = g_window_gs_image_records[index].base.y - g_brave_story_saved_cursor[0] * -0x10 + 0xE;
    g_window_records[g_menu_stack_records[depth].brave_story.label_window].position.x
        = g_window_gs_image_records[index].base.x + 3;
    g_window_records[g_menu_stack_records[depth].brave_story.label_window].position.y
        = g_window_gs_image_records[index].base.y - 2;
    g_window_records[g_menu_stack_records[depth].brave_story.land_completion_star].position.x
        = g_window_gs_image_records[index].base.x + 0x4C;
    {
        s32 base_y = g_window_gs_image_records[index].base.y;

        temp = g_window_records[g_menu_stack_records[depth].brave_story.land_completion_star].position.y + base_y;
        g_window_records[g_menu_stack_records[depth].brave_story.land_completion_star].position.y = temp;
    }
    g_window_records[g_menu_stack_records[depth].brave_story.treasure_completion_star].position.x
        = g_window_gs_image_records[index].base.x + 0x30;
    g_menu_stack_depth = depth + 1;
    g_window_records[g_menu_stack_records[depth].brave_story.treasure_completion_star].position.y
        += g_window_gs_image_records[index].base.y;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_BRAVE_STORY;
}

/* Build the Brave Story rows of the level brave_story_push_level
 * pushes. The facts row is listed when EVENT_SCRIPT_VAR_FACTS_ENABLED and any
 * fact flag are set; the story events and persons rows always; the
 * propositions row while any job is active or completed; the unexplored
 * lands and treasures rows once one is found; then Leave. The completion stars
 * show (DISPLAY_OBJECT_BLANK cleared) once all 16 lands or
 * the first 31 treasures are found.
 */
static void brave_story_build_rows(void) {
    s32 count;
    s32 i;

    g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count = 0;
    if (script_get_variable(EVENT_SCRIPT_VAR_FACTS_ENABLED) != 0) {
        count = 0;
        for (i = 0; i < EVENT_SCRIPT_VAR_FACT_COUNT; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_FACT_FIRST) != 0) {
                count++;
            }
        }
        if (count != 0) {
            g_menu_stack_records[g_menu_stack_depth].brave_story.entries[0] = BRAVE_STORY_ROW_FACTS;
            g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count = 1;
        }
    }
    g_menu_stack_records[g_menu_stack_depth]
        .brave_story.entries[g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count] = BRAVE_STORY_ROW_EVENTS;
    g_menu_stack_records[g_menu_stack_depth]
        .brave_story.entries[g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count + 1]
        = BRAVE_STORY_ROW_PERSONS;
    g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count += 2;

    count = g_proposition_count;
    for (i = 0; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_PROPOSITION_FIRST) & PROPOSITION_VARIABLE_COMPLETED) {
            count++;
        }
    }
    if (count != 0) {
        g_menu_stack_records[g_menu_stack_depth]
            .brave_story.entries[g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count]
            = BRAVE_STORY_ROW_PROPOSITIONS;
        g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count++;
    }

    count = 0;
    g_window_records[g_menu_stack_records[g_menu_stack_depth].brave_story.land_completion_star].flags
        |= DISPLAY_OBJECT_BLANK;
    for (i = 0; i < EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST) != 0) {
            count++;
        }
    }
    if (count != 0) {
        g_window_records[g_menu_stack_records[g_menu_stack_depth].brave_story.land_completion_star].position.y
            = g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count * 0x10 + 0xE;
        if (count >= 0x10) {
            g_window_records[g_menu_stack_records[g_menu_stack_depth].brave_story.land_completion_star].flags
                &= ~DISPLAY_OBJECT_BLANK;
        }
        g_menu_stack_records[g_menu_stack_depth]
            .brave_story.entries[g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count]
            = BRAVE_STORY_ROW_UNEXPLORED_LANDS;
        g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count++;
    }

    count = 0;
    g_window_records[g_menu_stack_records[g_menu_stack_depth].brave_story.treasure_completion_star].flags
        |= DISPLAY_OBJECT_BLANK;
    for (i = 0; i < EVENT_SCRIPT_VAR_TREASURE_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) != 0) {
            count++;
        }
    }
    if (count != 0) {
        count = 0;
        g_window_records[g_menu_stack_records[g_menu_stack_depth].brave_story.treasure_completion_star].position.y
            = g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count * 0x10 + 0xE;
        for (i = 0; i < 31; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) != 0) {
                count++;
            }
        }
        if (count >= 0x1F) {
            g_window_records[g_menu_stack_records[g_menu_stack_depth].brave_story.treasure_completion_star].flags
                &= ~DISPLAY_OBJECT_BLANK;
        }
        g_menu_stack_records[g_menu_stack_depth]
            .brave_story.entries[g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count]
            = BRAVE_STORY_ROW_TREASURES;
        g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count++;
    }
    g_menu_stack_records[g_menu_stack_depth]
        .brave_story.entries[g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count] = BRAVE_STORY_ROW_LEAVE;
    g_menu_stack_records[g_menu_stack_depth].brave_story.entry_count++;
}

/* Return the Brave Story menu to browsing when a sub-level closes: restore
 * its palettes and the idle cursor, show TEXT_ID_BRAVE_STORY_INTRO
 * again and show (with their pop-in) whichever of its records a sub-level
 * hid. */
void brave_story_resume_level(menu_stack_record_t* level) {
    g_window_records[level->brave_story.cursor_window].palette
        = g_window_records[level->brave_story.label_window].palette
        = g_window_gs_image_records[level->brave_story.rows_image].palette
        = g_window_gs_image_records[level->brave_story.casualty_image].palette = 0;
    g_window_records[level->brave_story.land_completion_star].palette
        = g_window_records[level->brave_story.treasure_completion_star].palette = 0;
    g_window_records[level->brave_story.cursor_window].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[level->brave_story.cursor_window].frame_index
        = g_window_records[level->brave_story.cursor_window].anim_counter = 0;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
    if (g_window_gs_image_records[level->brave_story.rows_image].flags & DISPLAY_OBJECT_HIDDEN) {
        g_window_records[level->brave_story.cursor_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_records[level->brave_story.label_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[level->brave_story.rows_image].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[level->brave_story.rows_image].flags |= DISPLAY_OBJECT_POP_IN;
        g_window_records[level->brave_story.land_completion_star].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_records[level->brave_story.treasure_completion_star].flags &= ~DISPLAY_OBJECT_HIDDEN;
    }
    if (g_window_gs_image_records[level->brave_story.casualty_image].flags & DISPLAY_OBJECT_HIDDEN) {
        g_window_gs_image_records[level->brave_story.casualty_image].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_system_flags &= ~SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        g_window_gs_image_records[level->brave_story.casualty_image].flags |= DISPLAY_OBJECT_POP_IN;
    }
    level->brave_story.mode = MENU_STACK_WINDOW_MODE_BROWSE;
}

/* Gray out the Brave Story menu while a sub-level opens over it: the cursor
 * takes palette 10 and restarts its press animation, the side label 6, the
 * frame 2 and both stars 5. Under a help message (FADE_OR_HELP) the
 * injured/casualties record is grayed too; otherwise any row but Persons hides
 * it and the play-time display (SYSTEM_FLAG_HIDE_CONTEXT_VALUE).
 *
 * The chained store keeps one main-window index for both cleared fields. */
void brave_story_suspend_level(menu_stack_record_t* state) {
    g_window_records[state->brave_story.cursor_window].palette = 10;
    g_window_records[state->brave_story.label_window].palette = 6;
    g_window_gs_image_records[state->brave_story.rows_image].palette = 2;
    g_window_records[state->brave_story.land_completion_star].palette = 5;
    g_window_records[state->brave_story.treasure_completion_star].palette = 5;
    g_window_records[state->brave_story.cursor_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
    g_window_records[state->brave_story.cursor_window].frame_index
        = g_window_records[state->brave_story.cursor_window].anim_counter = 0;
    if (state->brave_story.mode == MENU_STACK_WINDOW_MODE_FADE_OR_HELP) {
        g_window_gs_image_records[state->brave_story.casualty_image].palette = 2;
        return;
    }
    if (state->brave_story.entries[state->brave_story.selected_entry] != BRAVE_STORY_ROW_PERSONS) {
        g_system_flags |= SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        g_window_gs_image_records[state->brave_story.casualty_image].flags |= DISPLAY_OBJECT_HIDDEN;
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
 * cursor after storing it to g_brave_story_saved_cursor, which needs g_brave_story_saved_cursor
 * accessed as an array element (a scalar store does not alias the level field). The wrap-to-bottom step is written `y
 * -= n * -16` to keep y as the first addu operand.
 */
void brave_story_handle_input(menu_stack_record_t* level) {
    u32 flags;
    u32 buttons;

    g_hud_value = g_save_play_time_hours;
    g_hud_value_secondary = g_save_play_time_minutes;
    g_hud_value_tertiary = g_save_play_time_seconds;

    if (g_window_gs_image_records[level->brave_story.casualty_image].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    flags = g_system_flags;
    if (flags & SYSTEM_FLAG_INPUT_BLOCKED) {
        if (level->brave_story.mode == MENU_STACK_WINDOW_MODE_FADE_OR_HELP && !(flags & SYSTEM_FLAG_FADING)) {
            level->brave_story.mode = MENU_STACK_WINDOW_MODE_LEAVE_WAIT;
            g_system_flags = flags | SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
            world_map_clear_vram_and_scratch(1);
        }
        if (level->brave_story.mode != MENU_STACK_WINDOW_MODE_LEAVE_WAIT) {
            return;
        }
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
            return;
        }
        if ((g_system_flags & SYSTEM_FLAG_TITLE_BRAVE_STORY) && g_audio_queue.count != 0) {
            return;
        }

        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        window_set_render_objects_visible(1);
        g_menu_stack_depth--;
        g_system_flags
            &= ~(SYSTEM_FLAG_HIDE_CONTEXT_VALUE | SYSTEM_FLAG_HIDE_CALENDAR_DATE | SYSTEM_FLAG_HIDE_LOCATION_LABEL);
        menu_stack_dispatch_suspend();

        /* Opened from the title screen: go back there instead of to the map. */
        if (g_system_flags & SYSTEM_FLAG_TITLE_BRAVE_STORY) {
            g_system_flags = (g_system_flags ^ (SYSTEM_FLAG_RUNNING | SYSTEM_FLAG_TITLE_BRAVE_STORY))
                | SYSTEM_FLAG_RETURN_TO_TITLE;
            return;
        }
        world_map_fade_start(WORLD_MAP_FADE_REDRAW_MAP, 0x10);
        world_map_push_menu_level();
        return;
    }

    if (level->brave_story.mode == MENU_STACK_WINDOW_MODE_LEAVE_MESSAGE) {
        if (!(g_world_map_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS))) {
            return;
        }
        level->brave_story.mode = MENU_STACK_WINDOW_MODE_FADE_OR_HELP;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
        world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
        if (g_system_flags & SYSTEM_FLAG_TITLE_BRAVE_STORY) {
            audio_queue_add(AUDIO_COMMAND_FADE_OUT, 0x10);
            audio_queue_add(AUDIO_COMMAND_UNLOAD, 0);
        } else {
            audio_queue_start_music(AUDIO_MUSIC(1, MUSIC_WORLD_MAP));
        }
        g_brave_story_saved_cursor[0] = 0;
        return;
    }

    if (g_audio_queue.count != 0) {
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_GOODBYE, 0);
        level->brave_story.mode = MENU_STACK_WINDOW_MODE_LEAVE_MESSAGE;

        /* Release the level's four windows and two render records: six queue entries. */
        g_window_record_count -= 4;
        g_window_render_record_count -= 2;
        g_window_render_object_count -= 6;
        return;
    }

    if (buttons & PSX_PAD_SELECT) {
        level->brave_story.mode = MENU_STACK_WINDOW_MODE_FADE_OR_HELP;
        brave_story_suspend_level(level);
        menu_stack_push_message_level(
            g_brave_story_help_text_ids[level->brave_story.entries[level->brave_story.selected_entry]] | 0x1000, 1);
        return;
    }

    if (buttons & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        g_brave_story_saved_cursor[0] = level->brave_story.selected_entry;
        switch (level->brave_story.entries[level->brave_story.selected_entry]) {
        case BRAVE_STORY_ROW_FACTS:
            message_block_load_if_changed(MESSAGE_BLOCK_FACTS);
            brave_story_suspend_level(level);
            g_window_records[level->brave_story.cursor_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
            brave_story_open_fact_list();
            return;
        case BRAVE_STORY_ROW_EVENTS:
            message_block_load_if_changed(MESSAGE_BLOCK_STORY_EVENTS);
            brave_story_suspend_level(level);
            g_window_records[level->brave_story.cursor_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
            brave_story_open_event_list();
            return;
        case BRAVE_STORY_ROW_PERSONS:
            brave_story_suspend_level(level);
            g_window_records[level->brave_story.cursor_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
            brave_story_open_person_list();
            return;
        case BRAVE_STORY_ROW_PROPOSITIONS:
            message_block_load_if_changed(MESSAGE_BLOCK_BRAVE_STORY_PROPOSITIONS);
            brave_story_suspend_level(level);
            brave_story_push_propositions_level();
            return;
        case BRAVE_STORY_ROW_UNEXPLORED_LANDS:
            message_block_load_if_changed(MESSAGE_BLOCK_UNEXPLORED_LANDS);
            brave_story_suspend_level(level);
            g_window_records[level->brave_story.cursor_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
            brave_story_open_unexplored_land_list();
            return;
        case BRAVE_STORY_ROW_TREASURES:
            message_block_load_if_changed(MESSAGE_BLOCK_TREASURES);
            brave_story_suspend_level(level);
            g_window_records[level->brave_story.cursor_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
            brave_story_open_treasure_list();
            return;
        case BRAVE_STORY_ROW_LEAVE:
            /* The same goodbye and release as the cancel path above. */
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_GOODBYE, 0);
            level->brave_story.mode = MENU_STACK_WINDOW_MODE_LEAVE_MESSAGE;
            g_window_record_count -= 4;
            g_window_render_record_count -= 2;
            g_window_render_object_count -= 6;
            return;
        }
        return;
    }

    if (world_map_update_input_repeat(PSX_PAD_UP) != 0) {
        if (level->brave_story.selected_entry != 0) {
            g_window_records[level->brave_story.cursor_window].position.y -= 0x10;
            level->brave_story.selected_entry = level->brave_story.selected_entry - 1;
        } else {
            g_window_records[level->brave_story.cursor_window].position.y -= (level->brave_story.entry_count - 1) * -16;
            level->brave_story.selected_entry = level->brave_story.entry_count - 1;
        }
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    }

    if (world_map_update_input_repeat(PSX_PAD_DOWN) != 0) {
        if (level->brave_story.selected_entry < level->brave_story.entry_count - 1) {
            g_window_records[level->brave_story.cursor_window].position.y += 0x10;
            level->brave_story.selected_entry = level->brave_story.selected_entry + 1;
        } else {
            g_window_records[level->brave_story.cursor_window].position.y -= (level->brave_story.entry_count - 1) * 16;
            level->brave_story.selected_entry = 0;
        }
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    }
}

/* Opens the Brave Story's Record list: every story event whose script
 * variable (EVENT_SCRIPT_VAR_STORY_EVENT_FIRST + index) is set, by name
 * (TEXT_ID_STORY_EVENT_NAME_BASE + index), up to eight rows at (0x4c, 0x4f),
 * 0x94 wide, and pushes MENU_STACK_LEVEL_BRAVE_STORY_EVENTS. Twin of
 * brave_story_open_treasure_list, but walks the output arrays by pointer. */
static void brave_story_open_event_list(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* flags;
    s32 count;
    s32 i;
    s32 record_index;

    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
    count = 0;
    i = 0;
    flags = g_menu_stack_row_flags;
    values = g_menu_stack_entry_values;
    for (; i < EVENT_SCRIPT_VAR_STORY_EVENT_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_STORY_EVENT_FIRST) != 0) {
            *values = i + TEXT_ID_STORY_EVENT_NAME_BASE;
            *flags = 0;
            flags++;
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
    layout.text_width = 0x94;
    origin.x = 0x4C;
    layout.visible_rows = i;
    origin.y = 0x4F;
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_EVENTS, count, i);
    window_start_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_ot_offset = 9;
    record_index = g_menu_stack_depth;
    g_system_flags |= SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_BRAVE_STORY_EVENTS;
    g_menu_stack_depth = record_index + 1;
}

/* Resume handler of the story events list: block Circle, Cross and Select for the
 * levels underneath and show TEXT_ID_BRAVE_STORY_INTRO again. */
void brave_story_restore_event_list_prompt(menu_stack_record_t* unused_level) {
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void brave_story_block_event_list_buttons(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the story events list; twin of brave_story_handle_person_list_input with help
 * message 0x1055. Cancel and confirm wait for file loading to finish; a
 * confirmed entry's value is passed to brave_story_push_event_level. */
void brave_story_handle_event_list_input(menu_stack_record_t* level) {
    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_menu_stack_ot_offset = 1;
        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        if (level->panel.result != 0) {
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_EVENTS);
            brave_story_push_event_level(
                g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.casualty_image].flags
            |= DISPLAY_OBJECT_POP_IN;
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_EVENTS);
        menu_stack_dispatch_resume();
        return;
    }
    if ((g_world_map_input_new_button_presses & PSX_PAD_CROSS) && g_file_cd_state.state == 0) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        brave_story_block_event_list_buttons(level);
        menu_stack_push_message_level(0x1055, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0
        && g_file_cd_state.state == 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes MENU_STACK_LEVEL_BRAVE_STORY_EVENT for story event `value`: records
 * the entry on the new level, sets up g_text_window on its
 * text (TEXT_ID_WORLD_MESSAGE_BASE + value, from the story events message
 * block) with no substitutions, hides the Brave Story menu's four windows and
 * frame, and says TEXT_ID_BRAVE_STORY_ENTRY_INFO with the event's name.
 *
 * Masked-identical to brave_story_push_fact_level across all 165
 * instructions. The two differ only in the name base substituted
 * (TEXT_ID_FACT_NAME_BASE there), the menu level type and the frame size.
 *
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_position.y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one.
 *
 * The depth is spelled as the global at every use rather than cached in a
 * local: the target re-reads g_menu_stack_depth after each call.
 *
 * The frame is 32 bytes against the twin's 24: the target reserves eight bytes
 * below its two saved registers that it never touches, reproduced by the
 * unused local. */
void brave_story_push_event_level(s32 value) {
    s32 unused[2];
    s32 coordinate_mode = 1;

    g_menu_stack_records[g_menu_stack_depth].story_event_index = value;
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
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.cursor_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.label_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.rows_image].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.land_completion_star].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.treasure_completion_star].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_text_substitution_values[0] = value + TEXT_ID_STORY_EVENT_NAME_BASE;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_BRAVE_STORY_EVENT;
    g_menu_stack_depth = g_menu_stack_depth + 1;
}

/* Input step of a story event's text page. Triangle replays the event: it
 * sets EVENT_SCRIPT_VAR_CURRENT_EVENT, suppresses progress effects and fades
 * out, and once the fade ends the world loop stops with
 * SYSTEM_FLAG_STORY_EVENT_REPLAY, saving the list cursor and the weather
 * variable. When the text window closes, the story event list reopens. */
void brave_story_handle_event_input(menu_stack_record_t* level) {
    s32 depth;
    s32 index;
    s32 value;
    u32 flags;

    if ((g_window_gs_image_records[g_text_window.render_record_index].flags & DISPLAY_OBJECT_POP_IN) != 0) {
        return;
    }

    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
        flags = g_system_flags;
        if ((flags & SYSTEM_FLAG_FADING) == 0) {
            value = level->story_event_index;
            g_brave_story_saved_list_cursor_state = g_brave_story_event_list_cursor;
            g_system_flags = (flags ^ SYSTEM_FLAG_RUNNING) | SYSTEM_FLAG_STORY_EVENT_REPLAY;
            g_brave_story_replay_story_event_index = value;
            g_brave_story_saved_weather_variable = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
            return;
        }
    }

    if ((g_world_map_input_new_button_presses & PSX_PAD_TRIANGLE) != 0) {
        script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 1);
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_brave_story_event_ids[level->story_event_index]);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
        world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
        sound_play_effect(SOUND_SFX_CONFIRM);
        audio_queue_add(AUDIO_COMMAND_FADE_OUT, 0x10);
        return;
    }

    if (text_window_step(&g_text_window) != 0) {
        return;
    }

    sound_play_effect(SOUND_SFX_CANCEL);
    depth = g_menu_stack_depth;
    index = depth - 2;
    g_window_gs_image_records[g_menu_stack_records[index].brave_story.rows_image].flags |= DISPLAY_OBJECT_POP_IN;
    g_window_records[g_menu_stack_records[index].brave_story.cursor_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[index].brave_story.label_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[index].brave_story.rows_image].flags &= ~DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[index].brave_story.land_completion_star].flags &= ~DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth = depth - 1;
    g_window_records[g_menu_stack_records[index].brave_story.treasure_completion_star].flags &= ~DISPLAY_OBJECT_HIDDEN;
    brave_story_open_event_list();
}

/* Opens the Brave Story's Persons list: every person whose profile variable
 * (EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST + person) is set, by the name of that
 * profile (TEXT_ID_PERSON_PROFILE_NAME_BASE + person * 16 + profile), up to
 * eight rows at (0x80, 0x50), 0x60 wide; hides the injured/casualties record
 * and pushes MENU_STACK_LEVEL_BRAVE_STORY_PERSONS. Twin of
 * brave_story_open_work_history. */
static void brave_story_open_person_list(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* flags;
    s32 count;
    s32 i;
    s32 value;
    s32 record_index;

    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
    count = 0;
    i = 0;
    flags = g_menu_stack_row_flags;
    values = g_menu_stack_entry_values;
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
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_PERSONS, count, i);
    window_start_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_ot_offset = 9;
    record_index = g_menu_stack_depth;
    g_system_flags |= SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_window_gs_image_records[g_menu_stack_records[record_index - 1].brave_story.casualty_image].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_BRAVE_STORY_PERSONS;
    g_menu_stack_depth = record_index + 1;
}

/* Resume handler of the persons list: block Circle, Cross and Select for the
 * levels underneath and show TEXT_ID_BRAVE_STORY_INTRO again. */
void brave_story_restore_person_list_prompt(menu_stack_record_t* unused_level) {
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void brave_story_block_person_list_buttons(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the persons list opened by brave_story_open_person_list; twin of
 * brave_story_handle_unexplored_land_list_input with help message 0x1056.
 * Canceling pops the injured/casualties record back in; a confirmed entry's
 * value is passed to brave_story_push_person_level. */
void brave_story_handle_person_list_input(menu_stack_record_t* level) {
    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_menu_stack_ot_offset = 1;
        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        if (level->panel.result != 0) {
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_PERSONS);
            brave_story_push_person_level(
                g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.casualty_image].flags
            |= DISPLAY_OBJECT_POP_IN;
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_PERSONS);
        menu_stack_dispatch_resume();
        return;
    }
    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        brave_story_block_person_list_buttons(level);
        menu_stack_push_message_level(0x1056, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Opens the detail view (MENU_STACK_LEVEL_BRAVE_STORY_PERSON) of a person
 * chosen in brave_story_handle_person_list_input: value is person * 16 + profile.
 * It loads the message block of the person's group of 16 (blocks 6 on),
 * appends a hidden render record for the portrait, hides the Brave Story menu
 * and says TEXT_ID_RAMZA_INFO with his title (person 0) or
 * TEXT_ID_BRAVE_STORY_ENTRY_INFO with the profile's name.
 *
 * The unused 48-byte local reproduces the target's frame size. */
static void brave_story_push_person_level(s32 value) {
    u8 unused[0x30];
    s32 variable;
    s32 index;
    s32 depth;

    variable = value >> 4;
    message_block_load_if_changed(variable / 16 + 6);
    g_menu_stack_records[g_menu_stack_depth].detail.value = value;
    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].detail.render_index = index;
    g_window_gs_image_records[index].flags
        = (g_window_gs_image_records[index].flags & ~DISPLAY_OBJECT_8BIT_CLUT) | DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.cursor_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.label_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.rows_image].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.land_completion_star].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.treasure_completion_star].flags
        |= DISPLAY_OBJECT_HIDDEN;
    if (variable == 0) {
        g_text_substitution_values[0] = (value & 0xF) + TEXT_ID_RAMZA_TITLE_FIRST;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_RAMZA_INFO, 0);
    } else {
        g_text_substitution_values[0] = value + TEXT_ID_PERSON_PROFILE_NAME_BASE;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    }
    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].detail.phase = BRAVE_STORY_DETAIL_PHASE_LOAD;
    g_menu_stack_records[depth].detail.timer = 0;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_BRAVE_STORY_PERSON;
    g_menu_stack_depth = depth + 1;
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
 * The unused RECT reserves the 8 bytes that put the descriptor
 * at sp+0x18, and page_x keeps the target's (column + 0x280) association. */
void brave_story_step_person_level(menu_stack_record_t* level) {
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

    switch (level->detail.phase) {
    case BRAVE_STORY_DETAIL_PHASE_LOAD:
        level->detail.timer = level->detail.timer + 1;
        if (g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
            level->detail.phase = level->detail.phase + 1;
        }
        return;
    case BRAVE_STORY_DETAIL_PHASE_BUILD:
        if (level->detail.timer < 8 || screen_is_dialog_waiting() == 0) {
            level->detail.timer = level->detail.timer + 1;
            return;
        }
        script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, 0xFF);
        entry = level->detail.value;
        g_text_window.image_position.y = 0x70;
        g_text_window.base.x = -0x7A;
        g_text_window.base.y = -0x28;
        g_text_window.text_width = 0xDC;
        g_text_window.rows_per_page = 8;
        g_text_window.priority = 9;
        g_text_window.image_position.x = 0;
        g_text_window.image_coordinate_mode = 1;
        character = entry >> 4;
        g_text_window.text_id = (entry & 0xFF) | TEXT_ID_WORLD_MESSAGE_BASE;
        if (character == 0) {
            g_text_window.text_substitutions[0] = g_text_substitution_values[0];
            g_text_window.text_substitutions[1] = g_brave_story_character_ages[0];
        } else {
            g_text_window.text_substitutions[0] = g_brave_story_character_ages[character];
            g_text_window.text_substitutions[1] = -1;
        }
        g_text_window.extra_render_index = -1;
        text_window_open(&g_text_window);
        /* Rendering the text stores the portrait it names, if any, over the 0xff. */
        value = script_get_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
        render_index = level->detail.render_index;
        if (value != 0xFF) {
            portrait_build_unit_poly(&portrait, value);
            page_x = 0x280;
            g_window_gs_image_records[render_index].clut.x = ((value / 40) << 6) + ((value % 4) * 0x10 + page_x);
            g_window_gs_image_records[render_index].clut.y = ((value % 40) / 4) + 0x1F0;
            g_window_gs_image_records[render_index].tpage = portrait.tpage;
            /* The rectangle at +0x28 and the color triple at +0x30 are written
             * through their own addresses; the rectangle and the base point are
             * then copied as blocks into the text window's extra-record fields. */
            bounds = &g_window_gs_image_records[render_index].bounds;
            bounds->x = portrait.u0;
            bounds->y = portrait.v0;
            bounds->w = portrait.u1 - portrait.u0;
            bounds->h = portrait.v2 - portrait.v0;
            color = &g_window_gs_image_records[render_index].color;
            color->b = color->g = color->r = 0x80;
            g_window_gs_image_records[render_index].anim_counter = 0;
            base = &g_window_gs_image_records[render_index].base;
            base->x = -0x72;
            base->y = -0x22;
            g_window_gs_image_records[render_index].priority = 9;
            g_text_window.extra_render_index = render_index;
            g_text_window.extra_base = *base;
            g_text_window.extra_bounds = g_window_gs_image_records[render_index].bounds;
        } else {
            g_window_gs_image_records[render_index].flags |= DISPLAY_OBJECT_BLANK;
        }
        level->detail.phase = level->detail.phase + 1;
        return;
    case BRAVE_STORY_DETAIL_PHASE_POP_IN:
        if (!(g_window_gs_image_records[g_text_window.render_record_index].flags & DISPLAY_OBJECT_POP_IN)) {
            g_window_gs_image_records[level->detail.render_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
            level->detail.phase = BRAVE_STORY_DETAIL_PHASE_SHOWN;
        }
        return;
    default:
        if (text_window_step(&g_text_window) == 0) {
            depth = g_menu_stack_depth;
            g_window_gs_image_records[g_menu_stack_records[depth - 2].brave_story.rows_image].flags
                |= DISPLAY_OBJECT_POP_IN;
            g_window_records[g_menu_stack_records[depth - 2].brave_story.cursor_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
            g_window_records[g_menu_stack_records[depth - 2].brave_story.label_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
            g_window_gs_image_records[g_menu_stack_records[depth - 2].brave_story.rows_image].flags
                &= ~DISPLAY_OBJECT_HIDDEN;
            g_menu_stack_depth = depth - 1;
            g_window_records[g_menu_stack_records[depth - 2].brave_story.land_completion_star].flags
                &= ~DISPLAY_OBJECT_HIDDEN;
            g_window_render_record_count = g_window_render_record_count - 1;
            g_window_render_object_count = g_window_render_object_count - 1;
            g_window_records[g_menu_stack_records[depth - 2].brave_story.treasure_completion_star].flags
                &= ~DISPLAY_OBJECT_HIDDEN;
            brave_story_open_person_list();
        }
    }
}

#define append_panel_list_entry(count, value)                                                                          \
    do {                                                                                                               \
        g_menu_stack_entry_values[count] = value;                                                                      \
        g_menu_stack_row_flags[count] = 0;                                                                             \
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
    origin.x = 0x54;
    layout.visible_rows = visible;
    origin.y = 0x50;
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_TUTORIAL_TOPICS, count, visible);
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
void tutorial_restore_topic_list_buttons(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void tutorial_block_topic_list_buttons(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the tutorial topics list; twin of
 * brave_story_handle_treasure_list_input with help message 0x108d. A confirmed
 * topic's value is passed to tutorial_push_topic_level. */
void tutorial_handle_topic_list_input(menu_stack_record_t* level) {
    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_menu_stack_ot_offset = 1;
        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        if (level->panel.result != 0) {
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_TUTORIAL_TOPICS);
            tutorial_push_topic_level(
                g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_TUTORIAL_TOPICS);
        menu_stack_dispatch_resume();
        return;
    }
    if ((g_world_map_input_new_button_presses & PSX_PAD_CROSS) && g_file_cd_state.state == 0) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        tutorial_block_topic_list_buttons(level);
        menu_stack_push_message_level(0x108D, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0
        && g_file_cd_state.state == 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes MENU_STACK_LEVEL_TUTORIAL_TOPIC: records the topic on the new
 * level, sets up g_text_window on the topic's text
 * (TEXT_ID_WORLD_MESSAGE_BASE + topic), and hides the tutorial menu's two
 * windows and frame. tutorial_wait_topic_then_pop
 * steps the window and pops the level, showing all three again.
 *
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_position.y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one.
 *
 * The menu-stack type store comes last so the
 * compiler scales depth by four late and in place, forcing the copy into v0
 * for depth + 1. */
static void tutorial_push_topic_level(s32 topic) {
    s32 coordinate_mode = 1;
    s32 depth;

    g_menu_stack_records[g_menu_stack_depth].tutorial_topic_index = topic;
    g_text_window.text_id = topic + TEXT_ID_WORLD_MESSAGE_BASE;
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
    depth = g_menu_stack_depth;
    g_window_records[g_menu_stack_records[depth - 1].tutorial_menu.cursor_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[depth - 1].tutorial_menu.label_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth = depth + 1;
    g_window_gs_image_records[g_menu_stack_records[depth - 1].tutorial_menu.rows_image].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_TUTORIAL_TOPIC;
}

/* Step of the tutorial topic text level pushed by
 * tutorial_push_topic_level: once the text window has
 * closed (text_window_step), pop the level, show the
 * tutorial menu's windows and frame again and reopen the topic list. */
void tutorial_wait_topic_then_pop(menu_stack_record_t* unused_level) {
    s32 depth;

    if (!(g_window_gs_image_records[g_text_window.render_record_index].flags & DISPLAY_OBJECT_POP_IN)
        && text_window_step(&g_text_window) == 0) {
        sound_play_effect(SOUND_SFX_CANCEL);
        depth = g_menu_stack_depth;
        g_window_gs_image_records[g_menu_stack_records[depth - 2].tutorial_menu.rows_image].flags
            |= DISPLAY_OBJECT_POP_IN;
        g_window_records[g_menu_stack_records[depth - 2].tutorial_menu.cursor_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_records[g_menu_stack_records[depth - 2].tutorial_menu.label_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_menu_stack_depth = depth - 1;
        g_window_gs_image_records[g_menu_stack_records[depth - 2].tutorial_menu.rows_image].flags
            &= ~DISPLAY_OBJECT_HIDDEN;
        tutorial_open_topic_list();
    }
}

/* Push the Brave Story's job category menu (MENU_STACK_LEVEL_BRAVE_STORY_PROPOSITIONS).
 *
 * The active-jobs row is listed while any proposition is dispatched and the
 * completed-jobs row once any proposition is completed; each draws
 * TEXT_ID_PROPOSITION_CATEGORY_ROW_FIRST + row. The Brave Story menu
 * is hidden underneath. The cursor is restored from
 * g_brave_story_propositions_saved_cursor (reset when out of range).
 *
 * `value` holds the cursor and then the reloaded main-window index: as one
 * multi-set variable it is left to global allocation, which gives the
 * target's v1/v0 assignment in the tail. `unused` reproduces the 0x58 frame.
 */
static void brave_story_push_propositions_level(void) {
    point32_t point;
    RECT bounds;
    s32 unused[4];
    point32_t* base;
    s32 index;
    s32 i;
    s32 completed;
    s32 depth;
    s32 value;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.cursor_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[index].priority = 9;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
    g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entry_count = 0;
    completed = g_proposition_count;
    if (completed != 0) {
        g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entries[0]
            = BRAVE_STORY_PROPOSITION_ROW_ADVANCING;
        g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entry_count++;
    }
    completed = 0;
    for (i = 0; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_PROPOSITION_FIRST) & PROPOSITION_VARIABLE_COMPLETED) {
            completed++;
        }
    }
    if (completed != 0) {
        g_menu_stack_records[g_menu_stack_depth]
            .brave_story_propositions
            .entries[g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entry_count]
            = BRAVE_STORY_PROPOSITION_ROW_WORK_HISTORY;
        g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entry_count++;
    }

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.rows_image = index;
    bounds.x = 0x80;
    bounds.y = 0x80;
    bounds.w = 0x4C;
    bounds.h = g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entry_count * 0x10 + 0x10;
    window_build_render_record_image(index, bounds, 0, g_window_image_buffer);
    g_window_gs_image_records[index].priority = 9;
    g_window_gs_image_records[index].flags |= DISPLAY_OBJECT_POP_IN;
    base = &g_window_gs_image_records[index].base;
    base->x = -0x78;
    base->y = -0x28;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entry_count; i++) {
        window_display_text_entry(index,
            g_menu_stack_records[g_menu_stack_depth].brave_story_propositions.entries[i]
                + TEXT_ID_PROPOSITION_CATEGORY_ROW_FIRST,
            point, g_window_image_buffer);
        point.y += 0x10;
    }
    window_load_render_record_image(index, g_window_image_buffer);

    depth = g_menu_stack_depth;
    g_window_records[g_menu_stack_records[depth - 1].brave_story.cursor_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[depth - 1].brave_story.label_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[depth - 1].brave_story.rows_image].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[depth - 1].brave_story.land_completion_star].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[depth - 1].brave_story.treasure_completion_star].flags
        |= DISPLAY_OBJECT_HIDDEN;
    if (g_menu_stack_records[depth].brave_story_propositions.entry_count
        < g_brave_story_propositions_saved_cursor[0] + 1) {
        g_brave_story_propositions_saved_cursor[0] = 0;
    }
    value = g_brave_story_propositions_saved_cursor[0];
    g_menu_stack_records[depth].brave_story_propositions.selected_entry = value;
    g_window_records[g_menu_stack_records[depth].brave_story_propositions.cursor_window].position.x
        = g_window_gs_image_records[index].base.x + 6;
    g_menu_stack_depth = depth + 1;
    value = g_menu_stack_records[depth].brave_story_propositions.cursor_window;
    g_window_records[value].position.y
        = g_window_gs_image_records[index].base.y - -(g_brave_story_propositions_saved_cursor[0] * 0x10) + 0xE;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_BRAVE_STORY_PROPOSITIONS;
}
