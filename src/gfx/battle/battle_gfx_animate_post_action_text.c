#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Animate and queue the three numeric post-action display sprites.
 *
 * Scales each digit sprite by its per-frame zoom curve, starts the next
 * queued result once the animation passes frame 50, and retires the display
 * after frame 61.
 *
 * Keep the curves external: local initializers change the following switch
 * table's alignment relative to the original rodata. */
void battle_gfx_animate_post_action_text(battle_unit_t* unit, const SVECTOR* screen_coords) {
    SVECTOR position;
    SVECTOR zoom_0 = g_status_bubble_zoom;
    SVECTOR zoom_1 = g_status_bubble_zoom;
    SVECTOR zoom_2 = g_status_bubble_zoom;
    s16 scale_0[21];
    s16 scale_1[21];
    s16 scale_2[21];
    s16 critical_scale_0[21];
    s16 critical_scale_1[21];
    s16 critical_scale_2[21];
    battle_gfx_sprite_display_data_t* display_0;
    battle_gfx_sprite_display_data_t* display_1;
    battle_gfx_sprite_display_data_t* display_2;
    action_result_display_t* display;
    u16 progress;

    memcpy(scale_0, g_battle_numeric_digit_0_zoom_curve, sizeof(scale_0));
    memcpy(scale_1, g_battle_numeric_digit_1_zoom_curve, sizeof(scale_1));
    memcpy(scale_2, g_battle_numeric_digit_2_zoom_curve, sizeof(scale_2));
    memcpy(critical_scale_0, g_battle_numeric_digit_0_critical_zoom_curve, sizeof(critical_scale_0));
    memcpy(critical_scale_1, g_battle_numeric_digit_1_critical_zoom_curve, sizeof(critical_scale_1));
    memcpy(critical_scale_2, g_battle_numeric_digit_2_critical_zoom_curve, sizeof(critical_scale_2));
    display_0 = unit->numeric_display.displays[0];
    display_1 = unit->numeric_display.displays[1];
    display_2 = unit->numeric_display.displays[2];
    progress = unit->numeric_display.progress + g_main_system_animation_speed;
    display = &unit->numeric_display;
    if (progress < 21) {
        if (unit->numeric_display.selector & BATTLE_NUMERIC_DISPLAY_MODIFIER_CRITICAL) {
            main_util_set_svector(
                &zoom_0, critical_scale_0[progress], critical_scale_0[progress], critical_scale_0[progress]);
            main_util_set_svector(
                &zoom_1, critical_scale_1[progress], critical_scale_1[progress], critical_scale_1[progress]);
            main_util_set_svector(
                &zoom_2, critical_scale_2[progress], critical_scale_2[progress], critical_scale_2[progress]);
        } else {
            switch (unit->numeric_display.selector & BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK) {
            case BATTLE_NUMERIC_DISPLAY_HP_DAMAGE ... BATTLE_NUMERIC_DISPLAY_MA_GAIN:
                main_util_set_svector(&zoom_0, scale_0[progress], scale_0[progress], scale_0[progress]);
                main_util_set_svector(&zoom_1, scale_1[progress], scale_1[progress], scale_1[progress]);
                main_util_set_svector(&zoom_2, scale_2[progress], scale_2[progress], scale_2[progress]);
                break;
            case BATTLE_NUMERIC_DISPLAY_MISSED:
            case BATTLE_NUMERIC_DISPLAY_GUARDED:
            case BATTLE_NUMERIC_DISPLAY_GAINED_EXP:
            case BATTLE_NUMERIC_DISPLAY_GAINED_JP:
                main_util_set_svector(&zoom_0, scale_1[progress], scale_1[progress], scale_1[progress]);
                main_util_set_svector(&zoom_1, scale_1[progress], scale_1[progress], scale_1[progress]);
                main_util_set_svector(&zoom_2, scale_1[progress], scale_1[progress], scale_1[progress]);
                break;
            }
        }
    } else if (progress >= 50 && unit->numeric_display.progress < 50) {
        if (battle_gfx_build_next_action_result_display(unit) != 0) {
            return;
        }
        if (battle_gfx_build_next_special_action_result_display(unit) != 0) {
            return;
        }
        unit->numeric_display.displays[0]->tpage = unit->numeric_display.displays[1]->tpage
            = unit->numeric_display.displays[2]->tpage = 0x3f;
        map_modify_palette(MAP_COLOR_MODE_BASE, 2, MAP_CLUT_ROW_ACTION_RESULTS, unit->runtime_id, 0, -31, -31, -31);
        battle_gfx_set_sprite_part_semi_transparent(display_0, 0, 1);
        battle_gfx_set_sprite_part_semi_transparent(display_1, 0, 1);
        battle_gfx_set_sprite_part_semi_transparent(display_2, 0, 1);
    } else if (progress >= 61) {
        if (battle_gfx_build_next_action_result_display(unit) != 0) {
            return;
        }
        if (battle_gfx_build_next_special_action_result_display(unit) != 0) {
            return;
        }
        unit->numeric_display.active = 0;
    }
    position.vx = screen_coords->vx + 0x15;
    position.vy = screen_coords->vy - 8;
    switch (display->selector & BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK) {
    case BATTLE_NUMERIC_DISPLAY_HP_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_MP_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_SPEED_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_CT_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_BRAVE_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_FAITH_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_PA_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_MA_DAMAGE:
        position.vy = screen_coords->vy - 0x30;
        break;
    case BATTLE_NUMERIC_DISPLAY_HP_HEALING:
    case BATTLE_NUMERIC_DISPLAY_MP_HEALING:
    case BATTLE_NUMERIC_DISPLAY_SPEED_GAIN:
    case BATTLE_NUMERIC_DISPLAY_CT_GAIN:
    case BATTLE_NUMERIC_DISPLAY_BRAVE_GAIN:
    case BATTLE_NUMERIC_DISPLAY_FAITH_GAIN:
    case BATTLE_NUMERIC_DISPLAY_PA_GAIN:
    case BATTLE_NUMERIC_DISPLAY_MA_GAIN:
        position.vy = screen_coords->vy - 0x10;
        break;
    case BATTLE_NUMERIC_DISPLAY_MISSED:
    case BATTLE_NUMERIC_DISPLAY_GUARDED:
    case BATTLE_NUMERIC_DISPLAY_GAINED_EXP:
    case BATTLE_NUMERIC_DISPLAY_GAINED_JP:
        break;
    }
    battle_gfx_construct_polygon_data_for_units(
        display_0, display_0->part_count, 0, &position, 0, 0, &zoom_0, main_gfx_get_otag() + 1);
    battle_gfx_construct_polygon_data_for_units(
        display_1, display_1->part_count, 0, &position, 0, 0, &zoom_1, main_gfx_get_otag() + 1);
    battle_gfx_construct_polygon_data_for_units(
        display_2, display_2->part_count, 0, &position, 0, 0, &zoom_2, main_gfx_get_otag() + 1);
    display->progress += g_main_system_animation_speed;
}
