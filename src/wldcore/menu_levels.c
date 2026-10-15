/* Value, cursor, number and date drawing, the world frame, level dispatch, and the root, map cursor, focus, path,
 * visibility, entry, text, message, variable list, number panel, layout window and location menu levels. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/open.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

static void wldcore_bar_push_menu_level(void);
static void wldcore_gfx_draw_cursor_sprite_and_underline(GsOT* ot, GsSPRITE* sprite, GsLINE* line);
static void wldcore_gfx_draw_number_digits(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot);
static void wldcore_gfx_draw_number_digits_with_fill(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot, s32 zero_pad);
static void wldcore_menu_close_level_and_release_records(wldcore_menu_slot_set_t* slots, s32 restore);
static void wldcore_menu_push_fixed_number_panel_level(void);
static void wldcore_menu_push_focus_location_level(s32 dot_index);
static void wldcore_menu_push_map_path_level(s32 path_id, s32 path_id_hi, s32 erase);
static void wldcore_menu_push_number_panel_level_with_argument(s32 variable_id, s32 value);
static void wldcore_menu_push_root_level(void);
static void wldcore_menu_push_variable_list_level(void);
static void wldcore_menu_render_debug_variable_list(wldcore_menu_variable_list_level_t* level);
static void wldcore_menu_run_shop_and_rebuild_screen(s32 shop_id);
static void wldcore_push_map_location_visibility_level(s32 dot_index, s32 was_set);
static void wldcore_window_apply_layout(wldcore_window_layout_request_t* request);

/* Draws the HUD numeric readout at the context-value display position, laid
 * out by g_wldcore_context_value_display_mode: 1 is the labeled, underlined
 * value (war funds), 2 the play time as hours, minutes and seconds with
 * separators, 0 and 3 a plain eight-digit value (3 under a small label
 * sprite) and 4 a labeled five-digit secondary value plus the mode-1 readout.
 *
 * Case 3 falls through into case 0, so the case bodies are ordered 1, 2, 3, 0,
 * 4 to follow the jump table at 0x80067020. The color triple stores into
 * `separator` precede those into `sprite`. The position read back at the end
 * of case 4 is a plain 32-bit load narrowed by the store, where the chained
 * assignment before the switch reads only the low 16 bits. */
void wldcore_gfx_draw_context_value_display(GsOT* ot) {
    GsSPRITE sprite;
    GsSPRITE separator;
    GsLINE line;
    CVECTOR color;

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE) {
        return;
    }

    separator.attribute = sprite.attribute = 0;
    separator.tpage = sprite.tpage = GetTPage(0, 0, 0x3C0, 0x100);
    separator.cy = sprite.cy = 0x1E1;
    wldcore_gfx_copy_color_with_tint(&g_wldcore_hud_color, &color);
    separator.r = color.r;
    separator.g = color.g;
    separator.b = color.b;
    sprite.r = color.r;
    sprite.g = color.g;
    sprite.b = color.b;
    separator.x = sprite.x = g_wldcore_context_value_display_position.x;
    separator.y = sprite.y = g_wldcore_context_value_display_position.y;

    switch (g_wldcore_context_value_display_mode) {
    case WLDCORE_VALUE_DISPLAY_FUNDS:
        wldcore_gfx_draw_cursor_sprite_and_underline(ot, &sprite, &line);
        break;
    case WLDCORE_VALUE_DISPLAY_PLAY_TIME:
        sprite.u = 0x60;
        sprite.v = 0x10;
        sprite.w = 0x14;
        sprite.h = 8;
        sprite.cx = 0x20;
        GsSortFastSprite(&sprite, ot, g_wldcore_hud_ot_priority);

        separator.u = 0x28;
        separator.v = 0;
        separator.w = 6;
        separator.h = 8;
        separator.cx = 0x20;
        sprite.y += 8;
        separator.y = sprite.y + 4;
        sprite.x -= 8;
        wldcore_gfx_draw_number_digits(g_wldcore_displayed_numeric_value, 4, &sprite, ot);

        separator.x = sprite.x;
        GsSortFastSprite(&separator, ot, g_wldcore_hud_ot_priority);
        sprite.x += 6;
        wldcore_gfx_draw_number_digits_with_fill(g_wldcore_displayed_numeric_value_secondary, 2, &sprite, ot, 1);

        separator.x = sprite.x;
        GsSortFastSprite(&separator, ot, g_wldcore_hud_ot_priority);
        sprite.x += 6;
        wldcore_gfx_draw_number_digits_with_fill(g_wldcore_displayed_numeric_value_tertiary, 2, &sprite, ot, 1);
        break;
    case WLDCORE_VALUE_DISPLAY_LABELED:
        sprite.u = 0xA8;
        sprite.v = 0x20;
        sprite.w = 0xD;
        sprite.h = 9;
        sprite.cx = 0x20;
        GsSortFastSprite(&sprite, ot, g_wldcore_hud_ot_priority);
        /* fallthrough */
    case WLDCORE_VALUE_DISPLAY_PLAIN:
        sprite.x -= 0xC;
        sprite.y += 8;
        wldcore_gfx_draw_number_digits(g_wldcore_displayed_numeric_value, 8, &sprite, ot);
        break;
    case WLDCORE_VALUE_DISPLAY_COST_AND_FUNDS:
        sprite.u = 0x60;
        sprite.w = 0x18;
        sprite.h = 8;
        sprite.v = 0;
        sprite.cx = 0x20;
        sprite.x -= 0x40;
        GsSortFastSprite(&sprite, ot, g_wldcore_hud_ot_priority);

        sprite.x += 4;
        sprite.y += 8;
        wldcore_gfx_draw_number_digits_with_fill(g_wldcore_displayed_numeric_value_secondary, 5, &sprite, ot, 0);

        sprite.x = g_wldcore_context_value_display_position.x;
        sprite.y = g_wldcore_context_value_display_position.y;
        wldcore_gfx_draw_cursor_sprite_and_underline(ot, &sprite, &line);
        break;
    }
}

/* Draws one cursor sprite plus its underline into the ordering table, then
 * hands the sprite to 0x8006bd84 for the trailing decoration.
 *
 * The line is derived from the sprite's current position; the sprite origin is
 * then advanced by (4, 8) so the follow-up draws at the shifted position. */
static void wldcore_gfx_draw_cursor_sprite_and_underline(GsOT* ot, GsSPRITE* sprite, GsLINE* line) {
    /* u16: the target reads GsSPRITE's short x/y with lhu. */
    u16 x;
    u16 y;

    sprite->u = 0x9C;
    sprite->v = 0x78;
    sprite->w = 0x29;
    sprite->h = 8;
    sprite->cx = 0x20;
    GsSortFastSprite(sprite, ot, g_wldcore_hud_ot_priority);

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
    GsSortLine(line, ot, g_wldcore_hud_ot_priority);

    sprite->x += 4;
    sprite->y += 8;
    wldcore_gfx_draw_number_digits_with_fill(g_wldcore_displayed_numeric_value, 8, sprite, ot, 0);
}

/* The target wraps `value / divisor` in ASPSX's zero and INT_MIN traps
 * (divcheck expansion). Splitting the division and the remainder into two
 * statements gives the target's order; `s32 saved_y` (not s16) makes the
 * saved y load `lh`. */

/* Draws value right-aligned as digits glyphs, one 8x12 or 8x15 cell per
 * digit, stepping the sprite eight pixels right per column. Leading zeros are
 * skipped: a column only draws once the running divisor has dropped to or
 * below value, and the last column always draws. */
static void wldcore_gfx_draw_number_digits(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot) {
    s32 divisor;
    s32 i;
    s32 digit;
    s32 saved_y;

    divisor = 1;
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
            GsSortFastSprite(sprite, ot, g_wldcore_hud_ot_priority);
            sprite->y = saved_y;
        }
        sprite->x = sprite->x + 8;
        divisor = divisor / 10;
        i++;
    }
}

/* Draws value right-aligned in `digits` columns like
 * wldcore_gfx_draw_number_digits, but also draws the columns ahead of the
 * first significant digit: a zero glyph when `zero_pad` is set, otherwise the
 * 8x8 filler glyph at (0xd0, 0x28). */
static void wldcore_gfx_draw_number_digits_with_fill(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot, s32 zero_pad) {
    s32 divisor;
    s32 y;
    s32 digit;
    s32 i;

    divisor = 1;
    i = 0;
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
            GsSortFastSprite(sprite, ot, g_wldcore_hud_ot_priority);
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
            GsSortFastSprite(sprite, ot, g_wldcore_hud_ot_priority);
            sprite->y = y;
        }
        sprite->x += 8;
        divisor /= 10;
    }
}

/* Draws the world map's date: the month's zodiac sequence for the month
 * script variable, then the day as one or two digit sequences 0x1A pixels to
 * its right, in the tinted HUD color. Nothing is drawn while
 * MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE is set. */
void wldcore_gfx_draw_calendar_date(GsOT* ot) {
    world_animated_render_record_t request;
    CVECTOR color;
    s32 first;
    s32 second;

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE) {
        return;
    }

    request.flags = WORLD_RENDER_RECORD_ANIMATED;
    request.priority = 4;
    request.palette = 0;
    request.anim_counter = 0;
    request.frame_index = 0;
    request.position.x = g_wldcore_calendar_display_x;
    request.position.y = g_wldcore_calendar_display_y;
    wldcore_gfx_copy_color_with_tint(&g_wldcore_hud_color, &color);
    request.color.r = color.r;
    request.color.g = color.g;
    request.color.b = color.b;

    first = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
    second = script_get_variable(EVENT_SCRIPT_VAR_DAY);
    request.sequence = first + (WLDCORE_ANIM_SEQUENCE_MONTH_FIRST - 1);
    wldcore_gfx_draw_animated_display_object(&request, ot);

    request.position.x += 0x1A;
    request.position.y -= 4;
    if (second >= 10) {
        request.anim_counter = 0;
        request.frame_index = 0;
        request.sequence = second / 10 + WLDCORE_ANIM_SEQUENCE_DIGIT_FIRST;
        wldcore_gfx_draw_animated_display_object(&request, ot);
        request.position.x += 8;
    }
    request.sequence = second % 10 + WLDCORE_ANIM_SEQUENCE_DIGIT_FIRST;
    wldcore_gfx_draw_animated_display_object(&request, ot);
}

/* Builds the world-map request block for the current entry at 0x800d0bb4 and
 * submits it, unless system flag 0x2000 suppresses it or no entry is active.
 *
 * The color is the 32-bit shade value at 0x8009eef4 replicated across the three
 * channels and tinted in place by 0x80069718. sequence is written after that
 * call because the target reloads 0x800d0bb4 there. */
void wldcore_proposition_submit_marker_request(GsOT* ot) {
    world_animated_render_record_t request;
    CVECTOR color;
    s32 index;
    s32 shade;

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER) {
        return;
    }
    if (g_wldcore_proposition_selected_entry == 0) {
        return;
    }

    index = g_wldcore_proposition_selected_entry - 1;
    request.flags = WORLD_RENDER_RECORD_ANIMATED;
    request.position.x = g_wldcore_map_dots[index].render.position.x;
    request.position.y = g_wldcore_map_dots[index].render.position.y;
    shade = g_wldcore_location_marker_shade;
    request.priority = 4;
    request.palette = 0;
    request.anim_counter = 0;
    request.frame_index = 0;
    color.r = shade;
    color.g = shade;
    color.b = shade;
    wldcore_gfx_copy_color_with_tint(&color, &color);
    request.color.r = color.r;
    request.color.g = color.g;
    request.color.b = color.b;
    request.sequence = g_wldcore_proposition_selected_entry + (WLDCORE_ANIM_SEQUENCE_LOCATION_NAME_FIRST - 1);
    wldcore_gfx_draw_animated_display_object(&request, ot);
}

