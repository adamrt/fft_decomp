/* World-map departure effects: capture, draw and advance the spinning or fading transition into an event. */

#include "fft/display_object.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/music.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/world_map.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* The event transition's spinning screen-capture sprite, sorted with GsSortSprite by
 * event_transition_sprite_draw. */
static GsSPRITE s_event_transition_sprite = { 0 };

/* The byte color triple of the screen-wide fade overlay. It
 * overlaps g_world_map_fade.boxes[0].r, but the target addresses
 * it separately from the 32-bit flags: reaching it through the fade-state
 * struct makes the flags update use absolute addressing instead of a base
 * register. */
static CVECTOR s_world_map_fade_color;

/* Pushes the event transition level (MENU_STACK_LEVEL_EVENT_TRANSITION)
 * after starting the seek to BATTLE.BIN. A FADE transition freezes the map
 * and fades the screen out. The spinning ones hide everything on the map but
 * the tiles, save VRAM (0x100, 0) 128x256 to (0x200, 0x100) and clear it for
 * the screen capture, set up the transition sprite over that area, fade the
 * music out and pick a random drift and spin direction. */
void event_transition_open(s32 mode) {
    RECT rect;
    s32 depth;

    event_transition_seek_battle_bin();
    if (mode == EVENT_TRANSITION_MODE_FADE) {
        g_world_map_flags |= WORLD_MAP_FLAG_FREEZE_ANIMATION;
        world_map_fade_start(WORLD_MAP_FADE_OUT, 0x20);
    } else {
        rect.x = 0x100;
        rect.y = 0;
        rect.w = 0x80;
        rect.h = 0x100;
        g_world_map_flags |= WORLD_MAP_FLAG_DRAW_MAP | WORLD_MAP_FLAG_HIDE_MAP_DOTS | WORLD_MAP_FLAG_HIDE_MAP_ROUTES
            | WORLD_MAP_FLAG_NO_BACKGROUND_IMAGE | WORLD_MAP_FLAG_HIDE_CONTEXT_VALUE | WORLD_MAP_FLAG_HIDE_CALENDAR_DATE
            | WORLD_MAP_FLAG_HIDE_LOCATION_LABEL;
        g_world_map_marker_state.marker.render.flags |= DISPLAY_OBJECT_HIDDEN;
        MoveImage(&rect, 0x200, 0x100);
        DrawSync(0);
        rect.x = 0x100;
        rect.y = 0;
        rect.w = 0x100;
        rect.h = 0xF0;
        ClearImage(&rect, 0, 0, 0);
        s_event_transition_sprite.attribute = 0x42000000;
        s_event_transition_sprite.x = 0x20;
        s_event_transition_sprite.y = 0x20;
        s_event_transition_sprite.tpage = GetTPage(2, 0, 0x100, 0);
        s_event_transition_sprite.w = 0xFF;
        s_event_transition_sprite.mx = 0xA0;
        s_event_transition_sprite.my = 0x98;
        s_event_transition_sprite.scaley = ONE;
        s_event_transition_sprite.scalex = ONE;
        s_event_transition_sprite.cy = 0;
        s_event_transition_sprite.cx = 0;
        s_event_transition_sprite.v = 0;
        s_event_transition_sprite.u = 0;
        s_event_transition_sprite.h = 0xF0;
        s_event_transition_sprite.rotate = 0;
        s_event_transition_sprite.r = 0x80;
        s_event_transition_sprite.g = 0x80;
        s_event_transition_sprite.b = 0x80;
        music_queue_add(MUSIC_QUEUE_COMMAND_FADE_OUT, 0x10);
        g_menu_stack_records[g_menu_stack_depth].event_transition.frame = 0;
        g_menu_stack_records[g_menu_stack_depth].event_transition.anim_step = 0;
        g_menu_stack_records[g_menu_stack_depth].event_transition.drift_direction = (rand() * 4) >> 15;
        g_menu_stack_records[g_menu_stack_depth].event_transition.spin_direction = (rand() * 2) >> 15;
        g_menu_stack_records[g_menu_stack_depth].event_transition.phase = EVENT_TRANSITION_PHASE_SPIN_IN;
        g_menu_stack_records[g_menu_stack_depth].event_transition.fade_phase = EVENT_TRANSITION_FADE_NONE;
    }

    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].event_transition.mode = mode;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_EVENT_TRANSITION;
    g_menu_stack_depth = depth + 1;
}

/* Draws the event transition's spinning sprite while that level is on top of
 * the menu stack, unless it is a plain fade. */
