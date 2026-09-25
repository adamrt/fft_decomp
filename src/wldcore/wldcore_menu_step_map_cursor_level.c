#include "fft/wldcore.h"
#include "psx/gte.h"
#include "psx/pad.h"

/* Provisional: the map camera block at 0x8009f2c0. The target relates the
 * origin and scale addresses to one symbol (origin.vx is kept in a register
 * while both scale stores use the absolute form), so they are one object. */
typedef struct wldcore_map_projection_view {
    VECTOR origin;
    VECTOR scale;
} wldcore_map_projection_view_t;

extern wldcore_map_projection_view_t g_wldcore_map_projection_view;

/* Provisional view of wldcore_window_record_t with its x/y pair as one
 * point: the target passes records[i].x/y by value with two absolute-form
 * loads, which only a real point member reproduces (a local copy spills to
 * the stack and a cast view computes the address into a register). */
typedef struct wldcore_window_position_record {
    u8 unknown_00[0x18];
    wldcore_point32_t position;
    s32 field_20;
} wldcore_window_position_record_t;
extern wldcore_window_position_record_t g_wldcore_window_position_records[];

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
void wldcore_menu_step_map_cursor_level(wldcore_menu_map_cursor_level_t* level) {
    wldcore_point32_t amount;
    wldcore_point32_t point;
    wldcore_point32_t target;
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

    /* The exits funnel into the shared snap-hit tail at `finish`, and the
     * zoom-animation arm jumps into the redraw request inside the wide-view
     * scroll block (`raise`); in the target's block order these need gotos. */
    if (g_wldcore_location_entry_state.flags & 1) {
        g_main_system_flags |= 2;
        goto finish;
    }

    if (level->wide_view == 0 && !(g_wldcore_previous_system_flags & 2)) {
        if (level->pending_mode != 0) {
            flags = g_main_system_flags;
            if (flags & 8) {
                return;
            }
            g_main_system_flags = flags ^ 4;
            if (level->pending_mode == 3) {
                g_main_system_flags = flags ^ 5;
            }
            level->pending_mode = 0;
        }
        if (g_main_system_flags & 8) {
            /* Skips the zoom handling and the wide-view toggle below. */
            goto scroll;
        }
        if (!(g_wldcore_map_zoom_motion.flags & 3) && g_wldcore_audio_queue.count == 0) {
            if (g_wldcore_new_button_presses & PSX_PAD_CIRCLE) {
                hit = wldcore_map_find_dot_at_point(g_wldcore_window_position_records[level->window_index].position);
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
                        wldcore_window_reset_state_and_scroll((wldcore_menu_stack_record_t*)level);
                        wldcore_menu_push_location_menu_level(hit);
                        return;
                    }
                } else {
                    g_wldcore_window_records[level->window_index].flags |= 0x10;
                    wldcore_menu_push_focus_location_level(g_wldcore_map_projection_state.marker.kind);
                    return;
                }
            }
            buttons = g_wldcore_new_button_presses;
            if (buttons & (PSX_PAD_TRIANGLE | PSX_PAD_START)) {
                wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
                wldcore_window_reset_state_and_scroll((wldcore_menu_stack_record_t*)level);
                wldcore_menu_push_entry_window_level();
                return;
            }
            if (buttons & PSX_PAD_SELECT) {
                hit = wldcore_map_find_dot_at_point(g_wldcore_window_position_records[level->window_index].position);
                if (hit != 0) {
                    hit--;
                    wldcore_window_reset_state_and_scroll((wldcore_menu_stack_record_t*)level);
                    wldcore_gfx_toggle_captured_world_frame(1);
                    wldcore_menu_push_message_level(hit + 0x8000, 1);
                    return;
                }
            }
        }
    }

    motion = g_wldcore_map_zoom_motion.flags;
    if (!(motion & 1)) {
        if (level->wide_view == 0 && (g_wldcore_new_button_presses & (PSX_PAD_L1 | PSX_PAD_R1))) {
            g_wldcore_window_records[level->window_index].flags |= 0x10;
            g_wldcore_map_zoom_motion.progress = 0x20;
            g_wldcore_map_zoom_motion.distance = 0;
            g_wldcore_map_zoom_motion.flags = (g_wldcore_map_zoom_motion.flags ^ 2) | 1;
            if (g_wldcore_map_zoom_motion.flags & 2) {
                g_wldcore_map_zoom_motion.origin = g_wldcore_map_projection_view.origin;
            }
            level->snap_hit = 0;
            if (g_wldcore_map_zoom_motion.flags & 2) {
                wldcore_sound_play_effect(0x10);
            } else {
                wldcore_sound_play_effect(0xF);
            }
            goto finish;
        }
    } else {
        frame = g_wldcore_map_zoom_motion.distance;
        duration = g_wldcore_map_zoom_motion.progress;
        step_x = (frame << 11) / duration;
        if (motion & 2) {
            g_wldcore_map_projection_view.origin.vx
                = g_wldcore_map_zoom_motion.origin.vx + ((0 - g_wldcore_map_zoom_motion.origin.vx) * frame) / duration;
            g_wldcore_map_projection_view.origin.vy
                = g_wldcore_map_zoom_motion.origin.vy + ((8 - g_wldcore_map_zoom_motion.origin.vy) * frame) / duration;
            g_wldcore_map_projection_view.scale.vx = g_wldcore_map_projection_view.scale.vy = ONE - step_x;
            g_wldcore_map_zoom_motion.distance = frame + 1;
            g_main_system_flags |= 0x30;
            if (frame + 1 == duration) {
                g_wldcore_map_projection_view.scale.vy = 0x800;
                g_wldcore_map_projection_view.scale.vx = 0x800;
                g_wldcore_map_projection_view.origin.vx = 0;
                g_wldcore_map_projection_view.origin.vy = 8;
                g_wldcore_map_zoom_motion.flags = motion ^ 1;
            }
        } else {
            g_wldcore_map_projection_view.origin.vx = (g_wldcore_map_zoom_motion.origin.vx * frame) / duration;
            g_wldcore_map_projection_view.origin.vy
                = ((g_wldcore_map_zoom_motion.origin.vy - 8) * frame) / duration + 8;
            g_wldcore_map_projection_view.scale.vx = g_wldcore_map_projection_view.scale.vy = step_x + 0x800;
            g_wldcore_map_zoom_motion.distance = frame + 1;
            if (frame + 1 == duration) {
                g_wldcore_map_projection_view.scale.vy = ONE;
                g_wldcore_map_projection_view.scale.vx = ONE;
                g_wldcore_map_projection_view.origin = g_wldcore_map_zoom_motion.origin;
                g_wldcore_map_zoom_motion.flags ^= 1;
                g_main_system_flags &= ~0x30;
            }
        }
        goto raise;
    }

    if (g_wldcore_map_zoom_motion.flags & 2) {
        goto finish;
    }

    if (!(g_wldcore_current_button_input & PSX_PAD_SQUARE)) {
        if (level->wide_view != 0) {
            level->wide_view = 0;
            g_wldcore_window_records[level->alt_window_index].flags |= 0x10;
        }
        g_wldcore_window_records[level->window_index].flags &= ~0x10;
    } else if (level->wide_view != 1) {
        level->wide_view = 1;
        g_wldcore_window_records[level->window_index].flags |= 0x10;
        g_wldcore_window_records[level->alt_window_index].flags &= ~0x10;
        g_wldcore_window_records[level->alt_window_index].anim_counter
            = g_wldcore_window_records[level->alt_window_index].frame_index = 0;
        g_wldcore_map_cursor_scroll_y.value = 0;
        g_wldcore_map_cursor_scroll_x.value = 0;
        g_wldcore_map_cursor_scroll_y.output = 0;
        g_wldcore_map_cursor_scroll_x.output = 0;
    }

