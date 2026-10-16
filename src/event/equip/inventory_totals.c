/* Textured quad submission and inventory totals. */

#include "fft/data_item.h"
#include "fft/event_equip.h"
#include "fft/gfx.h"
#include "fft/main_item.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Takes one POLY_FT4 from the pool, fills it as the rect->w x rect->h quad at
 * (rect->x + 0x80, rect->y) with texture origin (u, v), and links it at the
 * ordering-table entry g_equip_gfx_current_ot points to; the last argument
 * is not read. With `color` the quad is shaded with it. */
void equip_gfx_enqueue_textured_quad_current_ot(const RECT* rect, s32 u, s32 v, const rgb24_t* color, s32 semitrans,
    u16 texture_page, u16 clut, s32 unused_priority) {
    POLY_FT4* poly;

    poly = &g_menu_gfx_context->textured_quads[g_menu_gfx_poly_ft4_count++];

    if (color != 0) {
        SetShadeTex(poly, 0);
        setRGB0(poly, color->r, color->g, color->b);
    } else {
        SetShadeTex(poly, 1);
    }
    SetSemiTrans(poly, semitrans);

    setXYWH(poly, rect->x + 0x80, rect->y, rect->w, rect->h);
    setUVWH(poly, u, v, rect->w, rect->h);
    poly->clut = clut;
    poly->tpage = texture_page;

    {
        u32* ot = g_equip_gfx_current_ot;
        addPrim(ot, poly);
    }
}

/* Snapshot the player inventory counts, then add every item currently equipped
 * by the party roster. */
void equip_item_build_inventory_totals(void) {
    s32 i;
    s32 j;
    party_unit_t* unit;
    u32 item;

    bcopy(g_main_item_quantities, g_equip_item_inventory_totals, sizeof(g_main_item_quantities));

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = main_party_get_unit(i);
        if (unit->party_id != PARTY_ID_NONE) {
            for (j = 0; j < UNIT_EQUIPMENT_SLOT_COUNT; j++) {
                item = unit->equipment[j];
                if (item != ITEM_ID_NOTHING && item < ITEM_ID_END) {
                    g_equip_item_inventory_totals[item]++;
                }
            }
        }
    }
}
