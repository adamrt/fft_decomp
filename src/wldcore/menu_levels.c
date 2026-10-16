/* Value, cursor, number and date drawing, the world frame, level dispatch, and the root, map cursor, focus, path,
 * visibility, entry, text, message, variable list, number panel, layout window and location menu levels. */

#include "fft/bar.h"
#include "fft/calendar.h"
#include "fft/encounter.h"
#include "fft/file.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/input.h"
#include "fft/location.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_selection.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/menu_window.h"
#include "fft/message.h"
#include "fft/message_box.h"
#include "fft/music.h"
#include "fft/proposition.h"
#include "fft/save.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/shop.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_panel.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

static void bar_push_level(void);
static void hud_draw_funds(GsOT* ot, GsSPRITE* sprite, GsLINE* line);
static void hud_draw_digits(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot);
static void hud_draw_digits_with_fill(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot, s32 zero_pad);
static void menu_stack_pop_and_release_records(menu_stack_slot_set_t* slots, s32 restore);
static void debug_push_party_number_level(void);
static void world_map_push_focus_level(s32 dot_index);
static void world_map_push_path_level(s32 from, s32 to, s32 erase);
static void debug_push_variable_edit_level(s32 variable_id, s32 value);
static void world_map_push_cursor_level(void);
static void debug_push_variable_list_level(void);
static void debug_draw_variable_list(debug_variable_list_level_t* level);
static void location_run_shop(s32 shop_id);
static void world_map_push_location_visibility_level(s32 dot_index, s32 was_set);
static void unit_panel_move_help_cursor(unit_panel_help_level_t* request);

/* Draws the HUD numeric readout at the context-value display position, laid
 * out by g_hud_value_mode: FUNDS is the labeled,
 * underlined war funds, PLAY_TIME hours, minutes and seconds with separators,
 * PLAIN and LABELED an eight-digit value (LABELED under a small label sprite)
 * and COST_AND_FUNDS a labeled five-digit secondary value plus the FUNDS
 * readout.
 *
 * LABELED falls through into PLAIN, so the case bodies are ordered FUNDS,
 * PLAY_TIME, LABELED, PLAIN, COST_AND_FUNDS to follow the target's jump
 * table. The color triple stores into
 * `separator` precede those into `sprite`. The position read back at the end
 * of case 4 is a plain 32-bit load narrowed by the store, where the chained
 * assignment before the switch reads only the low 16 bits. */
void hud_draw_value(GsOT* ot) {
    GsSPRITE sprite;
    GsSPRITE separator;
    GsLINE line;
    CVECTOR color;

    if (g_system_flags & SYSTEM_FLAG_HIDE_CONTEXT_VALUE) {
        return;
    }

    separator.attribute = sprite.attribute = 0;
    separator.tpage = sprite.tpage = GetTPage(0, 0, 0x3C0, 0x100);
    separator.cy = sprite.cy = 0x1E1;
    world_map_fade_tint_color(&g_hud_color, &color);
    separator.r = color.r;
    separator.g = color.g;
    separator.b = color.b;
    sprite.r = color.r;
    sprite.g = color.g;
    sprite.b = color.b;
    separator.x = sprite.x = g_hud_value_position.x;
    separator.y = sprite.y = g_hud_value_position.y;

    switch (g_hud_value_mode) {
    case HUD_VALUE_FUNDS:
        hud_draw_funds(ot, &sprite, &line);
        break;
    case HUD_VALUE_PLAY_TIME:
        sprite.u = 0x60;
        sprite.v = 0x10;
        sprite.w = 0x14;
        sprite.h = 8;
        sprite.cx = 0x20;
        GsSortFastSprite(&sprite, ot, g_hud_ot_priority);

        separator.u = 0x28;
        separator.v = 0;
        separator.w = 6;
        separator.h = 8;
        separator.cx = 0x20;
        sprite.y += 8;
        separator.y = sprite.y + 4;
        sprite.x -= 8;
        hud_draw_digits(g_hud_value, 4, &sprite, ot);

        separator.x = sprite.x;
        GsSortFastSprite(&separator, ot, g_hud_ot_priority);
        sprite.x += 6;
        hud_draw_digits_with_fill(g_hud_value_secondary, 2, &sprite, ot, 1);

        separator.x = sprite.x;
        GsSortFastSprite(&separator, ot, g_hud_ot_priority);
        sprite.x += 6;
        hud_draw_digits_with_fill(g_hud_value_tertiary, 2, &sprite, ot, 1);
        break;
    case HUD_VALUE_LABELED:
        sprite.u = 0xA8;
        sprite.v = 0x20;
        sprite.w = 0xD;
        sprite.h = 9;
        sprite.cx = 0x20;
        GsSortFastSprite(&sprite, ot, g_hud_ot_priority);
        /* fallthrough */
    case HUD_VALUE_PLAIN:
        sprite.x -= 0xC;
        sprite.y += 8;
        hud_draw_digits(g_hud_value, 8, &sprite, ot);
        break;
    case HUD_VALUE_COST_AND_FUNDS:
        sprite.u = 0x60;
        sprite.w = 0x18;
        sprite.h = 8;
        sprite.v = 0;
        sprite.cx = 0x20;
        sprite.x -= 0x40;
        GsSortFastSprite(&sprite, ot, g_hud_ot_priority);

        sprite.x += 4;
        sprite.y += 8;
        hud_draw_digits_with_fill(g_hud_value_secondary, 5, &sprite, ot, 0);

        sprite.x = g_hud_value_position.x;
        sprite.y = g_hud_value_position.y;
        hud_draw_funds(ot, &sprite, &line);
        break;
    }
}

/* Draws the FUNDS readout: the 41x8 label sprite at texel (0x9c, 0x78) with a
 * short black line under its right end, then g_hud_value
 * in eight columns offset by (4, 8), through
 * hud_draw_digits_with_fill. */
static void hud_draw_funds(GsOT* ot, GsSPRITE* sprite, GsLINE* line) {
    /* u16: the target reads GsSPRITE's short x/y with lhu. */
    u16 x;
    u16 y;

    sprite->u = 0x9C;
    sprite->v = 0x78;
    sprite->w = 0x29;
    sprite->h = 8;
    sprite->cx = 0x20;
    GsSortFastSprite(sprite, ot, g_hud_ot_priority);

    line->attribute = 0;
    line->r = 0;
    line->g = 0;
    line->b = 0;
    x = sprite->x;
    line->x0 = x + 0x24;
    line->x1 = x + 0x28;
    y = sprite->y + 8;
    line->y1 = y;
    line->y0 = y;
    GsSortLine(line, ot, g_hud_ot_priority);

    sprite->x += 4;
    sprite->y += 8;
    hud_draw_digits_with_fill(g_hud_value, 8, sprite, ot, 0);
}

/* Draws value right-aligned as digits glyphs, one 8x12 or 8x15 cell per
 * digit, stepping the sprite eight pixels right per column. Leading zeros are
 * skipped: a column only draws once the running divisor has dropped to or
 * below value, and the last column always draws. Digit d's glyph is at
 * u 0xa8 + 8 * d; 0 to 5 are 12 pixels tall and 6 to 9 are 15, each placed so
 * the glyphs share a bottom edge.
 *
 * The target wraps `value / divisor` in ASPSX's zero and INT_MIN traps
 * (divcheck expansion). Splitting the division and the remainder into two
 * statements gives the target's order; `s32 saved_y` (not s16) makes the
 * saved y load `lh`. */
static void hud_draw_digits(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot) {
    s32 divisor = 1;
    s32 i;
    s32 digit;
    s32 saved_y;

    sprite->w = 8;
    sprite->v = 0x31;
    saved_y = sprite->y;
    sprite->cx = 0x70;

    for (i = 0; i < digits - 1; i++) {
        divisor = divisor * 10;
    }

    for (i = 0; i < digits;) {
        if (value >= divisor || i == digits - 1) {
            digit = value / divisor;
            digit = digit % 10;
            sprite->u = digit * 8 - 0x58;
            if (digit < 6) {
                sprite->v = 0x34;
                sprite->h = 0xc;
                sprite->y = sprite->y + 4;
            } else {
                sprite->v = 0x31;
                sprite->h = 0xf;
                sprite->y = sprite->y + 1;
            }
            GsSortFastSprite(sprite, ot, g_hud_ot_priority);
            sprite->y = saved_y;
        }
        sprite->x = sprite->x + 8;
        divisor = divisor / 10;
        i++;
    }
}

/* Draws value right-aligned in `digits` columns like
 * hud_draw_digits, but also draws the columns ahead of the
 * first significant digit: a zero glyph when `zero_pad` is set, otherwise the
 * 8x8 filler glyph at (0xd0, 0x28). */
static void hud_draw_digits_with_fill(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot, s32 zero_pad) {
    s32 divisor = 1;
    s32 y;
    s32 digit;
    s32 i = 0;

    sprite->w = 8;
    y = sprite->y;
    sprite->h = 0x10;
    for (; i < digits - 1; i++) {
        divisor *= 10;
    }

    for (i = 0; i < digits; i++) {
        if (value >= divisor || i == digits - 1) {
            digit = value / divisor;
            digit = digit % 10;
            sprite->cx = 0x70;
            sprite->u = digit * 8 - 0x58;
            if (digit < 6) {
                sprite->v = 0x34;
                sprite->h = 0xC;
                sprite->y += 4;
            } else {
                sprite->v = 0x31;
                sprite->h = 0xF;
                sprite->y += 1;
            }
            GsSortFastSprite(sprite, ot, g_hud_ot_priority);
            sprite->y = y;
        } else {
            if (zero_pad != 0) {
                sprite->u = 0xA8;
                sprite->cx = 0x70;
                sprite->v = 0x31;
                sprite->h = 0xF;
                sprite->y += 1;
            } else {
                sprite->u = 0xD0;
                sprite->v = 0x28;
                sprite->cx = 0x70;
                sprite->h = 8;
                sprite->y += 4;
            }
            GsSortFastSprite(sprite, ot, g_hud_ot_priority);
            sprite->y = y;
        }
        sprite->x += 8;
        divisor /= 10;
    }
}

/* Draws the world map's date: the month's zodiac sequence for the month
 * script variable, then the day as one or two digit sequences 0x1A pixels to
 * its right, in the tinted HUD color. Nothing is drawn while
 * SYSTEM_FLAG_HIDE_CALENDAR_DATE is set. */
void calendar_draw_date(GsOT* ot) {
    display_object_animated_t request;
    CVECTOR color;
    s32 first;
    s32 second;

    if (g_system_flags & SYSTEM_FLAG_HIDE_CALENDAR_DATE) {
        return;
    }

    request.flags = DISPLAY_OBJECT_ANIMATED;
    request.priority = 4;
    request.palette = 0;
    request.anim_counter = 0;
    request.frame_index = 0;
    request.position.x = g_calendar_display_x;
    request.position.y = g_calendar_display_y;
    world_map_fade_tint_color(&g_hud_color, &color);
    request.color.r = color.r;
    request.color.g = color.g;
    request.color.b = color.b;

    first = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    second = script_get_variable(EVENT_SCRIPT_VAR_DAY);
    request.sequence = first + (WINDOW_ANIM_MONTH_FIRST - 1);
    window_draw_animated_record(&request, ot);

    request.position.x += 0x1A;
    request.position.y -= 4;
    if (second >= 10) {
        request.anim_counter = 0;
        request.frame_index = 0;
        request.sequence = second / 10 + WINDOW_ANIM_DIGIT_FIRST;
        window_draw_animated_record(&request, ot);
        request.position.x += 8;
    }
    request.sequence = second % 10 + WINDOW_ANIM_DIGIT_FIRST;
    window_draw_animated_record(&request, ot);
}

/* Draws the name label of the selected location (g_location_selected,
 * location + 1) over its map dot, unless SYSTEM_FLAG_HIDE_LOCATION_LABEL
 * is set or no location is selected.
 *
 * The color is g_location_label_shade replicated across the three
 * channels and tinted in place by world_map_fade_tint_color. sequence
 * is written after that call because the target reloads the selection
 * there. */
void location_draw_label(GsOT* ot) {
    display_object_animated_t request;
    CVECTOR color;
    s32 index;
    s32 shade;

    if (g_system_flags & SYSTEM_FLAG_HIDE_LOCATION_LABEL) {
        return;
    }
    if (g_location_selected == 0) {
        return;
    }

    index = g_location_selected - 1;
    request.flags = DISPLAY_OBJECT_ANIMATED;
    request.position.x = g_world_map_dots[index].render.position.x;
    request.position.y = g_world_map_dots[index].render.position.y;
    shade = g_location_label_shade;
    request.priority = 4;
    request.palette = 0;
    request.anim_counter = 0;
    request.frame_index = 0;
    color.r = shade;
    color.g = shade;
    color.b = shade;
    world_map_fade_tint_color(&color, &color);
    request.color.r = color.r;
    request.color.g = color.g;
    request.color.b = color.b;
    request.sequence = g_location_selected + (WINDOW_ANIM_LOCATION_NAME_FIRST - 1);
    window_draw_animated_record(&request, ot);
}