void wldcore_load_formation_unit_name_and_face(void) {
    s32 value;

    world_menu_load_text_1bd8_with_face();
    world_text_generate_formation_unit_name_string();
    value = script_get_variable(EVENT_SCRIPT_VAR_DATE_ADVANCE);
    if (value != 0) {
        wldcore_calendar_advance_days(value);
        script_set_variable(EVENT_SCRIPT_VAR_DATE_ADVANCE, 0);
    }
}

/*
 * Runs the WORLD menu code for one world-map frame, drawing into `context`.
 *
 * Skipped while MAIN_SYSTEM_FLAG_SKIP_MENU_FRAME is set. While the unit panel
 * threads are in use it draws the window frames, runs the event input update
 * and plays the frame's sound effect itself; otherwise it runs the formation
 * menu frame. Buttons in g_wldcore_menu_blocked_buttons are withheld from
 * the menus.
 */
void wldcore_menu_run_world_frame(GsOT* context) {
    u32* otag_entries;
    u32* sprite_entries;
    u32* menu_entries;

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_SKIP_MENU_FRAME) {
        return;
    }
    world_formation_set_ordering_table_pointer((u32*)(context->org + 15));
    world_formation_set_menu_allowed_buttons(~g_menu_blocked_buttons);
    if (g_main_system_flags & MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS) {
        otag_entries = (u32*)context->org;
        g_gfx_current_otag_entry = otag_entries + g_wldcore_menu_ordering_table_offset;
        menu_draw_active_window_frames_variant();
        sprite_entries = (u32*)context->org;
        menu_input_update_event_state_variant(sprite_entries + g_wldcore_menu_ordering_table_offset + 2,
            g_input_current_buttons, g_active_graphics_buffer_index * 240);
        if (g_sound_pending_sfx_id != -1) {
            wldcore_sound_play_effect(g_sound_pending_sfx_id);
        }
    } else {
        menu_entries = (u32*)context->org;
        world_formation_run_menu_frame(menu_entries + g_wldcore_menu_ordering_table_offset, g_input_current_buttons);
    }
}

/* Empty the menu stack, push the root level and place its window at the
 * projected location marker (4 pixels above it). */
void wldcore_menu_reset_window_list_and_store_origin(void) {
    g_wldcore_menu_stack_depth = 0;
    wldcore_menu_push_root_level();
    g_wldcore_window_records[g_wldcore_menu_stack_records[1].window_index].position.x
        = g_wldcore_map_projection_state.marker.render.position.x;
    g_wldcore_window_records[g_wldcore_menu_stack_records[1].window_index].position.y
        = g_wldcore_map_projection_state.marker.render.position.y - 4;
}

void wldcore_menu_dispatch_idle_handler(void) {
    s32 index;

    index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_idle_handlers[g_wldcore_menu_stack_types[index]](&g_wldcore_menu_stack_records[index]);
}

void wldcore_menu_dispatch_resume_handler(void) {
    s32 index;

    index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_resume_handlers[g_wldcore_menu_stack_types[index]](&g_wldcore_menu_stack_records[index]);
}

void wldcore_menu_dispatch_publish_handler(void) {
    s32 index;

    index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_publish_handlers[g_wldcore_menu_stack_types[index]](&g_wldcore_menu_stack_records[index]);
}

/* Pushes a two-window menu level: appends two window records, points the new
 * menu-stack record at both, resets both scroll states and records the
 * current location (+1) for the level.
 *
 * The level record is re-read into its own local after the script call; reusing
 * the window-level depth local changes the register allocation. */
static void wldcore_menu_push_root_level(void) {
    wldcore_window_xy_t* xy;
    world_animated_render_record_t* records;
    s32 window_index;
    s32 depth;
    s32 level;

    window_index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);

    /* Taken from the record array: naming g_wldcore_window_record_positions here changes the
     * target's address form. */
    xy = (wldcore_window_xy_t*)&g_wldcore_window_records[0].position.x;
    records = g_wldcore_window_records;

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].window_pair.first_window = window_index;
    g_wldcore_window_records[window_index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_STILL;
    g_wldcore_window_records[window_index].priority = 12;
    xy[window_index].x = 0;
    xy[window_index].y = 0;
    records[window_index].anim_counter = 0;
    g_wldcore_window_records[window_index].frame_index = 0;

    window_index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_context_value_display_mode = WLDCORE_VALUE_DISPLAY_FUNDS;
    g_wldcore_menu_stack_records_next[depth].window_pair.second_window = window_index;
    g_wldcore_window_records[window_index].sequence = WLDCORE_ANIM_SEQUENCE_MAP_EDGE_ARROWS;
    g_wldcore_window_records[window_index].priority = 12;
    g_wldcore_window_records[window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    xy[window_index].x = 0;
    xy[window_index].y = 0;
    records[window_index].anim_counter = 0;
    g_wldcore_window_records[window_index].frame_index = 0;

    g_wldcore_map_cursor_scroll_y.limit = 0x400;
    g_wldcore_map_cursor_scroll_x.limit = 0x400;
    g_wldcore_map_cursor_scroll_y.value = 0;
    g_wldcore_map_cursor_scroll_x.value = 0;
    g_wldcore_map_cursor_scroll_y.output = 0;
    g_wldcore_map_cursor_scroll_x.output = 0;
    g_wldcore_map_cursor_scroll_y.decay = 0x80;
    g_wldcore_map_cursor_scroll_x.decay = 0x80;
    g_wldcore_map_cursor_scroll_y.step = 0x80;
    g_wldcore_map_cursor_scroll_x.step = 0x80;

    g_wldcore_displayed_numeric_value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    level = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[level].map_cursor.wide_view = 0;
    g_wldcore_menu_stack_records_next[level].map_cursor.snap_hit = g_wldcore_map_projection_state.marker.kind + 1;
    g_wldcore_menu_stack_records_next[level].map_cursor.pending_mode = 0;
    g_wldcore_menu_stack_depth = level + 1;
    g_wldcore_menu_stack_types[level + 1] = WLDCORE_MENU_LEVEL_ROOT;
}

/* Resume handler of the world map's root level: show the war funds in the
 * context value display, reset the level's window palette, restore the live
 * map if the captured frame is showing and run any pending script requests. */
void wldcore_menu_resume_root_level(wldcore_menu_stack_record_t* level) {
    g_wldcore_context_value_display_mode = WLDCORE_VALUE_DISPLAY_FUNDS;
    g_wldcore_window_records[level->map_cursor.window_index].palette = 0;
    g_wldcore_displayed_numeric_value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    if (g_wldcore_window_records[g_wldcore_map_window_index].flags & WORLD_RENDER_RECORD_HIDDEN) {
        wldcore_gfx_toggle_captured_world_frame(0);
    }
    wldcore_menu_dispatch_pending_script_requests();
}

/* Reset the window palette and both map-cursor scroll axes. The scroll fields
 * preserve the original store order after the indexed window store. */
void wldcore_window_reset_state_and_scroll(wldcore_menu_stack_record_t* level) {
    g_wldcore_window_records[level->window_index].palette = 10;
    g_wldcore_map_cursor_scroll_y.value = 0;
    g_wldcore_map_cursor_scroll_x.value = 0;
    g_wldcore_map_cursor_scroll_y.output = 0;
    g_wldcore_map_cursor_scroll_x.output = 0;
}

/* Once the current location's script yields a map-menu action, reset the
 * current level's window and dispatch its path, location-visibility or focus
 * request. A focus location of 0xff means the current location.
 *
 * The flag tests are signed `> 0` comparisons; `!= 0` folds to srl/andi. */
void wldcore_menu_dispatch_pending_script_requests(void) {
    s32* location = &g_wldcore_map_projection_state.marker.kind;
    s32* flags;
    s32 id;

    if (wldcore_script_process_conditional_set(*location, WLDCORE_SCRIPT_ACTION_MAP_MENU_MASK) == 0) {
        return;
    }
    flags = &g_wldcore_script_state.flags;
    if (*flags & WLDCORE_SCRIPT_ACTION_PATH_MASK) {
        wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth]);
        wldcore_menu_push_map_path_level(g_wldcore_script_state.args[0], g_wldcore_script_state.args[1],
            (*flags & WLDCORE_SCRIPT_ACTION_ERASE_PATH) > 0);
    } else if (*flags & WLDCORE_SCRIPT_ACTION_LOCATION_VISIBILITY_MASK) {
        wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth]);
        wldcore_push_map_location_visibility_level(
            g_wldcore_script_state.args[0], (*flags & WLDCORE_SCRIPT_ACTION_ERASE_LOCATION) > 0);
    } else if (*flags & WLDCORE_SCRIPT_ACTION_FOCUS_LOCATION) {
        wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth]);
        id = g_wldcore_script_state.args[0];
        if (id == 0xFF) {
            id = *location;
        }
        wldcore_menu_push_focus_location_level(id);
    }
}

/* Per-frame step of the world map cursor.
 *
 * While the projection zoom animation (g_wldcore_map_zoom_motion.flags
 * bit 0) is running it interpolates the origin and the projection scale over
 * `duration` frames, in one direction or the other according to bit 1, and
 * raises system flag 0x2 plus projection flag 0x1 each frame.
 *
 * Otherwise it handles input: confirm (0x20) either walks the marker to
 * another location (0x8008e2bc), opens that location's menu (0x8006faf0) or,
 * with no dot under the cursor, hides the cursor window and hands off to
 * 0x8006d7f4; 0x810 closes the level through 0x8006e0fc; 0x100 opens the
 * location's help message; 0xC starts or ends the zoom animation; holding
 * 0x80 swaps the cursor window for the wide-view window. With no modal state
 * it accelerates the two scroll axes, snaps the cursor toward the nearest dot
 * through wldcore_map_get_dot_snap_step, scrolls the projection origin when
 * the cursor approaches a screen edge, and finally clamps the cursor window to
 * x in [-0x70, 0x78] and y in [-0x68, 0x68].
 *
 * The zoom interpolation uses four signed divisions by `duration`
 * (gcc-2.7.2_O2_aspsx-2.21_divcheck). `point` is never written; it
 * reproduces the target's unused 8-byte slot at sp+0x18. The chained scale
 * stores keep each branch's scale value a single-set temporary; one variable
 * shared by both branches is multi-set, is scheduled early and changes the
 * register allocation of the interpolation. */
