#include "fft/battle.h"

/* Int-argument view of battle_gfx_store_sprite_display_data: the target
 * sign-extends the (s16) u values, which its u16 prototype would andi 0xffff. */
#define STORE_SPRITE                                                                                                   \
    ((void (*)(battle_gfx_sprite_display_data_t*, s32, s32, s32, s32, s32, s32, s32,                                   \
        s32))battle_gfx_store_sprite_display_data)

/* Builds the next EXP/JP or failure-message display.
 *
 * The companion of battle_gfx_build_next_action_result_display for flags
 * 0x02000000 and above. Returns 0 when no flag is pending. Reading the
 * flags into a block-local before clearing keeps the target's and operand
 * order. */
s32 battle_gfx_build_next_special_action_result_display(battle_unit_misc_data_t* unit) {
    battle_action_result_display_t* display;
    battle_gfx_sprite_display_data_t* display_0;
    battle_gfx_sprite_display_data_t* display_1;
    battle_gfx_sprite_display_data_t* display_2;
    battle_gfx_sprite_display_data_t* color;
    s32 digits[3];
    u32 flags;
    u32 clear;
    u16 selector;

    flags = unit->action_display_flags.word;
    display = (battle_action_result_display_t*)&unit->numeric_display_active;
    if ((flags & BATTLE_ACTION_DISPLAY_FLAG_GAINED_EXP) != 0) {
        unit->numeric_display_selector = BATTLE_NUMERIC_DISPLAY_GAINED_EXP;
        unit->numeric_display_active = 1;
        unit->numeric_display_value = unit->action_rewards.earned_experience;
        unit->action_display_flags.word &= ~BATTLE_ACTION_DISPLAY_FLAG_GAINED_EXP;
        if ((u16)unit->numeric_display_progress >= 22) {
            unit->numeric_display_progress = 21;
        }
    } else if ((flags & BATTLE_ACTION_DISPLAY_FLAG_GAINED_JP) != 0) {
        unit->numeric_display_selector = BATTLE_NUMERIC_DISPLAY_GAINED_JP;
        unit->numeric_display_active = 1;
        unit->numeric_display_value = unit->action_rewards.earned_jp;
        unit->action_display_flags.word &= ~BATTLE_ACTION_DISPLAY_FLAG_GAINED_JP;
        if ((u16)unit->numeric_display_progress >= 22) {
            unit->numeric_display_progress = 21;
        }
    } else {
        if ((flags & BATTLE_ACTION_DISPLAY_FLAG_NO_TARGET) != 0) {
            clear = ~BATTLE_ACTION_DISPLAY_FLAG_NO_TARGET;
            selector = BATTLE_NUMERIC_DISPLAY_NO_TARGET;
        } else if ((flags & BATTLE_ACTION_DISPLAY_FLAG_SILENCED) != 0) {
            clear = ~BATTLE_ACTION_DISPLAY_FLAG_SILENCED;
            selector = BATTLE_NUMERIC_DISPLAY_SILENCED;
        } else if ((flags & BATTLE_ACTION_DISPLAY_FLAG_NO_MP) != 0) {
            clear = ~BATTLE_ACTION_DISPLAY_FLAG_NO_MP;
            selector = BATTLE_NUMERIC_DISPLAY_NO_MP;
        } else {
            return 0;
        }
        unit->numeric_display_selector = selector;
        unit->numeric_display_active = 1;
        {
            u32 current;
            current = unit->action_display_flags.word;
            unit->action_display_flags.word = current & clear;
        }
        if ((u16)unit->numeric_display_progress >= 22) {
            unit->numeric_display_progress = 21;
        }
    }

    display->displays[0]->red = display->displays[1]->red = display->displays[2]->red = 150;
    display->displays[0]->green = display->displays[1]->green = display->displays[2]->green = 140;
    display->displays[0]->blue = display->displays[1]->blue = display->displays[2]->blue = 20;

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

    STORE_SPRITE(display_2, 0, -21, 16, 0, 0, 0, 0, 0);
    STORE_SPRITE(display_1, 0, -21, 16, 0, 0, 0, 0, 0);

    switch (display->selector & BATTLE_NUMERIC_DISPLAY_SELECTOR_MASK) {
    case BATTLE_NUMERIC_DISPLAY_GAINED_EXP:
        if (digits[1] == 0) {
            STORE_SPRITE(display_0, 0, -7, -24, 0xc0, 0xb8, 0x12, 9, 0);
            STORE_SPRITE(display_1, 0, -14, -32, (s16)(0xa8 + digits[0] * 8), 0x30, 8, 16, 0);
        } else {
            STORE_SPRITE(display_0, 0, -7, -24, 0xc0, 0xb8, 0x12, 9, 0);
            STORE_SPRITE(display_1, 0, -14, -32, (s16)(0xa8 + digits[0] * 8), 0x30, 8, 16, 0);
            STORE_SPRITE(display_2, 0, -21, -32, (s16)(0xa8 + digits[1] * 8), 0x30, 8, 16, 0);
        }
        break;
    case BATTLE_NUMERIC_DISPLAY_GAINED_JP:
        if (digits[1] == 0) {
            STORE_SPRITE(display_0, 0, -7, -24, 0xb4, 0xb8, 0x0c, 9, 0);
            STORE_SPRITE(display_1, 0, -14, -32, (s16)(0xa8 + digits[0] * 8), 0x30, 8, 16, 0);
        } else {
            STORE_SPRITE(display_0, 0, -7, -24, 0xb4, 0xb8, 0x0c, 9, 0);
            STORE_SPRITE(display_1, 0, -14, -32, (s16)(0xa8 + digits[0] * 8), 0x30, 8, 16, 0);
            STORE_SPRITE(display_2, 0, -21, -32, (s16)(0xa8 + digits[1] * 8), 0x30, 8, 16, 0);
        }
        break;
    case BATTLE_NUMERIC_DISPLAY_NO_TARGET:
        color = display->displays[0];
        color->red = color->green = color->blue = 128;
        STORE_SPRITE(display_0, 0, -14, -24, 0x1a, 0xdb, 0x25, 10, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_SILENCED:
        color = display->displays[0];
        color->red = color->green = color->blue = 128;
        STORE_SPRITE(display_0, 0, -14, -24, 0xd8, 0xe9, 0x1e, 8, 0);
        break;
    case BATTLE_NUMERIC_DISPLAY_NO_MP:
        color = display->displays[0];
        color->red = color->green = color->blue = 128;
        STORE_SPRITE(display_0, 0, -14, -24, 0x64, 0x20, 0x1a, 9, 0);
        break;
    }

    battle_map_load_palette_data(g_battle_action_result_palette, 7, unit->unit_id, 0);
    return 1;
}
