#include "fft/event_attack.h"

/* Build the textured deployment banner and its two arrowhead triangles.
 * The banner fields are written in libgpu setRGB0/setXY4/setUV4 order. */
void attack_deploy_build_screen_arrow(void) {
    SetPolyFT4(&g_attack_deploy_screen_arrow.poly);
    SetSemiTrans(&g_attack_deploy_screen_arrow.poly, 1);
    SetShadeTex(&g_attack_deploy_screen_arrow.poly, 0);
    setRGB0(&g_attack_deploy_screen_arrow.poly, 0x80, 0x80, 0x80);
    setXY4(&g_attack_deploy_screen_arrow.poly, 0xAF, 0xB6, 0xEF, 0xB6, 0xAF, 0xDE, 0xEF, 0xDE);
    setUV4(&g_attack_deploy_screen_arrow.poly, 0x58, 0xB8, 0x98, 0xB8, 0x58, 0xE0, 0x98, 0xE0);
    g_attack_deploy_screen_arrow.poly.tpage = GetTPage(0, 1, 0x100, 0);
    g_attack_deploy_screen_arrow.poly.clut
        = GetClut(g_attack_deploy_arrow_clut_rect.x, g_attack_deploy_arrow_clut_rect.y);

    SetPolyF3(&g_attack_deploy_screen_arrow.tri[0]);
    SetSemiTrans(&g_attack_deploy_screen_arrow.tri[0], 0);
    SetShadeTex(&g_attack_deploy_screen_arrow.tri[0], 1);
    setRGB0(&g_attack_deploy_screen_arrow.tri[0], 0xFF, 0xFF, 0xFF);
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