/* Init step: load WORLD's menu text and unit portraits, build the formation unit name strings,
 * then apply and clear any day advance pending in EVENT_SCRIPT_VAR_DATE_ADVANCE. */
void world_map_init_menus_and_date(void) {
    s32 value;

    screen_init_with_world_text_and_faces();
    formation_build_roster_names();
    value = script_get_variable(EVENT_SCRIPT_VAR_DATE_ADVANCE);
    if (value != 0) {
        calendar_advance_days(value);
        script_set_variable(EVENT_SCRIPT_VAR_DATE_ADVANCE, 0);
    }
}

/*
 * Runs the WORLD menu code for one world-map frame, drawing into `context`.
 *
 * Skipped while SYSTEM_FLAG_SKIP_MENU_FRAME is set. While the unit panel
 * threads are in use it draws the window frames, runs the event input update
 * and plays the frame's sound effect itself; otherwise it runs the formation
 * menu frame. Buttons in g_menu_blocked_buttons are withheld from
 * the menus.
 */
void world_map_run_world_menus(GsOT* context) {
    u32* otag_entries;
    u32* sprite_entries;
    u32* menu_entries;

    if (g_system_flags & SYSTEM_FLAG_SKIP_MENU_FRAME) {
        return;
    }
    formation_set_backdrop_otag((u32*)(context->org + 15));
    screen_set_allowed_buttons(~g_menu_blocked_buttons);
    if (g_system_flags & SYSTEM_FLAG_UNIT_PANEL_THREADS) {
        otag_entries = (u32*)context->org;
        g_gfx_current_otag_entry = otag_entries + g_menu_stack_ot_offset;
        menu_draw_active_window_frames();
        sprite_entries = (u32*)context->org;
        menu_input_update_event_state(
            sprite_entries + g_menu_stack_ot_offset + 2, g_input_current_buttons, g_gfx_active_buffer_index * 240);
        if (g_sound_pending_sfx_id != -1) {
            sound_play_effect(g_sound_pending_sfx_id);
        }
    } else {
        menu_entries = (u32*)context->org;
        screen_menu_frame(menu_entries + g_menu_stack_ot_offset, g_input_current_buttons);
    }
}

/* Empty the menu stack, push the root level and place its window at the
 * projected location marker (4 pixels above it). */
void menu_stack_reset(void) {
    g_menu_stack_depth = 0;
    world_map_push_cursor_level();
    g_window_records[g_menu_stack_records[1].window_index].position.x
        = g_world_map_projection_state.marker.render.position.x;
    g_window_records[g_menu_stack_records[1].window_index].position.y
        = g_world_map_projection_state.marker.render.position.y - 4;
}

/* Run the per-frame handler of the menu level on top of the stack, passing its level record. */
void menu_stack_dispatch_idle(void) {
    s32 index = g_menu_stack_depth;

    g_menu_stack_idle_handlers[g_menu_stack_types[index]](&g_menu_stack_records[index]);
}

/* Run the resume handler of the level now on top of the stack after the level above it closed:
 * list levels restore their prompt and blocked buttons, window levels their palettes and cursor. */
void menu_stack_dispatch_resume(void) {
    s32 index = g_menu_stack_depth;

    g_menu_stack_resume_handlers[g_menu_stack_types[index]](&g_menu_stack_records[index]);
}

/* Run the top level's suspend handler, called just before another level opens over it: list levels
 * withhold every button, window levels dim their windows (palettes 10, 6, 2 and the pressed
 * cursor). The resume handler undoes it. */
void menu_stack_dispatch_suspend(void) {
    s32 index = g_menu_stack_depth;

    g_menu_stack_suspend_handlers[g_menu_stack_types[index]](&g_menu_stack_records[index]);
}

/* Pushes a two-window menu level: appends two window records, points the new
 * menu-stack record at both, resets both scroll states and records the
 * current location (+1) for the level.
 *
 * The level record is re-read into its own local after the script call; reusing
 * the window-level depth local changes the register allocation. */
static void world_map_push_cursor_level(void) {
    window_xy_t* xy;
    display_object_animated_t* records;
    s32 window_index;
    s32 depth;
    s32 level;

    window_index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);

    /* Taken from the record array: naming g_window_record_positions here changes the
     * target's address form. */
    xy = (window_xy_t*)&g_window_records[0].position.x;
    records = g_window_records;

    depth = g_menu_stack_depth;
    g_menu_stack_records_next[depth].window_pair.first_window = window_index;
    g_window_records[window_index].sequence = WINDOW_ANIM_CURSOR_STILL;
    g_window_records[window_index].priority = 12;
    xy[window_index].x = 0;
    xy[window_index].y = 0;
    records[window_index].anim_counter = 0;
    g_window_records[window_index].frame_index = 0;

    window_index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    depth = g_menu_stack_depth;
    g_hud_value_mode = HUD_VALUE_FUNDS;
    g_menu_stack_records_next[depth].window_pair.second_window = window_index;
    g_window_records[window_index].sequence = WINDOW_ANIM_MAP_EDGE_ARROWS;
    g_window_records[window_index].priority = 12;
    g_window_records[window_index].flags |= DISPLAY_OBJECT_HIDDEN;
    xy[window_index].x = 0;
    xy[window_index].y = 0;
    records[window_index].anim_counter = 0;
    g_window_records[window_index].frame_index = 0;

    g_world_map_scroll_y.limit = 0x400;
    g_world_map_scroll_x.limit = 0x400;
    g_world_map_scroll_y.value = 0;
    g_world_map_scroll_x.value = 0;
    g_world_map_scroll_y.output = 0;
    g_world_map_scroll_x.output = 0;
    g_world_map_scroll_y.decay = 0x80;
    g_world_map_scroll_x.decay = 0x80;
    g_world_map_scroll_y.step = 0x80;
    g_world_map_scroll_x.step = 0x80;

    g_hud_value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    level = g_menu_stack_depth;
    g_menu_stack_records_next[level].map_cursor.wide_view = 0;
    g_menu_stack_records_next[level].map_cursor.snap_hit = g_world_map_projection_state.marker.kind + 1;
    g_menu_stack_records_next[level].map_cursor.pending_mode = 0;
    g_menu_stack_depth = level + 1;
    g_menu_stack_types[level + 1] = MENU_STACK_LEVEL_WORLD_MAP;
}

/* Resume handler of the world map's root level: show the war funds in the
 * context value display, reset the level's window palette, restore the live
 * map if the captured frame is showing and run any pending script requests. */
void world_map_resume_cursor_level(menu_stack_record_t* level) {
    g_hud_value_mode = HUD_VALUE_FUNDS;
    g_window_records[level->map_cursor.window_index].palette = 0;
    g_hud_value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    if (g_window_records[g_world_map_window_index].flags & DISPLAY_OBJECT_HIDDEN) {
        world_map_toggle_captured_frame(0);
    }
    location_script_dispatch_requests();
}

/* Reset the window palette and both map-cursor scroll axes. The scroll fields
 * preserve the original store order after the indexed window store. */
void world_map_suspend_cursor_level(menu_stack_record_t* level) {
    g_window_records[level->window_index].palette = 10;
    g_world_map_scroll_y.value = 0;
    g_world_map_scroll_x.value = 0;
    g_world_map_scroll_y.output = 0;
    g_world_map_scroll_x.output = 0;
}

/* Once the current location's script yields a map-menu action, reset the
 * current level's window and dispatch its path, location-visibility or focus
 * request. A focus location of 0xff means the current location.
 *
 * The flag tests are signed `> 0` comparisons; `!= 0` folds to srl/andi. */
void location_script_dispatch_requests(void) {
    s32* location = &g_world_map_projection_state.marker.kind;
    s32* flags;
    s32 id;

    if (location_script_run(*location, LOCATION_SCRIPT_ACTION_MAP_MENU_MASK) == 0) {
        return;
    }
    flags = &g_location_script_state.flags;
    if (*flags & LOCATION_SCRIPT_ACTION_PATH_MASK) {
        world_map_suspend_cursor_level(&g_menu_stack_records[g_menu_stack_depth]);
        world_map_push_path_level(g_location_script_state.args[0], g_location_script_state.args[1],
            (*flags & LOCATION_SCRIPT_ACTION_ERASE_PATH) > 0);
    } else if (*flags & LOCATION_SCRIPT_ACTION_LOCATION_VISIBILITY_MASK) {
        world_map_suspend_cursor_level(&g_menu_stack_records[g_menu_stack_depth]);
        world_map_push_location_visibility_level(
            g_location_script_state.args[0], (*flags & LOCATION_SCRIPT_ACTION_ERASE_LOCATION) > 0);
    } else if (*flags & LOCATION_SCRIPT_ACTION_FOCUS_LOCATION) {
        world_map_suspend_cursor_level(&g_menu_stack_records[g_menu_stack_depth]);
        id = g_location_script_state.args[0];
        if (id == 0xFF) {
            id = *location;
        }
        world_map_push_focus_level(id);
    }
}

/* Per-frame step of the world map cursor (the root level).
 *
 * While route travel runs it only keeps the map redrawn. While a zoom runs
 * (WORLD_MAP_PAN_ACTIVE) it interpolates the view origin and the
 * projection scale over `duration` frames, out (ZOOMED_OUT) or back in, and
 * redraws the map each frame.
 *
 * Otherwise it handles input: Circle on the marker's own dot opens its
 * location menu, on another dot walks the marker there
 * (travel_start), and off every dot hides the cursor and
 * pans back to the marker (world_map_push_focus_level); Triangle
 * or Start opens the entry window (world_map_push_menu_level);
 * Select on a dot shows the location's help message (0x8000 + location) over
 * a captured frame; L1 or R1 starts or reverses the zoom; holding Square
 * swaps the cursor window for the wide-view window. With no modal state
 * it accelerates the two scroll axes, snaps the cursor toward the nearest dot
 * through world_map_get_dot_snap_step, scrolls the projection origin when
 * the cursor approaches a screen edge, and finally clamps the cursor window to
 * x in [-0x70, 0x78] and y in [-0x68, 0x68].
 *
 * The zoom interpolation uses four signed divisions by `duration`
 * (gcc-2.7.2_O2_aspsx-2.21_divcheck). `point` is never written; it
 * reproduces the target's unused 8-byte slot at sp+0x18. The chained scale
 * stores keep each branch's scale value a single-set temporary; one variable
 * shared by both branches is multi-set, is scheduled early and changes the
 * register allocation of the interpolation. */