void wldcore_menu_step_map_cursor_level(wldcore_menu_stack_record_t* level) {
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
    if (g_wldcore_location_entry_state.flags & WLDCORE_ROUTE_TRAVEL_ACTIVE) {
        g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
        goto finish;
    }

    if (level->map_cursor.wide_view == 0 && !(g_wldcore_previous_system_flags & MAIN_SYSTEM_FLAG_DRAW_MAP)) {
        if (level->map_cursor.pending_mode != 0) {
            flags = g_main_system_flags;
            if (flags & MAIN_SYSTEM_FLAG_FADING) {
                return;
            }
            g_main_system_flags = flags ^ MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            if (level->map_cursor.pending_mode == 3) {
                g_main_system_flags = flags ^ (MAIN_SYSTEM_FLAG_RUNNING | MAIN_SYSTEM_FLAG_INPUT_BLOCKED);
            }
            level->map_cursor.pending_mode = 0;
        }
        if (g_main_system_flags & MAIN_SYSTEM_FLAG_FADING) {
            /* Skips the zoom handling and the wide-view toggle below. */
            goto scroll;
        }
        if (!(g_wldcore_map_zoom_motion.flags
                & (WLDCORE_PROJECTION_MOTION_ACTIVE | WLDCORE_PROJECTION_MOTION_ZOOMED_OUT))
            && g_wldcore_audio_queue.count == 0) {
            if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
                hit = wldcore_map_find_dot_at_point(g_wldcore_window_records[level->map_cursor.window_index].position);
                if (hit != 0) {
                    wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
                    hit--;
                    if (hit != g_wldcore_map_projection_state.marker.kind) {
                        wldcore_location_process_entry(g_wldcore_map_projection_state.marker.kind, hit);
                        return;
                    }
                    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count
                        = wldcore_map_build_location_menu_entries(
                            hit, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entries);
                    if (g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count != 0) {
                        wldcore_window_reset_state_and_scroll(level);
                        wldcore_menu_push_location_menu_level(hit);
                        return;
                    }
                } else {
                    g_wldcore_window_records[level->map_cursor.window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
                    wldcore_menu_push_focus_location_level(g_wldcore_map_projection_state.marker.kind);
                    return;
                }
            }
            buttons = g_wldcore_input_new_button_presses;
            if (buttons & (PSX_PAD_TRIANGLE | PSX_PAD_START)) {
                wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
                wldcore_window_reset_state_and_scroll(level);
                wldcore_menu_push_entry_window_level();
                return;
            }
            if (buttons & PSX_PAD_SELECT) {
                hit = wldcore_map_find_dot_at_point(g_wldcore_window_records[level->map_cursor.window_index].position);
                if (hit != 0) {
                    hit--;
                    wldcore_window_reset_state_and_scroll(level);
                    wldcore_gfx_toggle_captured_world_frame(1);
                    wldcore_menu_push_message_level(hit + 0x8000, 1);
                    return;
                }
            }
        }
    }

    motion = g_wldcore_map_zoom_motion.flags;
    if (!(motion & WLDCORE_PROJECTION_MOTION_ACTIVE)) {
        if (level->map_cursor.wide_view == 0 && (g_wldcore_input_new_button_presses & (PSX_PAD_L1 | PSX_PAD_R1))) {
            g_wldcore_window_records[level->map_cursor.window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_map_zoom_motion.duration = 0x20;
            g_wldcore_map_zoom_motion.frame = 0;
            g_wldcore_map_zoom_motion.flags = (g_wldcore_map_zoom_motion.flags ^ WLDCORE_PROJECTION_MOTION_ZOOMED_OUT)
                | WLDCORE_PROJECTION_MOTION_ACTIVE;
            if (g_wldcore_map_zoom_motion.flags & WLDCORE_PROJECTION_MOTION_ZOOMED_OUT) {
                g_wldcore_map_zoom_motion.origin = g_wldcore_map_projection_view.origin;
            }
            level->map_cursor.snap_hit = 0;
            if (g_wldcore_map_zoom_motion.flags & WLDCORE_PROJECTION_MOTION_ZOOMED_OUT) {
                wldcore_sound_play_effect(MAIN_SFX_WORLD_MAP_ZOOM_OUT);
            } else {
                wldcore_sound_play_effect(MAIN_SFX_WORLD_MAP_ZOOM_IN);
            }
            goto finish;
        }
    } else {
        frame = g_wldcore_map_zoom_motion.frame;
        duration = g_wldcore_map_zoom_motion.duration;
        step_x = (frame << 11) / duration;
        if (motion & WLDCORE_PROJECTION_MOTION_ZOOMED_OUT) {
            g_wldcore_map_projection_view.origin.vx
                = g_wldcore_map_zoom_motion.origin.vx + ((0 - g_wldcore_map_zoom_motion.origin.vx) * frame) / duration;
            g_wldcore_map_projection_view.origin.vy
                = g_wldcore_map_zoom_motion.origin.vy + ((8 - g_wldcore_map_zoom_motion.origin.vy) * frame) / duration;
            g_wldcore_map_projection_view.scale.vx = g_wldcore_map_projection_view.scale.vy = ONE - step_x;
            g_wldcore_map_zoom_motion.frame = frame + 1;
            g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_MAP_DOTS | MAIN_SYSTEM_FLAG_HIDE_MAP_ROUTES;
            if (frame + 1 == duration) {
                g_wldcore_map_projection_view.scale.vy = 0x800;
                g_wldcore_map_projection_view.scale.vx = 0x800;
                g_wldcore_map_projection_view.origin.vx = 0;
                g_wldcore_map_projection_view.origin.vy = 8;
                g_wldcore_map_zoom_motion.flags = motion ^ WLDCORE_PROJECTION_MOTION_ACTIVE;
            }
        } else {
            g_wldcore_map_projection_view.origin.vx = (g_wldcore_map_zoom_motion.origin.vx * frame) / duration;
            g_wldcore_map_projection_view.origin.vy
                = ((g_wldcore_map_zoom_motion.origin.vy - 8) * frame) / duration + 8;
            g_wldcore_map_projection_view.scale.vx = g_wldcore_map_projection_view.scale.vy = step_x + 0x800;
            g_wldcore_map_zoom_motion.frame = frame + 1;
            if (frame + 1 == duration) {
                g_wldcore_map_projection_view.scale.vy = ONE;
                g_wldcore_map_projection_view.scale.vx = ONE;
                g_wldcore_map_projection_view.origin = g_wldcore_map_zoom_motion.origin;
                g_wldcore_map_zoom_motion.flags ^= WLDCORE_PROJECTION_MOTION_ACTIVE;
                g_main_system_flags &= ~(MAIN_SYSTEM_FLAG_HIDE_MAP_DOTS | MAIN_SYSTEM_FLAG_HIDE_MAP_ROUTES);
            }
        }
        g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
        g_wldcore_map_projection_state.flags |= WLDCORE_PROJECTION_FLAG_UPDATE_POSITION;
        goto finish;
    }

    if (g_wldcore_map_zoom_motion.flags & WLDCORE_PROJECTION_MOTION_ZOOMED_OUT) {
        goto finish;
    }

    if (!(g_input_current_buttons & PSX_PAD_SQUARE)) {
        if (level->map_cursor.wide_view != 0) {
            level->map_cursor.wide_view = 0;
            g_wldcore_window_records[level->map_cursor.alt_window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        }
        g_wldcore_window_records[level->map_cursor.window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    } else if (level->map_cursor.wide_view != 1) {
        level->map_cursor.wide_view = 1;
        g_wldcore_window_records[level->map_cursor.window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->map_cursor.alt_window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->map_cursor.alt_window_index].anim_counter
            = g_wldcore_window_records[level->map_cursor.alt_window_index].frame_index = 0;
        g_wldcore_map_cursor_scroll_y.value = 0;
        g_wldcore_map_cursor_scroll_x.value = 0;
        g_wldcore_map_cursor_scroll_y.output = 0;
        g_wldcore_map_cursor_scroll_x.output = 0;
    }

scroll:
    if (level->map_cursor.wide_view == 0) {
        held = g_input_current_buttons;
        if (held & (PSX_PAD_RIGHT | PSX_PAD_LEFT)) {
            if (held & PSX_PAD_LEFT) {
                wldcore_map_update_scroll_axis_speed(&g_wldcore_map_cursor_scroll_x, -1);
            } else {
                wldcore_map_update_scroll_axis_speed(&g_wldcore_map_cursor_scroll_x, 1);
            }
        } else {
            wldcore_map_update_scroll_axis_speed(&g_wldcore_map_cursor_scroll_x, 0);
        }
        held = g_input_current_buttons;
        if (held & (PSX_PAD_UP | PSX_PAD_DOWN)) {
            if (held & PSX_PAD_UP) {
                wldcore_map_update_scroll_axis_speed(&g_wldcore_map_cursor_scroll_y, -1);
            } else {
                wldcore_map_update_scroll_axis_speed(&g_wldcore_map_cursor_scroll_y, 1);
            }
        } else {
            wldcore_map_update_scroll_axis_speed(&g_wldcore_map_cursor_scroll_y, 0);
        }
        amount.x = g_wldcore_map_cursor_scroll_x.output;
        amount.y = g_wldcore_map_cursor_scroll_y.output;
        if (!(g_wldcore_map_zoom_motion.flags & WLDCORE_PROJECTION_MOTION_ZOOMED_OUT)) {
            target.x = g_wldcore_window_records[level->map_cursor.window_index].position.x + amount.x;
            target.y = g_wldcore_window_records[level->map_cursor.window_index].position.y + amount.y;
            level->map_cursor.snap_hit = wldcore_map_get_dot_snap_step(target, &step_x, &step_y);
            amount.x += step_x;
            amount.y += step_y;
            target.x = g_wldcore_window_records[level->map_cursor.window_index].position.x + amount.x;
            target.y = g_wldcore_window_records[level->map_cursor.window_index].position.y + amount.y;
            saved = g_wldcore_map_projection_view.origin;
            if ((amount.x < 0 && target.x < -0x40) || (amount.x > 0 && target.x >= 0x49)) {
                wldcore_map_apply_clamped_horizontal_scroll(&amount.x, &g_wldcore_map_projection_view.origin.vx);
            }
            if ((amount.y < 0 && target.y < -0x40) || (amount.y > 0 && target.y >= 0x41)) {
                wldcore_map_apply_clamped_vertical_scroll(&amount, &g_wldcore_map_projection_view.origin);
            }
            if (saved.vx != g_wldcore_map_projection_view.origin.vx
                || saved.vy != g_wldcore_map_projection_view.origin.vy) {
                g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
                g_wldcore_map_projection_state.flags |= WLDCORE_PROJECTION_FLAG_UPDATE_POSITION;
            }
        }
        g_wldcore_window_records[level->map_cursor.window_index].position.x += amount.x;
        g_wldcore_window_records[level->map_cursor.window_index].position.y += amount.y;
        if (g_wldcore_window_records[level->map_cursor.window_index].position.x < -0x70) {
            g_wldcore_window_records[level->map_cursor.window_index].position.x = -0x70;
        }
        if (g_wldcore_window_records[level->map_cursor.window_index].position.x >= 0x79) {
            g_wldcore_window_records[level->map_cursor.window_index].position.x = 0x78;
        }
        if (g_wldcore_window_records[level->map_cursor.window_index].position.y < -0x68) {
            g_wldcore_window_records[level->map_cursor.window_index].position.y = -0x68;
        }
        if (g_wldcore_window_records[level->map_cursor.window_index].position.y >= 0x69) {
            g_wldcore_window_records[level->map_cursor.window_index].position.y = 0x68;
        }
    } else {
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
            saved = g_wldcore_map_projection_view.origin;
            wldcore_map_apply_clamped_scroll(&amount, &g_wldcore_map_projection_view.origin);
            if (saved.vx != g_wldcore_map_projection_view.origin.vx
                || saved.vy != g_wldcore_map_projection_view.origin.vy) {
                g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
                g_wldcore_map_projection_state.flags |= WLDCORE_PROJECTION_FLAG_UPDATE_POSITION;
            }
        }
    }

finish:
    if (level->map_cursor.wide_view == 0 && (hit = level->map_cursor.snap_hit) != 0) {
        g_wldcore_proposition_selected_entry = hit;
        return;
    }
    g_wldcore_proposition_selected_entry = 0;
}

/* Push a map-dot focus level and hide its parent window during the projection move.
 * Keeping the render pointer as the base preserves the two related call addresses. */
static void wldcore_menu_push_focus_location_level(s32 dot_index) {
    world_animated_render_record_t* render;

    render = &g_wldcore_map_dots[dot_index].render;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].focus_dot_index = dot_index;
    wldcore_map_start_projection_motion_if_outside_bounds(&render->position, &g_wldcore_map_dots[dot_index].position);
    if (g_wldcore_map_projection_motion.flags & WLDCORE_PROJECTION_MOTION_ACTIVE) {
        g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].window_index].flags
            |= WORLD_RENDER_RECORD_HIDDEN;
    }
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_FOCUS_LOCATION;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}

