#include "fft/open.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Resets the OPEN script/credits subsystem for a new sequence.
 *
 * Sets up both screen environments, clears VRAM, resets the script state and
 * the 16 script records, and initializes the two credit-band and two fade
 * POLY_FT4 primitives.
 *
 * The record-clear loop counts up; loop optimization reverses it into the
 * target's descending 0x164-stride offset with a bgez test. Stores are grouped
 * as the scheduler needs them: each screen environment's offset pair, and the
 * primitives' color, UV and XY fields in that order (the fade loop's XY
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

    g_open_script_state.dispatch.flags = OPEN_SCRIPT_DISPATCH_RUNNING;
    g_open_script_state.dispatch.tpage = (s16)GetTPage(0, 2, 0x300, 0);
    g_open_script_state.dispatch.clut = (s16)GetClut(0, 0x1e0);
    g_open_script_state._unknown_1674 = 0;
    g_open_script_state.dispatch.external_counter = 0;
    g_open_script_state.dispatch.timing_fraction = 0;
    g_open_script_state.dispatch.timing_step = 0;
    g_open_script_state.dispatch.font_index = 0;
    g_open_script_state.dispatch.wide_glyphs = 0;
    g_open_script_state.dispatch.skew = 0;
    g_open_script_state.dispatch.cursor.byte_offset = 0;
    g_open_script_state.dispatch.cursor.data_base = g_open_script_initial_data_base;
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
        setRGB0(prim, 0x80, 0x80, 0x80);
    }

    setUV4(&g_open_gfx_credit_band_primitives[0], 0xf8, 0, 0xf9, 0, 0xf8, 0x30, 0xf9, 0x30);
    setXY4(&g_open_gfx_credit_band_primitives[0], -0x100, 0x48, 0x100, 0x48, -0x100, 0x78, 0x100, 0x78);
    setUV4(&g_open_gfx_credit_band_primitives[1], 0xfc, 0, 0xfd, 0, 0xfc, 0x30, 0xfd, 0x30);
    setXY4(&g_open_gfx_credit_band_primitives[1], -0x100, -0x78, 0x100, -0x78, -0x100, -0x48, 0x100, -0x48);

    for (i = 0; i < 2; i++) {
        prim = &g_open_gfx_fade_primitives[i];
        SetPolyFT4(prim);
        SetSemiTrans(prim, 1);
        g_open_gfx_fade_primitives[i].tpage = GetTPage(1, 2, 0x200, 0);
        g_open_gfx_fade_primitives[i].clut = GetClut(0, 0x1e1);
        setRGB0(prim, 0, 0, 0);
        setUV4(prim, 0, 0, 0xff, 0, 0, 0xb4, 0xff, 0xb4);
        setXY4(prim, -0x100, -0x5a, 0x100, -0x5a, -0x100, 0x5a, 0x100, 0x5a);
    }

    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x100;
    rect.h = 0x100;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
}
