#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/*
 * Set the texture page, CLUT and UVs of a unit's portrait quad.
 *
 * The unit whose ID equals g_main_special_portrait_unit_id uses the fixed 0x380/0x100 page; others
 * select a 32-line band of the spritesheet VRAM slot's page and a CLUT row
 * indexed by the low nibble of the Misc unit_t Data ID.
 */
void battle_menu_build_unit_portrait_poly(POLY_FT4* poly, u32 battle_id) {
    s32 vram_slot;
    s32 misc_id;
    u8 v;

    if (battle_unit_get_stats_from_battle_id(battle_id)->unit_id == g_main_special_portrait_unit_id) {
        poly->u0 = 0xe0;
        poly->v0 = 0xcf;
        poly->u1 = 0xff;
        poly->v1 = 0xcf;
        poly->u2 = 0xe0;
        poly->v2 = 0xff;
        poly->u3 = 0xff;
        poly->v3 = 0xff;
        poly->tpage = GetTPage(0, 0, 0x380, 0x100);
        poly->clut = GetClut(0x40, 0x1fe);
    } else {
        vram_slot = battle_gfx_get_spritesheet_vram_by_battle_id(battle_id);
        misc_id = battle_unit_get_misc_id_by_battle_id(battle_id);
        v = vram_slot << 5;
        if (vram_slot >= 8) {
            poly->tpage = GetTPage(0, 0, 0x380, 0x120);
            v = 0;
        } else {
            poly->tpage = GetTPage(0, 0, 0x340, 0x100);
        }
        poly->u0 = 0x80;
        poly->v0 = v;
        poly->u1 = 0x80;
        poly->v1 = v + 0x20;
        poly->u2 = 0x50;
        poly->v2 = v;
        poly->u3 = 0x50;
        poly->v3 = v + 0x20;
        poly->v1--;
        poly->v3--;
        poly->clut = GetClut((misc_id & 0xf) * 16, 0x1ea);
    }
}