/* Closes one menu level. While a projection move is active it advances the
 * move, sets projection-state flag 1 and system flag 2, and returns;
 * otherwise it pops the level, moves the parent level's window to the focused
 * dot's position (y - 4), clears system flag 0x2000, and dispatches the
 * parent level's handler.
 *
 * The entry must be a struct pointer: the target hoists its reload above the
 * scalar g_main_system_flags store, which GCC only allows for an in-struct
 * access. */
void wldcore_map_step_projection_motion_then_pop_level(wldcore_menu_stack_record_t* entry) {
    wldcore_projection_state_t* state;
    s32 level;

    if (g_wldcore_map_projection_motion.flags & WLDCORE_PROJECTION_MOTION_ACTIVE) {
        wldcore_map_step_projection_motion();
        state = &g_wldcore_map_projection_state;
        state->flags |= WLDCORE_PROJECTION_FLAG_UPDATE_POSITION;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
        return;
    }
    level = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_depth = level - 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[level - 2].window_index].position.x
        = g_wldcore_map_dots[entry->focus_dot_index].render.position.x;
    g_main_system_flags &= ~MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[level - 2].window_index].position.y
        = g_wldcore_map_dots[entry->focus_dot_index].render.position.y - 4;
    wldcore_menu_dispatch_resume_handler();
}

/* Start drawing or erasing a map path and hide the parent window until the
 * animation finishes. Indexed stores keep their named globals to preserve
 * the original address sequences. */
static void wldcore_menu_push_map_path_level(s32 path_id, s32 path_id_hi, s32 erase) {
    s32 depth;
    s32 window_index;

    wldcore_map_prepare_path_animation(path_id, path_id_hi);
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].path.path_id = path_id;
    g_wldcore_menu_stack_records_next[depth].path.path_id_hi = path_id_hi;
    g_wldcore_menu_stack_records_next[depth].path.erase = erase;
    g_wldcore_menu_stack_records_next[depth].path.pending_sound = 1;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
    window_index = g_wldcore_menu_stack_records_next[depth - 1].window_index;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_MAP_PATH;
    g_wldcore_window_records[window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth = depth + 1;
}

/*
 * Advances the world slideshow one frame: restarts it at the last page when
 * the level asks, steps the page timer forwards or backwards while the run
 * flag is set, and otherwise pops the menu level and resumes its parent.
 */
void wldcore_menu_step_map_path_level(wldcore_map_path_level_t* level) {
    s32 depth;
    s32 window_index;

    if (level->pending_sound != 0) {
        if (g_main_system_flags & MAIN_SYSTEM_FLAG_FADING) {
            return;
        }
        if (level->erase != 0) {
            script_set_variable(g_wldcore_map_path_animation.script_variable + EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST, 0);
            g_wldcore_map_path_animation.page_index = g_wldcore_map_path_animation.page_count - 1;
            g_wldcore_map_path_animation.frame_timer
                = g_wldcore_map_path_animation.page_frames[g_wldcore_map_path_animation.page_count - 1] - 1;
            wldcore_sound_play_effect(MAIN_SFX_WORLD_MAP_PATH);
        } else {
            wldcore_sound_play_effect(MAIN_SFX_WORLD_MAP_PATH);
        }
        level->pending_sound = 0;
    }
    if (g_wldcore_map_path_animation.flags & WLDCORE_SLIDESHOW_RUNNING) {
        if (level->erase == 0) {
            g_wldcore_map_path_animation.frame_timer++;
            if (g_wldcore_map_path_animation.frame_timer
                >= g_wldcore_map_path_animation.page_frames[g_wldcore_map_path_animation.page_index]) {
                g_wldcore_map_path_animation.page_index++;
                g_wldcore_map_path_animation.frame_timer = 0;
                if (g_wldcore_map_path_animation.page_index >= g_wldcore_map_path_animation.page_count) {
                    script_set_variable(
                        g_wldcore_map_path_animation.script_variable + EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST, 1);
                }
            }
        } else {
            g_wldcore_map_path_animation.frame_timer--;
            if (g_wldcore_map_path_animation.frame_timer <= 0) {
                g_wldcore_map_path_animation.page_index--;
                if (g_wldcore_map_path_animation.page_index >= 0) {
                    g_wldcore_map_path_animation.frame_timer
                        = g_wldcore_map_path_animation.page_frames[g_wldcore_map_path_animation.page_index];
                }
            }
        }
        g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
        return;
    }
    wldcore_sound_play_effect(MAIN_SFX_NONE);
    g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_depth = depth - 1;
    window_index = g_wldcore_menu_stack_records_next[depth - 2].window_index;
    g_main_system_flags &= ~MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
    g_wldcore_window_records[window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    wldcore_menu_dispatch_resume_handler();
}

/* Start a delayed map-location visibility change and hide the parent window
 * until it finishes. Indexed stores keep their named globals to preserve
 * the original address sequences. */
static void wldcore_push_map_location_visibility_level(s32 dot_index, s32 was_set) {
    s32 depth;
    s32 window_index;

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].visibility.dot_index = dot_index;
    g_wldcore_menu_stack_records_next[depth].visibility.was_set = was_set;
    g_wldcore_menu_stack_records_next[depth].visibility.delay = 0x10;
    g_wldcore_menu_stack_records_next[depth].visibility.pending_sound = 1;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
    window_index = g_wldcore_menu_stack_records_next[depth - 1].window_index;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_MAP_LOCATION_VISIBILITY;
    g_wldcore_window_records[window_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth = depth + 1;
}

/* Menu step that toggles script variable 0x200 + dot_index. Once system
 * flag 0x08 is clear it plays the pending sound (setting the variable to 1
 * when it was clear), waits out the delay with system flag 0x02 raised, then,
 * if the variable was set, flags the map dot 0x10 and clears the variable
 * before closing the level.
 *
 * The window index is read into a local before the flag clear, which keeps
 * the target's load and store order. */
void wldcore_map_step_location_visibility_change(wldcore_map_visibility_level_t* level) {
    s32 index;

    if (level->pending_sound != 0) {
        if (g_main_system_flags & MAIN_SYSTEM_FLAG_FADING) {
            return;
        }
        if (level->was_set != 0) {
            wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        } else {
            script_set_variable(level->dot_index + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST, 1);
            wldcore_sound_play_effect(MAIN_SFX_WORLD_MAP_LOCATION_APPEAR);
        }
        level->pending_sound = 0;
    }
    if (level->delay != 0) {
        g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
        level->delay--;
        return;
    }
    if (level->was_set != 0) {
        g_wldcore_map_dots[level->dot_index].render.flags |= WORLD_RENDER_RECORD_HIDDEN;
        script_set_variable(level->dot_index + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST, 0);
    }
    g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
    g_wldcore_menu_stack_depth--;
    index = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].window_index;
    g_main_system_flags &= ~MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
    g_wldcore_window_records[index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    wldcore_menu_dispatch_resume_handler();
}

/* Open the entry window and push its world-map return level. */
void wldcore_menu_push_entry_window_level(void) {
    u32 flags;
    s32 depth;
    s32* counter;

    g_menu_blocked_buttons = -1;
    world_menu_open_entry_window(4, -0x30, -0x38);
    g_wldcore_context_value_display_mode = 1;
    g_wldcore_displayed_numeric_value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    depth = g_wldcore_menu_stack_depth;
    flags = g_main_system_flags;
    g_wldcore_menu_stack_records_next[depth].return_level.delay = 3;
    g_wldcore_menu_stack_records_next[depth].return_level.pending = 0;
    g_main_system_flags = flags & ~MAIN_SYSTEM_FLAG_SCREEN_CLEARED;
    if ((flags & MAIN_SYSTEM_FLAG_FADING) != 0) {
        g_main_system_flags = (flags & ~MAIN_SYSTEM_FLAG_SCREEN_CLEARED) | MAIN_SYSTEM_FLAG_FREEZE_ANIMATION;
        counter = &g_wldcore_screen_fade_state.elapsed;
        *counter = *counter - 4;
    }
    {
        s32 last;
        last = g_wldcore_menu_stack_depth;
        g_wldcore_menu_stack_types[last + 1] = WLDCORE_MENU_LEVEL_RETURN;
        g_wldcore_menu_stack_depth = last + 1;
    }
}

/* Per-frame handler of the world-map return level: waits out its own delay,
 * then pops the level and dispatches whichever pending system-flag request
 * asked for the return. */
void wldcore_menu_step_return_level(wldcore_menu_return_level_t* level) {
    s32 flags;
    s32 cleared;
    s32 location;

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_SCREEN_CLEARED) {
        level->pending = 1;
    }
    if (g_main_system_flags & MAIN_SYSTEM_FLAG_FADING) {
        if (level->pending != 0) {
            level->pending = 0;
            wldcore_gfx_toggle_captured_world_frame(0);
            level->delay = 3;
            g_main_system_flags |= MAIN_SYSTEM_FLAG_FADING | MAIN_SYSTEM_FLAG_FREEZE_ANIMATION;
        }
        g_menu_blocked_buttons = -1;
        return;
    }
    if (level->delay != 0) {
        level->delay -= 1;
        if (level->delay == 0) {
            wldcore_gfx_toggle_captured_world_frame(1);
        }
        return;
    }
    g_menu_blocked_buttons = 0;
    if (menu_get_thread_activity_variant() != 0) {
        return;
    }
    wldcore_input_init_repeat_state();
    flags = g_main_system_flags;
    wldcore_gfx_toggle_captured_world_frame(0);
    cleared = g_main_system_flags & ~MAIN_SYSTEM_FLAG_FREEZE_ANIMATION;
    g_main_system_flags = cleared;
    g_wldcore_menu_stack_depth -= 1;
    if (flags & MAIN_SYSTEM_FLAG_REQUEST_BRAVE_STORY) {
        g_main_system_flags = cleared ^ MAIN_SYSTEM_FLAG_DRAW_MAP;
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_BRAVE_STORY, 0, 0);
        return;
    }
    if (flags & MAIN_SYSTEM_FLAG_REQUEST_DESTINATION) {
        location = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP);
        wldcore_init_core();
        wldcore_menu_dispatch_resume_handler();
        if (g_wldcore_map_projection_state.marker.kind != location) {
            wldcore_location_process_entry(g_wldcore_map_projection_state.marker.kind, location);
        }
        return;
    }
    if (flags & MAIN_SYSTEM_FLAG_REQUEST_TUTORIAL) {
        g_main_system_flags = cleared ^ MAIN_SYSTEM_FLAG_DRAW_MAP;
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES, 0, 1);
        return;
    }
    if (flags & MAIN_SYSTEM_FLAG_REQUEST_VARIABLE_LIST) {
        wldcore_menu_push_variable_list_level();
        return;
    }
    if (flags & MAIN_SYSTEM_FLAG_REQUEST_FIXED_NUMBER_PANEL) {
        wldcore_menu_push_fixed_number_panel_level();
        return;
    }
    if (flags & MAIN_SYSTEM_FLAG_RESTART) {
        g_main_system_flags = cleared & ~(MAIN_SYSTEM_FLAG_RUNNING | MAIN_SYSTEM_FLAG_UNLOCK_ALL);
        if (script_get_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT) != 0
            || script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) == 0) {
            g_main_system_flags |= MAIN_SYSTEM_FLAG_RESTART;
        } else {
            g_main_system_flags &= ~MAIN_SYSTEM_FLAG_RESTART;
        }
        g_main_system_flags |= MAIN_SYSTEM_FLAG_NO_BACKGROUND_IMAGE;
        return;
    }
    wldcore_menu_dispatch_resume_handler();
}