void world_map_step_cursor_level(menu_stack_record_t* level) {
    point32_t amount;
    point32_t point;
    point32_t target;
    VECTOR saved;
    s32 step_x;
    s32 step_y;
    s32 flags;
    s32 motion;
    s32 buttons;
    s32 held;
    s32 hit;
    s32 frame;
    s32 duration;

    /* The exits funnel into the shared snap-hit tail at `finish`; in the
     * target's block order these need gotos. */
    if (g_travel.flags & TRAVEL_ACTIVE) {
        g_system_flags |= SYSTEM_FLAG_DRAW_MAP;
        goto finish;
    }

    if (level->map_cursor.wide_view == 0 && !(g_world_map_previous_system_flags & SYSTEM_FLAG_DRAW_MAP)) {
        if (level->map_cursor.pending_mode != 0) {
            flags = g_system_flags;
            if (flags & SYSTEM_FLAG_FADING) {
                return;
            }
            g_system_flags = flags ^ SYSTEM_FLAG_INPUT_BLOCKED;
            if (level->map_cursor.pending_mode == 3) {
                g_system_flags = flags ^ (SYSTEM_FLAG_RUNNING | SYSTEM_FLAG_INPUT_BLOCKED);
            }
            level->map_cursor.pending_mode = 0;
        }
        if (g_system_flags & SYSTEM_FLAG_FADING) {
            /* Skips the zoom handling and the wide-view toggle below. */
            goto scroll;
        }
        if (!(g_world_map_zoom.flags & (WORLD_MAP_PAN_ACTIVE | WORLD_MAP_PAN_ZOOMED_OUT)) && g_audio_queue.count == 0) {
            if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
                /* The hit test returns a one-based dot index, 0 for no dot. */
                hit = world_map_find_dot_at(g_window_records[level->map_cursor.window_index].position);
                if (hit != 0) {
                    sound_play_effect(SOUND_SFX_CONFIRM);
                    hit--;

                    /* A dot other than the marker's walks the marker there; the marker's own dot opens its menu. */
                    if (hit != g_world_map_projection_state.marker.kind) {
                        travel_start(g_world_map_projection_state.marker.kind, hit);
                        return;
                    }
                    g_menu_stack_records_next[g_menu_stack_depth].panel.entry_count
                        = location_build_menu_entries(hit, g_menu_stack_records_next[g_menu_stack_depth].panel.entries);
                    if (g_menu_stack_records_next[g_menu_stack_depth].panel.entry_count != 0) {
                        world_map_suspend_cursor_level(level);
                        location_push_menu_level(hit);
                        return;
                    }
                } else {
                    g_window_records[level->map_cursor.window_index].flags |= DISPLAY_OBJECT_HIDDEN;
                    world_map_push_focus_level(g_world_map_projection_state.marker.kind);
                    return;
                }
            }

            buttons = g_world_map_input_new_button_presses;
            if (buttons & (PSX_PAD_TRIANGLE | PSX_PAD_START)) {
                sound_play_effect(SOUND_SFX_CONFIRM);
                world_map_suspend_cursor_level(level);
                world_map_push_menu_level();
                return;
            }
            if (buttons & PSX_PAD_SELECT) {
                hit = world_map_find_dot_at(g_window_records[level->map_cursor.window_index].position);
                if (hit != 0) {
                    hit--;
                    world_map_suspend_cursor_level(level);
                    world_map_toggle_captured_frame(1);
                    menu_stack_push_message_level(hit + 0x8000, 1);
                    return;
                }
            }
        }
    }

    /* L1 or R1 starts a zoom; while one runs, interpolate it and skip the cursor. */
    motion = g_world_map_zoom.flags;
    if (!(motion & WORLD_MAP_PAN_ACTIVE)) {
        if (level->map_cursor.wide_view == 0 && (g_world_map_input_new_button_presses & (PSX_PAD_L1 | PSX_PAD_R1))) {
            g_window_records[level->map_cursor.window_index].flags |= DISPLAY_OBJECT_HIDDEN;
            g_world_map_zoom.duration = 0x20;
            g_world_map_zoom.frame = 0;
            g_world_map_zoom.flags = (g_world_map_zoom.flags ^ WORLD_MAP_PAN_ZOOMED_OUT) | WORLD_MAP_PAN_ACTIVE;

            /* Zooming out remembers the view origin so zooming back in can return to it. */
            if (g_world_map_zoom.flags & WORLD_MAP_PAN_ZOOMED_OUT) {
                g_world_map_zoom.origin = g_world_map_projection_view.origin;
            }
            level->map_cursor.snap_hit = 0;
            if (g_world_map_zoom.flags & WORLD_MAP_PAN_ZOOMED_OUT) {
                sound_play_effect(SOUND_SFX_WORLD_MAP_ZOOM_OUT);
            } else {
                sound_play_effect(SOUND_SFX_WORLD_MAP_ZOOM_IN);
            }
            goto finish;
        }
    } else {
        /* The scale moves between ONE and 0x800 (half size); the zoomed-out view is centered at (0, 8). */
        frame = g_world_map_zoom.frame;
        duration = g_world_map_zoom.duration;
        step_x = (frame << 11) / duration;
        if (motion & WORLD_MAP_PAN_ZOOMED_OUT) {
            g_world_map_projection_view.origin.vx
                = g_world_map_zoom.origin.vx + ((0 - g_world_map_zoom.origin.vx) * frame) / duration;
            g_world_map_projection_view.origin.vy
                = g_world_map_zoom.origin.vy + ((8 - g_world_map_zoom.origin.vy) * frame) / duration;
            g_world_map_projection_view.scale.vx = g_world_map_projection_view.scale.vy = ONE - step_x;
            g_world_map_zoom.frame = frame + 1;
            g_system_flags |= SYSTEM_FLAG_HIDE_MAP_DOTS | SYSTEM_FLAG_HIDE_MAP_ROUTES;
            if (frame + 1 == duration) {
                g_world_map_projection_view.scale.vy = 0x800;
                g_world_map_projection_view.scale.vx = 0x800;
                g_world_map_projection_view.origin.vx = 0;
                g_world_map_projection_view.origin.vy = 8;
                g_world_map_zoom.flags = motion ^ WORLD_MAP_PAN_ACTIVE;
            }
        } else {
            g_world_map_projection_view.origin.vx = (g_world_map_zoom.origin.vx * frame) / duration;
            g_world_map_projection_view.origin.vy = ((g_world_map_zoom.origin.vy - 8) * frame) / duration + 8;
            g_world_map_projection_view.scale.vx = g_world_map_projection_view.scale.vy = step_x + 0x800;
            g_world_map_zoom.frame = frame + 1;
            if (frame + 1 == duration) {
                g_world_map_projection_view.scale.vy = ONE;
                g_world_map_projection_view.scale.vx = ONE;
                g_world_map_projection_view.origin = g_world_map_zoom.origin;
                g_world_map_zoom.flags ^= WORLD_MAP_PAN_ACTIVE;
                g_system_flags &= ~(SYSTEM_FLAG_HIDE_MAP_DOTS | SYSTEM_FLAG_HIDE_MAP_ROUTES);
            }
        }

        g_system_flags |= SYSTEM_FLAG_DRAW_MAP;
        g_world_map_projection_state.flags |= WORLD_MAP_PROJECTION_UPDATE_POSITION;
        goto finish;
    }

    if (g_world_map_zoom.flags & WORLD_MAP_PAN_ZOOMED_OUT) {
        goto finish;
    }

    /* Holding square swaps the cursor window for the wide-view window. */
    if (!(g_input_current_buttons & PSX_PAD_SQUARE)) {
        if (level->map_cursor.wide_view != 0) {
            level->map_cursor.wide_view = 0;
            g_window_records[level->map_cursor.alt_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        }
        g_window_records[level->map_cursor.window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
    } else if (level->map_cursor.wide_view != 1) {
        level->map_cursor.wide_view = 1;
        g_window_records[level->map_cursor.window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        g_window_records[level->map_cursor.alt_window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_records[level->map_cursor.alt_window_index].anim_counter
            = g_window_records[level->map_cursor.alt_window_index].frame_index = 0;
        g_world_map_scroll_y.value = 0;
        g_world_map_scroll_x.value = 0;
        g_world_map_scroll_y.output = 0;
        g_world_map_scroll_x.output = 0;
    }

scroll:
    if (level->map_cursor.wide_view == 0) {
        held = g_input_current_buttons;
        if (held & (PSX_PAD_RIGHT | PSX_PAD_LEFT)) {
            if (held & PSX_PAD_LEFT) {
                world_map_update_scroll_axis(&g_world_map_scroll_x, -1);
            } else {
                world_map_update_scroll_axis(&g_world_map_scroll_x, 1);
            }
        } else {
            world_map_update_scroll_axis(&g_world_map_scroll_x, 0);
        }

        held = g_input_current_buttons;
        if (held & (PSX_PAD_UP | PSX_PAD_DOWN)) {
            if (held & PSX_PAD_UP) {
                world_map_update_scroll_axis(&g_world_map_scroll_y, -1);
            } else {
                world_map_update_scroll_axis(&g_world_map_scroll_y, 1);
            }
        } else {
            world_map_update_scroll_axis(&g_world_map_scroll_y, 0);
        }

        /* Pull the move toward the nearest dot, then scroll the map when the cursor nears a screen edge. */
        amount.x = g_world_map_scroll_x.output;
        amount.y = g_world_map_scroll_y.output;
        if (!(g_world_map_zoom.flags & WORLD_MAP_PAN_ZOOMED_OUT)) {
            target.x = g_window_records[level->map_cursor.window_index].position.x + amount.x;
            target.y = g_window_records[level->map_cursor.window_index].position.y + amount.y;
            level->map_cursor.snap_hit = world_map_get_dot_snap_step(target, &step_x, &step_y);
            amount.x += step_x;
            amount.y += step_y;

            target.x = g_window_records[level->map_cursor.window_index].position.x + amount.x;
            target.y = g_window_records[level->map_cursor.window_index].position.y + amount.y;
            saved = g_world_map_projection_view.origin;
            if ((amount.x < 0 && target.x < -0x40) || (amount.x > 0 && target.x >= 0x49)) {
                world_map_apply_clamped_horizontal_scroll(&amount.x, &g_world_map_projection_view.origin.vx);
            }
            if ((amount.y < 0 && target.y < -0x40) || (amount.y > 0 && target.y >= 0x41)) {
                world_map_apply_clamped_vertical_scroll(&amount, &g_world_map_projection_view.origin);
            }
            if (saved.vx != g_world_map_projection_view.origin.vx
                || saved.vy != g_world_map_projection_view.origin.vy) {
                g_system_flags |= SYSTEM_FLAG_DRAW_MAP;
                g_world_map_projection_state.flags |= WORLD_MAP_PROJECTION_UPDATE_POSITION;
            }
        }

        g_window_records[level->map_cursor.window_index].position.x += amount.x;
        g_window_records[level->map_cursor.window_index].position.y += amount.y;
        if (g_window_records[level->map_cursor.window_index].position.x < -0x70) {
            g_window_records[level->map_cursor.window_index].position.x = -0x70;
        }
        if (g_window_records[level->map_cursor.window_index].position.x >= 0x79) {
            g_window_records[level->map_cursor.window_index].position.x = 0x78;
        }
        if (g_window_records[level->map_cursor.window_index].position.y < -0x68) {
            g_window_records[level->map_cursor.window_index].position.y = -0x68;
        }
        if (g_window_records[level->map_cursor.window_index].position.y >= 0x69) {
            g_window_records[level->map_cursor.window_index].position.y = 0x68;
        }
    } else {
        /* In wide view the d-pad scrolls the map 4 units a frame instead of moving the cursor. */
        held = g_input_current_buttons;
        if (held & PSX_PAD_DPAD_MASK) {
            amount.y = 0;
            amount.x = 0;
            if (held & PSX_PAD_LEFT) {
                amount.x = -4;
            }
            if (held & PSX_PAD_RIGHT) {
                amount.x = 4;
            }
            if (held & PSX_PAD_UP) {
                amount.y = -4;
            }
            if (held & PSX_PAD_DOWN) {
                amount.y = 4;
            }

            saved = g_world_map_projection_view.origin;
            world_map_apply_clamped_scroll(&amount, &g_world_map_projection_view.origin);
            if (saved.vx != g_world_map_projection_view.origin.vx
                || saved.vy != g_world_map_projection_view.origin.vy) {
                g_system_flags |= SYSTEM_FLAG_DRAW_MAP;
                g_world_map_projection_state.flags |= WORLD_MAP_PROJECTION_UPDATE_POSITION;
            }
        }
    }

finish:
    if (level->map_cursor.wide_view == 0 && (hit = level->map_cursor.snap_hit) != 0) {
        g_location_selected = hit;
        return;
    }

    g_location_selected = 0;
}

/* Push a map-dot focus level and hide its parent window during the projection move.
 * Keeping the render pointer as the base preserves the two related call addresses. */
static void world_map_push_focus_level(s32 dot_index) {
    display_object_animated_t* render;

    render = &g_world_map_dots[dot_index].render;
    g_menu_stack_records_next[g_menu_stack_depth].focus_dot_index = dot_index;
    world_map_start_pan_if_outside(&render->position, &g_world_map_dots[dot_index].position);
    if (g_world_map_pan.flags & WORLD_MAP_PAN_ACTIVE) {
        g_system_flags |= SYSTEM_FLAG_HIDE_LOCATION_LABEL;
        g_window_records[g_menu_stack_records_next[g_menu_stack_depth - 1].window_index].flags |= DISPLAY_OBJECT_HIDDEN;
    }
    g_menu_stack_types[g_menu_stack_depth + 1] = MENU_STACK_LEVEL_WORLD_MAP_FOCUS;
    g_menu_stack_depth = g_menu_stack_depth + 1;
}

/* Step of the focus-location level. While the view pans it advances the pan
 * and redraws the map; afterwards it pops the level, moves the parent level's
 * window (the map cursor) onto the focused dot (y - 4), shows the location
 * label again and runs the parent's resume handler.
 *
 * The entry must be a struct pointer: the target hoists its reload above the
 * scalar g_system_flags store, which GCC only allows for an in-struct
 * access. */
void world_map_step_focus_level(menu_stack_record_t* entry) {
    world_map_projection_state_t* state;
    s32 level;

    if (g_world_map_pan.flags & WORLD_MAP_PAN_ACTIVE) {
        world_map_step_pan();
        state = &g_world_map_projection_state;
        state->flags |= WORLD_MAP_PROJECTION_UPDATE_POSITION;
        g_system_flags |= SYSTEM_FLAG_DRAW_MAP;
        return;
    }
    level = g_menu_stack_depth;
    g_menu_stack_depth = level - 1;
    g_window_records[g_menu_stack_records_next[level - 2].window_index].position.x
        = g_world_map_dots[entry->focus_dot_index].render.position.x;
    g_system_flags &= ~SYSTEM_FLAG_HIDE_LOCATION_LABEL;
    g_window_records[g_menu_stack_records_next[level - 2].window_index].position.y
        = g_world_map_dots[entry->focus_dot_index].render.position.y - 4;
    menu_stack_dispatch_resume();
}

/* Start drawing or erasing the map route between locations `from` and `to`
 * and hide the parent window until the animation finishes. Indexed stores
 * keep their named globals to preserve the original address sequences. */
static void world_map_push_path_level(s32 from, s32 to, s32 erase) {
    s32 depth;
    s32 window_index;

    world_map_prepare_path_animation(from, to);
    depth = g_menu_stack_depth;
    g_menu_stack_records_next[depth].path.path_id = from;
    g_menu_stack_records_next[depth].path.path_id_hi = to;
    g_menu_stack_records_next[depth].path.erase = erase;
    g_menu_stack_records_next[depth].path.pending_sound = 1;
    g_system_flags |= SYSTEM_FLAG_HIDE_LOCATION_LABEL;
    window_index = g_menu_stack_records_next[depth - 1].window_index;
    g_menu_stack_types[depth + 1] = MENU_STACK_LEVEL_WORLD_MAP_PATH;
    g_window_records[window_index].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth = depth + 1;
}

/*
 * Advances a map path animation one frame. The first frame plays the path
 * sound; an erase also clears the path's visible variable and starts at the
 * last page. While the animation runs, the page timer steps forwards to draw
 * the route (setting the visible variable once all pages are drawn) or
 * backwards to erase it; afterwards the level pops and its parent resumes.
 */
void world_map_step_path_level(world_map_path_level_t* level) {
    s32 depth;
    s32 window_index;

    if (level->pending_sound != 0) {
        if (g_system_flags & SYSTEM_FLAG_FADING) {
            return;
        }
        if (level->erase != 0) {
            script_set_variable(g_world_map_path_animation.route_id + EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST, 0);
            g_world_map_path_animation.segment_index = g_world_map_path_animation.segment_count - 1;
            g_world_map_path_animation.frame_timer
                = g_world_map_path_animation.segment_frames[g_world_map_path_animation.segment_count - 1] - 1;
            sound_play_effect(SOUND_SFX_WORLD_MAP_PATH);
        } else {
            sound_play_effect(SOUND_SFX_WORLD_MAP_PATH);
        }
        level->pending_sound = 0;
    }

    if (g_world_map_path_animation.flags & WORLD_MAP_PATH_ANIMATION_RUNNING) {
        if (level->erase == 0) {
            g_world_map_path_animation.frame_timer++;
            if (g_world_map_path_animation.frame_timer
                >= g_world_map_path_animation.segment_frames[g_world_map_path_animation.segment_index]) {
                g_world_map_path_animation.segment_index++;
                g_world_map_path_animation.frame_timer = 0;
                if (g_world_map_path_animation.segment_index >= g_world_map_path_animation.segment_count) {
                    script_set_variable(g_world_map_path_animation.route_id + EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST, 1);
                }
            }
        } else {
            g_world_map_path_animation.frame_timer--;
            if (g_world_map_path_animation.frame_timer <= 0) {
                g_world_map_path_animation.segment_index--;
                if (g_world_map_path_animation.segment_index >= 0) {
                    g_world_map_path_animation.frame_timer
                        = g_world_map_path_animation.segment_frames[g_world_map_path_animation.segment_index];
                }
            }
        }
        g_system_flags |= SYSTEM_FLAG_DRAW_MAP;
        return;
    }

    sound_play_effect(SOUND_SFX_NONE);
    g_system_flags |= SYSTEM_FLAG_DRAW_MAP;
    depth = g_menu_stack_depth;
    g_menu_stack_depth = depth - 1;
    window_index = g_menu_stack_records_next[depth - 2].window_index;
    g_system_flags &= ~SYSTEM_FLAG_HIDE_LOCATION_LABEL;
    g_window_records[window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
    menu_stack_dispatch_resume();
}

/* Start a delayed map-location visibility change and hide the parent window
 * until it finishes. Indexed stores keep their named globals to preserve
 * the original address sequences. */
static void world_map_push_location_visibility_level(s32 dot_index, s32 was_set) {
    s32 depth = g_menu_stack_depth;
    s32 window_index;

    g_menu_stack_records_next[depth].visibility.dot_index = dot_index;
    g_menu_stack_records_next[depth].visibility.was_set = was_set;
    g_menu_stack_records_next[depth].visibility.delay = 0x10;
    g_menu_stack_records_next[depth].visibility.pending_sound = 1;
    g_system_flags |= SYSTEM_FLAG_HIDE_LOCATION_LABEL;
    window_index = g_menu_stack_records_next[depth - 1].window_index;
    g_menu_stack_types[depth + 1] = MENU_STACK_LEVEL_WORLD_MAP_LOCATION_VISIBILITY;
    g_window_records[window_index].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth = depth + 1;
}

/* Step of the location visibility level, which reveals or erases one
 * location's dot. Once the screen fade is over it plays the sound; a reveal
 * sets the location's visible variable (EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST
 * + dot) at once. The map is redrawn for `delay` frames, then an erase hides
 * the dot and clears the variable, and the level closes.
 *
 * The window index is read into a local before the flag clear, which keeps
 * the target's load and store order. */
void world_map_step_location_visibility_level(world_map_visibility_level_t* level) {
    s32 index;

    if (level->pending_sound != 0) {
        if (g_system_flags & SYSTEM_FLAG_FADING) {
            return;
        }
        if (level->was_set != 0) {
            sound_play_effect(SOUND_SFX_CANCEL);
        } else {
            script_set_variable(level->dot_index + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST, 1);
            sound_play_effect(SOUND_SFX_WORLD_MAP_LOCATION_APPEAR);
        }
        level->pending_sound = 0;
    }
    if (level->delay != 0) {
        g_system_flags |= SYSTEM_FLAG_DRAW_MAP;
        level->delay--;
        return;
    }
    if (level->was_set != 0) {
        g_world_map_dots[level->dot_index].render.flags |= DISPLAY_OBJECT_HIDDEN;
        script_set_variable(level->dot_index + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST, 0);
    }
    g_system_flags |= SYSTEM_FLAG_DRAW_MAP;
    g_menu_stack_depth--;
    index = g_menu_stack_records_next[g_menu_stack_depth - 1].window_index;
    g_system_flags &= ~SYSTEM_FLAG_HIDE_LOCATION_LABEL;
    g_window_records[index].flags &= ~DISPLAY_OBJECT_HIDDEN;
    menu_stack_dispatch_resume();
}

/* Open the entry window and push its world-map return level. */
void world_map_push_menu_level(void) {
    u32 flags;
    s32 depth;
    s32* counter;

    g_menu_blocked_buttons = -1;
    screen_open_entry_window(4, -0x30, -0x38);
    g_hud_value_mode = HUD_VALUE_FUNDS;
    g_hud_value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    depth = g_menu_stack_depth;
    flags = g_system_flags;
    g_menu_stack_records_next[depth].return_level.delay = 3;
    g_menu_stack_records_next[depth].return_level.pending = 0;
    g_system_flags = flags & ~SYSTEM_FLAG_SCREEN_CLEARED;
    if ((flags & SYSTEM_FLAG_FADING) != 0) {
        g_system_flags = (flags & ~SYSTEM_FLAG_SCREEN_CLEARED) | SYSTEM_FLAG_FREEZE_ANIMATION;
        counter = &g_world_map_fade.elapsed;
        *counter = *counter - 4;
    }
    {
        s32 last;
        last = g_menu_stack_depth;
        g_menu_stack_types[last + 1] = MENU_STACK_LEVEL_WORLD_MENU;
        g_menu_stack_depth = last + 1;
    }
}

/* Per-frame handler of the world-map return level: waits out its own delay,
 * then pops the level and dispatches whichever pending system-flag request
 * asked for the return. */
void world_map_step_menu_level(world_map_menu_level_t* level) {
    s32 flags;
    s32 cleared;
    s32 location;

    if (g_system_flags & SYSTEM_FLAG_SCREEN_CLEARED) {
        level->pending = 1;
    }

    if (g_system_flags & SYSTEM_FLAG_FADING) {
        if (level->pending != 0) {
            level->pending = 0;
            world_map_toggle_captured_frame(0);
            level->delay = 3;
            g_system_flags |= SYSTEM_FLAG_FADING | SYSTEM_FLAG_FREEZE_ANIMATION;
        }
        g_menu_blocked_buttons = -1;
        return;
    }

    if (level->delay != 0) {
        level->delay -= 1;
        if (level->delay == 0) {
            world_map_toggle_captured_frame(1);
        }
        return;
    }

    g_menu_blocked_buttons = 0;
    if (menu_get_thread_activity() != 0) {
        return;
    }

    world_map_init_input_repeat();
    flags = g_system_flags;
    world_map_toggle_captured_frame(0);
    cleared = g_system_flags & ~SYSTEM_FLAG_FREEZE_ANIMATION;
    g_system_flags = cleared;
    g_menu_stack_depth -= 1;
    if (flags & SYSTEM_FLAG_REQUEST_BRAVE_STORY) {
        g_system_flags = cleared ^ SYSTEM_FLAG_DRAW_MAP;
        screen_transition_push_level(MENU_STACK_LEVEL_BRAVE_STORY, 0, 0);
        return;
    }

    if (flags & SYSTEM_FLAG_REQUEST_DESTINATION) {
        location = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP);
        world_map_capture_backdrop();
        menu_stack_dispatch_resume();
        if (g_world_map_projection_state.marker.kind != location) {
            travel_start(g_world_map_projection_state.marker.kind, location);
        }
        return;
    }

    if (flags & SYSTEM_FLAG_REQUEST_TUTORIAL) {
        g_system_flags = cleared ^ SYSTEM_FLAG_DRAW_MAP;
        screen_transition_push_level(MENU_STACK_LEVEL_TUTORIAL, 0, 1);
        return;
    }

    if (flags & SYSTEM_FLAG_REQUEST_VARIABLE_LIST) {
        debug_push_variable_list_level();
        return;
    }

    if (flags & SYSTEM_FLAG_REQUEST_FIXED_NUMBER_PANEL) {
        debug_push_party_number_level();
        return;
    }

    if (flags & SYSTEM_FLAG_RESTART) {
        g_system_flags = cleared & ~(SYSTEM_FLAG_RUNNING | SYSTEM_FLAG_UNLOCK_ALL);
        if (script_get_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT) != 0
            || script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) == 0) {
            g_system_flags |= SYSTEM_FLAG_RESTART;
        } else {
            g_system_flags &= ~SYSTEM_FLAG_RESTART;
        }
        g_system_flags |= SYSTEM_FLAG_NO_BACKGROUND_IMAGE;
        return;
    }

    menu_stack_dispatch_resume();
}

/* Pushes the self-sizing message window (MENU_STACK_LEVEL_TEXT_WINDOW), the
 * windowed sibling of menu_stack_push_message_level. It stores the reload
 * flag and, when set, re-points the text formatter at
 * g_screen_text_section_offsets; appends the window's label record (priority
 * 1); measures the entry, clamped to three rows; fills the shared scrollable
 * window and centers it and the label on the measured box; then saves
 * g_menu_blocked_buttons in the level, blocks every button, plays
 * SOUND_SFX_WINDOW_OPEN and pushes the level.
 *
 * Taking the record base into `records` at the last block-one store lets CSE
 * reuse that register after the measure call, which gives the base
 * priority over `text` in global allocation (s1/s2 as in the target). The
 * combined-away address pseudo of that store keeps a reload stack slot, part
 * of the target's 40-byte locals area. `box` is an aggregate so its 32-bit values stay
 * in memory across text_window_open; `dims` is an array so
 * it is laid out in declaration order at 32(sp). `unused_point` and `unused`
 * are sized from the target frame only; their original types are unknown. */
void menu_stack_push_text_window_level(s32 text, s32 reload_text) {
    text_window_t* window;
    display_object_animated_t* records;
    point32_t* positions;
    point32_t* position;
    s32* text_id;
    s32 index;
    point32_t box;
    point32_t unused_point;
    s16 dims[2];
    s32 unused[2];
    u8* entry;

    g_menu_stack_records_next[g_menu_stack_depth].message.reload_text = reload_text;
    if (reload_text != 0) {
        text_save_section_pointers();
        text_relocate_pointer_table(g_screen_text_section_offsets);
    }

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records_next[g_menu_stack_depth].message.window_index = index;
    g_window_records[index].sequence = WINDOW_ANIM_MESSAGE_LABEL;
    g_window_records[index].priority = 1;
    g_window_records[index].anim_counter = 0;
    records = g_window_records;
    records[index].frame_index = 0;

    entry = text_find_entry(text);
    text_measure_pixels(&dims[0], &dims[1], entry);
    if (dims[1] > 3) {
        dims[1] = 3;
    }

    text_id = &g_text_window.text_id;
    window = &g_text_window;

    *text_id = text;
    window->image_x = 0;
    g_text_window.image_y = 0xC0;
    box.x = (dims[0] + 24) & 0xFFFC;
    box.y = (dims[1] << 4) + 16;
    g_text_window.base.x = -(box.x / 2);
    g_text_window.base.y = -(box.y >> 1);
    g_text_window.text_width = dims[0];
    g_text_window.rows_per_page = dims[1];
    g_text_window.priority = 1;
    g_text_window.image_coordinate_mode = 0;
    g_text_window.extra_render_index = -1;
    g_text_window.text_substitutions[1] = -1;
    g_text_window.text_substitutions[0] = -1;
    text_window_open(window);

    index = g_menu_stack_records_next[g_menu_stack_depth].message.window_index;
    positions = &records[0].position;
    /* This byte-view stride retains the target addiu of &records[0].x;
     * &records[index].x removes it and breaks the exact match. */
    position = (point32_t*)((u8*)positions + index * sizeof(display_object_animated_t));
    position->x = -(box.x / 2) + 2;
    position->y = -(box.y / 2) - 2;
    sound_play_effect(SOUND_SFX_WINDOW_OPEN);

    g_menu_stack_records_next[g_menu_stack_depth].message.saved_blocked_buttons = g_menu_blocked_buttons;
    g_menu_blocked_buttons = -1;
    g_menu_stack_types[g_menu_stack_depth + 1] = MENU_STACK_LEVEL_TEXT_WINDOW;
    g_menu_stack_depth = g_menu_stack_depth + 1;
}

/* Steps a message level: once the text window has popped in and
 * text_window_step reports it closed, pops the level, restores the
 * blocked-button mask it saved and (when asked) the text section pointers,
 * and resumes the level underneath. */
void menu_stack_wait_text_then_pop(menu_stack_message_level_t* level) {
    if (g_window_render_records[g_text_window.render_record_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }
    if (text_window_step(&g_text_window) != 0) {
        return;
    }
    g_input_current_buttons = 0;
    g_menu_blocked_buttons = level->saved_blocked_buttons;
    g_window_record_count -= 1;
    g_window_render_object_count -= 1;
    g_menu_stack_depth -= 1;
    if (level->reload_text != 0) {
        text_restore_section_pointers();
    }
    menu_stack_dispatch_resume();
}

/* Pushes MENU_STACK_LEVEL_MESSAGE, a dialog box on message `text`:
 * optionally points the text formatter at WORLD's sections, starts the dialog
 * thread (mode 0x3b while SYSTEM_FLAG_UNIT_PANEL_THREADS is set, else
 * 0x33), saves the blocked buttons in the level and blocks them all, and
 * plays SOUND_SFX_WINDOW_OPEN. */
void menu_stack_push_message_level(s32 text, s32 reload_text) {
    s32 index;

    g_menu_stack_records_next[g_menu_stack_depth].message.reload_text = reload_text;
    if (reload_text != 0) {
        text_save_section_pointers();
        text_relocate_pointer_table(g_screen_text_section_offsets);
    }
    thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_dialog_thread);
    if (g_system_flags & SYSTEM_FLAG_UNIT_PANEL_THREADS) {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x3B, text, 0);
    } else {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x33, text, 0);
    }
    g_menu_stack_records_next[g_menu_stack_depth].message.saved_blocked_buttons = g_menu_blocked_buttons;
    g_menu_blocked_buttons = -1;
    sound_play_effect(SOUND_SFX_WINDOW_OPEN);
    index = g_menu_stack_depth;
    g_menu_stack_records_next[index].message._unknown_0c = 8;
    g_menu_stack_records_next[index].message.window_index = 0;
    g_menu_stack_types[index + 1] = MENU_STACK_LEVEL_MESSAGE;
    g_menu_stack_depth = index + 1;
}

