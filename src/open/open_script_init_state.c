#include "fft/open.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Resets the OPEN script/credits subsystem for a new sequence.
 *
 * Sets up both screen environments, clears VRAM, resets the script state and
 * the 16 script records, and initialises the two credit-band and two fade
 * POLY_FT4 primitives.
 *
 * The record-clear loop counts up; loop optimisation reverses it into the
 * target's descending 0x164-stride offset with a bgez test. Stores are grouped
 * as the scheduler needs them: each screen environment's offset pair, and the
 * primitives' colour, UV and XY fields in that order (the fade loop's XY
 * stores last keep 0x5a from being hoisted into a sixth saved register).
 */
void open_script_init_state(void) {
    RECT rect;
    s32 i;
    s32 j;
    POLY_FT4* prim;

    SetDefDrawEnv(&g_open_gfx_screen_environments[0].draw, 0, 0, 0x200, 0xf0);
    SetDefDispEnv(&g_open_gfx_screen_environments[0].disp, 0, 0xf0, 0x200, 0xf0);
    SetDefDrawEnv(&g_open_gfx_screen_environments[1].draw, 0, 0xf0, 0x200, 0xf0);
    SetDefDispEnv(&g_open_gfx_screen_environments[1].disp, 0, 0, 0x200, 0xf0);
    g_open_gfx_screen_environments[0].draw.ofs[0] = 0x100;
    g_open_gfx_screen_environments[0].draw.ofs[1] = 0x78;
    g_open_gfx_screen_environments[1].draw.ofs[0] = 0x100;
    g_open_gfx_screen_environments[1].draw.ofs[1] = 0x168;
    g_open_gfx_screen_environments[0].draw.isbg = g_open_gfx_screen_environments[1].draw.isbg = 0;
    g_open_gfx_overlay_fade.rect.x = -0x100;
    g_open_gfx_overlay_fade.rect.y = -0x78;
    g_open_gfx_overlay_fade.rect.w = 0x200;
    g_open_gfx_overlay_fade.rect.h = 0xf0;
    g_open_gfx_screen_environments[0].disp.isrgb24 = g_open_gfx_screen_environments[1].disp.isrgb24 = 0;
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x200;
    rect.h = 0x1e0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);

    g_open_script_state.dispatch.flags = 1;
    g_open_script_state.dispatch.tpage = (s16)GetTPage(0, 2, 0x300, 0);
    g_open_script_state.dispatch.clut = (s16)GetClut(0, 0x1e0);
    g_open_script_state._unknown_1674 = 0;
    g_open_script_state.dispatch.external_counter = 0;
    g_open_script_state.dispatch.timing_fraction = 0;
    g_open_script_state.dispatch.timing_step = 0;
    g_open_script_state.dispatch.font_bank = 0;
    g_open_script_state.dispatch.wide_glyphs = 0;
    g_open_script_state.dispatch.skew = 0;
    g_open_script_state.dispatch.byte_offset = 0;
    g_open_script_state.dispatch.data_base = g_open_script_initial_data_base;
    for (j = 0; j < 16; j++) {
        g_open_script_state.records[j].flags = 0;
    }
    g_open_script_state.opntex.fade_intensity = 0;

    for (i = 0; i < 2; i++) {
        prim = &g_open_gfx_credit_band_primitives[i];
        SetPolyFT4(prim);
        SetSemiTrans(prim, 1);
        g_open_gfx_credit_band_primitives[i].tpage = GetTPage(0, 2, 0x300, 0);
        g_open_gfx_credit_band_primitives[i].clut = GetClut(0x10, 0x1e0);
        prim->r0 = 0x80;
        prim->g0 = 0x80;
        prim->b0 = 0x80;
    }

    g_open_gfx_credit_band_primitives[0].u0 = 0xf8;
    g_open_gfx_credit_band_primitives[0].v0 = 0;
    g_open_gfx_credit_band_primitives[0].u1 = 0xf9;
    g_open_gfx_credit_band_primitives[0].v1 = 0;
    g_open_gfx_credit_band_primitives[0].u2 = 0xf8;
    g_open_gfx_credit_band_primitives[0].v2 = 0x30;
    g_open_gfx_credit_band_primitives[0].u3 = 0xf9;
    g_open_gfx_credit_band_primitives[0].v3 = 0x30;
    g_open_gfx_credit_band_primitives[0].x0 = -0x100;
    g_open_gfx_credit_band_primitives[0].y0 = 0x48;
    g_open_gfx_credit_band_primitives[0].x1 = 0x100;
    g_open_gfx_credit_band_primitives[0].y1 = 0x48;
    g_open_gfx_credit_band_primitives[0].x2 = -0x100;
    g_open_gfx_credit_band_primitives[0].y2 = 0x78;
    g_open_gfx_credit_band_primitives[0].x3 = 0x100;
    g_open_gfx_credit_band_primitives[0].y3 = 0x78;
    g_open_gfx_credit_band_primitives[1].u0 = 0xfc;
    g_open_gfx_credit_band_primitives[1].v0 = 0;
    g_open_gfx_credit_band_primitives[1].u1 = 0xfd;
    g_open_gfx_credit_band_primitives[1].v1 = 0;
    g_open_gfx_credit_band_primitives[1].u2 = 0xfc;
    g_open_gfx_credit_band_primitives[1].v2 = 0x30;
    g_open_gfx_credit_band_primitives[1].u3 = 0xfd;
    g_open_gfx_credit_band_primitives[1].v3 = 0x30;
    g_open_gfx_credit_band_primitives[1].x0 = -0x100;
    g_open_gfx_credit_band_primitives[1].y0 = -0x78;
    g_open_gfx_credit_band_primitives[1].x1 = 0x100;
    g_open_gfx_credit_band_primitives[1].y1 = -0x78;
    g_open_gfx_credit_band_primitives[1].x2 = -0x100;
    g_open_gfx_credit_band_primitives[1].y2 = -0x48;
    g_open_gfx_credit_band_primitives[1].x3 = 0x100;
    g_open_gfx_credit_band_primitives[1].y3 = -0x48;

    for (i = 0; i < 2; i++) {
        prim = &g_open_gfx_fade_primitives[i];
        SetPolyFT4(prim);
        SetSemiTrans(prim, 1);
        g_open_gfx_fade_primitives[i].tpage = GetTPage(1, 2, 0x200, 0);
        g_open_gfx_fade_primitives[i].clut = GetClut(0, 0x1e1);
        prim->r0 = 0;
        prim->g0 = 0;
        prim->b0 = 0;
        prim->u0 = 0;
        prim->v0 = 0;
        prim->u1 = 0xff;
        prim->v1 = 0;
        prim->u2 = 0;
        prim->v2 = 0xb4;
        prim->u3 = 0xff;
        prim->v3 = 0xb4;
        prim->x0 = -0x100;
        prim->y0 = -0x5a;
        prim->x1 = 0x100;
        prim->y1 = -0x5a;
        prim->x2 = -0x100;
        prim->y2 = 0x5a;
        prim->x3 = 0x100;
        prim->y3 = 0x5a;
    }

    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x100;
    rect.h = 0x100;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
}