/* Pushes the self-sizing message window as menu level type 2, the windowed
 * sibling of wldcore_menu_push_message_level (0x8006e860, type 3). It stores
 * the reload flag and, when set, re-points the text formatter at
 * g_open_text_section_offsets; appends a window record (sequence 0xe,
 * priority 1); measures the entry, clamped to three rows; fills the shared
 * scrollable window and centers it and the window record on the measured box;
 * then saves g_wldcore_menu_blocked_buttons in the level, sets it to -1, plays sound
 * 0x12, writes the level type and bumps the depth.
 *
 * Taking the record base into `records` at the last block-one store lets CSE
 * reuse that register after the measure call, which gives the base
 * priority over `text` in global allocation (s1/s2 as in the target). The
 * combined-away address pseudo of that store keeps a reload stack slot, part
 * of the target's 40-byte locals area. `box` is an aggregate so its 32-bit values stay
 * in memory across wldcore_text_init_scrollable_window; `dims` is an array so
 * it is laid out in declaration order at 32(sp). `unused_point` and `unused`
 * are sized from the target frame only; their original types are unknown. */
void wldcore_menu_push_text_window_level(s32 text, s32 reload_text) {
    wldcore_text_scrollable_window_t* window;
    world_animated_render_record_t* records;
    point32_t* positions;
    point32_t* position;
    s32* text_id;
    s32 index;
    point32_t box;
    point32_t unused_point;
    s16 dims[2];
    s32 unused[2];
    u8* entry;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].message.reload_text = reload_text;
    if (reload_text != 0) {
        text_save_section_pointers();
        text_relocate_pointer_table(g_open_text_section_offsets);
    }

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].message.window_index = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_MESSAGE_LABEL;
    g_wldcore_window_records[index].priority = 1;
    g_wldcore_window_records[index].anim_counter = 0;
    records = g_wldcore_window_records;
    records[index].frame_index = 0;

    entry = text_find_entry(text);
    text_measure_pixels(&dims[0], &dims[1], entry);
    if (dims[1] > 3) {
        dims[1] = 3;
    }

    text_id = &g_wldcore_scrollable_text_window.text_id;
    window = &g_wldcore_scrollable_text_window;

    *text_id = text;
    window->image_x = 0;
    g_wldcore_scrollable_text_window.image_y = 0xC0;
    box.x = (dims[0] + 24) & 0xFFFC;
    box.y = (dims[1] << 4) + 16;
    g_wldcore_scrollable_text_window.base.x = -(box.x / 2);
    g_wldcore_scrollable_text_window.base.y = -(box.y >> 1);
    g_wldcore_scrollable_text_window.text_width = dims[0];
    g_wldcore_scrollable_text_window.rows_per_page = dims[1];
    g_wldcore_scrollable_text_window.priority = 1;
    g_wldcore_scrollable_text_window.image_coordinate_mode = 0;
    g_wldcore_scrollable_text_window.extra_render_index = -1;
    g_wldcore_scrollable_text_window.text_substitutions[1] = -1;
    g_wldcore_scrollable_text_window.text_substitutions[0] = -1;
    wldcore_text_init_scrollable_window(window);

    index = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].message.window_index;
    positions = &records[0].position;
    /* This byte-view stride retains the target addiu of &records[0].x;
     * &records[index].x removes it and breaks the exact match. */
    position = (point32_t*)((u8*)positions + index * sizeof(world_animated_render_record_t));
    position->x = -(box.x / 2) + 2;
    position->y = -(box.y / 2) - 2;
    wldcore_sound_play_effect(MAIN_SFX_WINDOW_OPEN);

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].message.saved_blocked_buttons
        = g_menu_blocked_buttons;
    g_menu_blocked_buttons = -1;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_TEXT_WINDOW;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}

/* Steps a message level: once the text window has popped in and
 * wldcore_text_is_window_finished reports 0, pops the level, restores the
 * blocked-button mask it saved and (when asked) the text section pointers,
 * and resumes the level underneath. */
void wldcore_menu_wait_text_then_pop_level(wldcore_menu_message_level_t* level) {
    if (g_wldcore_window_render_records[g_wldcore_scrollable_text_window.render_record_index].flags
        & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) != 0) {
        return;
    }
    g_input_current_buttons = 0;
    g_menu_blocked_buttons = level->saved_blocked_buttons;
    g_wldcore_window_record_count -= 1;
    g_wldcore_window_render_object_count -= 1;
    g_wldcore_menu_stack_depth -= 1;
    if (level->reload_text != 0) {
        text_restore_section_pointers();
    }
    wldcore_menu_dispatch_resume_handler();
}

/* Appends a window-list record for a text window: optionally reloads the
 * text-section pointers, starts the text-character thread on `text` (mode
 * 0x3b when system flag 0x20000 is set, else 0x33), saves the blocked
 * buttons in the record and blocks them all, and plays sound 0x12. */
void wldcore_menu_push_message_level(s32 text, s32 reload_text) {
    s32 index;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].message.reload_text = reload_text;
    if (reload_text != 0) {
        text_save_section_pointers();
        text_relocate_pointer_table(g_open_text_section_offsets);
    }
    thread_start(THREAD_SCREEN_SLOT_MESSAGE, text_run_dialog_thread_variant);
    if (g_main_system_flags & MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS) {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x3B, text, 0);
    } else {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x33, text, 0);
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].message.saved_blocked_buttons
        = g_menu_blocked_buttons;
    g_menu_blocked_buttons = -1;
    wldcore_sound_play_effect(MAIN_SFX_WINDOW_OPEN);
    index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[index].message._unknown_0c = 8;
    g_wldcore_menu_stack_records_next[index].message.window_index = 0;
    g_wldcore_menu_stack_types[index + 1] = WLDCORE_MENU_LEVEL_MESSAGE;
    g_wldcore_menu_stack_depth = index + 1;
}

/* Steps a level that waits for the message box to close: once the message
 * thread has ended it pops the level, restores the blocked buttons, resumes
 * the level underneath and (when asked) restores the text section pointers.
 * Over the Brave Story or proposition category level it keeps the displayed
 * play time current while waiting. */
void wldcore_menu_finish_close_request(wldcore_menu_message_level_t* level) {
    if (g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth - 1] == WLDCORE_MENU_LEVEL_BRAVE_STORY
        || g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth - 1] == WLDCORE_MENU_LEVEL_PROPOSITION_CATEGORY) {
        g_wldcore_displayed_numeric_value = g_main_system_play_time_hours;
        g_wldcore_displayed_numeric_value_secondary = g_main_system_play_time_minutes;
        g_wldcore_displayed_numeric_value_tertiary = g_main_system_play_time_seconds;
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
        g_menu_blocked_buttons = level->saved_blocked_buttons;
        g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth - 1;
        wldcore_menu_dispatch_resume_handler();
        if (level->reload_text != 0) {
            text_restore_section_pointers();
        }
    }
}

/* Pushes the debug script-variable list level (type 0xb): one cursor window
 * record and one render record, laid out by 0x8006ed30 and then positioned
 * from the render record's origin.
 *
 * Both appended indices share one local, which is what holds them in $a2.
 * The window position is written after 0x8006ed30 returns, from the render
 * record origin the callee filled in, so the level fields are read back out
 * of the stack record rather than kept in locals; the `- -` keeps the row
 * offset's constant on the multiply chain, which a plain `+` reassociates
 * onto base_y. */
static void wldcore_menu_push_variable_list_level(void) {
    s32 index;
    s32 record_index;
    u8 unused_locals[0x10];

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_list.window_index = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 3;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_list.render_index = index;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    g_wldcore_window_render_records[index].priority = 3;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_list.first_variable = 0;
    wldcore_menu_render_debug_variable_list(
        &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_list);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].variable_list.cursor = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].variable_list.window_index].position.x
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].variable_list.render_index]
              .base.x
        + 8;
    g_wldcore_menu_stack_depth = record_index + 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].variable_list.window_index].position.y
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].variable_list.render_index]
              .base.y
        - -(g_wldcore_menu_stack_records_next[record_index].variable_list.cursor * 0x10 + 0xC);
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_VARIABLE_LIST;
}

/* Renders up to 10 script-variable rows in the debug list panel.
 *
 * `value` first holds the row count for the panel height and then each
 * variable's value: one register (v1) in the target. */
static void wldcore_menu_render_debug_variable_list(wldcore_menu_variable_list_level_t* level) {
    RECT bounds;
    point32_t point;
    s32 index;
    point32_t* base;
    point32_t* records;
    s32 variable;
    s32 text_id;
    u32 value;
    s32 i;

    variable = level->first_variable;
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
    wldcore_window_build_render_record_image(index, bounds, 0, g_wldcore_window_image_buffer);
    /* Direct record indexing moves the symbol load after the index scaling
     * and breaks the exact match. Retain the target's biased base and stride. */
    records = &g_wldcore_window_render_records[0].base;
    base = (point32_t*)((u8*)records + index * sizeof(wldcore_window_render_record_t));
    base->x = -(bounds.w / 2);
    base->y = -88;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < level->row_count; i++) {
        value = script_get_variable(variable);
        if (value < 10000) {
            g_text_substitution_values[0] = variable;
            g_text_substitution_values[1] = value;
            text_id = 0x24;
        } else {
            g_text_substitution_values[0] = variable;
            text_id = 0x25;
        }
        window_display_text_entry_variant(
            index, text_id | TEXT_ID_WORLD_MENU_BASE, point, g_wldcore_window_image_buffer);
        point.y += 0x10;
        variable++;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);
}

