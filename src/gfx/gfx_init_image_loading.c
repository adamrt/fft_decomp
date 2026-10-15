#include "fft/gfx.h"
#include "psx/libgpu.h"

/* Place a SPRT or POLY_FT4 from texture and screen parameters. */
void gfx_init_image_loading(
    void* primitive, const point16_t* base_load, const RECT* base_screen, const image_load_parameters_t* parameters) {
    int texture_column;
    SPRT* sprite = primitive;
    POLY_FT4* quad = primitive;

    if ((getcode(primitive) & 0x7c) == 0x64) {
        if (parameters->x_load >= 0) {
            texture_column = base_load->x & 0x3f;
            sprite->u0 = (texture_column << 2) + parameters->x_load;
            sprite->v0 = base_load->y + parameters->y_load;
            sprite->w = parameters->width;
            sprite->h = parameters->height;
        }
        sprite->x0 = base_screen->x + parameters->x_screen_offset;
        sprite->y0 = base_screen->y + parameters->y_screen_offset;
        return;
    }

    if (parameters->x_load >= 0) {
        quad->tpage = GetTPage(0, 0, base_load->x, base_load->y & 0xf00);
        setUV4(quad, parameters->x_load, base_load->y + parameters->y_load, parameters->x_load + parameters->width,
            base_load->y + parameters->y_load, parameters->x_load,
            base_load->y + parameters->y_load + parameters->height, parameters->x_load + parameters->width,
            base_load->y + parameters->y_load + parameters->height);
    }

    setXY4(quad, base_screen->x + parameters->x_screen_offset, base_screen->y + parameters->y_screen_offset,
        base_screen->x + parameters->x_screen_offset + parameters->width, base_screen->y + parameters->y_screen_offset,
        base_screen->x + parameters->x_screen_offset, base_screen->y + parameters->y_screen_offset + parameters->height,
        base_screen->x + parameters->x_screen_offset + parameters->width,
        base_screen->y + parameters->y_screen_offset + parameters->height);
}
