#include "fft/event_attack.h"

/* Upload the cursor texture and build both deployment cursors' marker and
 * tile-highlight quads; attack_update_deployment_cursor_primitives positions them each frame. */
void attack_init_deployment_cursor_primitives(void) {
    s32 i;
    s32 j;

    LoadImage(&attack_D_801CD0A4, g_attack_deploy_cursor_texture_data);
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            battle_gfx_init_default_poly_ft4(&g_attack_deploy_render_buffers[0].cursor[i][j]);
        }
    }
    for (i = 0; i < 2; i++) {
        POLY_FT4* marker = &g_attack_deploy_render_buffers[0].cursor[i][0];
        POLY_FT4* highlight = &g_attack_deploy_render_buffers[0].cursor[i][1];

        marker->u0 = 0x40;
        marker->v0 = 0x80;
        marker->u1 = 0x50;
        marker->v1 = 0x80;
        marker->u2 = 0x40;
        marker->v2 = 0x98;
        marker->u3 = 0x50;
        marker->v3 = 0x98;
        highlight->u0 = 0;
        highlight->v0 = 0xa0;
        highlight->u1 = 0xe;
        highlight->v1 = 0xa0;
        highlight->u2 = 0;
        highlight->v2 = 0xae;
        highlight->u3 = 0xe;
        highlight->v3 = 0xae;
        SetSemiTrans(marker, 1);
        SetSemiTrans(highlight, 1);
        g_attack_deploy_render_buffers[0].cursor[i][0].tpage = GetTPage(0, 2, 0x3c0, 0x100);
        g_attack_deploy_render_buffers[0].cursor[i][1].tpage = GetTPage(0, 1, 0x3c0, 0x100);
        g_attack_deploy_render_buffers[0].cursor[i][0].clut = GetClut(0x140, 0xf0);
        g_attack_deploy_render_buffers[0].cursor[i][1].clut = GetClut(0x140, 0xf2);
    }
}