/* Input handler for the debug script-variable list level (type 0xb).
 *
 * Circle opens the number panel for the selected variable, X pops the level.
 * Up/down move the cursor, or scroll the first displayed variable by 10 with
 * L1 held and by 100 with R1 held. Scrolling redraws the rows through
 * wldcore_menu_render_debug_variable_list and clamps the cursor; a plain cursor move only repositions
 * the cursor window.
 *
 * Every scroll arm falls into the one redraw, which clamps the cursor and
 * joins the cursor move. The cursor-down arm sits between the redraw and the
 * move in the target, so it and the move are reached by goto: a redraw
 * repeated in each arm is not merged back (its clamp store is cross-jumped
 * into the cursor-down arm first). */
void wldcore_menu_handle_debug_variable_list_input(wldcore_menu_variable_list_level_t* level) {
    s32 variable;
    s32 dead[2];

    if (g_wldcore_window_render_records[level->render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        g_wldcore_window_records[level->window_index].palette = 0xA;
        g_wldcore_window_render_records[level->render_index].palette = 2;
        g_wldcore_window_records[level->window_index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
        g_wldcore_window_records[level->window_index].anim_counter
            = g_wldcore_window_records[level->window_index].frame_index = 0;
        variable = level->first_variable + level->cursor;
        wldcore_menu_push_number_panel_level_with_argument(variable, script_get_variable(variable));
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        g_wldcore_window_record_count -= 1;
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 2;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
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
    } else if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
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
    wldcore_menu_render_debug_variable_list(level);
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
    g_wldcore_window_records[level->window_index].position.x
        = g_wldcore_window_render_records[level->render_index].base.x + 8;
    g_wldcore_window_records[level->window_index].position.y
        = g_wldcore_window_render_records[level->render_index].base.y - -(level->cursor * 0x10 + 0xC);
}

/* Pushes the number-panel menu level (type 0x30) that edits script variable
 * `variable_id`, starting the panel at `value`. */
static void wldcore_menu_push_number_panel_level_with_argument(s32 variable_id, s32 value) {
    point32_t origin;
    s32 record_index;

    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS;
    g_wldcore_menu_ordering_table_offset = 2;
    g_wldcore_active_menu_value = value;
    origin.x = 0x20;
    origin.y = 0x20;
    wldcore_window_init_number_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0x270F0000, 0, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_records_next[record_index].panel.variable_id = variable_id;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_NUMBER_PANEL_WITH_ARGUMENT;
    g_wldcore_menu_stack_depth = record_index + 1;
}

/* Step handler for the number-panel menu level (type 0x30). While the panel
 * runs it accepts the confirm button (0x20), which plays sound 1 and writes
 * the panel's value to the level's script variable, or the cancel button
 * (0x40); either way it restarts thread 0xC to close the panel. Once the
 * panel thread has stopped it pops the level, redraws the script-variable
 * list on a confirm, and restores the parent level's window and render
 * records.
 *
 * Reading the parent window index before the render-record store is what
 * lets the scheduler place that load inside the render index's multiply
 * sequence, as the target does. */
void wldcore_menu_step_number_panel_level(wldcore_menu_panel_level_t* level) {
    s32 depth;
    s32 parent;
    s32 slot;
    world_animated_render_record_t* record;
    u32 buttons;

    if (level->result != 0 && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        depth = g_wldcore_menu_stack_depth - 1;
        g_wldcore_menu_stack_depth = depth;
        if (level->result == 1) {
            wldcore_menu_render_debug_variable_list(&g_wldcore_menu_stack_records[depth].variable_list);
        }
        parent = g_wldcore_menu_stack_depth - 1;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[parent].window_render.window_index].sequence
            = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
        slot = g_wldcore_menu_stack_records_next[parent].window_render.window_index;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[parent].window_render.render_index].palette
            = 0;
        record = &g_wldcore_window_records[slot];
        record->palette = 0;
        record->frame_index = 0;
        g_wldcore_window_records[slot].anim_counter = 0;
        return;
    }
    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        level->result = 2;
    } else if (buttons & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        script_set_variable(level->variable_id, g_wldcore_active_menu_value);
    } else {
        return;
    }
    thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
}

/* Appends a window-list record: saves g_wldcore_menu_ordering_table_offset into it, allocates a window
 * record from the render-object queue, initializes that window's delay and
 * counters, and applies layout 0 to it before advancing the list count.
 *
 * Writing the window columns as fields of g_wldcore_window_records (not separate stride
 * arrays) keeps the scheduler from sinking the record's window_index store
 * below them. */
void wldcore_menu_push_layout_window_level(void) {
    s32 window;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].layout_window.saved_state
        = g_wldcore_menu_ordering_table_offset;
    g_wldcore_menu_ordering_table_offset = 1;
    window = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].layout_window.window_index = window;
    g_wldcore_window_records[window].sequence = WLDCORE_ANIM_SEQUENCE_LAYOUT_CURSOR_IDLE;
    g_wldcore_window_records[window].priority = 2;
    g_wldcore_window_records[window].anim_counter = 0;
    g_wldcore_window_records[window].frame_index = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].layout_window.layout = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].layout_window.cached_layout_kind = 1;
    wldcore_window_apply_layout(&g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].layout_window);
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_LAYOUT_WINDOW;
    g_wldcore_menu_stack_depth++;
}

/* Routing both stores through one base variable (rather than
 * `&g_wldcore_window_record_positions[index]`) is what reproduces the target's separate base register
 * held in $a0 across the whole function. */
static void wldcore_window_apply_layout(wldcore_window_layout_request_t* request) {
    wldcore_window_xy_t* xy = g_wldcore_window_record_positions;
    s32 index = request->window_index;
    s32 layout = request->layout;
    s32 kind;

    xy[index].x = g_wldcore_window_layout_rows[layout].x + 14;
    xy[index].y = g_wldcore_window_layout_rows[layout].y + 12;
    kind = g_wldcore_window_layout_rows[layout].kind;
    if (kind != request->cached_layout_kind) {
        request->cached_layout_kind = kind;
        g_wldcore_window_records[index].sequence
            = (kind != 0) ? WLDCORE_ANIM_SEQUENCE_LAYOUT_CURSOR_IDLE : WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
        g_wldcore_window_records[index].anim_counter = 0;
        g_wldcore_window_records[index].frame_index = 0;
    }
}

/* Clear the text-task flag and move a window record to the sequence after
 * its current one, restarting the animation with the default palette. */
void wldcore_window_finalize_record_and_stop_task(wldcore_menu_stack_record_t* level) {
    s32 index = level->layout_window.window_index;
    world_animated_render_record_t* entry = &g_wldcore_window_records[index];

    g_menu_input_lock_mode = 0;
    entry->sequence = g_wldcore_window_records[index].sequence + 1;
    g_wldcore_window_records[index].palette = 0;
    entry->anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
}

/* The g_wldcore_menu_publish_handlers entry of WLDCORE_MENU_LEVEL_LAYOUT_WINDOW: nothing to publish. */
void wldcore_menu_publish_layout_window_level(void) {
}

/* Input handler for a layout-window menu level.
 *
 * Circle pops the level, X opens the row's help message, and a direction moves
 * to the neighboring layout named by the row. A row with a counter pointer
 * adds the counter to the base message id, and suppresses the message entirely
 * when the counter reads 0 or -1 (except for layout 0x0b). */
void wldcore_menu_handle_layout_window_input(wldcore_window_layout_request_t* request) {
    s32 layout;
    s16* counter;
    s16 message;
    world_animated_render_record_t* record;
    s32 window;

    if ((g_wldcore_input_new_button_presses & PSX_PAD_CROSS) != 0) {
        g_wldcore_window_record_count -= 1;
        g_wldcore_window_render_object_count -= 1;
        g_wldcore_menu_ordering_table_offset = request->saved_state;
        g_wldcore_menu_stack_depth -= 1;
        wldcore_menu_dispatch_resume_handler();
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        return;
    }

    if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) != 0) {
        layout = request->layout;
        counter = g_wldcore_window_layout_rows[layout].counter;
        if (counter != 0) {
            if (layout != 0xB && *counter >= -1 && *counter < 1) {
                return;
            }
            message = g_wldcore_window_layout_rows[layout].message_base + *counter;
        } else {
            message = g_wldcore_window_layout_rows[layout].message_base;
        }
        wldcore_menu_push_message_level(message, 1);
        g_menu_input_lock_mode = 1;
        window = request->window_index;
        record = &g_wldcore_window_records[window];
        record->sequence = g_wldcore_window_records[window].sequence - 1;
        g_wldcore_window_records[window].palette = 0xA;
        record->anim_counter = 0;
        g_wldcore_window_records[window].frame_index = 0;
        return;
    }

    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (g_wldcore_window_layout_rows[request->layout].up != request->layout) {
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
            request->layout = g_wldcore_window_layout_rows[request->layout].up;
            wldcore_window_apply_layout(request);
            return;
        }
    }
    if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        if (g_wldcore_window_layout_rows[request->layout].down != request->layout) {
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
            request->layout = g_wldcore_window_layout_rows[request->layout].down;
            wldcore_window_apply_layout(request);
            return;
        }
    }
    if (wldcore_input_update_repeat(PSX_PAD_RIGHT) != 0) {
        if (g_wldcore_window_layout_rows[request->layout].left != request->layout) {
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
            request->layout = g_wldcore_window_layout_rows[request->layout].left;
            wldcore_window_apply_layout(request);
            return;
        }
    }
    if (wldcore_input_update_repeat(PSX_PAD_LEFT) != 0) {
        if (g_wldcore_window_layout_rows[request->layout].right != request->layout) {
            wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
            request->layout = g_wldcore_window_layout_rows[request->layout].right;
            wldcore_window_apply_layout(request);
        }
    }
}

/* Pushes the town/location service list level (type 4) for menu id `menu`.
 *
 * Loads the location's picture, appends the cursor window (window kind
 * menu + 0x37) and, when a picture is present, uploads both its TIM and the
 * script picture at g_wldcore_picture_buffer to VRAM, binds a render record to the image
 * and adds a second framing window at (-8, 0x10). It then measures every
 * entry's text through text_find_entry / text_measure_pixels to
 * size the panel, copies the entry values into g_wldcore_list_entry_values with
 * cleared row flags, clamps the cursor to at most four visible rows, and
 * starts the panel render state centerd on the measured width.
 *
 * The identical `menu += 0x37` arms split the block for the scheduler (jump2
 * cross-jumps them back together), keeping the add after the picture call so
 * reorg puts it in the wait call's delay slot. The label window's position is
 * stored through a stride-36 view based at &records->x, as the target forms
 * (records + 0x18) + index * 36. The unused array reproduces the 0xb8 frame.
 */
