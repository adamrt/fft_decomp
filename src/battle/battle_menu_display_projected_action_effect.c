#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_menu_window.h"
#include "fft/main_unit.h"
#include "fft/menu_types.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Provisional layout of the preview value panel at 0x801687a0. */
typedef struct battle_menu_preview_value_panel {
    s16 x;                /* 0x00 */
    s16 y;                /* 0x02 */
    u8 unknown_04[4];     /* 0x04 */
    s16 format;           /* 0x08; digit count with 0x400 (plus) / 0x800 (minus) sign flags */
    s16 state;            /* 0x0a */
    u16 digits_x;         /* 0x0c */
    u8 unknown_0e[8];     /* 0x0e */
    s16 hit_percent_kind; /* 0x16; second number entry's kind: 0 draws, 2 hides */
} battle_menu_preview_value_panel_t;

/* Provisional 0x3c-byte label entries at 0x80168474; entry 1 holds the defaults. */
typedef struct battle_menu_preview_label_panel {
    u16 x;               /* 0x00 */
    u8 unknown_02[0xa];  /* 0x02 */
    u16 unknown_0c;      /* 0x0c */
    u8 unknown_0e[0x2e]; /* 0x0e */
} battle_menu_preview_label_panel_t;

extern battle_menu_preview_value_panel_t g_battle_menu_projected_value_panel;
extern battle_menu_preview_label_panel_t g_battle_menu_projected_label_panels[2];

/*
 * Loads the action-preview result image (HP/MP/gil/EXP/level/status/stat
 * change) and sets up the value panel for the current preview action.
 *
 * Returns the display image id, or the highest-order status image when a
 * status change accompanies a damage/stat change. The early exits use a bare
 * `return;`: the target branches to the shared `move v0,s1` epilogue, which
 * `return image;` would fold to a constant 0.
 */
