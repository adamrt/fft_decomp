#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Shares battle_menu_copy_cursor_frame_data's (source, frame, step) shape:
 * scales a cursor SPRT to g_battle_menu_zoom_percentages[step] percent about its centre, keeping
 * position and texture coordinates even. Writing the half extent first in
 * each sum (`w / 2 + x0`) is required; the reverse order reschedules the
 * reloads and adds a copy of `step`.
 */

void battle_menu_zoom_cursor_frame(const void* source, SPRT* sprite, s32 step) {
    s32 scale;
    s32 offset;
    if (step < 12) {
        scale = g_battle_menu_zoom_percentages[step];
        offset = sprite->w * scale / 200;
        sprite->x0 = (sprite->w / 2) + sprite->x0 - offset;
        sprite->u0 = (sprite->w / 2) + sprite->u0 - offset;
        sprite->x0 &= ~1;
        sprite->u0 &= ~1;
        offset = sprite->h * scale / 200;
        sprite->y0 = sprite->h / 2 + sprite->y0 - offset;
        sprite->w = sprite->w * scale / 100;
        sprite->v0 = sprite->h / 2 + sprite->v0 - offset;
        sprite->y0 &= ~1;
        sprite->v0 &= ~1;
        sprite->h = sprite->h * scale / 100;
        sprite->w &= 0xfe;
        sprite->h &= 0xfe;
    }
}
