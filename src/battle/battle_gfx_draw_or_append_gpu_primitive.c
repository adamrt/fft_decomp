#include "fft/battle.h"
#include "psx/types.h"

/*
 * Submit a primitive for the active rendering mode.
 *
 * ETC full-screen graphics draw immediately. Normal battle rendering links
 * the primitive into the current ordering-table entry.
 */
void battle_gfx_draw_or_append_gpu_primitive(void* primitive) {
    s32* prim = primitive;
    s32* otag;

    if (g_battle_etc_graphics_enabled != 0) {
        DrawPrim(prim);
        return;
    }
    otag = (s32*)g_current_otag_entry;
    *prim = (*prim & 0xFF000000) | (*otag & 0xFFFFFF);
    *otag = (*otag & 0xFF000000) | ((s32)prim & 0xFFFFFF);
}