/* Steps a level that waits for the message box to close: once the message
 * thread has ended it pops the level, restores the blocked buttons, resumes
 * the level underneath and (when asked) restores the text section pointers.
 * Over the Brave Story or proposition category level it keeps the displayed
 * play time current while waiting. */
void menu_stack_wait_message_then_pop(menu_stack_message_level_t* level) {
    if (g_menu_stack_types[g_menu_stack_depth - 1] == MENU_STACK_LEVEL_BRAVE_STORY
        || g_menu_stack_types[g_menu_stack_depth - 1] == MENU_STACK_LEVEL_BRAVE_STORY_PROPOSITIONS) {
        g_hud_value = g_save_play_time_hours;
        g_hud_value_secondary = g_save_play_time_minutes;
        g_hud_value_tertiary = g_save_play_time_seconds;
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
        g_menu_blocked_buttons = level->saved_blocked_buttons;
        g_menu_stack_depth = g_menu_stack_depth - 1;
        menu_stack_dispatch_resume();
        if (level->reload_text != 0) {
            text_restore_section_pointers();
        }
    }
}

/* Pushes the debug script-variable list (MENU_STACK_LEVEL_DEBUG_VARIABLE_LIST):
 * one cursor window record and one render record, laid out by
 * debug_draw_variable_list and then positioned from the render
 * record's origin.
 *
 * Both appended indices share one local, which is what holds them in $a2.
 * The window position is written after the render call returns, from the render
 * record origin the callee filled in, so the level fields are read back out
 * of the stack record rather than kept in locals; the `- -` keeps the row
 * offset's constant on the multiply chain, which a plain `+` reassociates
 * onto base_y. */
