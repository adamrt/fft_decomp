#include "fft/battle.h"
#include "psx/types.h"

/* Apply palette modulation to a renderer-side unit's current CLUT.
 *
 * Crystal, Chicken, Frog, and Treasure use their dedicated RAM palette
 * tables; every other sheet uses its VRAM-slot palette. The color parameters
 * must remain s16 to preserve the target's register allocation and tail
 * sharing.
 */
void battle_gfx_modify_misc_unit_palette(battle_unit_misc_data_t* misc, s32 mode, s32 section_id, s32 unit_misc_id,
    s32 preset, s16 red, s16 green, s16 blue, s32 final_value) {
    s32 slot;
    s32 sheet;
    s32 palette;

    slot = misc->spritesheet_vram_slot;
    sheet = misc->spritesheet_id;
    palette = misc->stored_palette;
    switch (sheet) {
    case BATTLE_SPRITESHEET_ID_CRYSTAL:
        battle_gfx_modify_vram_palette(mode, (u16*)g_battle_gfx_crystal_palettes[palette], section_id, unit_misc_id,
            preset, red, green, blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_CHICKEN:
        battle_gfx_modify_vram_palette(mode, (u16*)g_battle_gfx_chicken_palettes[palette], section_id, unit_misc_id,
            preset, red, green, blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_FROG:
        battle_gfx_modify_vram_palette(mode, (u16*)g_battle_gfx_frog_palettes[palette], section_id, unit_misc_id,
            preset, red, green, blue, final_value);
        break;
    case BATTLE_SPRITESHEET_ID_TREASURE:
        battle_gfx_modify_vram_palette(mode, (u16*)g_battle_gfx_treasure_palettes[palette], section_id, unit_misc_id,
            preset, red, green, blue, final_value);
        break;
    default:
        battle_gfx_modify_vram_palette(mode, (u16*)g_battle_gfx_spritesheet_slots[slot].palettes[palette], section_id,
            unit_misc_id, preset, red, green, blue, final_value);
        break;
    }
}
