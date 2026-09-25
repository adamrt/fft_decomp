#include "fft/attack.h"

typedef struct {
    POLY_FT4 poly;
    POLY_F3 tri[2];
} attack_deploy_arrow_gfx_t;

extern attack_deploy_arrow_gfx_t g_attack_deploy_screen_arrow;

/* Build the textured deployment banner and its two arrowhead triangles. */
void attack_deploy_build_screen_arrow(void) {
    s32 value;
    s32 left;
    s32 right;
    s32 texture_mode;
    s32 blend_mode;

    SetPolyFT4(&g_attack_deploy_screen_arrow.poly);
    SetSemiTrans(&g_attack_deploy_screen_arrow.poly, 1);
    SetShadeTex(&g_attack_deploy_screen_arrow.poly, 0);
    texture_mode = 0;
    blend_mode = 1;
    value = 0x80;
    left = 0xAF;
    /* Loads the GetTPage mode arguments into a0/a1 here, not just before the call. */
    __asm__("" : "=r"(texture_mode), "=r"(blend_mode) : "0"(texture_mode), "1"(blend_mode));
    g_attack_deploy_screen_arrow.poly.r0 = value;
    g_attack_deploy_screen_arrow.poly.g0 = value;
    g_attack_deploy_screen_arrow.poly.b0 = value;
    value = 0xB6;
    right = 0xEF;
    g_attack_deploy_screen_arrow.poly.y0 = value;
    g_attack_deploy_screen_arrow.poly.y1 = value;
    value = 0xDE;
    g_attack_deploy_screen_arrow.poly.x0 = left;
    g_attack_deploy_screen_arrow.poly.x2 = left;
    left = 0x58;
    g_attack_deploy_screen_arrow.poly.y2 = value;
    g_attack_deploy_screen_arrow.poly.y3 = value;
    value = 0xB8;
    g_attack_deploy_screen_arrow.poly.x1 = right;
    g_attack_deploy_screen_arrow.poly.x3 = right;
    right = 0x98;
    g_attack_deploy_screen_arrow.poly.v0 = value;
    g_attack_deploy_screen_arrow.poly.v1 = value;
    value = 0xE0;
    g_attack_deploy_screen_arrow.poly.u0 = left;
    g_attack_deploy_screen_arrow.poly.u2 = left;
    left = 0x100;
    g_attack_deploy_screen_arrow.poly.u1 = right;
    g_attack_deploy_screen_arrow.poly.v2 = value;
    g_attack_deploy_screen_arrow.poly.u3 = right;
    g_attack_deploy_screen_arrow.poly.v3 = value;
    g_attack_deploy_screen_arrow.poly.tpage = GetTPage(texture_mode, blend_mode, left, 0);
    g_attack_deploy_screen_arrow.poly.clut
        = GetClut(g_attack_deploy_arrow_clut_rect.x, g_attack_deploy_arrow_clut_rect.y);

    SetPolyF3(&g_attack_deploy_screen_arrow.tri[0]);
    SetSemiTrans(&g_attack_deploy_screen_arrow.tri[0], 0);
    SetShadeTex(&g_attack_deploy_screen_arrow.tri[0], 1);
    g_attack_deploy_screen_arrow.tri[0].r0 = 0xFF;
    g_attack_deploy_screen_arrow.tri[0].g0 = 0xFF;
    g_attack_deploy_screen_arrow.tri[0].b0 = 0xFF;
    if (g_attack_deploy_tiles_only_mode != 0) {
        g_attack_deploy_screen_arrow.tri[0].x0 = 0x99;
        g_attack_deploy_screen_arrow.tri[0].x1 = 0xC0;
        g_attack_deploy_screen_arrow.tri[0].x2 = 0xA2;
        g_attack_deploy_screen_arrow.tri[0].y0 = 0x9E;
        g_attack_deploy_screen_arrow.tri[0].y1 = 0xB1;
        g_attack_deploy_screen_arrow.tri[0].y2 = 0xB0;
    } else {
        g_attack_deploy_screen_arrow.tri[0].x0 = 0xB9;
        g_attack_deploy_screen_arrow.tri[0].x1 = 0xE0;
        g_attack_deploy_screen_arrow.tri[0].x2 = 0xC2;
        g_attack_deploy_screen_arrow.tri[0].y0 = 0xBE;
        g_attack_deploy_screen_arrow.tri[0].y1 = 0xD1;
        g_attack_deploy_screen_arrow.tri[0].y2 = 0xD0;
    }

    SetPolyF3(&g_attack_deploy_screen_arrow.tri[1]);
    SetSemiTrans(&g_attack_deploy_screen_arrow.tri[1], 0);
    SetShadeTex(&g_attack_deploy_screen_arrow.tri[1], 1);
}
