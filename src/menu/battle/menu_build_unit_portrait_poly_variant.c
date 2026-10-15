#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Set the texture page, CLUT and UVs of a unit's portrait quad.
 *
 * The unit whose ID equals g_main_special_portrait_unit_id uses the fixed 0x380/0x100 page; others
 * select a 32-line band of the spritesheet VRAM slot's page and the CLUT of
 * MAP_CLUT_ROW_UNITS_SECOND indexed by the low nibble of the unit runtime record ID.
 */
void menu_build_unit_portrait_poly_variant(POLY_FT4* poly, s32 battle_id) {
    s32 vram_slot;
    s32 runtime_id;
    u8 v;

    if (battle_unit_get_stats_from_battle_id(battle_id)->unit_id == g_main_special_portrait_unit_id) {
        setUV4(poly, 0xe0, 0xcf, 0xff, 0xcf, 0xe0, 0xff, 0xff, 0xff);
        poly->tpage = GetTPage(0, 0, 0x380, 0x100);
        poly->clut = GetClut(0x40, 0x1fe);
    } else {
        vram_slot = battle_gfx_get_spritesheet_vram_by_battle_id(battle_id);
        runtime_id = battle_unit_get_runtime_id_by_battle_id(battle_id);
        v = vram_slot << 5;
        if (vram_slot >= 8) {
            poly->tpage = GetTPage(0, 0, 0x380, 0x120);
            v = 0;
        } else {
            poly->tpage = GetTPage(0, 0, 0x340, 0x100);
        }
        setUV4(poly, 0x80, v, 0x80, v + 0x20, 0x50, v, 0x50, v + 0x20);
        poly->v1--;
        poly->v3--;
        poly->clut = GetClut((runtime_id & 0xf) * 16, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_UNITS_SECOND);
    }
}
