#include "fft/world.h"
#include "psx/gpu.h"

/* Copy an image record's first four halfwords (the third scaled down by 4)
 * and set up a menu quad from the record's load/screen/parameter blocks. */
void world_menu_init_quad_from_record(u16* out, u16* record, POLY_FT4* primitive) {
    out[0] = record[0];
    out[1] = record[1];
    out[2] = (s16)record[2] >> 2;
    out[3] = record[3];
    world_menu_init_sprite((SPRT*)primitive);
    primitive->clut = 0x7C3C;
    world_gfx_init_image_loading(primitive, (const world_image_location_t*)record,
        (const world_image_location_t*)(record + 4), (const world_gfx_image_load_parameters_t*)(record + 8));
}
