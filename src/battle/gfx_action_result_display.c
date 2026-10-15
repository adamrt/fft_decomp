/* Gfx action result display. Its start is a cut chosen by topic; its end is a cut chosen by topic. */

#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"

/* Builds the next ordinary damage, healing, stat, status, or result display.
 *
 * EXP/JP and the three failure messages above bit 0x01000000 are handled by
 * the following dispatcher at 0x800808b8. */
s32 battle_gfx_build_next_action_result_display(battle_unit_t* unit) {
    unit_stats_t* stats;
    u32 scan_flags;
    s32 flags;
    action_result_display_t* display;
    battle_gfx_sprite_display_data_t* display_0;
    battle_gfx_sprite_display_data_t* display_1;
    battle_gfx_sprite_display_data_t* display_2;
    const action_display_image_rect_t* image;
    s32 flag;
    s32 index;
    s32 digits[3];
    u8 red;
    u8 green;
    u8 blue;
    s16 y_shift;
    s32 result;
    u8 change;
    u8 status;

    flags = unit->action_display_flags;
    stats = unit->stats;
    display = &unit->numeric_display;

    if ((flags & ACTION_DISPLAY_NUMERIC_FLAGS_MASK) != 0) {
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_HP_DAMAGE] = stats->action.hp_damage;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_HP_HEALING] = stats->action.hp_healing;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_MP_DAMAGE] = stats->action.mp_damage;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_MP_HEALING] = stats->action.mp_healing;
        change = stats->action.sp_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_SPEED_GAIN] = change;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_SPEED_DAMAGE] = change;
        change = stats->action.ct_change & 0x7f;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_CT_GAIN] = change;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_CT_DAMAGE] = change;
        change = stats->action.brave_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_BRAVE_GAIN] = change;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_BRAVE_DAMAGE] = change;
        change = stats->action.faith_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_FAITH_GAIN] = change;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_FAITH_DAMAGE] = change;
        flag = 1;
        change = stats->action.pa_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_PA_GAIN] = change;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_PA_DAMAGE] = change;
        index = 0;
        change = stats->action.ma_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_MA_GAIN] = change;
        g_action_display_values[BATTLE_NUMERIC_DISPLAY_MA_DAMAGE] = change;

        scan_flags = unit->action_display_flags;
        while (index < ACTION_DISPLAY_VALUE_COUNT) {
            if ((flag & scan_flags) != 0) {
                flags = index;
                display->value = g_action_display_values[flags];
                display->selector = g_action_display_numeric_selectors[flags];
                index = unit->action_display_flags;
                unit->action_display_flags = ~flag & index;
                break;
            }
            index++;
            flag <<= 1;
        }
    } else if ((flags & ACTION_DISPLAY_FLAG_LEVEL_DOWN) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_LEVEL_DOWN;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_LEVEL_DOWN;
    } else if ((flags & ACTION_DISPLAY_FLAG_LEVEL_UP) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_LEVEL_UP;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_LEVEL_UP;
    } else if ((flags & ACTION_DISPLAY_FLAG_STOLEN) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_STOLEN;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_STOLEN;
    } else if ((flags & ACTION_DISPLAY_FLAG_BROKEN) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_BROKEN;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_BROKEN;
    } else if ((flags & ACTION_DISPLAY_FLAG_QUICK) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_QUICK;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_QUICK;
    } else if ((flags & ACTION_DISPLAY_FLAG_CT_ZERO) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_CT_ZERO;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_CT_ZERO;
    } else if ((flags & ACTION_DISPLAY_FLAG_MISSED) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_MISSED;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_MISSED;
    } else if ((flags & ACTION_DISPLAY_FLAG_GUARDED) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_GUARDED;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_GUARDED;
    } else if ((flags & ACTION_DISPLAY_FLAG_CAUGHT) != 0) {
        u32 branch_flags;
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_CAUGHT;
        branch_flags = unit->action_display_flags;
        unit->action_display_flags = branch_flags & ~ACTION_DISPLAY_FLAG_CAUGHT;
    } else if (unit->status_display_image_count != 0) {
        unit->status_display_image_count--;
        status = unit->status_display_image_list[unit->status_display_image_count];
        if ((status & ACTION_STATUS_DISPLAY_REMOVAL) != 0) {
            display->selector = BATTLE_NUMERIC_DISPLAY_STATUS_REMOVED;
        } else {
            display->selector = BATTLE_NUMERIC_DISPLAY_STATUS_INFLICTED;
        }
        status &= ACTION_STATUS_DISPLAY_IMAGE_MASK;
    } else {
        return 0;
    }

    if (unit->stats->action.critical != 0) {
        display->selector |= BATTLE_NUMERIC_DISPLAY_MODIFIER_CRITICAL;
    } else {
        display->selector &= ~BATTLE_NUMERIC_DISPLAY_MODIFIER_CRITICAL;
    }
    display->active = 1;
    if (display->progress >= 22) {
        display->progress = 21;
    }

    /* Every message produced above selects a color case.  The target has no
     * default initialization for these three registers. */
    switch (display->selector & BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK) {
    case BATTLE_NUMERIC_DISPLAY_HP_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_SPEED_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_CT_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_BRAVE_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_FAITH_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_PA_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_MA_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_STATUS_INFLICTED:
    case BATTLE_NUMERIC_DISPLAY_LEVEL_DOWN:
    case BATTLE_NUMERIC_DISPLAY_STOLEN:
    case BATTLE_NUMERIC_DISPLAY_BROKEN:
    case BATTLE_NUMERIC_DISPLAY_QUICK:
    case BATTLE_NUMERIC_DISPLAY_CT_ZERO:
        blue = 128;
        green = 128;
        red = 128;
        break;
    case BATTLE_NUMERIC_DISPLAY_MP_DAMAGE:
        red = 120;
        green = 60;
        blue = 220;
        break;
    case BATTLE_NUMERIC_DISPLAY_HP_HEALING:
    case BATTLE_NUMERIC_DISPLAY_SPEED_GAIN:
    case BATTLE_NUMERIC_DISPLAY_CT_GAIN:
    case BATTLE_NUMERIC_DISPLAY_BRAVE_GAIN:
    case BATTLE_NUMERIC_DISPLAY_FAITH_GAIN:
    case BATTLE_NUMERIC_DISPLAY_PA_GAIN:
    case BATTLE_NUMERIC_DISPLAY_MA_GAIN:
    case BATTLE_NUMERIC_DISPLAY_STATUS_REMOVED:
    case BATTLE_NUMERIC_DISPLAY_LEVEL_UP:
        red = 40;
        green = 140;
        blue = 220;
        break;
    case BATTLE_NUMERIC_DISPLAY_MP_HEALING:
        red = 78;
        green = 140;
        blue = 30;
        break;
    case BATTLE_NUMERIC_DISPLAY_MISSED:
    case BATTLE_NUMERIC_DISPLAY_GUARDED:
    case BATTLE_NUMERIC_DISPLAY_CAUGHT:
        red = 190;
        green = 40;
        blue = 10;
        break;
    }

    display->displays[0]->color.r = display->displays[1]->color.r = display->displays[2]->color.r = red;
    display->displays[0]->color.g = display->displays[1]->color.g = display->displays[2]->color.g = green;
    display->displays[0]->color.b = display->displays[1]->color.b = display->displays[2]->color.b = blue;

    if (display->value / 1000) {
        digits[2] = 9;
        digits[1] = 9;
        digits[0] = 9;
    } else {
        digits[0] = display->value % 10;
        digits[1] = (display->value / 10) % 10;
        digits[2] = display->value / 100;
    }

    display_0 = display->displays[0];
    display_1 = display->displays[1];
    display_2 = display->displays[2];

    battle_gfx_store_sprite_display_data(display_0, 0, 0, -32, 0, 0, 0, 0, 0);
    battle_gfx_store_sprite_display_data(display_1, 0, 0, -32, 0, 0, 0, 0, 0);

    switch (display->selector & BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK) {
    case BATTLE_NUMERIC_DISPLAY_HP_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_HP_HEALING:
    case BATTLE_NUMERIC_DISPLAY_MP_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_MP_HEALING:
        y_shift = -32;
        if ((display->selector & 1) == 0) {
            y_shift = 16;
        }
        battle_gfx_store_sprite_display_data(display_1, 0, -14, y_shift, 0, 0, 0, 0, 0);
        battle_gfx_store_sprite_display_data(display_2, 0, -21, y_shift, 0, 0, 0, 0, 0);
        if ((digits[2] | digits[1]) == 0) {
            battle_gfx_store_sprite_display_data(display_0, 0, -21, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else if (digits[2] == 0) {
            battle_gfx_store_sprite_display_data(display_0, 0, -14, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            battle_gfx_store_sprite_display_data(display_1, 0, -21, y_shift, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        } else {
            battle_gfx_store_sprite_display_data(display_0, 0, -7, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            battle_gfx_store_sprite_display_data(display_1, 0, -14, y_shift, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
            battle_gfx_store_sprite_display_data(display_2, 0, -21, y_shift, 0xa8 + digits[2] * 8, 0x30, 8, 16, 0);
        }
        break;

    case BATTLE_NUMERIC_DISPLAY_SPEED_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_SPEED_GAIN:
    case BATTLE_NUMERIC_DISPLAY_CT_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_CT_GAIN:
    case BATTLE_NUMERIC_DISPLAY_BRAVE_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_BRAVE_GAIN:
    case BATTLE_NUMERIC_DISPLAY_FAITH_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_FAITH_GAIN:
    case BATTLE_NUMERIC_DISPLAY_PA_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_PA_GAIN:
    case BATTLE_NUMERIC_DISPLAY_MA_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_MA_GAIN:
        y_shift = -32;
        if ((display->selector & 1) == 0) {
            y_shift = 16;
        }
        if (digits[1] == 0) {
            battle_gfx_store_sprite_display_data(display_0, 0, -14, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else {
            battle_gfx_store_sprite_display_data(display_0, 0, -14, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            battle_gfx_store_sprite_display_data(display_1, 0, -21, y_shift, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        }

        switch (display->selector & 0x0e) {
        case BATTLE_NUMERIC_DISPLAY_SPEED_DAMAGE:
            battle_gfx_store_sprite_display_data(display_2, 0, -7, y_shift, 0x9c, 0xc2, 0x18, 0x0a, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_CT_DAMAGE:
            battle_gfx_store_sprite_display_data(display_2, 0, -7, y_shift, 0xc2, 0xc2, 0x0e, 0x0a, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_BRAVE_DAMAGE:
            battle_gfx_store_sprite_display_data(display_2, 0, -7, y_shift, 0xa8, 0xb8, 0x0b, 0x0a, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_FAITH_DAMAGE:
            battle_gfx_store_sprite_display_data(display_2, 0, -7, y_shift, 0x9c, 0xb8, 0x0c, 0x0a, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_PA_DAMAGE:
            battle_gfx_store_sprite_display_data(display_2, 0, -7, y_shift, 0x40, 0, 0x0c, 0x0d, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_MA_DAMAGE:
            battle_gfx_store_sprite_display_data(display_2, 0, -7, y_shift, 0x40, 0x0d, 0x0b, 0x0a, 0);
            break;
        }
        break;

    case BATTLE_NUMERIC_DISPLAY_MISSED:
        battle_gfx_store_sprite_display_data(display_2, 0, -21, -32, 0xbe, 0x48, 0x1b, 8, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_GUARDED:
        battle_gfx_store_sprite_display_data(display_2, 0, -21, -32, 0xb8, 0x60, 0x1e, 8, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_CAUGHT:
        battle_gfx_store_sprite_display_data(display_2, 0, -21, -32, 0x30, 0xe6, 0x1a, 0x0a, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_STATUS_INFLICTED:
    case BATTLE_NUMERIC_DISPLAY_STATUS_REMOVED:
        result = status;
        battle_gfx_store_sprite_display_data(display_2, 0, -21, -32, g_action_display_image_rects[result].u,
            g_action_display_image_rects[result].v, g_action_display_image_rects[result].width,
            (g_action_display_image_rects + result)->height, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_LEVEL_DOWN:
        battle_gfx_store_sprite_display_data(display_1, 0, -7, 16, 0xb0, 0x30, 8, 16, 0);
        battle_gfx_store_sprite_display_data(display_2, 0, 0, 16, 0xb4, 0xc1, 0x0e, 0x0a, 0);
        /* fall through */
    case BATTLE_NUMERIC_DISPLAY_LEVEL_UP:
        battle_gfx_store_sprite_display_data(display_1, 0, -7, -32, 0xb0, 0x30, 8, 16, 0);
        battle_gfx_store_sprite_display_data(display_2, 0, 0, -32, 0xb4, 0xc1, 0x0e, 0x0a, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_STOLEN:
        image = &g_action_display_image_rects[ACTION_DISPLAY_IMAGE_STOLEN];
        battle_gfx_store_sprite_display_data(
            display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_BROKEN:
        image = &g_action_display_image_rects[ACTION_DISPLAY_IMAGE_BROKEN];
        battle_gfx_store_sprite_display_data(
            display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_QUICK:
        image = &g_action_display_image_rects[ACTION_DISPLAY_IMAGE_QUICK];
        battle_gfx_store_sprite_display_data(
            display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_CT_ZERO:
        battle_gfx_store_sprite_display_data(display_1, 0, -16, y_shift, 0xc2, 0xc2, 0x0e, 8, 0);
        battle_gfx_store_sprite_display_data(display_2, 0, 0, y_shift, 0xa8, 0x34, 8, 8, 0);
        break;
    }

    map_load_palette_data(&g_action_result_palette, MAP_CLUT_ROW_ACTION_RESULTS, unit->runtime_id, 0);
    return 1;
}

/* Queues the post-action displays for a unit's resolved action.
 *
 * A hit sets one action_display_flags bit per HP/MP, stat, level, steal and
 * break result and lists the inflicted/removed status images (removals
 * marked 0x80); a miss queues MISSED, GUARDED or CAUGHT by miss type. The
 * first display is built immediately unless a numeric display is active. */
void battle_gfx_prepare_post_action_display(battle_unit_t* unit) {
    unit_stats_t* stats;
    action_data_t* action;
    s32 i;
    s32 j;
    s32 bit;
    u8 count;
    u8 image;
    u8 miss;

    unit->numeric_display.displays[0]->tpage = unit->numeric_display.displays[1]->tpage
        = unit->numeric_display.displays[2]->tpage = BATTLE_GFX_TPAGE_FRAME;
    stats = unit->stats;
    action = &stats->action;
    if (action->hit != 0) {
        if (action->attack_type & ACTION_TYPE_HP_DAMAGE) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_HP_DAMAGE;
        }
        if (action->attack_type & ACTION_TYPE_HP_HEALING) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_HP_HEALING;
        }
        if (action->attack_type & ACTION_TYPE_MP_DAMAGE) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_MP_DAMAGE;
        }
        if (action->attack_type & ACTION_TYPE_MP_HEALING) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_MP_HEALING;
        }
        if (action->attack_type & ACTION_TYPE_PSEUDO_STATUS) {
            if (action->sp_change != 0) {
                if (action->sp_change & 0x80) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_SPEED_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_SPEED_DAMAGE;
                }
            }
            if (action->ct_change != 0) {
                if (action->ct_change == ACTION_CT_CHANGE_QUICK) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_QUICK;
                } else if (action->ct_change == ACTION_CT_CHANGE_ZERO) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_CT_ZERO;
                } else if (action->ct_change & 0x80) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_CT_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_CT_DAMAGE;
                }
            }
            if (action->pa_change != 0) {
                if (action->pa_change & 0x80) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_PA_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_PA_DAMAGE;
                }
            }
            if (action->ma_change != 0) {
                if (action->ma_change & 0x80) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_MA_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_MA_DAMAGE;
                }
            }
            if (action->brave_change != 0) {
                if (action->brave_change & 0x80) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_BRAVE_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_BRAVE_DAMAGE;
                }
            }
            if (action->faith_change != 0) {
                if (action->faith_change & 0x80) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_FAITH_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_FAITH_DAMAGE;
                }
            }
        }
        if (action->special_effect & ACTION_SPECIAL_EFFECT_LEVEL_UP) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_LEVEL_UP;
        }
        if (action->special_effect & ACTION_SPECIAL_EFFECT_LEVEL_DOWN) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_LEVEL_DOWN;
        }
        if (action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_STOLEN;
        }
        if (action->special_effect
            & (ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT | ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN)) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_BROKEN;
        }
        if (action->attack_type & ACTION_TYPE_STATUS_CHANGE) {
            unit->status_display_image_count = 0;
            for (i = 0; i < 5; i++) {
                for (j = 0; j < 8; j++) {
                    count = unit->status_display_image_count;
                    bit = 0x80 >> j;
                    if (count < ACTION_STATUS_DISPLAY_CAPACITY) {
                        if (action->status_changes.infliction[i] & bit) {
                            image = g_action_status_display_image_ids[i][j];
                            unit->status_display_image_list[count] = image;
                            if (image != 0) {
                                unit->status_display_image_count++;
                            }
                        } else if (action->status_changes.removal[i] & bit) {
                            image = g_action_status_display_image_ids[i][j];
                            unit->status_display_image_list[count] = image;
                            if (image != 0) {
                                unit->status_display_image_list[unit->status_display_image_count]
                                    |= ACTION_STATUS_DISPLAY_REMOVAL;
                                unit->status_display_image_count++;
                            }
                        }
                    }
                }
            }
        }
    } else {
        miss = action->miss_type;
        if (miss != ACTION_MISS_TYPE_HIT) {
            if ((u32)(action->miss_type - 1) < 2 || miss == ACTION_MISS_TYPE_LEFT_HAND_EVADE
                || miss == ACTION_MISS_TYPE_FORCED_FAILURE || miss == ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD) {
                if ((action->miss_display_flags & ACTION_MISS_DISPLAY_TEAM_MASK) == 1) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_MISSED;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_GUARDED;
                }
            } else if (miss == ACTION_MISS_TYPE_CATCH) {
                unit->action_display_flags |= ACTION_DISPLAY_FLAG_CAUGHT;
            } else if (miss != ACTION_MISS_TYPE_CANCELED && miss != ACTION_MISS_TYPE_HIT
                && miss != ACTION_MISS_TYPE_REFLECTED) {
                unit->action_display_flags |= ACTION_DISPLAY_FLAG_MISSED;
            }
        }
    }
    if (unit->numeric_display.active == 0) {
        unit->numeric_display.progress = 0;
        battle_gfx_build_next_action_result_display(unit);
    }
}

/* Builds the next EXP/JP or failure-message display.
 *
 * The companion of battle_gfx_build_next_action_result_display for flags
 * 0x02000000 and above. Returns 0 when no flag is pending. Reading the
 * flags into a block-local before clearing keeps the target's and operand
 * order. */
s32 battle_gfx_build_next_special_action_result_display(battle_unit_t* unit) {
    action_result_display_t* display;
    battle_gfx_sprite_display_data_t* display_0;
    battle_gfx_sprite_display_data_t* display_1;
    battle_gfx_sprite_display_data_t* display_2;
    s32 digits[3];
    u32 flags;
    u32 clear;
    u16 selector;

    flags = unit->action_display_flags;
    display = &unit->numeric_display;
    if ((flags & ACTION_DISPLAY_FLAG_GAINED_EXP) != 0) {
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_GAINED_EXP;
        unit->numeric_display.active = 1;
        unit->numeric_display.value = unit->strike.action_rewards.earned_experience;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_GAINED_EXP;
        if (unit->numeric_display.progress >= 22) {
            unit->numeric_display.progress = 21;
        }
    } else if ((flags & ACTION_DISPLAY_FLAG_GAINED_JP) != 0) {
        unit->numeric_display.selector = BATTLE_NUMERIC_DISPLAY_GAINED_JP;
        unit->numeric_display.active = 1;
        unit->numeric_display.value = unit->strike.action_rewards.earned_jp;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_GAINED_JP;
        if (unit->numeric_display.progress >= 22) {
            unit->numeric_display.progress = 21;
        }
    } else {
        if ((flags & ACTION_DISPLAY_FLAG_NO_TARGET) != 0) {
            clear = ~ACTION_DISPLAY_FLAG_NO_TARGET;
            selector = BATTLE_NUMERIC_DISPLAY_NO_TARGET;
        } else if ((flags & ACTION_DISPLAY_FLAG_SILENCED) != 0) {
            clear = ~ACTION_DISPLAY_FLAG_SILENCED;
            selector = BATTLE_NUMERIC_DISPLAY_SILENCED;
        } else if ((flags & ACTION_DISPLAY_FLAG_NO_MP) != 0) {
            clear = ~ACTION_DISPLAY_FLAG_NO_MP;
            selector = BATTLE_NUMERIC_DISPLAY_NO_MP;
        } else {
            return 0;
        }
        unit->numeric_display.selector = selector;
        unit->numeric_display.active = 1;
        {
            u32 current;
            current = unit->action_display_flags;
            unit->action_display_flags = current & clear;
        }
        if (unit->numeric_display.progress >= 22) {
            unit->numeric_display.progress = 21;
        }
    }

    display->displays[0]->color.r = display->displays[1]->color.r = display->displays[2]->color.r = 150;
    display->displays[0]->color.g = display->displays[1]->color.g = display->displays[2]->color.g = 140;
    display->displays[0]->color.b = display->displays[1]->color.b = display->displays[2]->color.b = 20;

    if ((display->value / 1000) & 0xffff) {
        digits[2] = 9;
        digits[1] = 9;
        digits[0] = 9;
    } else {
        digits[0] = (display->value % 10) & 0xffff;
        digits[1] = (((display->value / 10) & 0xffff) % 10) & 0xffff;
        digits[2] = (display->value / 100) & 0xffff;
    }

    display_0 = display->displays[0];
    display_1 = display->displays[1];
    display_2 = display->displays[2];

    battle_gfx_store_sprite_display_data(display_2, 0, -21, 16, 0, 0, 0, 0, 0);
    battle_gfx_store_sprite_display_data(display_1, 0, -21, 16, 0, 0, 0, 0, 0);

    switch (display->selector & BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK) {
    case BATTLE_NUMERIC_DISPLAY_GAINED_EXP:
        if (digits[1] == 0) {
            battle_gfx_store_sprite_display_data(display_0, 0, -7, -24, 0xc0, 0xb8, 0x12, 9, 0);
            battle_gfx_store_sprite_display_data(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else {
            battle_gfx_store_sprite_display_data(display_0, 0, -7, -24, 0xc0, 0xb8, 0x12, 9, 0);
            battle_gfx_store_sprite_display_data(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            battle_gfx_store_sprite_display_data(display_2, 0, -21, -32, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        }
        break;
    case BATTLE_NUMERIC_DISPLAY_GAINED_JP:
        if (digits[1] == 0) {
            battle_gfx_store_sprite_display_data(display_0, 0, -7, -24, 0xb4, 0xb8, 0x0c, 9, 0);
            battle_gfx_store_sprite_display_data(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else {
            battle_gfx_store_sprite_display_data(display_0, 0, -7, -24, 0xb4, 0xb8, 0x0c, 9, 0);
            battle_gfx_store_sprite_display_data(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            battle_gfx_store_sprite_display_data(display_2, 0, -21, -32, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        }
        break;
    case BATTLE_NUMERIC_DISPLAY_NO_TARGET:
        display->displays[0]->color.r = display->displays[0]->color.g = display->displays[0]->color.b = 128;
        battle_gfx_store_sprite_display_data(display_0, 0, -14, -24, 0x1a, 0xdb, 0x25, 10, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_SILENCED:
        display->displays[0]->color.r = display->displays[0]->color.g = display->displays[0]->color.b = 128;
        battle_gfx_store_sprite_display_data(display_0, 0, -14, -24, 0xd8, 0xe9, 0x1e, 8, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_NO_MP:
        display->displays[0]->color.r = display->displays[0]->color.g = display->displays[0]->color.b = 128;
        battle_gfx_store_sprite_display_data(display_0, 0, -14, -24, 0x64, 0x20, 0x1a, 9, 0);
        break;
    }

    map_load_palette_data(&g_action_result_palette, MAP_CLUT_ROW_ACTION_RESULTS, unit->runtime_id, 0);
    return 1;
}

/* Queues the gained Exp and JP displays after an action, when the Display Gained
 * Exp/JP option is on and the amount is not 0. The first display is built at once
 * unless one is already running. */
void battle_gfx_init_earned_exp_jp_display(battle_unit_t* unit) {
    battle_gfx_sprite_display_data_t* display_0;
    battle_gfx_sprite_display_data_t* display_1;
    battle_gfx_sprite_display_data_t* display_2;

    if (g_main_game_options.fields.display_gained_exp_jp == GAME_OPTION_ON) {
        display_0 = unit->numeric_display.displays[0];
        display_1 = unit->numeric_display.displays[1];
        display_2 = unit->numeric_display.displays[2];
        display_2->tpage = BATTLE_GFX_TPAGE_FRAME;
        display_1->tpage = BATTLE_GFX_TPAGE_FRAME;
        display_0->tpage = BATTLE_GFX_TPAGE_FRAME;
        if (unit->strike.action_rewards.earned_experience != 0) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_GAINED_EXP;
        }
        if (unit->strike.action_rewards.earned_jp != 0) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_GAINED_JP;
        }
        if (unit->numeric_display.active == 0) {
            unit->numeric_display.progress = 0;
            battle_gfx_build_next_special_action_result_display(unit);
        }
    }
}

/* Queues the failure message for a charged action that cannot start: mode 1 no
 * target, 2 silenced, 3 not enough MP (the result of
 * action_init_current_data); any other mode queues none.
 *
 * The three numeric displays are pointed at the FRAME.BIN page, and the first
 * display is built at once unless one is already running. */
void battle_gfx_activate_numerical_sprite_data(battle_unit_t* unit, s32 mode) {
    battle_gfx_sprite_display_data_t* display_0 = unit->numeric_display.displays[0];
    battle_gfx_sprite_display_data_t* display_1 = unit->numeric_display.displays[1];
    battle_gfx_sprite_display_data_t* display_2 = unit->numeric_display.displays[2];

    display_2->tpage = BATTLE_GFX_TPAGE_FRAME;
    display_1->tpage = BATTLE_GFX_TPAGE_FRAME;
    display_0->tpage = BATTLE_GFX_TPAGE_FRAME;
    switch (mode) {
    case 1:
        unit->action_display_flags |= ACTION_DISPLAY_FLAG_NO_TARGET;
        break;
    case 2:
        unit->action_display_flags |= ACTION_DISPLAY_FLAG_SILENCED;
        break;
    case 3:
        unit->action_display_flags |= ACTION_DISPLAY_FLAG_NO_MP;
        break;
    }
    if (unit->numeric_display.active == 0) {
        unit->numeric_display.progress = 0;
        battle_gfx_build_next_special_action_result_display(unit);
    }
}
