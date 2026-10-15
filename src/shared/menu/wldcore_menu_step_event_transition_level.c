#include "fft/wldcore.h"
#include "psx/libgpu.h"

/* The byte color triple of the screen-wide fade overlay. It
 * overlaps g_wldcore_screen_fade_state.boxes[0].r, but the target addresses
 * it separately from the 32-bit flags: reaching it through the fade-state
 * struct makes the flags update use absolute addressing instead of a base
 * register. */
extern CVECTOR g_wldcore_screen_fade_box_0_color;

/* Per-frame step of the event-transition menu level (type 0x23) pushed by
 * wldcore_menu_push_event_transition_level.
 *
 * Mode 2 only toggles the display flag. Otherwise the indicator sprite spins
 * and scales in on phase 0 and back out on phase 1, one update every third
 * frame; anim_step is the animation step. The fade overlay at g_wldcore_screen_fade_box_0_color ramps
 * in 0x10 increments (phase 1) and 8 increments (phase 2), and once it
 * saturates the captured screen is moved back from (0x200,0x100) to
 * (0x100,0). */
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
    if (level->fade_phase == WLDCORE_EVENT_TRANSITION_FADE_WHITE_OUT) {
        red_sum = g_wldcore_screen_fade_box_0_color.r + 8;
        g_wldcore_screen_fade_box_0_color.r = red_sum;
        g_wldcore_screen_fade_box_0_color.g = g_wldcore_screen_fade_box_0_color.g + 8;
        g_wldcore_screen_fade_box_0_color.b = g_wldcore_screen_fade_box_0_color.b + 8;
        if (red_sum >= 0xF8) {
            g_wldcore_screen_fade_box_0_color.b = 0xFF;
            g_wldcore_screen_fade_box_0_color.g = 0xFF;
            g_wldcore_screen_fade_box_0_color.r = 0xFF;
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
    if (level->fade_phase == WLDCORE_EVENT_TRANSITION_FADE_FLASH) {
        g_wldcore_screen_fade_box_0_color.r = g_wldcore_screen_fade_box_0_color.r + 0x10;
        g_wldcore_screen_fade_box_0_color.g = g_wldcore_screen_fade_box_0_color.g + 0x10;
        g_wldcore_screen_fade_box_0_color.b = g_wldcore_screen_fade_box_0_color.b + 0x10;
        if (g_wldcore_screen_fade_box_0_color.g >= 0x80) {
            g_wldcore_screen_fade_box_0_color.r = 0;
            g_wldcore_screen_fade_box_0_color.g = 0;
            g_wldcore_screen_fade_box_0_color.b = 0;
            level->fade_phase = WLDCORE_EVENT_TRANSITION_FADE_WHITE_OUT;
        }
    }
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
            shade = -0x80 - (level->anim_step * 4);
            g_wldcore_menu_indicator_sprite.b = shade;
            g_wldcore_menu_indicator_sprite.g = shade;
            g_wldcore_menu_indicator_sprite.r = shade;
            if (level->mode == WLDCORE_EVENT_TRANSITION_MODE_SPIN) {
                if (level->anim_step == 4) {
                    wldcore_sound_play_effect(MAIN_SFX_EVENT_TRANSITION_MODE_0);
                }
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
                    g_wldcore_screen_fade_box_0_color.r = 0;
                    g_wldcore_screen_fade_box_0_color.g = 0;
                    g_wldcore_screen_fade_box_0_color.b = 0;
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
                g_wldcore_screen_fade_box_0_color.r = 0;
                g_wldcore_screen_fade_box_0_color.g = 0;
                g_wldcore_screen_fade_box_0_color.b = 0;
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
