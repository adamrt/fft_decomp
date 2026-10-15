#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"

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
