#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/map.h"
#include "fft/script_variables.h"
#include "psx/types.h"

/* Reload a unit's palette tinted for tile effects, weather and time of day.
 *
 * The unit's spritesheet palette is installed first (monster sheets
 * 0x60-0x9a use the upper eight palettes). The RGB offset starts at -4, -8 or
 * -0xc for tile effect levels 1-3; unless weather bit 1 is set, script
 * variables 0x23 (weather) and 0x24 (night) shift it further, and a unit more
 * than 0x30 below its projected screen Z stays untinted. Crystal, chicken,
 * frog and treasure sheets take fixed palettes.
 *
 * Each branch makes its own palette call, as the original evidently did:
 * cross-jumping merges them only after register allocation, so the extra
 * references still rank row above unit in global allocation. The two snow
 * (weather bit 0) branches are identical. */
void battle_gfx_tint_unit_palette_for_weather_and_tile(battle_unit_misc_data_t* unit, s32 level, s32 final_value) {
    s32 row;
    s32 stored_palette;
    s32 slot;
    s32 weather;
    s32 blue;
    s32 green;
    s32 red;
    s16 screen_z;

    slot = unit->spritesheet_vram_slot;
    stored_palette = unit->stored_palette;
    row = unit->unit_id;

    if ((u32)(g_battle_gfx_spritesheet_slots[slot].spritesheet_id - 0x60) < 0x3b) {
        battle_map_load_palette_data(
            (const u16*)g_battle_gfx_spritesheet_slots[slot].palettes[stored_palette + 8], 0xa, row, 0);
    } else {
        battle_map_load_palette_data((const u16*)g_battle_gfx_spritesheet_slots[slot].palettes[8], 0xa, row, 0);
    }

    blue = 0;
    green = 0;
    red = 0;
    switch (level) {
    case 1:
        blue = -4;
        green = -4;
        red = -4;
        break;
    case 2:
        blue = -8;
        green = -8;
        red = -8;
        break;
    case 3:
        blue = -0xc;
        green = -0xc;
        red = -0xc;
        break;
    }

    {
        const s32* weather_flags = &g_battle_map_weather_flags;

        if (!(*weather_flags & 2)) {
            weather = battle_script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
            if (battle_script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY) == 0) {
                if (*weather_flags & 1) {
                    switch (weather) {
                    case 0:
                    case 1:
                        break;
                    case BATTLE_WEATHER_RAIN:
                        red -= 1;
                        blue += 1;
                        break;
                    case BATTLE_WEATHER_STORM:
                    case BATTLE_WEATHER_STRONG_STORM:
                    default:
                        red -= 2;
                        green -= 1;
                        blue += 2;
                        break;
                    }
                } else {
                    switch (weather) {
                    case 0:
                    case 1:
                        break;
                    case BATTLE_WEATHER_RAIN:
                        red -= 1;
                        blue += 1;
                        break;
                    case BATTLE_WEATHER_STORM:
                    case BATTLE_WEATHER_STRONG_STORM:
                    default:
                        red -= 2;
                        green -= 1;
                        blue += 2;
                        break;
                    }
                }
            } else {
                if (*weather_flags & 1) {
                    switch (weather) {
                    case 0:
                    case 1:
                        red -= 3;
                        green -= 1;
                        blue += 2;
                        break;
                    case BATTLE_WEATHER_RAIN:
                        red -= 4;
                        green -= 1;
                        blue += 3;
                        break;
                    case BATTLE_WEATHER_STORM:
                    case BATTLE_WEATHER_STRONG_STORM:
                    default:
                        red -= 4;
                        green -= 2;
                        blue += 4;
                        break;
                    }
                } else {
                    switch (weather) {
                    case 0:
                    case 1:
                        red -= 3;
                        green -= 1;
                        blue += 2;
                        break;
                    case BATTLE_WEATHER_RAIN:
                        red -= 4;
                        green -= 1;
                        blue += 3;
                        break;
                    case BATTLE_WEATHER_STORM:
                    case BATTLE_WEATHER_STRONG_STORM:
                    default:
                        red -= 4;
                        green -= 2;
                        blue += 4;
                        break;
                    }
                }
            }
        }
    }

    screen_z = battle_gfx_calculate_screen_z_from_misc_screen_data(unit);
    if (!(g_battle_map_weather_flags & 2) && screen_z - 0x30 > unit->screen.vy) {
        blue = 0;
        green = 0;
        red = 0;
    }

    switch (unit->spritesheet_id) {
    case BATTLE_SPRITESHEET_ID_CRYSTAL:
        battle_map_update_palette_from_other((u16*)g_battle_gfx_crystal_palettes[stored_palette], 3, row, 0, (s16)red,
            (s16)green, (s16)blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_CHICKEN:
        battle_map_update_palette_from_other((u16*)g_battle_gfx_chicken_palettes[stored_palette], 3, row, 0, (s16)red,
            (s16)green, (s16)blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_FROG:
        battle_map_update_palette_from_other(
            (u16*)g_battle_gfx_frog_palettes[stored_palette], 3, row, 0, (s16)red, (s16)green, (s16)blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_TREASURE:
        battle_map_update_palette_from_other((u16*)g_battle_gfx_treasure_palettes[stored_palette], 3, row, 0, (s16)red,
            (s16)green, (s16)blue, final_value);
        break;
    default:
        battle_map_update_palette_from_other((u16*)g_battle_gfx_spritesheet_slots[slot].palettes[stored_palette], 3,
            row, 0, (s16)red, (s16)green, (s16)blue, final_value);
        break;
    }
}