static void debug_push_variable_list_level(void) {
    s32 index;
    s32 record_index;
    u8 unused_locals[0x10];

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records_next[g_menu_stack_depth].variable_list.window_index = index;
    g_window_records[index].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[index].priority = 3;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records_next[g_menu_stack_depth].variable_list.render_index = index;
    g_window_render_records[index].flags |= DISPLAY_OBJECT_POP_IN;
    g_window_render_records[index].priority = 3;
    g_menu_stack_records_next[g_menu_stack_depth].variable_list.first_variable = 0;
    debug_draw_variable_list(&g_menu_stack_records_next[g_menu_stack_depth].variable_list);
    record_index = g_menu_stack_depth;
    g_menu_stack_records_next[record_index].variable_list.cursor = 0;
    g_window_records[g_menu_stack_records_next[record_index].variable_list.window_index].position.x
        = g_window_render_records[g_menu_stack_records_next[record_index].variable_list.render_index].base.x + 8;
    g_menu_stack_depth = record_index + 1;
    g_window_records[g_menu_stack_records_next[record_index].variable_list.window_index].position.y
        = g_window_render_records[g_menu_stack_records_next[record_index].variable_list.render_index].base.y
        - -(g_menu_stack_records_next[record_index].variable_list.cursor * 0x10 + 0xC);
    g_menu_stack_types[record_index + 1] = MENU_STACK_LEVEL_DEBUG_VARIABLE_LIST;
}

/* Renders up to 10 rows of the debug variable list, from first_variable on
 * (script variables 0 to 1023). TEXT_ID_DEBUG_VARIABLE_VALUE shows a
 * variable and its value; a value of 10000 or more uses
 * TEXT_ID_DEBUG_VARIABLE, which shows the variable alone. The entry is
 * chosen first and its section added at the call.
 *
 * `value` first holds the row count for the panel height and then each
 * variable's value: one register (v1) in the target. */
static void debug_draw_variable_list(debug_variable_list_level_t* level) {
    RECT bounds;
    point32_t point;
    s32 index;
    point32_t* base;
    point32_t* records;
    s32 variable = level->first_variable;
    s32 text_id;
    u32 value;
    s32 i;

    if (variable + 10 >= 1024) {
        level->row_count = 1024 - variable;
    } else {
        level->row_count = 10;
    }
    value = level->row_count;
    index = level->render_index;
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = 0x68;
    bounds.h = value * 0x10 + 0x10;
    window_build_render_record_image(index, bounds, 0, g_window_image_buffer);
    /* Direct record indexing moves the symbol load after the index scaling
     * and breaks the exact match. Retain the target's biased base and stride. */
    records = &g_window_render_records[0].base;
    base = (point32_t*)((u8*)records + index * sizeof(window_render_record_t));
    base->x = -(bounds.w / 2);
    base->y = -88;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < level->row_count; i++) {
        value = script_get_variable(variable);
        if (value < 10000) {
            g_text_substitution_values[0] = variable;
            g_text_substitution_values[1] = value;
            text_id = TEXT_ID_DEBUG_VARIABLE_VALUE & TEXT_ID_ENTRY_MASK;
        } else {
            g_text_substitution_values[0] = variable;
            text_id = TEXT_ID_DEBUG_VARIABLE & TEXT_ID_ENTRY_MASK;
        }
        window_display_text_entry(index, text_id | TEXT_ID_WORLD_MENU_BASE, point, g_window_image_buffer);
        point.y += 0x10;
        variable++;
    }
    window_load_render_record_image(index, g_window_image_buffer);
}

/* Input handler for the debug script-variable list.
 *
 * Circle opens the number panel for the selected variable, Cross pops the
 * level. Up/down move the cursor, or scroll the first displayed variable by
 * 100 with Triangle held and by 10 with Square held (Square wraps between 0
 * and 1020). Scrolling redraws the rows through
 * debug_draw_variable_list and clamps the cursor; a plain cursor move only repositions
 * the cursor window.
 *
 * Every scroll arm falls into the one redraw, which clamps the cursor and
 * joins the cursor move. The cursor-down arm sits between the redraw and the
 * move in the target, so it and the move are reached by goto: a redraw
 * repeated in each arm is not merged back (its clamp store is cross-jumped
 * into the cursor-down arm first). */
void debug_handle_variable_list_input(debug_variable_list_level_t* level) {
    s32 variable;
    s32 dead[2];

    if (g_window_render_records[level->render_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
        g_window_records[level->window_index].palette = 0xA;
        g_window_render_records[level->render_index].palette = 2;
        g_window_records[level->window_index].sequence = WINDOW_ANIM_CURSOR_PRESS;
        g_window_records[level->window_index].anim_counter = g_window_records[level->window_index].frame_index = 0;
        variable = level->first_variable + level->cursor;
        debug_push_variable_edit_level(variable, script_get_variable(variable));
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        g_window_record_count -= 1;
        g_window_render_record_count -= 1;
        g_window_render_object_count -= 2;
        g_menu_stack_depth -= 1;
        menu_stack_dispatch_resume();
        return;
    }

    if (world_map_update_input_repeat(PSX_PAD_UP) != 0) {
        if (g_input_current_buttons & PSX_PAD_TRIANGLE) {
            if (level->first_variable < 0x64) {
                level->first_variable = 0;
            } else {
                level->first_variable -= 0x64;
            }
        } else if (g_input_current_buttons & PSX_PAD_SQUARE) {
            if (level->first_variable == 0) {
                level->first_variable = 0x3fc;
            } else {
                level->first_variable -= 0xa;
            }
        } else {
            if (level->cursor == 0) {
                level->cursor = level->row_count - 1;
            } else {
                level->cursor--;
            }
            goto move_cursor;
        }
    } else if (world_map_update_input_repeat(PSX_PAD_DOWN) != 0) {
        if (g_input_current_buttons & PSX_PAD_TRIANGLE) {
            if (level->first_variable + 0x64 < 0x3fd) {
                level->first_variable += 0x64;
            } else {
                level->first_variable = 0x3fc;
            }
        } else if (g_input_current_buttons & PSX_PAD_SQUARE) {
            if (level->first_variable == 0x3fc) {
                level->first_variable = 0;
            } else {
                level->first_variable += 0xa;
            }
        } else {
            goto cursor_down;
        }
    } else {
        return;
    }

    debug_draw_variable_list(level);
    if (level->cursor < level->row_count) {
        return;
    }

    level->cursor = level->row_count - 1;
    goto move_cursor;
cursor_down:
    if (level->cursor == level->row_count - 1) {
        level->cursor = 0;
    } else {
        level->cursor++;
    }
move_cursor:
    g_window_records[level->window_index].position.x = g_window_render_records[level->render_index].base.x + 8;
    g_window_records[level->window_index].position.y
        = g_window_render_records[level->render_index].base.y - -(level->cursor * 0x10 + 0xC);
}

/* Pushes MENU_STACK_LEVEL_DEBUG_VARIABLE_EDIT, a number panel (0 to
 * 9999) that edits script variable `variable_id`, starting at `value`. */
static void debug_push_variable_edit_level(s32 variable_id, s32 value) {
    point32_t origin;
    s32 record_index;

    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS;
    g_menu_stack_ot_offset = 2;
    g_window_panel_value = value;
    origin.x = 0x20;
    origin.y = 0x20;
    window_start_number_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 9999 << 16, 0, &origin);
    record_index = g_menu_stack_depth;
    g_menu_stack_records_next[record_index].panel.result = 0;
    g_menu_stack_records_next[record_index].panel.variable_id = variable_id;
    g_menu_stack_types[record_index + 1] = MENU_STACK_LEVEL_DEBUG_VARIABLE_EDIT;
    g_menu_stack_depth = record_index + 1;
}