void event_transition_sprite_draw(GsOT* ordering_table) {
    s32 current = g_menu_stack_depth;

    if (g_menu_stack_types[current - 1] == MENU_STACK_LEVEL_EVENT_TRANSITION
        && g_menu_stack_records[current - 1].event_transition.mode != EVENT_TRANSITION_MODE_FADE) {
        GsSortSprite(&s_event_transition_sprite, ordering_table, 2);
    }
}

/* Per-frame step of the event-transition level (MENU_STACK_LEVEL_EVENT_TRANSITION)
 * pushed by event_transition_open: the screen effect that
 * leads from the map into an event or battle.
 *
 * In FADE mode it only waits for the screen fade and then ends the world loop
 * (toggling WORLD_MAP_FLAG_RUNNING off). Otherwise frame 1 copies the screen
 * to VRAM (0x100, 0) and hides the map and root windows; the sprite drifts a
 * pixel a frame and, every third frame, spins and shrinks in SPIN_IN, then in
 * SPIN mode spins back out (SPIN_OUT) textured with the captured screen. The
 * fade box over g_world_map_fade_color flashes up by 0x10 a frame
 * to 0x80 (FLASH), then whites out by 8 a frame (WHITE_OUT); at full white the
 * world loop ends and the picture at VRAM (0x200, 0x100) is moved to
 * (0x100, 0). */