void wldcore_menu_push_location_menu_level(s32 menu) {
    GsIMAGE image;
    RECT bounds;
    point32_t origin;
    wldcore_window_panel_layout_t layout;
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
    world_animated_render_record_t* records;
    wldcore_location_list_level_t* level;
    point32_t* base;
    wldcore_window_xy_t* position;

    g_wldcore_map_projection_state.marker.sub_kind
        = wldcore_proposition_load_picture(g_wldcore_map_projection_state.marker.kind);
    if (g_wldcore_map_projection_state.marker.sub_kind != 0) {
        menu += 0x37;
    } else {
        menu += 0x37;
    }
    file_wait_for_pending_variant();
    flags = g_main_system_flags | MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
    level = &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list;
    level->extra_render = -1;
    level->label_window = -1;
    level->frame_render = -1;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.cursor_window = -1;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.mode = 0;
    g_main_system_flags = flags;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.cursor_window = index;
    records = g_wldcore_window_records;
    /* Direct record.color access changes the column base and instruction scheduling. */
    color = &records->color;
    g_wldcore_window_records[index].priority = 0xB;
    g_wldcore_window_records[index].sequence = menu;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    ((CVECTOR*)((u8*)color + index * sizeof(g_wldcore_window_records[0])))->r = 0;
    ((CVECTOR*)((u8*)color + index * sizeof(g_wldcore_window_records[0])))->g = 0;
    ((CVECTOR*)((u8*)color + index * sizeof(g_wldcore_window_records[0])))->b = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.mode = 8;

    if (g_wldcore_map_projection_state.marker.sub_kind != 0) {
        GsGetTimInfo(g_wldcore_location_frame_tim + 1, &image);
        LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
        LoadClut(image.clut, image.cx, image.cy);
        GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
        LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
        LoadClut(image.clut, image.cx, image.cy);

        index
            = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.frame_render = index;
        bounds.x = image.px;
        bounds.y = image.py;
        bounds.w = image.pw;
        bounds.h = image.ph;
        origin.x = image.cx;
        origin.y = image.cy;
        wldcore_window_init_vram_render_record(index, bounds, origin, WORLD_RENDER_RECORD_8BIT_CLUT);
        base = &g_wldcore_window_render_records[index].base;
        g_wldcore_window_render_records[index].priority = 0xB;
        base->x = -0x40;
        base->y = -0x4C;
        g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;

        index = window_append_animated_record_variant(
            g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.label_window = index;
        g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_LOCATION_PICTURE_BORDER;
        g_wldcore_window_records[index].priority = 0xB;
        records[index].anim_counter = 0;
        g_wldcore_window_records[index].frame_index = 0;
        position = (wldcore_window_xy_t*)&records->position.x;
        position[index].x = -8;
        position[index].y = 0x10;
        ((CVECTOR*)((u8*)color + index * sizeof(g_wldcore_window_records[0])))->r = 0;
        ((CVECTOR*)((u8*)color + index * sizeof(g_wldcore_window_records[0])))->g = 0;
        ((CVECTOR*)((u8*)color + index * sizeof(g_wldcore_window_records[0])))->b = 0;
    }

    widest = 0;
    count = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.entry_count;
    for (i = 0; i < count; i++) {
        text_measure_pixels(&text_width, &text_rows,
            text_find_entry(g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.entries[i]));
        if (widest < text_width) {
            widest = text_width;
        }
    }
    widest += 8;

    for (i = 0; i < count; i++) {
        value = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.entries[i];
        g_wldcore_list_entry_values[i] = value;
        g_wldcore_list_row_flags[i] = 0;
    }

    i = 4;
    if (count < 4) {
        i = count;
    }
    layout.text_width = widest;
    layout.visible_rows = i;
    origin.x = 0x80 - (((layout.text_width + 0x18) & 0xFFFC) >> 1);
    if (g_wldcore_map_projection_state.marker.sub_kind != 0) {
        origin.y = 0x80;
    } else {
        origin.y = 0x70 - i * 8;
    }
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_LOCATION_MENU, count, i);
    wldcore_window_init_panel_render_state(
        0xC, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].location_list.entry_count, &layout, &origin);
    g_wldcore_window_panel_render_state.vram_origin.x = 0x240;
    g_wldcore_window_panel_render_state.header_id = 0;
    g_wldcore_window_panel_render_state.vram_origin.y = 0x100;

    if (g_wldcore_map_projection_state.marker.sub_kind != 0) {
        s32 unused[6]; /* frame padding: the target frame is 0xb8 */

        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                     .location_list.cursor_window]
            .position.x = -4;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                     .location_list.cursor_window]
            .position.y = 0;
    } else {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                     .location_list.cursor_window]
            .position.x = -4;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                     .location_list.cursor_window]
            .position.y = g_wldcore_window_render_records[index].base.y - 8;
    }

    depth = g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_stack_records_next[depth].location_list.delay = 8;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_FREEZE_ANIMATION;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_LOCATION_MENU;
    g_wldcore_menu_stack_depth = depth + 1;
}

/* Restore the location list's optional renderer palette. */
void wldcore_window_restore_location_list_palette(wldcore_menu_stack_record_t* level) {
    if (level->location_list.extra_render != -1) {
        g_wldcore_window_render_records[level->location_list.extra_render].palette = 0;
    }
}

/* Sets the second owned render record's palette selector when its slot is present. */
void wldcore_window_set_secondary_render_palette_2(wldcore_menu_slot_set_t* slots) {
    if (slots->render_b != -1) {
        g_wldcore_window_render_records[slots->render_b].palette = 2;
    }
}

/* Per-frame step of the location menu level: fades its two windows in, starts
 * the menu thread after the delay and handles input. A confirmed entry opens
 * the bar, a shop, a deep-dungeon battle chosen by the location's conditional
 * set or the selected destination map; the help button shows the entry's
 * message and cancel closes the level. */
void wldcore_menu_step_location_menu_level(wldcore_menu_location_menu_level_t* level) {
    s16* selected;
    u8 unused[0x28];
    s16* values;
    s32 value;
    s32 text;
    s32 offset;
    wldcore_deep_dungeon_battle_t* records;
    wldcore_deep_dungeon_battle_t* record;

    if (level->fade != 0) {
        if (level->fade >= 0x80) {
            level->fade = 0x80;
        }
        g_wldcore_window_records[level->slots.window_a].color.r = level->fade;
        g_wldcore_window_records[level->slots.window_a].color.g = level->fade;
        g_wldcore_window_records[level->slots.window_a].color.b = level->fade;
        g_wldcore_window_records[level->slots.window_b].color.r = level->fade;
        g_wldcore_window_records[level->slots.window_b].color.g = level->fade;
        g_wldcore_window_records[level->slots.window_b].color.b = level->fade;
        if (level->fade == 0x80) {
            level->fade = 0;
            wldcore_gfx_toggle_captured_world_frame(1);
        } else {
            level->fade += 7;
        }
        return;
    }
    if (level->delay != 0) {
        if (level->delay == 8) {
            thread_start(WLDCORE_THREAD_SLOT_WINDOW_PANEL, world_screen_build_at_list);
            thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, (s32)&g_wldcore_window_panel_render_state, 0, 0);
        }
        level->delay--;
        return;
    }
    if (g_main_system_flags & MAIN_SYSTEM_FLAG_FADING) {
        return;
    }
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        wldcore_gfx_toggle_captured_world_frame(0);
        g_main_system_flags = (g_main_system_flags | MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
            & ~(MAIN_SYSTEM_FLAG_DRAW_MAP | MAIN_SYSTEM_FLAG_FREEZE_ANIMATION);
        if (level->slots.result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_LOCATION_MENU);
            selected = &g_wldcore_window_panel_render_state.selected_index;
            values = g_wldcore_list_entry_values;
            value = values[*selected];
            if ((value & TEXT_ID_SECTION_MASK) == TEXT_ID_WORLD_MENU_BASE) {
                g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
                value &= TEXT_ID_ENTRY_MASK;
                switch (value) {
                case WLDCORE_TEXT_ID_BAR& TEXT_ID_ENTRY_MASK:
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_BAR));
                    wldcore_bar_push_menu_level();
                    return;
                case WLDCORE_TEXT_ID_SHOP& TEXT_ID_ENTRY_MASK:
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    wldcore_menu_run_shop_and_rebuild_screen(0);
                    return;
                case WLDCORE_TEXT_ID_SOLDIER_OFFICE& TEXT_ID_ENTRY_MASK:
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    wldcore_menu_run_shop_and_rebuild_screen(WORLD_MENU_CONTEXT_SOLDIER_OFFICE);
                    return;
                case WLDCORE_TEXT_ID_FUR_SHOP& TEXT_ID_ENTRY_MASK:
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    wldcore_menu_run_shop_and_rebuild_screen(WORLD_MENU_CONTEXT_FUR_SHOP);
                    return;
                default:
                    script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_SELECTION,
                        value - (WLDCORE_TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST & TEXT_ID_ENTRY_MASK));
                    wldcore_menu_close_level_and_release_records(&level->slots, 0);
                    if (wldcore_script_process_conditional_set(
                            g_wldcore_map_projection_state.marker.kind, WLDCORE_SCRIPT_ACTION_DEEP_DUNGEON_ENTRY)
                        != 0) {
                        offset = g_wldcore_script_state.args[0] * sizeof(wldcore_deep_dungeon_battle_t);
                        records = g_wldcore_deep_dungeon_battles;
                        record = &records[g_wldcore_script_state.args[0]];
                        g_wldcore_next_map_id[0] = record->map_id;
                        /* Direct ENTD indexing folds the table base and removes a retail addition. */
                        script_set_variable(
                            EVENT_SCRIPT_VAR_CURRENT_ENTD, (&records->entd_ids[0])[((rand() * 8) >> 15) + offset]);
                        script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
                        script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, record->squad_id);
                        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_script_state.args[1]);
                        script_set_variable(
                            EVENT_SCRIPT_VAR_DEEP_DUNGEON_EXIT, (rand() * g_wldcore_script_state.args[2]) >> 15);
                        g_main_system_flags
                            = (g_main_system_flags | MAIN_SYSTEM_FLAG_DEEP_DUNGEON_BATTLE) ^ MAIN_SYSTEM_FLAG_RUNNING;
                    } else {
                        wldcore_fade_start_screen(0, 0x10);
                        wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(1, MAIN_MUSIC_WORLD_MAP));
                    }
                    return;
                }
            }
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_script_state.args[*selected]);
            g_main_system_flags ^= MAIN_SYSTEM_FLAG_RUNNING;
            value = values[*selected] & TEXT_ID_ENTRY_MASK;
            g_wldcore_next_map_id[0] = value + 1;
            return;
        }
        g_main_system_flags = (g_main_system_flags | MAIN_SYSTEM_FLAG_DRAW_MAP) & ~MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_LOCATION_MENU);
        wldcore_menu_close_level_and_release_records(&level->slots, 1);
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        value = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index];
        if ((value & TEXT_ID_SECTION_MASK) == TEXT_ID_WORLD_MENU_BASE) {
            wldcore_sound_play_effect(MAIN_SFX_CANCEL);
            level->slots.result = 0;
            thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
            g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        }
    } else if (g_wldcore_input_new_button_presses & PSX_PAD_SELECT) {
        wldcore_window_set_secondary_render_palette_2(&level->slots);
        value = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index];
        if ((value & TEXT_ID_SECTION_MASK) == TEXT_ID_MAP_NAME_BASE) {
            wldcore_menu_push_message_level((value & TEXT_ID_ENTRY_MASK) + 0x8801, 1);
        } else {
            value &= TEXT_ID_ENTRY_MASK;
            text = value + 0xFEE;
            if (value >= (WLDCORE_TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST & TEXT_ID_ENTRY_MASK)) {
                text = value + 0xFA3;
            }
            wldcore_menu_push_message_level(text, 1);
        }
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_UNLOAD, 2);
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->slots.result = 1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Pop the current menu level and give back the display records it appended:
 * each of the four slots that is not -1 releases one window or render record
 * and its queue entry. With `restore` the level underneath is resumed. Also
 * unblocks the menu buttons and re-shows the location marker. */