s32 battle_menu_display_projected_action_effect(POLY_FT4* prim, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, world_gfx_image_load_parameters_t* params) {
    battle_menu_preview_value_panel_t* panel;
    battle_menu_preview_label_panel_t* label;
    const battle_action_display_image_rect_t* rect;
    s32 value;
    s32 image;
    s32 extra;
    s32 show_empty;
    s32 width;
    s32 x;
    u32 exp;
    u16* digits_x;
    u16* label_0c;
    battle_action_data_t* action;

    extra = 0;
    show_empty = 0;
    panel = &g_battle_menu_projected_value_panel;
    panel->x = 0xf;
    g_battle_menu_projected_value_panel.state = 1;
    g_battle_menu_projected_value_panel.format = 0x803;
    g_battle_menu_projected_value_panel.y = 0;
    digits_x = &panel->digits_x;
    *digits_x = 0x32;
    label = &g_battle_menu_projected_label_panels[0];
    image = 0;
    g_battle_menu_projected_value_panel.hit_percent_kind = 0;
    label->x = g_battle_menu_projected_label_panels[1].x;
    label_0c = &label->unknown_0c;
    *label_0c = g_battle_menu_projected_label_panels[1].unknown_0c;
    action = g_battle_menu_preview_target_action;
    g_battle_menu_active_turn_banner.projected_hit_percent = action->attack_accuracy;
    if ((u8)g_battle_preview_target_unit_id == 0xff || action == 0) {
        g_battle_menu_projected_value_panel.state = 2;
        g_battle_menu_projected_value_panel.hit_percent_kind = 2;
        return;
    }
    if (action->attack_type == 0) {
        g_battle_menu_projected_value_panel.state = 2;
    }

    if (action->attack_type & BATTLE_ACTION_TYPE_HP_DAMAGE) {
        value = (s16)action->hp_damage;
        image = BATTLE_ACTION_DISPLAY_IMAGE_HP;
    } else if (action->attack_type & BATTLE_ACTION_TYPE_HP_HEALING) {
        value = (s16)action->hp_healing;
        image = BATTLE_ACTION_DISPLAY_IMAGE_HP;
        g_battle_menu_projected_value_panel.format = 0x403;
    } else if (action->attack_type & BATTLE_ACTION_TYPE_MP_DAMAGE) {
        value = (s16)action->mp_damage;
        image = BATTLE_ACTION_DISPLAY_IMAGE_MP;
    } else if (action->attack_type & BATTLE_ACTION_TYPE_MP_HEALING) {
        image = BATTLE_ACTION_DISPLAY_IMAGE_MP;
        value = (s16)action->mp_healing;
        g_battle_menu_projected_value_panel.format = 0x403;
    } else if (action->gil_change != 0 && (action->attack_type & BATTLE_ACTION_TYPE_PSEUDO_STATUS)) {
        value = action->gil_change;
        image = BATTLE_ACTION_DISPLAY_IMAGE_GIL;
        if (value >= 0) {
            g_battle_menu_projected_value_panel.format = 0x404;
        } else {
            value = -value;
            g_battle_menu_projected_value_panel.format = 0x804;
        }
        panel->state = 0;
        label->x -= 4;
        panel->x -= 10;
        panel->y += 4;
        *digits_x += 2;
    } else if ((exp = g_battle_menu_preview_target_action->exp_change) != 0
        && (g_battle_menu_preview_target_action->attack_type & BATTLE_ACTION_TYPE_PSEUDO_STATUS)) {
        image = BATTLE_ACTION_DISPLAY_IMAGE_EXP;
        value = exp;
        if (exp & 0x80) {
            value &= 0x7f;
            panel->format = 0x403;
        } else {
            panel->format = 0x803;
        }
        panel->x -= 8;
        label->x -= 8;
    } else if ((g_battle_menu_preview_target_action->special_effect
                   & (BATTLE_ACTION_SPECIAL_EFFECT_LEVEL_UP | BATTLE_ACTION_SPECIAL_EFFECT_LEVEL_DOWN))
        && (g_battle_menu_preview_target_action->attack_type & BATTLE_ACTION_TYPE_PSEUDO_STATUS)) {
        image = BATTLE_ACTION_DISPLAY_IMAGE_LEVEL;
        value = 1;
        if (g_battle_menu_preview_target_action->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_LEVEL_UP) {
            panel->format = 0x401;
        } else {
            panel->format = 0x801;
        }
        panel->x += 12;
    } else if ((g_battle_menu_preview_target_action->attack_type & BATTLE_ACTION_TYPE_PSEUDO_STATUS)
        && g_battle_menu_preview_target_action->ct_change == 0xff) {
        /* The three shifted-label tails are duplicated in the original and
         * merged by cross-jumping; a shared goto tail allocates differently. */
        image = BATTLE_ACTION_DISPLAY_IMAGE_QUICK;
        value = 0;
        panel->state = 2;
        label->x = label->x - g_battle_action_display_image_rects[BATTLE_ACTION_DISPLAY_IMAGE_QUICK].width + 8;
    } else if ((g_battle_menu_preview_target_action->attack_type & BATTLE_ACTION_TYPE_PSEUDO_STATUS)
        && (g_battle_menu_preview_target_action->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_STEAL_ITEM)) {
        image = BATTLE_ACTION_DISPLAY_IMAGE_STOLEN;
        value = 0;
        panel->state = 2;
        label->x = label->x - g_battle_action_display_image_rects[BATTLE_ACTION_DISPLAY_IMAGE_STOLEN].width + 8;
    } else if ((g_battle_menu_preview_target_action->attack_type & BATTLE_ACTION_TYPE_PSEUDO_STATUS)
        && (g_battle_menu_preview_target_action->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT)) {
        image = BATTLE_ACTION_DISPLAY_IMAGE_BROKEN;
        value = 0;
        panel->state = 2;
        label->x = label->x - g_battle_action_display_image_rects[BATTLE_ACTION_DISPLAY_IMAGE_BROKEN].width + 8;
    } else if (g_battle_menu_preview_target_action->attack_type & BATTLE_ACTION_TYPE_STATUS_CHANGE) {
        g_battle_thread_call_target = (void (*)(void))main_status_find_action_highest_order_effect;
        image = battle_thread_call_on_main_stack(g_battle_menu_preview_target_action);
        if (image > 0) {
            if (image & 0x80) {
                extra = 0x1f;
            } else if ((image & 0x180) == 0x180) {
                extra = 0x1e;
            }
            image &= 0x7f;
            image = g_battle_status_display_image_ids[image - 1];
            /* Duplicated store: it keeps the state write in its own block
             * ahead of the label arithmetic, as in the target. */
            if (image == 0) {
                show_empty = 1;
                panel->state = 2;
            } else {
                panel->state = 2;
            }
            width = g_battle_action_display_image_rects[image].width;
            x = label->x - width;
            value = 0;
            label->x = x + 8;
            if (extra != 0) {
                *label_0c = x - 0x12;
            }
        } else {
            panel->state = 2;
        }
    } else if (g_battle_menu_preview_target_action->attack_type & BATTLE_ACTION_TYPE_PSEUDO_STATUS) {
        image = BATTLE_ACTION_DISPLAY_IMAGE_NONE;
        if (g_battle_menu_preview_target_action->sp_change != 0) {
            image = BATTLE_ACTION_DISPLAY_IMAGE_SPEED;
            value = g_battle_menu_preview_target_action->sp_change;
            label->x -= 0x26;
        } else if (g_battle_menu_preview_target_action->ct_change != 0) {
            image = BATTLE_ACTION_DISPLAY_IMAGE_CT;
            value = g_battle_menu_preview_target_action->ct_change;
            label->x -= 0x1e;
        } else if (g_battle_menu_preview_target_action->pa_change != 0) {
            image = BATTLE_ACTION_DISPLAY_IMAGE_PHYSICAL_ATTACK;
            value = g_battle_menu_preview_target_action->pa_change;
            label->x -= 0x14;
        } else if (g_battle_menu_preview_target_action->ma_change != 0) {
            image = BATTLE_ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK;
            value = g_battle_menu_preview_target_action->ma_change;
            label->x -= 0x14;
        } else if (g_battle_menu_preview_target_action->brave_change != 0) {
            image = BATTLE_ACTION_DISPLAY_IMAGE_BRAVE;
            value = g_battle_menu_preview_target_action->brave_change;
            label->x -= 0x24;
        } else if (g_battle_menu_preview_target_action->faith_change != 0) {
            image = BATTLE_ACTION_DISPLAY_IMAGE_FAITH_STAT;
            value = g_battle_menu_preview_target_action->faith_change;
            label->x -= 0x24;
        }
        if (image >= BATTLE_ACTION_DISPLAY_IMAGE_CT && image <= BATTLE_ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK) {
            panel->x += 0x10;
            if (value & BATTLE_ACTION_STAT_CHANGE_INCREASE) {
                panel->format = 0x402;
                if (image == BATTLE_ACTION_DISPLAY_IMAGE_CT && value == 0xff) {
                    value = 0x64;
                    panel->format = 3;
                    label->x = label->x; /* reloads and rewrites x, as the target does */
                    panel->x -= 8;
                }
            } else {
                panel->format = 0x802;
                if (image == BATTLE_ACTION_DISPLAY_IMAGE_CT && value == 0x7f) {
                    value = 0;
                    panel->format = 2;
                    label->x += 8;
                }
            }
            value &= BATTLE_ACTION_STAT_CHANGE_VALUE_MASK;
        } else {
            panel->state = 2;
            return;
        }
    }

    g_battle_menu_active_turn_banner.projected_display_value = value;
    if (image != BATTLE_ACTION_DISPLAY_IMAGE_NONE && (image != 0 || show_empty != 0)) {
        rect = &g_battle_action_display_image_rects[image];
        params->x_load = rect->u;
        params->y_load = rect->v;
        params->width = rect->width;
        params->height = rect->height;
        battle_gfx_init_image_loading(prim, base_load, base_screen, params);
    }
    prim->clut = 0x7d7c;
    if (image >= BATTLE_ACTION_DISPLAY_IMAGE_HP && image <= BATTLE_ACTION_DISPLAY_IMAGE_EXP) {
        prim->clut = 0x7cbc;
    }
    if (image == BATTLE_ACTION_DISPLAY_IMAGE_PHYSICAL_ATTACK || image == BATTLE_ACTION_DISPLAY_IMAGE_MAGICAL_ATTACK) {
        prim->clut = 0x7d7c;
    }

    if (extra != 0) {
        prim++;
        params++;
        prim->clut = 0x7d7c;
        rect = &g_battle_action_display_image_rects[extra];
        params->x_load = rect->u;
        params->y_load = rect->v;
        params->width = rect->width;
        params->height = rect->height;
        battle_gfx_init_image_loading(prim, base_load, base_screen, params);
        return image;
    }

    if ((g_battle_menu_preview_target_action->attack_type & BATTLE_ACTION_TYPE_STATUS_CHANGE)
        && g_battle_menu_preview_target_action->attack_type != BATTLE_ACTION_TYPE_STATUS_CHANGE) {
        prim++;
        params++;
        g_battle_thread_call_target = (void (*)(void))main_status_find_action_highest_order_effect;
        image = battle_thread_call_on_main_stack(g_battle_menu_preview_target_action);
        if (image > 0) {
            if (image & 0x80) {
                prim->r0 = 0x30;
                prim->g0 = 0x8c;
                prim->b0 = 0xdc;
            } else {
                prim->r0 = 0xbe;
                prim->g0 = 0x28;
                prim->b0 = 0xa;
            }
            image &= 0x7f;
            image = g_battle_status_display_image_ids[image - 1];
            if (image != BATTLE_ACTION_DISPLAY_IMAGE_NONE && image != BATTLE_ACTION_DISPLAY_IMAGE_INVITE) {
                rect = &g_battle_action_display_image_rects[image];
                params->x_load = rect->u;
                params->y_load = rect->v;
                params->width = rect->width;
                params->height = rect->height;
                battle_gfx_init_image_loading(prim, base_load, base_screen, params);
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
