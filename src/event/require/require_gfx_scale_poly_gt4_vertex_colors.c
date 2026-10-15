#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/event_helpmenu.h"
#include "fft/event_require.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Scale each vertex color by scale / 128. */
void require_gfx_scale_poly_gt4_vertex_colors(
    const require_display_condition_colors_t* colors, s32 scale, POLY_GT4* poly) {
    s32 r;
    s32 g;
    s32 b;

    r = colors->vertices[0].r * scale / 128;
    g = colors->vertices[0].g * scale / 128;
    b = colors->vertices[0].b * scale / 128;
    setRGB0(poly, r, g, b);
    r = colors->vertices[1].r * scale / 128;
    g = colors->vertices[1].g * scale / 128;
    b = colors->vertices[1].b * scale / 128;
    setRGB1(poly, r, g, b);
    r = colors->vertices[2].r * scale / 128;
    g = colors->vertices[2].g * scale / 128;
    b = colors->vertices[2].b * scale / 128;
    setRGB2(poly, r, g, b);
    r = colors->vertices[3].r * scale / 128;
    g = colors->vertices[3].g * scale / 128;
    b = colors->vertices[3].b * scale / 128;
    setRGB3(poly, r, g, b);
}