/* Step handler of the variable number panel. While the panel runs it accepts
 * Circle, which plays SOUND_SFX_CONFIRM and writes the panel's value to the
 * level's script variable, or Cross; either way it tells the panel thread to
 * close. Once the
 * panel thread has stopped it pops the level, redraws the script-variable
 * list on a confirm, and restores the parent level's window and render
 * records.
 *
 * Reading the parent window index before the render-record store is what
 * lets the scheduler place that load inside the render index's multiply
 * sequence, as the target does. */
void debug_step_variable_edit_level(menu_stack_panel_level_t* level) {
    s32 depth;
    s32 parent;
    s32 slot;
    display_object_animated_t* record;
    u32 buttons;

    if (level->result != 0 && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_menu_stack_ot_offset = 1;
        depth = g_menu_stack_depth - 1;
        g_menu_stack_depth = depth;
        if (level->result == 1) {
            debug_draw_variable_list(&g_menu_stack_records[depth].variable_list);
        }
        parent = g_menu_stack_depth - 1;
        g_window_records[g_menu_stack_records_next[parent].window_render.window_index].sequence
            = WINDOW_ANIM_CURSOR_IDLE;
        slot = g_menu_stack_records_next[parent].window_render.window_index;
        g_window_render_records[g_menu_stack_records_next[parent].window_render.render_index].palette = 0;
        record = &g_window_records[slot];
        record->palette = 0;
        record->frame_index = 0;
        g_window_records[slot].anim_counter = 0;
        return;
    }
    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        level->result = 2;
    } else if (buttons & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->result = 1;
        script_set_variable(level->variable_id, g_window_panel_value);
    } else {
        return;
    }
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
}

/* Appends a window-list record: saves g_menu_stack_ot_offset into it, allocates a window
 * record from the render-object queue, initializes that window's delay and
 * counters, and applies layout 0 to it before advancing the list count.
 *
 * Writing the window columns as fields of g_window_records (not separate stride
 * arrays) keeps the scheduler from sinking the record's window_index store
 * below them. */
void unit_panel_push_help_level(void) {
    s32 window;

    g_menu_stack_records_next[g_menu_stack_depth].layout_window.saved_state = g_menu_stack_ot_offset;
    g_menu_stack_ot_offset = 1;
    window = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records_next[g_menu_stack_depth].layout_window.window_index = window;
    g_window_records[window].sequence = WINDOW_ANIM_LAYOUT_CURSOR_IDLE;
    g_window_records[window].priority = 2;
    g_window_records[window].anim_counter = 0;
    g_window_records[window].frame_index = 0;
    g_menu_stack_records_next[g_menu_stack_depth].layout_window.layout = 0;
    g_menu_stack_records_next[g_menu_stack_depth].layout_window.cached_layout_kind = 1;
    unit_panel_move_help_cursor(&g_menu_stack_records_next[g_menu_stack_depth].layout_window);
    g_menu_stack_types[g_menu_stack_depth + 1] = MENU_STACK_LEVEL_UNIT_PANEL_HELP;
    g_menu_stack_depth++;
}

/* Move the layout window's cursor onto row `layout` of
 * g_unit_panel_help_rows, switching the cursor's sequence when the
 * row's kind differs from the last one.
 *
 * Routing both stores through one base variable (rather than
 * `&g_window_record_positions[index]`) is what reproduces the target's separate base register
 * held in $a0 across the whole function. */
static void unit_panel_move_help_cursor(unit_panel_help_level_t* request) {
    window_xy_t* xy = g_window_record_positions;
    s32 index = request->window_index;
    s32 layout = request->layout;
    s32 kind;

    xy[index].x = g_unit_panel_help_rows[layout].x + 14;
    xy[index].y = g_unit_panel_help_rows[layout].y + 12;
    kind = g_unit_panel_help_rows[layout].kind;
    if (kind != request->cached_layout_kind) {
        request->cached_layout_kind = kind;
        g_window_records[index].sequence = (kind != 0) ? WINDOW_ANIM_LAYOUT_CURSOR_IDLE : WINDOW_ANIM_CURSOR_IDLE;
        g_window_records[index].anim_counter = 0;
        g_window_records[index].frame_index = 0;
    }
}

/* Clear the text-task flag and move a window record to the sequence after
 * its current one, restarting the animation with the default palette. */
void unit_panel_resume_help_level(menu_stack_record_t* level) {
    s32 index = level->layout_window.window_index;
    display_object_animated_t* entry = &g_window_records[index];

    g_menu_input_lock_mode = 0;
    entry->sequence = g_window_records[index].sequence + 1;
    g_window_records[index].palette = 0;
    entry->anim_counter = 0;
    g_window_records[index].frame_index = 0;
}

/* The g_menu_stack_suspend_handlers entry of MENU_STACK_LEVEL_UNIT_PANEL_HELP: nothing to suspend. */
void unit_panel_suspend_help_level(void) {
}

/* Input handler for a layout-window menu level.
 *
 * Cross pops the level, Circle opens the row's help message, and a direction moves
 * to the neighboring layout named by the row. A row with a counter pointer
 * adds the counter to the base message id, and suppresses the message entirely
 * when the counter reads 0 or -1 (except for layout 0x0b). */
void unit_panel_handle_help_input(unit_panel_help_level_t* request) {
    s32 layout;
    s16* counter;
    s16 message;
    display_object_animated_t* record;
    s32 window;

    if ((g_world_map_input_new_button_presses & PSX_PAD_CROSS) != 0) {
        g_window_record_count -= 1;
        g_window_render_object_count -= 1;
        g_menu_stack_ot_offset = request->saved_state;
        g_menu_stack_depth -= 1;
        menu_stack_dispatch_resume();
        sound_play_effect(SOUND_SFX_CANCEL);
        return;
    }

    if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) != 0) {
        layout = request->layout;
        counter = g_unit_panel_help_rows[layout].counter;
        if (counter != 0) {
            if (layout != 0xB && *counter >= -1 && *counter < 1) {
                return;
            }
            message = g_unit_panel_help_rows[layout].message_base + *counter;
        } else {
            message = g_unit_panel_help_rows[layout].message_base;
        }
        menu_stack_push_message_level(message, 1);
        g_menu_input_lock_mode = 1;
        window = request->window_index;
        record = &g_window_records[window];
        record->sequence = g_window_records[window].sequence - 1;
        g_window_records[window].palette = 0xA;
        record->anim_counter = 0;
        g_window_records[window].frame_index = 0;
        return;
    }

    if (world_map_update_input_repeat(PSX_PAD_UP) != 0) {
        if (g_unit_panel_help_rows[request->layout].up != request->layout) {
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
            request->layout = g_unit_panel_help_rows[request->layout].up;
            unit_panel_move_help_cursor(request);
            return;
        }
    }
    if (world_map_update_input_repeat(PSX_PAD_DOWN) != 0) {
        if (g_unit_panel_help_rows[request->layout].down != request->layout) {
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
            request->layout = g_unit_panel_help_rows[request->layout].down;
            unit_panel_move_help_cursor(request);
            return;
        }
    }
    if (world_map_update_input_repeat(PSX_PAD_RIGHT) != 0) {
        if (g_unit_panel_help_rows[request->layout].right != request->layout) {
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
            request->layout = g_unit_panel_help_rows[request->layout].right;
            unit_panel_move_help_cursor(request);
            return;
        }
    }
    if (world_map_update_input_repeat(PSX_PAD_LEFT) != 0) {
        if (g_unit_panel_help_rows[request->layout].left != request->layout) {
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
            request->layout = g_unit_panel_help_rows[request->layout].left;
            unit_panel_move_help_cursor(request);
        }
    }
}

/* Pushes the location menu (MENU_STACK_LEVEL_LOCATION) for menu id `menu`.
 *
 * Loads the location's picture, appends the cursor window (window kind
 * menu + 0x37) and, when a picture is present, uploads the frame TIM and the
 * location's picture at g_picture_buffer to VRAM, binds a render
 * record to the picture and adds its border window at (-8, 0x10). It then
 * measures every
 * entry's text through text_find_entry / text_measure_pixels to
 * size the panel, copies the entry values into g_menu_stack_entry_values with
 * cleared row flags, clamps the cursor to at most four visible rows, and
 * starts the panel render state centered on the measured width.
 *
 * The identical `menu += 0x37` arms split the block for the scheduler (jump2
 * cross-jumps them back together), keeping the add after the picture call so
 * reorg puts it in the wait call's delay slot. The label window's position is
 * stored through a stride-36 view based at &records->x, as the target forms
 * (records + 0x18) + index * 36. The unused array reproduces the 0xb8 frame.
 */
