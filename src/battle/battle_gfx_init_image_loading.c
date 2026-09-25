#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Battle twin of world_gfx_init_image_loading: fill a textured quad from load and screen placement parameters. */
void battle_gfx_init_image_loading(POLY_FT4* primitive, const battle_image_location_t* base_load,
    const battle_image_location_t* base_screen, const world_gfx_image_load_parameters_t* parameters) {
    int texture_column;

    if ((primitive->code & 0x7c) == 0x64) {
        if (parameters->x_load >= 0) {
            texture_column = (u16)base_load->x & 0x3f;
            primitive->u0 = (u8)((texture_column << 2) + (u8)parameters->x_load);
            primitive->v0 = (u8)(base_load->y + parameters->y_load);
            primitive->x1 = (s16)parameters->width;
            primitive->y1 = (s16)parameters->height;
        }
        primitive->x0 = (s16)(base_screen->x + parameters->x_screen_offset);
        primitive->y0 = (s16)(base_screen->y + parameters->y_screen_offset);
        return;
    }

    if (parameters->x_load >= 0) {
        primitive->tpage = (u16)GetTPage(0, 0, base_load->x, base_load->y & 0xf00);
        primitive->u0 = (u8)parameters->x_load;
        primitive->v0 = (u8)(base_load->y + parameters->y_load);
        primitive->u1 = (u8)(parameters->x_load + parameters->width);
        primitive->v1 = (u8)(base_load->y + parameters->y_load);
        primitive->u2 = (u8)parameters->x_load;
        primitive->v2 = (u8)(base_load->y + parameters->y_load + parameters->height);
        primitive->u3 = (u8)(parameters->x_load + parameters->width);
        primitive->v3 = (u8)(base_load->y + parameters->y_load + parameters->height);
    }

    primitive->x0 = (s16)(base_screen->x + parameters->x_screen_offset);
    primitive->y0 = (s16)(base_screen->y + parameters->y_screen_offset);
    primitive->x1 = (s16)(base_screen->x + parameters->x_screen_offset + parameters->width);
    primitive->y1 = (s16)(base_screen->y + parameters->y_screen_offset);
    primitive->x2 = (s16)(base_screen->x + parameters->x_screen_offset);
    primitive->y2 = (s16)(base_screen->y + parameters->y_screen_offset + parameters->height);
    primitive->x3 = (s16)(base_screen->x + parameters->x_screen_offset + parameters->width);
    primitive->y3 = (s16)(base_screen->y + parameters->y_screen_offset + parameters->height);
}
