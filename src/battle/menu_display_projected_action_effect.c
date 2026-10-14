#include "fft/battle_action.h"
#include "fft/battle_menu.h"
#include "fft/battle_status.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Loads the action-preview result image (HP/MP/gil/EXP/level/status/stat
 * change) and sets up the value panel for the current preview action.
 *
 * Returns the display image id, or the highest-order status image when a
 * status change accompanies a damage/stat change. The two early exits that
 * hide the panel return 2: the target jumps past the epilogue's `move v0,s1`
 * with the panel state's 2 still in $v0. An explicit `return 2;` adds a second
 * constant load, so they use a bare `return;`. The caller discards the result.
 */
s32 menu_display_projected_action_effect(
    POLY_FT4* prim, const point16_t* base_load, const RECT* base_screen, image_load_parameters_t* params) {
    menu_number_entry_t* panel;
    u16* label_x;
    const action_display_image_rect_t* rect;
    s32 value;
    s32 image;
    s32 extra;
    s32 show_empty;
    s32 width;
    s32 x;
    u32 exp;
    u16* digits_x;
    u16* extra_label_x;
    action_data_t* action;
    u8 target_id;

    extra = 0;
    show_empty = 0;
    panel = g_battle_menu_projected_value_panel;
    panel[0].x = 0xf;
    g_battle_menu_projected_value_panel[0].kind = 1;
    g_battle_menu_projected_value_panel[0].format = 0x803;
    g_battle_menu_projected_value_panel[0].y = 0;
    digits_x = &panel[1].x;
    *digits_x = 0x32;
    label_x = &g_battle_menu_attack_caster_quad_image_params[0][2].x_screen_offset;
    image = 0;
    g_battle_menu_projected_value_panel[1].kind = 0;
    *label_x = g_battle_menu_attack_caster_quad_image_params[1][2].x_screen_offset;
    extra_label_x = &g_battle_menu_attack_caster_quad_image_params[0][3].x_screen_offset;
    *extra_label_x = g_battle_menu_attack_caster_quad_image_params[1][3].x_screen_offset;
    /* A skipped preview leaves the action null, and its accuracy is read
     * (from kernel RAM) before the null test. */
    action = g_menu_preview_target_action;
    g_menu_selected_unit_status.projected_hit_percent = action->attack_accuracy;
    target_id = g_menu_comparison_battle_id;
    if (target_id == 0xff || action == 0) {
        g_battle_menu_projected_value_panel[0].kind = 2;
        g_battle_menu_projected_value_panel[1].kind = 2;
        return;
    }
    if (action->attack_type == 0) {
        g_battle_menu_projected_value_panel[0].kind = 2;
    }

    if (action->attack_type & ACTION_TYPE_HP_DAMAGE) {
        value = action->hp_damage;
        image = ACTION_DISPLAY_IMAGE_HP;
    } else if (action->attack_type & ACTION_TYPE_HP_HEALING) {
        value = action->hp_healing;
        image = ACTION_DISPLAY_IMAGE_HP;
        g_battle_menu_projected_value_panel[0].format = 0x403;
    } else if (action->attack_type & ACTION_TYPE_MP_DAMAGE) {
        value = action->mp_damage;
        image = ACTION_DISPLAY_IMAGE_MP;
    } else if (action->attack_type & ACTION_TYPE_MP_HEALING) {
        image = ACTION_DISPLAY_IMAGE_MP;
        value = action->mp_healing;
        g_battle_menu_projected_value_panel[0].format = 0x403;
    } else if (action->gil_change != 0 && (action->attack_type & ACTION_TYPE_PSEUDO_STATUS)) {
        value = action->gil_change;
        image = ACTION_DISPLAY_IMAGE_GIL;
        if (value >= 0) {
            g_battle_menu_projected_value_panel[0].format = 0x404;
        } else {
            value = -value;
            g_battle_menu_projected_value_panel[0].format = 0x804;
        }
        panel[0].kind = 0;
        *label_x -= 4;
        panel[0].x -= 10;
        panel[0].y += 4;
        *digits_x += 2;
    } else if ((exp = g_menu_preview_target_action->exp_change) != 0
        && (g_menu_preview_target_action->attack_type & ACTION_TYPE_PSEUDO_STATUS)) {
        s32 number_x;

        image = ACTION_DISPLAY_IMAGE_EXP;
        value = exp;
        if (exp & 0x80) {
            value &= 0x7f;
            panel[0].format = 0x403;
        } else {
            panel[0].format = 0x803;
        }
        number_x = panel[0].x;
        number_x -= 8;
        panel[0].x = number_x;
        *label_x -= 8;
    } else if ((g_menu_preview_target_action->special_effect
                   & (ACTION_SPECIAL_EFFECT_LEVEL_UP | ACTION_SPECIAL_EFFECT_LEVEL_DOWN))
        && (g_menu_preview_target_action->attack_type & ACTION_TYPE_PSEUDO_STATUS)) {
        image = ACTION_DISPLAY_IMAGE_LEVEL;
        value = 1;
        if (g_menu_preview_target_action->special_effect & ACTION_SPECIAL_EFFECT_LEVEL_UP) {
            panel[0].format = 0x401;
        } else {
            panel[0].format = 0x801;
        }
        panel[0].x += 12;
    } else if ((g_menu_preview_target_action->attack_type & ACTION_TYPE_PSEUDO_STATUS)
        && g_menu_preview_target_action->ct_change == ACTION_CT_CHANGE_QUICK) {
        /* The three shifted-label tails are duplicated in the original and
         * merged by cross-jumping; a shared goto tail allocates differently. */
        image = ACTION_DISPLAY_IMAGE_QUICK;
        value = 0;
        panel[0].kind = 2;
        *label_x = *label_x - g_action_display_image_rects[ACTION_DISPLAY_IMAGE_QUICK].width + 8;
    } else if ((g_menu_preview_target_action->attack_type & ACTION_TYPE_PSEUDO_STATUS)
        && (g_menu_preview_target_action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM)) {
        image = ACTION_DISPLAY_IMAGE_STOLEN;
        value = 0;
        panel[0].kind = 2;
        *label_x = *label_x - g_action_display_image_rects[ACTION_DISPLAY_IMAGE_STOLEN].width + 8;
    } else if ((g_menu_preview_target_action->attack_type & ACTION_TYPE_PSEUDO_STATUS)
        && (g_menu_preview_target_action->special_effect & ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT)) {
        image = ACTION_DISPLAY_IMAGE_BROKEN;
        value = 0;
        panel[0].kind = 2;
        *label_x = *label_x - g_action_display_image_rects[ACTION_DISPLAY_IMAGE_BROKEN].width + 8;
    } else if (g_menu_preview_target_action->attack_type & ACTION_TYPE_STATUS_CHANGE) {
        g_thread_call_target = (void (*)(void))main_status_find_action_highest_order_effect;
        image = thread_call_on_main_stack(g_menu_preview_target_action);
        if (image > 0) {
            if (image & MAIN_ACTION_STATUS_RESULT_REMOVAL) {
                extra = 0x1f;
            } else if ((image & (MAIN_ACTION_STATUS_RESULT_REMOVAL | MAIN_ACTION_STATUS_RESULT_SCRIPT_FILTER_EXCLUDED))
                == (MAIN_ACTION_STATUS_RESULT_REMOVAL | MAIN_ACTION_STATUS_RESULT_SCRIPT_FILTER_EXCLUDED)) {
                /* Unreachable: the preceding removal test also catches these flags. */
                extra = 0x1e;
            }
            image &= MAIN_ACTION_STATUS_RESULT_INDEX_MASK;
            image = g_status_display_image_ids[image - 1];
            /* Duplicated store: it keeps the state write in its own block
             * ahead of the label arithmetic, as in the target. */
            if (image == 0) {
                show_empty = 1;
                panel[0].kind = 2;
            } else {
                panel[0].kind = 2;
            }
            width = g_action_display_image_rects[image].width;
            x = *label_x - width;
            value = 0;
            *label_x = x + 8;
            if (extra != 0) {
                *extra_label_x = x - 0x12;
            }
        } else {
            panel[0].kind = 2;
        }
    } else if (g_menu_preview_target_action->attack_type & ACTION_TYPE_PSEUDO_STATUS) {
        image = ACTION_DISPLAY_IMAGE_NONE;
        if (g_menu_preview_target_action->sp_change != 0) {
            image = ACTION_DISPLAY_IMAGE_SPEED;
            value = g_menu_preview_target_action->sp_change;
            *label_x -= 0x26;
        } else if (g_menu_preview_target_action->ct_change != 0) {
            image = ACTION_DISPLAY_IMAGE_CT;
            value = g_menu_preview_target_action->ct_change;
            *label_x -= 0x1e;
        } else if (g_menu_preview_target_action->pa_change != 0) {
            image = ACTION_DISPLAY_IMAGE_PHYSICAL_ATTACK;
            value = g_menu_preview_target_action->pa_change;
            *label_x -= 0x14;
        } else if (g_menu_preview_target_action->ma_change != 0) {
            image = ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK;
            value = g_menu_preview_target_action->ma_change;
            *label_x -= 0x14;
        } else if (g_menu_preview_target_action->brave_change != 0) {
            image = ACTION_DISPLAY_IMAGE_BRAVE;
            value = g_menu_preview_target_action->brave_change;
            *label_x -= 0x24;
        } else if (g_menu_preview_target_action->faith_change != 0) {
            image = ACTION_DISPLAY_IMAGE_FAITH_STAT;
            value = g_menu_preview_target_action->faith_change;
            *label_x -= 0x24;
        }
        if (image >= ACTION_DISPLAY_IMAGE_CT && image <= ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK) {
            panel[0].x += 0x10;
            if (value & ACTION_STAT_CHANGE_INCREASE) {
                panel[0].format = 0x402;
                if (image == ACTION_DISPLAY_IMAGE_CT && value == 0xff) {
                    value = 0x64;
                    panel[0].format = 3;
                    *label_x = *label_x; /* reloads and rewrites x, as the target does */
                    panel[0].x -= 8;
                }
            } else {
                panel[0].format = 0x802;
                if (image == ACTION_DISPLAY_IMAGE_CT && value == 0x7f) {
                    value = 0;
                    panel[0].format = 2;
                    *label_x += 8;
                }
            }
            value &= ACTION_STAT_CHANGE_VALUE_MASK;
        } else {
            panel[0].kind = 2;
            return;
        }
    }

    g_menu_selected_unit_status.projected_display_value = value;
    if (image != ACTION_DISPLAY_IMAGE_NONE && (image != 0 || show_empty != 0)) {
        rect = &g_action_display_image_rects[image];
        params->x_load = rect->u;
        params->y_load = rect->v;
        params->width = rect->width;
        params->height = rect->height;
        gfx_init_image_loading(prim, base_load, base_screen, params);
    }
    prim->clut = 0x7d7c;
    if (image >= ACTION_DISPLAY_IMAGE_HP && image <= ACTION_DISPLAY_IMAGE_EXP) {
        prim->clut = 0x7cbc;
    }
    if (image == ACTION_DISPLAY_IMAGE_PHYSICAL_ATTACK || image == ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK) {
        prim->clut = 0x7d7c;
    }

    if (extra != 0) {
        prim++;
        params++;
        prim->clut = 0x7d7c;
        rect = &g_action_display_image_rects[extra];
        params->x_load = rect->u;
        params->y_load = rect->v;
        params->width = rect->width;
        params->height = rect->height;
        gfx_init_image_loading(prim, base_load, base_screen, params);
        return image;
    }

    if ((g_menu_preview_target_action->attack_type & ACTION_TYPE_STATUS_CHANGE)
        && g_menu_preview_target_action->attack_type != ACTION_TYPE_STATUS_CHANGE) {
        prim++;
        params++;
        g_thread_call_target = (void (*)(void))main_status_find_action_highest_order_effect;
        image = thread_call_on_main_stack(g_menu_preview_target_action);
        if (image > 0) {
            if (image & MAIN_ACTION_STATUS_RESULT_REMOVAL) {
                setRGB0(prim, 0x30, 0x8c, 0xdc);
            } else {
                setRGB0(prim, 0xbe, 0x28, 0xa);
            }
            image &= MAIN_ACTION_STATUS_RESULT_INDEX_MASK;
            image = g_status_display_image_ids[image - 1];
            if (image != ACTION_DISPLAY_IMAGE_NONE && image != ACTION_DISPLAY_IMAGE_INVITE) {
                rect = &g_action_display_image_rects[image];
                params->x_load = rect->u;
                params->y_load = rect->v;
                params->width = rect->width;
                params->height = rect->height;
                gfx_init_image_loading(prim, base_load, base_screen, params);
                value = prim->x1 - prim->x0;
                if (value >= 0x18) {
                    prim->x0 += 0x18 - value;
                    prim->x1 += 0x18 - value;
                    prim->x2 += 0x18 - value;
                    prim->x3 += 0x18 - value;
                    prim->y0 -= 9;
                    prim->y1 -= 9;
                    prim->y2 -= 9;
                    prim->y3 -= 9;
                }
                prim->clut = 0x7d7c;
            }
        }
    }
    return image;
}