static void wldcore_menu_close_level_and_release_records(wldcore_menu_slot_set_t* slots, s32 restore) {
    g_menu_blocked_buttons = 0;
    if (slots->window_a != -1) {
        g_wldcore_window_record_count -= 1;
        g_wldcore_window_render_object_count -= 1;
    }
    if (slots->render_a != -1) {
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 1;
    }
    if (slots->window_b != -1) {
        g_wldcore_window_record_count -= 1;
        g_wldcore_window_render_object_count -= 1;
    }
    if (slots->render_b != -1) {
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 1;
    }
    g_wldcore_menu_stack_depth -= 1;
    if (restore != 0) {
        wldcore_menu_dispatch_resume_handler();
    }
    g_main_system_flags &= ~MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
}

/* At full brightness, run the shop / soldier-office loop (0x80133478),
 * then reload the war-funds script variable, reset the parent level's window,
 * rebuild the panel list for the current location, fade in, clear the scratch
 * buffer and restart the world-map music (0x11b).
 *
 * The local alias for the current location id reproduces the target's
 * saved-register base for its two loads. */
static void wldcore_menu_run_shop_and_rebuild_screen(s32 shop_id) {
    s32* selected;
    u32* clear;
    s32 i;

    world_menu_set_brightness(0x80, 0x80, 0x80);
    wldcore_sound_wait_for_queue_drain();
    world_shop_run_screen(shop_id);
    g_wldcore_displayed_numeric_value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    wldcore_window_reset_state_and_scroll(&g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]);
    selected = &g_wldcore_map_projection_state.marker.kind;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count
        = wldcore_map_build_location_menu_entries(
            *selected, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entries);
    wldcore_menu_push_location_menu_level(*selected);
    wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_REDRAW_MAP, 0x10);
    wldcore_clear_screen_and_set_map_clip_rect(0);
    i = 0x77FF;
    clear = &g_wldcore_scratch_buffer[0x77FF];
    for (; i >= 0; i--) {
        *clear = 0;
        clear--;
    }
    wldcore_sound_enqueue_music_start(WLDCORE_AUDIO_MUSIC(1, MAIN_MUSIC_WORLD_MAP));
}

/* Pushes the wide scrollable text window (0x8009eef8) as menu level type
 * 0x33: it appends one window record for the frame, measures the text, sizes
 * the window around it (at most four rows), initializes the scrollable text
 * state, hides the text render record and the parent level's window, and sets
 * system flags 0x08002000. */
void wldcore_menu_push_scrollable_text_window_level(s32 text) {
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

        index = window_append_animated_record_variant(
            g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_index = index;
        g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CHOICE_PANEL_HEADER;
        g_wldcore_window_records[index].priority = 3;
        g_wldcore_window_records[index].anim_counter = 0;
        g_wldcore_window_records[index].frame_index = 0;
        g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;

        g_wldcore_scrollable_text_window.text_id = text;
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x60;
        g_wldcore_scrollable_text_window.base.x = -(((width + 24) & 0xFFFC) >> 1);
        g_wldcore_scrollable_text_window.base.y = -((rows << 3) + 8);
        g_wldcore_scrollable_text_window.text_width = width;
        g_wldcore_scrollable_text_window.rows_per_page = rows;
        g_wldcore_scrollable_text_window.priority = 3;
        g_wldcore_scrollable_text_window.image_coordinate_mode = 0;
        g_wldcore_scrollable_text_window.extra_render_index = -1;
        g_wldcore_scrollable_text_window.text_substitutions[1] = -1;
        g_wldcore_scrollable_text_window.text_substitutions[0] = -1;
        wldcore_text_init_scrollable_window(&g_wldcore_scrollable_text_window);

        g_wldcore_window_render_records[g_wldcore_scrollable_text_window.render_record_index].flags
            |= WORLD_RENDER_RECORD_HIDDEN;
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].window_index].position.x
            = g_wldcore_scrollable_text_window.base.x + 3;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].window_index].position.y
            = g_wldcore_scrollable_text_window.base.y - 2;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER | MAIN_SYSTEM_FLAG_FREEZE_ANIMATION;
        g_wldcore_menu_stack_depth = depth + 1;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_index].flags
            |= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_menu_stack_records_next[depth].scrollable_text._unknown_20 = 0x28;
        g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_SCROLLABLE_TEXT_WINDOW;
    }
}

/* Menu-level step for the scrollable text window at 0x8009eef8. While its
 * render record is hidden (flag 0x10), an unblocked frame plays sound 0x12
 * and shows the level window and the render record again. Otherwise, once
 * 0x800903e4 reports the window finished, it drops one window and one render
 * object, pops the level, clears system flags 0x08002000, shows the parent
 * level's window and resumes the bar menu. */
void wldcore_menu_step_scrollable_text_window_level(wldcore_menu_stack_record_t* level) {
    s32* index = &g_wldcore_scrollable_text_window.render_record_index;
    s32 depth;

    if (g_wldcore_window_render_records[*index].flags & WORLD_RENDER_RECORD_HIDDEN) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            wldcore_sound_play_effect(MAIN_SFX_WINDOW_OPEN);
            g_wldcore_window_records[level->window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_records[*index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        }
    } else if (!(g_wldcore_window_render_records[*index].flags & WORLD_RENDER_RECORD_POP_IN)
        && wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) == 0) {
        g_wldcore_window_record_count--;
        g_wldcore_window_render_object_count--;
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_menu_stack_depth = depth - 1;
        g_main_system_flags &= ~(MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER | MAIN_SYSTEM_FLAG_FREEZE_ANIMATION);
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_index].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        wldcore_menu_dispatch_resume_handler();
    }
}

/* Queue the fixed "9999"-style number panel used on the world map and append
 * its 0x5c-byte record. Sibling of wldcore_menu_push_number_panel_level_with_argument. */
static void wldcore_menu_push_fixed_number_panel_level(void) {
    point32_t origin;
    s32 record_index;

    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS;
    g_wldcore_menu_ordering_table_offset = 3;
    origin.x = -0x28;
    origin.y = -0x16;
    g_wldcore_active_menu_value = 0;
    wldcore_window_init_number_panel_render_thread(
        WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0x3E70000, WLDCORE_TEXT_ID_PARTY_NUMBER_LABEL, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_FIXED_NUMBER_PANEL;
    g_wldcore_menu_stack_depth = record_index + 1;
}

/* Poll the confirm/cancel buttons for the world-map unit browser panel and
 * tear it down once the panel's render thread has finished.
 *
 * level->result is 0 while waiting, 1 on confirm and -1 on cancel. */
void wldcore_menu_step_unit_browser_panel(wldcore_menu_panel_level_t* level) {
    u32 buttons;

    if ((level->result != 0) && (thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0)) {
        g_menu_blocked_buttons = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        g_wldcore_menu_stack_depth--;
        if (level->result == 1) {
            if (g_wldcore_active_menu_value < 0x200) {
                main_entd_init_event_unit_data(g_wldcore_active_menu_value);
                world_text_generate_formation_unit_name_string();
                g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
            }
        }
        wldcore_menu_dispatch_resume_handler();
        return;
    }

    buttons = g_wldcore_input_new_button_presses;
    if ((buttons & PSX_PAD_CROSS) != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = -1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
    } else if ((buttons & PSX_PAD_CIRCLE) != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
    }
}

/* Opens a full-screen menu level (type 5): hides the map, loads data set
 * 0x16 or 0x17 by `EVENT_SCRIPT_VAR_TOWN_BACKGROUND`, starts WORLD thread 14 on
 * 0x8010f250 with text 0xb806, and allocates two window records and one
 * render record for the level before 0x8007148c fills it. */
static void wldcore_bar_push_menu_level(void) {
    s32 value;
    s32 set;
    s32 record_index;

    wldcore_clear_screen_and_set_map_clip_rect(1);
    wldcore_window_set_render_objects_visible(0);
    wldcore_fade_start_screen(0, 0x10);
    value = script_get_variable(EVENT_SCRIPT_VAR_TOWN_BACKGROUND);
    set = value == 1 ? 0x16 : 0;
    if (value == 2) {
        set = 0x17;
    }
    wldcore_gfx_load_background_set(set);
    file_wait_for_pending_variant();
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE | MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
    thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BAR_WELCOME, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_pair_render.first_window
        = window_append_animated_record_variant(
            g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_pair_render.second_window
        = window_append_animated_record_variant(
            g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_pair_render.render_index
        = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    wldcore_bar_build_menu_rows(&g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].window_pair_render.panel_rows = 0;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_BAR_MENU;
    g_wldcore_menu_stack_depth = record_index + 1;
}

void wldcore_bar_build_menu_rows(wldcore_menu_list_window_level_t* level) {
    s32 index;
    point32_t point;
    RECT bounds;
    s32 unused[8];
    menu_cursor_state_t* cursor;
    point32_t* base;
    wldcore_window_render_record_t* render;
    s32 i;

    index = level->main_window;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    index = level->side_window;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_LIST_SIDE_LABEL;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    level->entries[0] = WLDCORE_BAR_MENU_ROW_RUMORS;
    level->entry_count = 1;
    if (script_get_variable(EVENT_SCRIPT_VAR_PROPOSITIONS_ENABLED) != 0) {
        level->entries[1] = WLDCORE_BAR_MENU_ROW_AVAILABLE_PROPOSITIONS;
        level->entry_count++;
    }
    if (wldcore_proposition_count_completed(g_wldcore_map_projection_state.marker.kind) != 0) {
        level->entries[level->entry_count] = WLDCORE_BAR_MENU_ROW_COMPLETED_PROPOSITIONS;
        level->entry_count++;
    }
    if (g_main_save_proposition_count != wldcore_proposition_count_completed(-1)) {
        level->entries[level->entry_count] = WLDCORE_BAR_MENU_ROW_PROPOSITIONS;
        level->entry_count++;
    }
    level->entries[level->entry_count] = WLDCORE_BAR_MENU_ROW_LEAVE;
    level->entry_count++;
    index = level->frame_render;
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = 0x48;
    bounds.h = level->entry_count * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 0xA;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x78;
    base->y = -0x28;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < level->entry_count; i++) {
        window_display_text_entry_variant(
            index, level->entries[i] + TEXT_ID_WORLD_MENU_BASE, point, g_wldcore_window_image_buffer);
        point.y += 0x10;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);
    cursor = &g_wldcore_menu_cursor_states[WLDCORE_CURSOR_SLOT_BAR_MENU];
    if (level->entry_count < cursor->selected_index + 1) {
        cursor->selected_index = 0;
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.selected_entry = cursor->selected_index;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.mode = WLDCORE_LIST_WINDOW_MODE_BROWSE;
    render = &g_wldcore_window_render_records[index];
    g_wldcore_window_records[level->main_window].position.x = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_window_records[level->main_window].position.y
        = g_wldcore_window_render_records[index].base.y + cursor->selected_index * 0x10 + 0xE;
    g_wldcore_window_records[level->side_window].position.x = render->base.x + 3;
    g_wldcore_window_records[level->side_window].position.y = g_wldcore_window_render_records[index].base.y - 2;
}

/* Restore the bar list's palettes and cursor animation, then show its welcome text. */
void wldcore_window_clear_layout_states_and_queue_msg_b806(wldcore_menu_stack_record_t* level) {
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
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BAR_WELCOME, 0);
}
