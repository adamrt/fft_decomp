#include "fft/event_attack.h"
#include "fft/menu.h"

/* Upload the cursor texture and build both deployment cursors' marker and
 * tile-highlight quads; attack_deploy_update_cursor_primitives positions them each frame. */
void attack_deploy_init_cursor_primitives(void) {
    s32 i;
    s32 j;

    LoadImage(&g_attack_deploy_cursor_texture_rect, g_attack_deploy_cursor_texture_data);
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            menu_init_default_poly_ft4(&g_attack_deploy_render_buffers[0].cursor[i][j]);
        }
    }
    for (i = 0; i < 2; i++) {
        POLY_FT4* marker = &g_attack_deploy_render_buffers[0].cursor[i][0];
        POLY_FT4* highlight = &g_attack_deploy_render_buffers[0].cursor[i][1];

        setUV4(marker, 0x40, 0x80, 0x50, 0x80, 0x40, 0x98, 0x50, 0x98);
        setUV4(highlight, 0, 0xa0, 0xe, 0xa0, 0, 0xae, 0xe, 0xae);
        SetSemiTrans(marker, 1);
        SetSemiTrans(highlight, 1);
        g_attack_deploy_render_buffers[0].cursor[i][0].tpage = GetTPage(0, 2, 0x3c0, 0x100);
        g_attack_deploy_render_buffers[0].cursor[i][1].tpage = GetTPage(0, 1, 0x3c0, 0x100);
        g_attack_deploy_render_buffers[0].cursor[i][0].clut = GetClut(0x140, 0xf0);
        g_attack_deploy_render_buffers[0].cursor[i][1].clut = GetClut(0x140, 0xf2);
    }
}