void event_transition_update(menu_stack_record_t* level) {
    RECT rect;
    s32 step;
    s32 shade;
    u8 red_sum;
    s32 angle;

    if (level->event_transition.mode == EVENT_TRANSITION_MODE_FADE) {
        if (!(g_world_map_flags & WORLD_MAP_FLAG_FADING)) {
            g_world_map_flags = g_world_map_flags ^ WORLD_MAP_FLAG_RUNNING;
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
        g_display_object_animated_pool[g_world_map_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[g_world_map_cursor_window].flags |= DISPLAY_OBJECT_HIDDEN;
    }

    /* White-out: brighten the overlay by 8 a frame; at full white, toggle the running flag
     * and move the image at VRAM (0x200, 0x100) back to (0x100, 0). */
    if (level->event_transition.fade_phase == EVENT_TRANSITION_FADE_WHITE_OUT) {
        red_sum = s_world_map_fade_color.r + 8;
        s_world_map_fade_color.r = red_sum;
        s_world_map_fade_color.g = s_world_map_fade_color.g + 8;
        s_world_map_fade_color.b = s_world_map_fade_color.b + 8;
        if (red_sum >= 0xF8) {
            s_world_map_fade_color.b = 0xFF;
            s_world_map_fade_color.g = 0xFF;
            s_world_map_fade_color.r = 0xFF;
            rect.x = 0x200;
            rect.y = 0x100;
            rect.w = 0x80;
            rect.h = 0x100;
            g_world_map_flags = g_world_map_flags ^ WORLD_MAP_FLAG_RUNNING;
            MoveImage(&rect, 0x100, 0);
            DrawSync(0);
        }
        return;
    }

    /* Flash: brighten by 0x10 a frame to 0x80, then cut to black and start the white-out. */
    if (level->event_transition.fade_phase == EVENT_TRANSITION_FADE_FLASH) {
        s_world_map_fade_color.r = s_world_map_fade_color.r + 0x10;
        s_world_map_fade_color.g = s_world_map_fade_color.g + 0x10;
        s_world_map_fade_color.b = s_world_map_fade_color.b + 0x10;
        if (s_world_map_fade_color.g >= 0x80) {
            s_world_map_fade_color.r = 0;
            s_world_map_fade_color.g = 0;
            s_world_map_fade_color.b = 0;
            level->event_transition.fade_phase = EVENT_TRANSITION_FADE_WHITE_OUT;
        }
    }

    /* The sprite drifts a pixel a frame on each axis; spin, scale and shade step every third frame. */
    switch (level->event_transition.phase) {
    case EVENT_TRANSITION_PHASE_SPIN_IN:
        if (level->event_transition.drift_direction & EVENT_TRANSITION_DRIFT_RIGHT) {
            s_event_transition_sprite.x = s_event_transition_sprite.x + 1;
        } else {
            s_event_transition_sprite.x = s_event_transition_sprite.x - 1;
        }
        if (level->event_transition.drift_direction & EVENT_TRANSITION_DRIFT_DOWN) {
            s_event_transition_sprite.y = s_event_transition_sprite.y + 1;
        } else {
            s_event_transition_sprite.y = s_event_transition_sprite.y - 1;
        }

        if (level->event_transition.frame % 3 == 0) {
            step = level->event_transition.anim_step + 2;
            level->event_transition.anim_step = step;
            s_event_transition_sprite.scalex = 0x1100 - (step << 7);
            s_event_transition_sprite.scaley = 0x1100 - (level->event_transition.anim_step << 7);
            if (level->event_transition.spin_direction != 0) {
                angle = s_event_transition_sprite.rotate + 0x2000;
            } else {
                angle = s_event_transition_sprite.rotate - 0x2000;
            }
            s_event_transition_sprite.rotate = angle;

            /* Stored as a byte, -0x80 - 4 * step darkens the sprite from 0x80. */
            shade = -0x80 - (level->event_transition.anim_step * 4);
            s_event_transition_sprite.b = shade;
            s_event_transition_sprite.g = shade;
            s_event_transition_sprite.r = shade;

            if (level->event_transition.mode == EVENT_TRANSITION_MODE_SPIN) {
                if (level->event_transition.anim_step == 4) {
                    sound_play_effect(SOUND_SFX_EVENT_TRANSITION_MODE_0);
                }

                /* Spin back out at full size, textured from the 16-bit page holding the capture. */
                if (level->event_transition.anim_step >= 0x20) {
                    level->event_transition.phase = EVENT_TRANSITION_PHASE_SPIN_OUT;
                    s_event_transition_sprite.attribute = 0x52000000;
                    s_event_transition_sprite.tpage = GetTPage(2, 1, 0x100, 0);
                    s_event_transition_sprite.scaley = ONE;
                    s_event_transition_sprite.scalex = ONE;
                }
            } else {
                if (level->event_transition.anim_step == 4) {
                    sound_play_effect(SOUND_SFX_EVENT_TRANSITION_MODE_1);
                }
                if (level->event_transition.anim_step >= 0x20) {
                    level->event_transition.phase = EVENT_TRANSITION_PHASE_DONE;
                }
                if (level->event_transition.anim_step == 0x18) {
                    s_world_map_fade_color.r = 0;
                    s_world_map_fade_color.g = 0;
                    s_world_map_fade_color.b = 0;
                    g_world_map_fade.flags[0] |= WORLD_MAP_FADE_BOX_VISIBLE;
                    level->event_transition.fade_phase = EVENT_TRANSITION_FADE_WHITE_OUT;
                }
            }
        }
        break;

    case EVENT_TRANSITION_PHASE_SPIN_OUT:
        if (level->event_transition.drift_direction & EVENT_TRANSITION_DRIFT_RIGHT) {
            s_event_transition_sprite.x = s_event_transition_sprite.x - 1;
        } else {
            s_event_transition_sprite.x = s_event_transition_sprite.x + 1;
        }
        if (level->event_transition.drift_direction & EVENT_TRANSITION_DRIFT_DOWN) {
            s_event_transition_sprite.y = s_event_transition_sprite.y - 1;
        } else {
            s_event_transition_sprite.y = s_event_transition_sprite.y + 1;
        }

        if (level->event_transition.frame % 3 == 0) {
            step = level->event_transition.anim_step + 4;
            level->event_transition.anim_step = step;
            s_event_transition_sprite.scalex = 0x1100 - ((0x40 - step) << 7);
            s_event_transition_sprite.scaley = 0x1100 - ((0x40 - level->event_transition.anim_step) << 7);
            s_event_transition_sprite.b = (level->event_transition.anim_step - 0x20) * 4;
            if (level->event_transition.spin_direction != 0) {
                angle = s_event_transition_sprite.rotate - 0x2000;
            } else {
                angle = s_event_transition_sprite.rotate + 0x2000;
            }
            s_event_transition_sprite.rotate = angle;

            if (level->event_transition.anim_step >= 0x38) {
                s_world_map_fade_color.r = 0;
                s_world_map_fade_color.g = 0;
                s_world_map_fade_color.b = 0;
                g_world_map_fade.flags[0] |= WORLD_MAP_FADE_BOX_VISIBLE;
                level->event_transition.fade_phase = EVENT_TRANSITION_FADE_FLASH;
            }
            if (level->event_transition.anim_step >= 0x50) {
                s_event_transition_sprite.r = 0;
                s_event_transition_sprite.g = 0;
                s_event_transition_sprite.b = 0;
                level->event_transition.phase = EVENT_TRANSITION_PHASE_DONE;
            }
        }
        break;
    }

    level->event_transition.frame = level->event_transition.frame + 1;
}