void location_push_menu_level(s32 menu) {
    GsIMAGE image;
    RECT bounds;
    point32_t origin;
    window_panel_layout_t layout;
    s16 text_width;
    s16 text_rows;
    s32 index;
    s32 count;
    s32 depth;
    s16 widest;
    s32 value;
    s32 kind;
    u32 flags;
    s32 i;
    CVECTOR* color;
    display_object_animated_t* records;
    location_menu_level_t* level;
    point32_t* base;
    window_xy_t* position;

    /* menu + 0x37 is the cursor window's sequence; the identical arms are explained above. */
    g_world_map_projection_state.marker.sub_kind = location_load_picture(g_world_map_projection_state.marker.kind);
    if (g_world_map_projection_state.marker.sub_kind != 0) {
        menu += 0x37;
    } else {
        menu += 0x37;
    }
    file_wait_for_pending();

    flags = g_system_flags | SYSTEM_FLAG_HIDE_LOCATION_LABEL;
    level = &g_menu_stack_records_next[g_menu_stack_depth].location_list;
    level->extra_render = -1;
    level->label_window = -1;
    level->frame_render = -1;
    g_menu_stack_records_next[g_menu_stack_depth].location_list.cursor_window = -1;
    g_menu_stack_records_next[g_menu_stack_depth].location_list.mode = 0;
    g_system_flags = flags;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records_next[g_menu_stack_depth].location_list.cursor_window = index;
    records = g_window_records;
    /* Direct record.color access changes the column base and instruction scheduling. */
    color = &records->color;
    g_window_records[index].priority = 0xB;
    g_window_records[index].sequence = menu;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    /* The windows start black; the step reads `mode` as its fade level and brightens them from 8. */
    ((CVECTOR*)((u8*)color + index * sizeof(g_window_records[0])))->r = 0;
    ((CVECTOR*)((u8*)color + index * sizeof(g_window_records[0])))->g = 0;
    ((CVECTOR*)((u8*)color + index * sizeof(g_window_records[0])))->b = 0;
    g_menu_stack_records_next[g_menu_stack_depth].location_list.mode = 8;

    /* With a picture: upload the frame and picture TIMs, then add the picture and its border window. */
    if (g_world_map_projection_state.marker.sub_kind != 0) {
        GsGetTimInfo(g_location_picture_frame_tim + 1, &image);
        LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
        LoadClut(image.clut, image.cx, image.cy);
        GsGetTimInfo(g_picture_buffer + 1, &image);
        LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
        LoadClut(image.clut, image.cx, image.cy);

        index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
        g_menu_stack_records_next[g_menu_stack_depth].location_list.frame_render = index;
        bounds.x = image.px;
        bounds.y = image.py;
        bounds.w = image.pw;
        bounds.h = image.ph;
        origin.x = image.cx;
        origin.y = image.cy;
        window_init_vram_render_record(index, bounds, origin, DISPLAY_OBJECT_8BIT_CLUT);
        base = &g_window_render_records[index].base;
        g_window_render_records[index].priority = 0xB;
        base->x = -0x40;
        base->y = -0x4C;
        g_window_render_records[index].flags |= DISPLAY_OBJECT_POP_IN;

        index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
        g_menu_stack_records_next[g_menu_stack_depth].location_list.label_window = index;
        g_window_records[index].sequence = WINDOW_ANIM_LOCATION_PICTURE_BORDER;
        g_window_records[index].priority = 0xB;
        records[index].anim_counter = 0;
        g_window_records[index].frame_index = 0;
        position = (window_xy_t*)&records->position.x;
        position[index].x = -8;
        position[index].y = 0x10;
        ((CVECTOR*)((u8*)color + index * sizeof(g_window_records[0])))->r = 0;
        ((CVECTOR*)((u8*)color + index * sizeof(g_window_records[0])))->g = 0;
        ((CVECTOR*)((u8*)color + index * sizeof(g_window_records[0])))->b = 0;
    }

    /* Size the panel to the widest entry plus an 8-pixel margin. */
    widest = 0;
    count = g_menu_stack_records_next[g_menu_stack_depth].location_list.entry_count;
    for (i = 0; i < count; i++) {
        text_measure_pixels(&text_width, &text_rows,
            text_find_entry(g_menu_stack_records_next[g_menu_stack_depth].location_list.entries[i]));
        if (widest < text_width) {
            widest = text_width;
        }
    }
    widest += 8;

    for (i = 0; i < count; i++) {
        value = g_menu_stack_records_next[g_menu_stack_depth].location_list.entries[i];
        g_menu_stack_entry_values[i] = value;
        g_menu_stack_row_flags[i] = 0;
    }

    /* At most four rows show; the panel is centered on x, and without a picture it rises 8 pixels a row. */
    i = 4;
    if (count < 4) {
        i = count;
    }
    layout.text_width = widest;
    layout.visible_rows = i;
    origin.x = 0x80 - (((layout.text_width + 0x18) & 0xFFFC) >> 1);
    if (g_world_map_projection_state.marker.sub_kind != 0) {
        origin.y = 0x80;
    } else {
        origin.y = 0x70 - i * 8;
    }
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_LOCATION, count, i);
    window_init_panel_state(
        0xC, g_menu_stack_records_next[g_menu_stack_depth].location_list.entry_count, &layout, &origin);
    g_window_panel_render_state.vram_origin.x = 0x240;
    g_window_panel_render_state.header_id = 0;
    g_window_panel_render_state.vram_origin.y = 0x100;

    if (g_world_map_projection_state.marker.sub_kind != 0) {
        s32 unused[6]; /* frame padding: the target frame is 0xb8 */

        g_window_records[g_menu_stack_records_next[g_menu_stack_depth].location_list.cursor_window].position.x = -4;
        g_window_records[g_menu_stack_records_next[g_menu_stack_depth].location_list.cursor_window].position.y = 0;
    } else {
        /* Here `index` is still the cursor window's record index, yet it reads the render record pool (retail). */
        g_window_records[g_menu_stack_records_next[g_menu_stack_depth].location_list.cursor_window].position.x = -4;
        g_window_records[g_menu_stack_records_next[g_menu_stack_depth].location_list.cursor_window].position.y
            = g_window_render_records[index].base.y - 8;
    }

    depth = g_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_records_next[depth].location_list.delay = 8;
    g_system_flags |= SYSTEM_FLAG_FREEZE_ANIMATION;
    g_menu_stack_types[depth + 1] = MENU_STACK_LEVEL_LOCATION;
    g_menu_stack_depth = depth + 1;
}

/* Restore the location list's optional renderer palette. */
void location_resume_menu_level(menu_stack_record_t* level) {
    if (level->location_list.extra_render != -1) {
        g_window_render_records[level->location_list.extra_render].palette = 0;
    }
}

/* Sets the second owned render record's palette selector when its slot is present. */
void location_suspend_menu_level(menu_stack_slot_set_t* slots) {
    if (slots->render_b != -1) {
        g_window_render_records[slots->render_b].palette = 2;
    }
}

/* Per-frame step of the location menu level: fades its two windows in, starts
 * the menu thread after the delay and handles input. A confirmed entry opens
 * the bar, a shop, a deep-dungeon battle chosen by the location's conditional
 * set or the selected destination map; the help button shows the entry's
 * message and cancel closes the level. */
