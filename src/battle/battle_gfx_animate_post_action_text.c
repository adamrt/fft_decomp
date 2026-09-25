#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/* The -31 colour deltas are passed as sign-extended words; the u16 prototype
 * would zero-extend them, so this caller uses an all-s32 view.
 * The definition itself only matches with u16 colour parameters (s16 or s32
 * ones change its code), so the two views stay. */
#define MODIFY_PALETTE_WORDS ((void (*)(s32, s32, s32, s32, s32, s32, s32, s32))battle_map_modify_palette)

/* Per-frame zoom curves for the three numeric display digits, indexed by
 * display progress (0x80067bac..0x80067cb3, six 0x2c-byte records). */
typedef struct battle_gfx_numeric_scale_table {
    s16 value[21];
} battle_gfx_numeric_scale_table_t;

extern battle_gfx_numeric_scale_table_t g_battle_numeric_digit_0_zoom_curve;
extern battle_gfx_numeric_scale_table_t g_battle_numeric_digit_1_zoom_curve;
extern battle_gfx_numeric_scale_table_t g_battle_numeric_digit_2_zoom_curve;
extern battle_gfx_numeric_scale_table_t g_battle_numeric_digit_0_critical_zoom_curve;
extern battle_gfx_numeric_scale_table_t g_battle_numeric_digit_1_critical_zoom_curve;
extern battle_gfx_numeric_scale_table_t g_battle_numeric_digit_2_critical_zoom_curve;

/* Animate and queue the three numeric post-action display sprites.
 *
 * Scales each digit sprite by its per-frame zoom curve, starts the next
 * queued result once the animation passes frame 50, and retires the display
 * after frame 61.
 *
 * The zoom curves are copied from extern records rather than written as local
 * initializers: the switch table that follows them must stay 8-byte aligned
 * relative to the TU's original .rdata base (0x80067cb8), which a standalone
 * .rodata placement at 0x80067bac cannot reproduce. */