scroll:
    if (level->wide_view == 0) {
        held = g_wldcore_current_button_input;
        if (held & (PSX_PAD_RIGHT | PSX_PAD_LEFT)) {
            if (held & PSX_PAD_LEFT) {
                wldcore_map_update_scroll_axis_speed(&g_wldcore_map_cursor_scroll_x, -1);
            } else {
                wldcore_map_update_scroll_axis_speed(&g_wldcore_map_cursor_scroll_x, 1);
            }
        } else {
            wldcore_map_update_scroll_axis_speed(&g_wldcore_map_cursor_scroll_x, 0);
        }
        held = g_wldcore_current_button_input;
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
        if (!(g_wldcore_map_zoom_motion.flags & 2)) {
            target.x = g_wldcore_window_records[level->window_index].x + amount.x;
            target.y = g_wldcore_window_records[level->window_index].y + amount.y;
            level->snap_hit = wldcore_map_get_dot_snap_step(target, &step_x, &step_y);
            amount.x += step_x;
            amount.y += step_y;
            target.x = g_wldcore_window_records[level->window_index].x + amount.x;
            target.y = g_wldcore_window_records[level->window_index].y + amount.y;
            saved = g_wldcore_map_projection_view.origin;
            if ((amount.x < 0 && target.x < -0x40) || (amount.x > 0 && target.x >= 0x49)) {
                wldcore_map_apply_clamped_horizontal_scroll(&amount.x, &g_wldcore_map_projection_view.origin.vx);
            }
            if ((amount.y < 0 && target.y < -0x40) || (amount.y > 0 && target.y >= 0x41)) {
                wldcore_map_apply_clamped_vertical_scroll(
                    &amount, (wldcore_point32_t*)&g_wldcore_map_projection_view.origin);
            }
            if (saved.vx != g_wldcore_map_projection_view.origin.vx
                || saved.vy != g_wldcore_map_projection_view.origin.vy) {
                g_main_system_flags |= 2;
                g_wldcore_map_projection_state.flags |= 1;
            }
        }
        g_wldcore_window_records[level->window_index].x += amount.x;
        g_wldcore_window_records[level->window_index].y += amount.y;
        if (g_wldcore_window_records[level->window_index].x < -0x70) {
            g_wldcore_window_records[level->window_index].x = -0x70;
        }
        if (g_wldcore_window_records[level->window_index].x >= 0x79) {
            g_wldcore_window_records[level->window_index].x = 0x78;
        }
        if (g_wldcore_window_records[level->window_index].y < -0x68) {
            g_wldcore_window_records[level->window_index].y = -0x68;
        }
        if (g_wldcore_window_records[level->window_index].y >= 0x69) {
            g_wldcore_window_records[level->window_index].y = 0x68;
        }
    } else {
        held = g_wldcore_current_button_input;
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
            wldcore_map_apply_clamped_scroll(&amount, (wldcore_point32_t*)&g_wldcore_map_projection_view.origin);
            if (saved.vx != g_wldcore_map_projection_view.origin.vx
                || saved.vy != g_wldcore_map_projection_view.origin.vy) {
            raise:
                g_main_system_flags |= 2;
                g_wldcore_map_projection_state.flags |= 1;
            }
        }
    }

finish:
    if (level->wide_view == 0 && (hit = level->snap_hit) != 0) {
        g_wldcore_proposition_selected_entry = hit;
        return;
    }
    g_wldcore_proposition_selected_entry = 0;
}
