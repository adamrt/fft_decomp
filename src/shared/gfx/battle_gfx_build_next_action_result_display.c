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