void battle_gfx_animate_post_action_text(battle_unit_misc_data_t* unit, const u16* screen_coords) {
    SVECTOR position;
    SVECTOR zoom_0 = g_battle_status_bubble_zoom;
    SVECTOR zoom_1 = g_battle_status_bubble_zoom;
    SVECTOR zoom_2 = g_battle_status_bubble_zoom;
    battle_gfx_numeric_scale_table_t scale_0 = g_battle_numeric_digit_0_zoom_curve;
    battle_gfx_numeric_scale_table_t scale_1 = g_battle_numeric_digit_1_zoom_curve;
    battle_gfx_numeric_scale_table_t scale_2 = g_battle_numeric_digit_2_zoom_curve;
    battle_gfx_numeric_scale_table_t critical_scale_0 = g_battle_numeric_digit_0_critical_zoom_curve;
    battle_gfx_numeric_scale_table_t critical_scale_1 = g_battle_numeric_digit_1_critical_zoom_curve;
    battle_gfx_numeric_scale_table_t critical_scale_2 = g_battle_numeric_digit_2_critical_zoom_curve;
    battle_gfx_sprite_display_data_t* display_0;
    battle_gfx_sprite_display_data_t* display_1;
    battle_gfx_sprite_display_data_t* display_2;
    battle_action_result_display_t* display;
    u16 progress;

    display_0 = unit->numeric_displays[0];
    display_1 = unit->numeric_displays[1];
    display_2 = unit->numeric_displays[2];
    progress = unit->numeric_display_progress + g_animation_speed;
    display = (battle_action_result_display_t*)&unit->numeric_display_active;
    if (progress < 21) {
        if (unit->numeric_display_selector & BATTLE_NUMERIC_DISPLAY_MODIFIER_CRITICAL) {
            main_util_set_svector(&zoom_0, critical_scale_0.value[progress], critical_scale_0.value[progress],
                critical_scale_0.value[progress]);
            main_util_set_svector(&zoom_1, critical_scale_1.value[progress], critical_scale_1.value[progress],
                critical_scale_1.value[progress]);
            main_util_set_svector(&zoom_2, critical_scale_2.value[progress], critical_scale_2.value[progress],
                critical_scale_2.value[progress]);
        } else {
            switch (unit->numeric_display_selector & BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK) {
            case 0 ... 15:
                main_util_set_svector(
                    &zoom_0, scale_0.value[progress], scale_0.value[progress], scale_0.value[progress]);
                main_util_set_svector(
                    &zoom_1, scale_1.value[progress], scale_1.value[progress], scale_1.value[progress]);
                main_util_set_svector(
                    &zoom_2, scale_2.value[progress], scale_2.value[progress], scale_2.value[progress]);
                break;
            case BATTLE_NUMERIC_DISPLAY_MISSED:
            case BATTLE_NUMERIC_DISPLAY_GUARDED:
            case BATTLE_NUMERIC_DISPLAY_GAINED_EXP:
            case BATTLE_NUMERIC_DISPLAY_GAINED_JP:
                main_util_set_svector(
                    &zoom_0, scale_1.value[progress], scale_1.value[progress], scale_1.value[progress]);
                main_util_set_svector(
                    &zoom_1, scale_1.value[progress], scale_1.value[progress], scale_1.value[progress]);
                main_util_set_svector(
                    &zoom_2, scale_1.value[progress], scale_1.value[progress], scale_1.value[progress]);
                break;
            }
        }
    } else if (progress >= 50 && (u16)unit->numeric_display_progress < 50) {
        if (battle_gfx_build_next_action_result_display(unit) != 0) {
            return;
        }
        if (battle_gfx_build_next_special_action_result_display(unit) != 0) {
            return;
        }
        unit->numeric_displays[0]->spritesheet_id = unit->numeric_displays[1]->spritesheet_id
            = unit->numeric_displays[2]->spritesheet_id = 0x3f;
        MODIFY_PALETTE_WORDS(4, 2, 7, unit->unit_id, 0, -31, -31, -31);
        battle_gfx_set_sprite_part_flag_bit_0(display_0, 0, 1);
        battle_gfx_set_sprite_part_flag_bit_0(display_1, 0, 1);
        battle_gfx_set_sprite_part_flag_bit_0(display_2, 0, 1);
    } else if (progress >= 61) {
        if (battle_gfx_build_next_action_result_display(unit) != 0) {
            return;
        }
        if (battle_gfx_build_next_special_action_result_display(unit) != 0) {
            return;
        }
        unit->numeric_display_active = 0;
    }
    position.vx = screen_coords[0] + 0x15;
    position.vy = screen_coords[1] - 8;
    switch (display->selector & BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK) {
    case 0:
    case 2:
    case 4:
    case 6:
    case 8:
    case 10:
    case 12:
    case 14:
        position.vy = screen_coords[1] - 0x30;
        break;
    case 1:
    case 3:
    case 5:
    case 7:
    case 9:
    case 11:
    case 13:
    case 15:
        position.vy = screen_coords[1] - 0x10;
        break;
    case BATTLE_NUMERIC_DISPLAY_MISSED:
    case BATTLE_NUMERIC_DISPLAY_GUARDED:
    case BATTLE_NUMERIC_DISPLAY_GAINED_EXP:
    case BATTLE_NUMERIC_DISPLAY_GAINED_JP:
        break;
    }
    battle_gfx_construct_polygon_data_for_units((battle_gfx_sprite_display_t*)display_0, display_0->part_count, 0,
        (s16*)&position, 0, 0, (s16*)&zoom_0, main_gfx_get_otag() + 1);
    battle_gfx_construct_polygon_data_for_units((battle_gfx_sprite_display_t*)display_1, display_1->part_count, 0,
        (s16*)&position, 0, 0, (s16*)&zoom_1, main_gfx_get_otag() + 1);
    battle_gfx_construct_polygon_data_for_units((battle_gfx_sprite_display_t*)display_2, display_2->part_count, 0,
        (s16*)&position, 0, 0, (s16*)&zoom_2, main_gfx_get_otag() + 1);
    display->progress += g_animation_speed;
}