void location_step_menu_level(location_menu_state_t* level) {
    s16* selected;
    u8 unused[0x28];
    s16* values;
    s32 value;
    s32 text;
    s32 offset;
    deep_dungeon_battle_t* records;
    deep_dungeon_battle_t* record;

    /* Fade both windows in by 7 a frame. Without a picture window_b is -1, so the
     * second set of stores lands one record before the pool (retail). */
    if (level->fade != 0) {
        if (level->fade >= 0x80) {
            level->fade = 0x80;
        }
        g_window_records[level->slots.window_a].color.r = level->fade;
        g_window_records[level->slots.window_a].color.g = level->fade;
        g_window_records[level->slots.window_a].color.b = level->fade;
        g_window_records[level->slots.window_b].color.r = level->fade;
        g_window_records[level->slots.window_b].color.g = level->fade;
        g_window_records[level->slots.window_b].color.b = level->fade;
        if (level->fade == 0x80) {
            level->fade = 0;
            world_map_toggle_captured_frame(1);
        } else {
            level->fade += 7;
        }
        return;
    }

    /* Start the panel thread on the first delay frame, then ignore input until the delay ends. */
    if (level->delay != 0) {
        if (level->delay == 8) {
            thread_start(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, screen_text_list_thread);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, (s32)&g_window_panel_render_state, 0, 0);
        }
        level->delay--;
        return;
    }

    if (g_system_flags & SYSTEM_FLAG_FADING) {
        return;
    }

    /* A confirm or cancel below closed the panel; act on slots.result once its thread has finished. */
    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        world_map_toggle_captured_frame(0);
        g_system_flags
            = (g_system_flags | SYSTEM_FLAG_INPUT_BLOCKED) & ~(SYSTEM_FLAG_DRAW_MAP | SYSTEM_FLAG_FREEZE_ANIMATION);
        if (level->slots.result != 0) {
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_LOCATION);
            selected = &g_window_panel_render_state.selected_index;
            values = g_menu_stack_entry_values;
            value = values[*selected];

            /* World-menu entries are services and deep-dungeon floors; anything else names a map. */
            if ((value & TEXT_ID_SECTION_MASK) == TEXT_ID_WORLD_MENU_BASE) {
                g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
                value &= TEXT_ID_ENTRY_MASK;
                switch (value) {
                case TEXT_ID_BAR& TEXT_ID_ENTRY_MASK:
                    menu_stack_pop_and_release_records(&level->slots, 0);
                    audio_queue_start_music(AUDIO_MUSIC(2, MUSIC_BAR));
                    bar_push_level();
                    return;
                case TEXT_ID_SHOP& TEXT_ID_ENTRY_MASK:
                    menu_stack_pop_and_release_records(&level->slots, 0);
                    location_run_shop(0);
                    return;
                case TEXT_ID_SOLDIER_OFFICE& TEXT_ID_ENTRY_MASK:
                    menu_stack_pop_and_release_records(&level->slots, 0);
                    location_run_shop(SCREEN_CONTEXT_SOLDIER_OFFICE);
                    return;
                case TEXT_ID_FUR_SHOP& TEXT_ID_ENTRY_MASK:
                    menu_stack_pop_and_release_records(&level->slots, 0);
                    location_run_shop(SCREEN_CONTEXT_FUR_SHOP);
                    return;
                default:
                    script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_SELECTION,
                        value - (TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST & TEXT_ID_ENTRY_MASK));
                    menu_stack_pop_and_release_records(&level->slots, 0);
                    if (location_script_run(
                            g_world_map_projection_state.marker.kind, LOCATION_SCRIPT_ACTION_DEEP_DUNGEON_ENTRY)
                        != 0) {
                        /* rand() is 15 bits: pick one of the battle's eight ENTDs and an exit below args[2]. */
                        offset = g_location_script_state.args[0] * sizeof(deep_dungeon_battle_t);
                        records = g_deep_dungeon_battles;
                        record = &records[g_location_script_state.args[0]];
                        g_travel_next_map_id[0] = record->map_id;
                        /* Direct ENTD indexing folds the table base and removes a retail addition. */
                        script_set_variable(
                            EVENT_SCRIPT_VAR_CURRENT_ENTD, (&records->entd_ids[0])[((rand() * 8) >> 15) + offset]);
                        script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
                        script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, record->squad_id);
                        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_location_script_state.args[1]);
                        script_set_variable(
                            EVENT_SCRIPT_VAR_DEEP_DUNGEON_EXIT, (rand() * g_location_script_state.args[2]) >> 15);
                        g_system_flags = (g_system_flags | SYSTEM_FLAG_DEEP_DUNGEON_BATTLE) ^ SYSTEM_FLAG_RUNNING;
                    } else {
                        world_map_fade_start(0, 0x10);
                        audio_queue_start_music(AUDIO_MUSIC(1, MUSIC_WORLD_MAP));
                    }
                    return;
                }
            }

            /* A map entry runs the row's event and travels to map entry + 1. */
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_location_script_state.args[*selected]);
            g_system_flags ^= SYSTEM_FLAG_RUNNING;
            value = values[*selected] & TEXT_ID_ENTRY_MASK;
            g_travel_next_map_id[0] = value + 1;
            return;
        }

        g_system_flags = (g_system_flags | SYSTEM_FLAG_DRAW_MAP) & ~SYSTEM_FLAG_INPUT_BLOCKED;
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_LOCATION);
        menu_stack_pop_and_release_records(&level->slots, 1);
        return;
    }

    /* Cancel only works while the cursor is on a world-menu entry, not a map name. */
    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        value = g_menu_stack_entry_values[g_window_panel_render_state.selected_index];
        if ((value & TEXT_ID_SECTION_MASK) == TEXT_ID_WORLD_MENU_BASE) {
            sound_play_effect(SOUND_SFX_CANCEL);
            level->slots.result = 0;
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
            g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
        }
    } else if (g_world_map_input_new_button_presses & PSX_PAD_SELECT) {
        /* Help: map names, services and deep-dungeon floors each have their own message range. */
        location_suspend_menu_level(&level->slots);
        value = g_menu_stack_entry_values[g_window_panel_render_state.selected_index];
        /* A map's help is 0x8801 + map, a service's 0x104b on and a floor's 0x1090 on. */
        if ((value & TEXT_ID_SECTION_MASK) == TEXT_ID_MAP_NAME_BASE) {
            menu_stack_push_message_level((value & TEXT_ID_ENTRY_MASK) + 0x8801, 1);
        } else {
            value &= TEXT_ID_ENTRY_MASK;
            text = value + 0xFEE;
            if (value >= (TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST & TEXT_ID_ENTRY_MASK)) {
                text = value + 0xFA3;
            }
            menu_stack_push_message_level(text, 1);
        }
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_file_still_loading == 0) {
        world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
        audio_queue_add(AUDIO_COMMAND_FADE_OUT, 0x10);
        audio_queue_add(AUDIO_COMMAND_UNLOAD, 2);
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->slots.result = 1;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Pop the current menu level and give back the display records it appended:
 * each of the four slots that is not -1 releases one window or render record
 * and its queue entry. With `restore` the level underneath is resumed. Also
 * unblocks the menu buttons and re-shows the location label. */
static void menu_stack_pop_and_release_records(menu_stack_slot_set_t* slots, s32 restore) {
    g_menu_blocked_buttons = 0;
    if (slots->window_a != -1) {
        g_window_record_count -= 1;
        g_window_render_object_count -= 1;
    }
    if (slots->render_a != -1) {
        g_window_render_record_count -= 1;
        g_window_render_object_count -= 1;
    }
    if (slots->window_b != -1) {
        g_window_record_count -= 1;
        g_window_render_object_count -= 1;
    }
    if (slots->render_b != -1) {
        g_window_render_record_count -= 1;
        g_window_render_object_count -= 1;
    }
    g_menu_stack_depth -= 1;
    if (restore != 0) {
        menu_stack_dispatch_resume();
    }
    g_system_flags &= ~SYSTEM_FLAG_HIDE_LOCATION_LABEL;
}

/* At full brightness, run WORLD's shop, soldier office or fur shop screen
 * (shop_run_screen), then reload the war funds, reset the parent
 * level's window, push the location menu again, fade in, blank the backdrop
 * image and restart the world-map music.
 *
 * The local alias for the current location id reproduces the target's
 * saved-register base for its two loads. */
static void location_run_shop(s32 shop_id) {
    s32* selected;
    u32* clear;
    s32 i;

    screen_set_brightness(0x80, 0x80, 0x80);
    audio_queue_wait_until_empty();
    shop_run_screen(shop_id);
    g_hud_value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    world_map_suspend_cursor_level(&g_menu_stack_records_next[g_menu_stack_depth - 1]);
    selected = &g_world_map_projection_state.marker.kind;
    g_menu_stack_records_next[g_menu_stack_depth].panel.entry_count
        = location_build_menu_entries(*selected, g_menu_stack_records_next[g_menu_stack_depth].panel.entries);
    location_push_menu_level(*selected);
    world_map_fade_start(WORLD_MAP_FADE_REDRAW_MAP, 0x10);
    world_map_clear_screen_and_set_clip_rect(0);
    /* Blank the backdrop image from the end. */
    i = WORLD_MAP_BACKDROP_WORDS - 1;
    clear = &g_world_map_scratch_buffer[WORLD_MAP_BACKDROP_WORDS - 1];
    for (; i >= 0; i--) {
        *clear = 0;
        clear--;
    }
    audio_queue_start_music(AUDIO_MUSIC(1, MUSIC_WORLD_MAP));
}

/* Pushes g_text_window on message `text` as
 * MENU_STACK_LEVEL_SCROLLABLE_TEXT: it appends one window record for
 * the header, measures the text, sizes the window around it (at most four
 * rows), initializes the scrollable text state, hides the text render record
 * and the parent level's window until the step shows them, and hides the
 * location label and freezes the map animation. */
void menu_stack_push_scrollable_text_level(s32 text) {
    s16 width;
    s16 rows;

    text_measure_pixels(&width, &rows, text_find_entry(text));
    if (rows > 4) {
        rows = 4;
    }

    {
        /* Sixteen frame bytes above the measured size that retail reserves
         * and never touches. */
        s32 unused[4];
        s32 index;
        s32 depth;

        index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
        g_menu_stack_records_next[g_menu_stack_depth].window_index = index;
        g_window_records[index].sequence = WINDOW_ANIM_CHOICE_PANEL_HEADER;
        g_window_records[index].priority = 3;
        g_window_records[index].anim_counter = 0;
        g_window_records[index].frame_index = 0;
        g_window_records[index].flags |= DISPLAY_OBJECT_HIDDEN;

        g_text_window.text_id = text;
        g_text_window.image_x = 0;
        g_text_window.image_y = 0x60;
        g_text_window.base.x = -(((width + 24) & 0xFFFC) >> 1);
        g_text_window.base.y = -((rows << 3) + 8);
        g_text_window.text_width = width;
        g_text_window.rows_per_page = rows;
        g_text_window.priority = 3;
        g_text_window.image_coordinate_mode = 0;
        g_text_window.extra_render_index = -1;
        g_text_window.text_substitutions[1] = -1;
        g_text_window.text_substitutions[0] = -1;
        text_window_open(&g_text_window);

        g_window_render_records[g_text_window.render_record_index].flags |= DISPLAY_OBJECT_HIDDEN;
        depth = g_menu_stack_depth;
        g_window_records[g_menu_stack_records_next[depth].window_index].position.x = g_text_window.base.x + 3;
        g_window_records[g_menu_stack_records_next[depth].window_index].position.y = g_text_window.base.y - 2;
        g_system_flags |= SYSTEM_FLAG_HIDE_LOCATION_LABEL | SYSTEM_FLAG_FREEZE_ANIMATION;
        g_menu_stack_depth = depth + 1;
        g_window_records[g_menu_stack_records_next[depth - 1].window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        g_menu_stack_records_next[depth].scrollable_text._unknown_20 = 0x28;
        g_menu_stack_types[depth + 1] = MENU_STACK_LEVEL_SCROLLABLE_TEXT;
    }
}

/* Menu-level step for the scrollable text window g_text_window.
 * While its render record is hidden, the first frame without a screen fade
 * plays SOUND_SFX_WINDOW_OPEN and shows the header window and the text.
 * Otherwise, once text_window_step reports the window
 * closed, it releases the header window, pops the level, shows the location label
 * and unfreezes the map, shows the parent level's window and resumes the
 * parent level. */
void menu_stack_step_scrollable_text_level(menu_stack_record_t* level) {
    s32* index = &g_text_window.render_record_index;
    s32 depth;

    if (g_window_render_records[*index].flags & DISPLAY_OBJECT_HIDDEN) {
        if (!(g_system_flags & SYSTEM_FLAG_FADING)) {
            sound_play_effect(SOUND_SFX_WINDOW_OPEN);
            g_window_records[level->window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
            g_window_render_records[*index].flags &= ~DISPLAY_OBJECT_HIDDEN;
        }
    } else if (!(g_window_render_records[*index].flags & DISPLAY_OBJECT_POP_IN)
        && text_window_step(&g_text_window) == 0) {
        g_window_record_count--;
        g_window_render_object_count--;
        depth = g_menu_stack_depth;
        g_menu_stack_depth = depth - 1;
        g_system_flags &= ~(SYSTEM_FLAG_HIDE_LOCATION_LABEL | SYSTEM_FLAG_FREEZE_ANIMATION);
        g_window_records[g_menu_stack_records_next[depth - 2].window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
        menu_stack_dispatch_resume();
    }
}

/* Push the debug "Party number" panel (SYSTEM_FLAG_REQUEST_FIXED_NUMBER_PANEL):
 * a number from 0 to 999, starting at 0. Sibling of
 * debug_push_variable_edit_level. */
static void debug_push_party_number_level(void) {
    point32_t origin;
    s32 record_index;

    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS;
    g_menu_stack_ot_offset = 3;
    origin.x = -0x28;
    origin.y = -0x16;
    g_window_panel_value = 0;
    window_start_number_panel_thread(
        WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 999 << 16, TEXT_ID_PARTY_NUMBER_LABEL, &origin);
    record_index = g_menu_stack_depth;
    g_menu_stack_records_next[record_index].panel.result = 0;
    g_menu_stack_types[record_index + 1] = MENU_STACK_LEVEL_DEBUG_PARTY_NUMBER;
    g_menu_stack_depth = record_index + 1;
}

/* Step of the debug party number panel: Circle or Cross closes it, and once
 * the panel thread has ended a confirmed number below 0x200 is loaded with
 * encounter_init_event_units and the roster names are rebuilt.
 *
 * level->result is 0 while waiting, 1 on confirm and -1 on cancel. */
void debug_step_party_number_level(menu_stack_panel_level_t* level) {
    u32 buttons;

    if ((level->result != 0) && (thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0)) {
        g_menu_blocked_buttons = 0;
        g_menu_stack_ot_offset = 1;
        g_menu_stack_depth--;
        if (level->result == 1) {
            if (g_window_panel_value < 0x200) {
                encounter_init_event_units(g_window_panel_value);
                formation_build_roster_names();
                g_system_flags |= SYSTEM_FLAG_DRAW_MAP;
            }
        }
        menu_stack_dispatch_resume();
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if ((buttons & PSX_PAD_CROSS) != 0) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->result = -1;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
    } else if ((buttons & PSX_PAD_CIRCLE) != 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->result = 1;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
    }
}

/* Opens the bar (MENU_STACK_LEVEL_BAR): hides the map, loads the bar
 * background (set 0x16 or 0x17 by EVENT_SCRIPT_VAR_TOWN_BACKGROUND, else 0),
 * starts the message box on TEXT_ID_BAR_WELCOME, and allocates two
 * window records and one render record for the level before
 * bar_build_rows fills it. */
static void bar_push_level(void) {
    s32 value;
    s32 set;
    s32 record_index;

    world_map_clear_screen_and_set_clip_rect(1);
    window_set_render_objects_visible(0);
    world_map_fade_start(0, 0x10);
    value = script_get_variable(EVENT_SCRIPT_VAR_TOWN_BACKGROUND);
    set = value == 1 ? 0x16 : 0;
    if (value == 2) {
        set = 0x17;
    }
    picture_load_background_set(set);
    file_wait_for_pending();
    g_system_flags |= SYSTEM_FLAG_HIDE_CALENDAR_DATE | SYSTEM_FLAG_HIDE_LOCATION_LABEL;
    thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BAR_WELCOME, 0);
    g_menu_stack_records_next[g_menu_stack_depth].window_pair_render.first_window
        = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records_next[g_menu_stack_depth].window_pair_render.second_window
        = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records_next[g_menu_stack_depth].window_pair_render.render_index
        = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    bar_build_rows(&g_menu_stack_records_next[g_menu_stack_depth].list_window);
    record_index = g_menu_stack_depth;
    g_menu_stack_records_next[record_index].window_pair_render.panel_rows = 0;
    g_menu_stack_types[record_index + 1] = MENU_STACK_LEVEL_BAR;
    g_menu_stack_depth = record_index + 1;
}

/* Fill the bar menu: Rumors always, Propositions once EVENT_SCRIPT_VAR_PROPOSITIONS_ENABLED is
 * set, the job report while jobs have returned to this town, the in-progress list while some
 * accepted job has not returned, then Leave; draw the rows into the frame window. */
void bar_build_rows(menu_stack_window_level_t* level) {
    s32 index;
    point32_t point;
    RECT bounds;
    s32 unused[8];
    menu_cursor_state_t* cursor;
    point32_t* base;
    window_render_record_t* render;
    s32 i;

    index = level->main_window;
    g_window_records[index].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[index].priority = 0xA;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
    index = level->side_window;
    g_window_records[index].sequence = WINDOW_ANIM_LIST_SIDE_LABEL;
    g_window_records[index].priority = 0xA;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
    level->entries[0] = BAR_ROW_RUMORS;
    level->entry_count = 1;
    if (script_get_variable(EVENT_SCRIPT_VAR_PROPOSITIONS_ENABLED) != 0) {
        level->entries[1] = BAR_ROW_AVAILABLE_PROPOSITIONS;
        level->entry_count++;
    }

    if (proposition_count_returned(g_world_map_projection_state.marker.kind) != 0) {
        level->entries[level->entry_count] = BAR_ROW_RETURNED;
        level->entry_count++;
    }

    if (g_proposition_count != proposition_count_returned(-1)) {
        level->entries[level->entry_count] = BAR_ROW_ACTIVE_PROPOSITIONS;
        level->entry_count++;
    }

    level->entries[level->entry_count] = BAR_ROW_LEAVE;
    level->entry_count++;
    index = level->frame_render;
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = 0x48;
    bounds.h = level->entry_count * 0x10 + 0x10;
    window_build_render_record_image(index, bounds, 0, g_window_image_buffer);
    g_window_render_records[index].priority = 0xA;
    g_window_render_records[index].flags |= DISPLAY_OBJECT_POP_IN;
    base = &g_window_render_records[index].base;
    base->x = -0x78;
    base->y = -0x28;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < level->entry_count; i++) {
        window_display_text_entry(index, level->entries[i] + TEXT_ID_WORLD_MENU_BASE, point, g_window_image_buffer);
        point.y += 0x10;
    }

    window_load_render_record_image(index, g_window_image_buffer);
    cursor = &g_menu_stack_cursor_states[MENU_STACK_CURSOR_SLOT_BAR];
    if (level->entry_count < cursor->selected_index + 1) {
        cursor->selected_index = 0;
    }

    g_menu_stack_records_next[g_menu_stack_depth].list_window.selected_entry = cursor->selected_index;
    g_menu_stack_records_next[g_menu_stack_depth].list_window.mode = MENU_STACK_WINDOW_MODE_BROWSE;
    render = &g_window_render_records[index];
    g_window_records[level->main_window].position.x = g_window_render_records[index].base.x + 6;
    g_window_records[level->main_window].position.y
        = g_window_render_records[index].base.y + cursor->selected_index * 0x10 + 0xE;
    g_window_records[level->side_window].position.x = render->base.x + 3;
    g_window_records[level->side_window].position.y = g_window_render_records[index].base.y - 2;
}

/* Restore the bar list's palettes and cursor animation, then show its welcome text. */
void bar_resume_level(menu_stack_record_t* level) {
    s32 main_window;
    s32 side_window;
    s32 slot;
    /* The original frame reserves eight unused bytes. */
    s32 pad[2];

    main_window = level->list_window.main_window;
    side_window = level->list_window.side_window;
    g_window_render_records[level->list_window.frame_render].palette = 0;
    g_window_records[side_window].palette = 0;
    g_window_records[main_window].palette = 0;
    slot = level->list_window.main_window;
    g_window_records[slot].sequence = WINDOW_ANIM_CURSOR_IDLE;
    slot = level->list_window.main_window;
    g_window_records[slot].anim_counter = 0;
    g_window_records[slot].frame_index = 0;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BAR_WELCOME, 0);
}
