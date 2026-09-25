#include "fft/battle.h"

/* Non-const view of g_battle_action_display_image_rects: the header's const
 * lets GCC move these loads and changes the schedule. */
extern battle_action_display_image_rect_t g_battle_action_display_image_rects_view[];

/* Int-argument view of battle_gfx_store_sprite_display_data: the target
 * sign-extends y_shift and the u values from 16 bits, which its s8/u16
 * prototype would narrow to a byte and zero-extend. */
#define STORE_SPRITE                                                                                                   \
    ((void (*)(battle_gfx_sprite_display_data_t*, s32, s32, s32, s32, s32, s32, s32,                                   \
        s32))battle_gfx_store_sprite_display_data)

/* Builds the next ordinary damage, healing, stat, status, or result display.
 *
 * EXP/JP and the three failure messages above bit 0x01000000 are handled by
 * the following dispatcher at 0x800808b8. */
s32 battle_gfx_build_next_action_result_display(battle_unit_misc_data_t* unit) {
    union {
        battle_stats_t* stats;
        u32 scan_flags;
    } data;
    s32 flags;
    battle_action_result_display_t* display;
    battle_gfx_sprite_display_data_t* display_0;
    battle_gfx_sprite_display_data_t* display_1;
    battle_gfx_sprite_display_data_t* display_2;
    battle_gfx_sprite_display_data_t* color_2;
    battle_gfx_sprite_display_data_t* color_0;
    battle_gfx_sprite_display_data_t* color_1;
    battle_gfx_sprite_display_data_t* blue_0;
    battle_gfx_sprite_display_data_t* blue_1;
    battle_gfx_sprite_display_data_t* blue_2;
    const battle_action_display_image_rect_t* image;
    s32 flag;
    s32 index;
    s32 digits[3];
    u32 red;
    u32 green;
    u32 blue;
    s16 y_shift;
    s32 result;
    u8 change;
    u8 status;

    flags = unit->action_display_flags.word;
    data.stats = unit->battle_data;
    display = (battle_action_result_display_t*)&unit->numeric_display_active;

    if ((u16)flags != 0) {
        g_battle_action_display_values[0] = data.stats->action.hp_damage;
        g_battle_action_display_values[1] = data.stats->action.hp_healing;
        g_battle_action_display_values[2] = data.stats->action.mp_damage;
        g_battle_action_display_values[3] = data.stats->action.mp_healing;
        change = data.stats->action.sp_change & BATTLE_ACTION_STAT_CHANGE_VALUE_MASK;
        g_battle_action_display_values[5] = change;
        g_battle_action_display_values[4] = change;
        change = data.stats->action.ct_change & 0x7f;
        g_battle_action_display_values[7] = change;
        g_battle_action_display_values[6] = change;
        change = data.stats->action.brave_change & BATTLE_ACTION_STAT_CHANGE_VALUE_MASK;
        g_battle_action_display_values[9] = change;
        g_battle_action_display_values[8] = change;
        change = data.stats->action.faith_change & BATTLE_ACTION_STAT_CHANGE_VALUE_MASK;
        g_battle_action_display_values[11] = change;
        g_battle_action_display_values[10] = change;
        flag = 1;
        change = data.stats->action.pa_change & BATTLE_ACTION_STAT_CHANGE_VALUE_MASK;
        g_battle_action_display_values[13] = change;
        g_battle_action_display_values[12] = change;
        index = 0;
        change = data.stats->action.ma_change & BATTLE_ACTION_STAT_CHANGE_VALUE_MASK;
        g_battle_action_display_values[15] = change;
        g_battle_action_display_values[14] = change;

        data.scan_flags = unit->action_display_flags.word;
        while (index < 16) {
            if ((flag & data.scan_flags) != 0) {
                flags = index;
                display->value = g_battle_action_display_values[flags];
                display->selector = g_battle_action_display_numeric_selectors[flags];
                index = *(volatile u32*)&unit->action_display_flags.word;
                unit->action_display_flags.word = ~flag & index;
                break;
            }
            index++;
            flag <<= 1;
        }
    } else if ((flags & BATTLE_ACTION_DISPLAY_FLAG_LEVEL_DOWN) != 0) {
        u32 branch_flags;
        unit->numeric_display_selector = BATTLE_NUMERIC_DISPLAY_LEVEL_DOWN;
        branch_flags = *(volatile u32*)&unit->action_display_flags.word;
        unit->action_display_flags.word = branch_flags & ~BATTLE_ACTION_DISPLAY_FLAG_LEVEL_DOWN;
    } else if ((flags & BATTLE_ACTION_DISPLAY_FLAG_LEVEL_UP) != 0) {
        u32 branch_flags;
        unit->numeric_display_selector = BATTLE_NUMERIC_DISPLAY_LEVEL_UP;
        branch_flags = *(volatile u32*)&unit->action_display_flags.word;
        unit->action_display_flags.word = branch_flags & ~BATTLE_ACTION_DISPLAY_FLAG_LEVEL_UP;
    } else if ((flags & BATTLE_ACTION_DISPLAY_FLAG_STOLEN) != 0) {
        u32 branch_flags;
        unit->numeric_display_selector = BATTLE_NUMERIC_DISPLAY_STOLEN;
        branch_flags = *(volatile u32*)&unit->action_display_flags.word;
        unit->action_display_flags.word = branch_flags & ~BATTLE_ACTION_DISPLAY_FLAG_STOLEN;
    } else if ((flags & BATTLE_ACTION_DISPLAY_FLAG_BROKEN) != 0) {
        u32 branch_flags;
        unit->numeric_display_selector = BATTLE_NUMERIC_DISPLAY_BROKEN;
        branch_flags = *(volatile u32*)&unit->action_display_flags.word;
        unit->action_display_flags.word = branch_flags & ~BATTLE_ACTION_DISPLAY_FLAG_BROKEN;
    } else if ((flags & BATTLE_ACTION_DISPLAY_FLAG_QUICK) != 0) {
        u32 branch_flags;
        unit->numeric_display_selector = BATTLE_NUMERIC_DISPLAY_QUICK;
        branch_flags = *(volatile u32*)&unit->action_display_flags.word;
        unit->action_display_flags.word = branch_flags & ~BATTLE_ACTION_DISPLAY_FLAG_QUICK;
    } else if ((flags & BATTLE_ACTION_DISPLAY_FLAG_CT_ZERO) != 0) {
        u32 branch_flags;
        unit->numeric_display_selector = BATTLE_NUMERIC_DISPLAY_CT_ZERO;
        branch_flags = *(volatile u32*)&unit->action_display_flags.word;
        unit->action_display_flags.word = branch_flags & ~BATTLE_ACTION_DISPLAY_FLAG_CT_ZERO;
    } else if ((flags & BATTLE_ACTION_DISPLAY_FLAG_MISSED) != 0) {
        u32 branch_flags;
        unit->numeric_display_selector = BATTLE_NUMERIC_DISPLAY_MISSED;
        branch_flags = *(volatile u32*)&unit->action_display_flags.word;
        unit->action_display_flags.word = branch_flags & ~BATTLE_ACTION_DISPLAY_FLAG_MISSED;
    } else if ((flags & BATTLE_ACTION_DISPLAY_FLAG_GUARDED) != 0) {
        u32 branch_flags;
        unit->numeric_display_selector = BATTLE_NUMERIC_DISPLAY_GUARDED;
        branch_flags = *(volatile u32*)&unit->action_display_flags.word;
        unit->action_display_flags.word = branch_flags & ~BATTLE_ACTION_DISPLAY_FLAG_GUARDED;
    } else if ((flags & BATTLE_ACTION_DISPLAY_FLAG_CAUGHT) != 0) {
        u32 branch_flags;
        unit->numeric_display_selector = BATTLE_NUMERIC_DISPLAY_CAUGHT;
        branch_flags = *(volatile u32*)&unit->action_display_flags.word;
        unit->action_display_flags.word = branch_flags & ~BATTLE_ACTION_DISPLAY_FLAG_CAUGHT;
    } else if (unit->status_display_image_count != 0) {
        unit->status_display_image_count--;
        status = unit->status_display_image_list[unit->status_display_image_count];
        if ((status & BATTLE_ACTION_STATUS_DISPLAY_REMOVAL) != 0) {
            display->selector = BATTLE_NUMERIC_DISPLAY_STATUS_REMOVED;
        } else {
            display->selector = BATTLE_NUMERIC_DISPLAY_STATUS_INFLICTED;
        }
        status &= BATTLE_ACTION_STATUS_DISPLAY_IMAGE_MASK;
    } else {
        return 0;
    }

    if (unit->battle_data->action.critical != 0) {
        display->selector |= BATTLE_NUMERIC_DISPLAY_MODIFIER_CRITICAL;
    } else {
        display->selector &= ~BATTLE_NUMERIC_DISPLAY_MODIFIER_CRITICAL;
    }
    display->active = 1;
    if ((u16)display->progress >= 22) {
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

    color_2 = *(battle_gfx_sprite_display_data_t* volatile*)&display->displays[2];
    color_0 = *(battle_gfx_sprite_display_data_t* volatile*)&display->displays[0];
    color_1 = *(battle_gfx_sprite_display_data_t* volatile*)&display->displays[1];
    color_2->red = red;
    color_1->red = red;
    color_0->red = red;
    color_2 = *(battle_gfx_sprite_display_data_t* volatile*)&display->displays[2];
    color_0 = *(battle_gfx_sprite_display_data_t* volatile*)&display->displays[0];
    color_1 = *(battle_gfx_sprite_display_data_t* volatile*)&display->displays[1];
    color_2->green = green;
    color_1->green = green;
    color_0->green = green;
    blue_0 = *(battle_gfx_sprite_display_data_t* volatile*)&display->displays[0];
    blue_1 = *(battle_gfx_sprite_display_data_t* volatile*)&display->displays[1];
    blue_2 = *(battle_gfx_sprite_display_data_t* volatile*)&display->displays[2];
    blue_2->blue = blue;
    blue_1->blue = blue;
    blue_0->blue = blue;

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

    STORE_SPRITE(display_0, 0, 0, -32, 0, 0, 0, 0, 0);
    STORE_SPRITE(display_1, 0, 0, -32, 0, 0, 0, 0, 0);

    switch (display->selector & BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK) {
    case BATTLE_NUMERIC_DISPLAY_HP_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_HP_HEALING:
    case BATTLE_NUMERIC_DISPLAY_MP_DAMAGE:
    case BATTLE_NUMERIC_DISPLAY_MP_HEALING:
        y_shift = -32;
        if ((display->selector & 1) == 0) {
            y_shift = 16;
        }
        STORE_SPRITE(display_1, 0, -14, y_shift, 0, 0, 0, 0, 0);
        STORE_SPRITE(display_2, 0, -21, y_shift, 0, 0, 0, 0, 0);
        if ((digits[2] | digits[1]) == 0) {
            STORE_SPRITE(display_0, 0, -21, y_shift, (s32)(s16)(0xa8 + digits[0] * 8), 0x30, 8, 16, 0);
        } else if (digits[2] == 0) {
            STORE_SPRITE(display_0, 0, -14, y_shift, (s32)(s16)(0xa8 + digits[0] * 8), 0x30, 8, 16, 0);
            STORE_SPRITE(display_1, 0, -21, y_shift, (s32)(s16)(0xa8 + digits[1] * 8), 0x30, 8, 16, 0);
        } else {
            STORE_SPRITE(display_0, 0, -7, y_shift, (s32)(s16)(0xa8 + digits[0] * 8), 0x30, 8, 16, 0);
            STORE_SPRITE(display_1, 0, -14, y_shift, (s32)(s16)(0xa8 + digits[1] * 8), 0x30, 8, 16, 0);
            STORE_SPRITE(display_2, 0, -21, y_shift, (s32)(s16)(0xa8 + digits[2] * 8), 0x30, 8, 16, 0);
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
            STORE_SPRITE(display_0, 0, -14, y_shift, (s32)(s16)(0xa8 + digits[0] * 8), 0x30, 8, 16, 0);
        } else {
            STORE_SPRITE(display_0, 0, -14, y_shift, (s32)(s16)(0xa8 + digits[0] * 8), 0x30, 8, 16, 0);
            STORE_SPRITE(display_1, 0, -21, y_shift, (s32)(s16)(0xa8 + digits[1] * 8), 0x30, 8, 16, 0);
        }

        switch (display->selector & 0x0e) {
        case BATTLE_NUMERIC_DISPLAY_SPEED_DAMAGE:
            STORE_SPRITE(display_2, 0, -7, y_shift, 0x9c, 0xc2, 0x18, 0x0a, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_CT_DAMAGE:
            STORE_SPRITE(display_2, 0, -7, y_shift, 0xc2, 0xc2, 0x0e, 0x0a, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_BRAVE_DAMAGE:
            STORE_SPRITE(display_2, 0, -7, y_shift, 0xa8, 0xb8, 0x0b, 0x0a, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_FAITH_DAMAGE:
            STORE_SPRITE(display_2, 0, -7, y_shift, 0x9c, 0xb8, 0x0c, 0x0a, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_PA_DAMAGE:
            STORE_SPRITE(display_2, 0, -7, y_shift, 0x40, 0, 0x0c, 0x0d, 0);
            break;
        case BATTLE_NUMERIC_DISPLAY_MA_DAMAGE:
            STORE_SPRITE(display_2, 0, -7, y_shift, 0x40, 0x0d, 0x0b, 0x0a, 0);
            break;
        }
        break;

    case BATTLE_NUMERIC_DISPLAY_MISSED:
        STORE_SPRITE(display_2, 0, -21, -32, 0xbe, 0x48, 0x1b, 8, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_GUARDED:
        STORE_SPRITE(display_2, 0, -21, -32, 0xb8, 0x60, 0x1e, 8, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_CAUGHT:
        STORE_SPRITE(display_2, 0, -21, -32, 0x30, 0xe6, 0x1a, 0x0a, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_STATUS_INFLICTED:
    case BATTLE_NUMERIC_DISPLAY_STATUS_REMOVED:
        result = status;
        STORE_SPRITE(display_2, 0, -21, -32, g_battle_action_display_image_rects_view[result].u,
            g_battle_action_display_image_rects_view[result].v, g_battle_action_display_image_rects_view[result].width,
            (g_battle_action_display_image_rects_view + result)->height, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_LEVEL_DOWN:
        STORE_SPRITE(display_1, 0, -7, 16, 0xb0, 0x30, 8, 16, 0);
        STORE_SPRITE(display_2, 0, 0, 16, 0xb4, 0xc1, 0x0e, 0x0a, 0);
        /* fall through */
    case BATTLE_NUMERIC_DISPLAY_LEVEL_UP:
        STORE_SPRITE(display_1, 0, -7, -32, 0xb0, 0x30, 8, 16, 0);
        STORE_SPRITE(display_2, 0, 0, -32, 0xb4, 0xc1, 0x0e, 0x0a, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_STOLEN:
        image = &g_battle_action_display_image_rects[BATTLE_ACTION_DISPLAY_IMAGE_STOLEN];
        STORE_SPRITE(display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_BROKEN:
        image = &g_battle_action_display_image_rects[BATTLE_ACTION_DISPLAY_IMAGE_BROKEN];
        STORE_SPRITE(display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_QUICK:
        image = &g_battle_action_display_image_rects[BATTLE_ACTION_DISPLAY_IMAGE_QUICK];
        STORE_SPRITE(display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_CT_ZERO:
        STORE_SPRITE(display_1, 0, -16, y_shift, 0xc2, 0xc2, 0x0e, 8, 0);
        STORE_SPRITE(display_2, 0, 0, y_shift, 0xa8, 0x34, 8, 8, 0);
        break;
    }

    battle_map_load_palette_data(g_battle_action_result_palette, 7, unit->unit_id, 0);
    return 1;
}
