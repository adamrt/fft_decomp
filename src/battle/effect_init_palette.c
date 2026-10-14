#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "psx/libgpu.h"

/* Send an effect file's palette and texture section to VRAM. Its two
 * 256-color blocks go to MAP_CLUT_ROW_EFFECT and MAP_CLUT_ROW_EFFECT_SECOND; the
 * texture image that follows them to (0x180, 0).
 *
 * The image is preceded by its byte size as a 24-bit little-endian value and a
 * width flag: nonzero for an image 0x80 16-bit VRAM columns (256 bytes) wide, zero for
 * 0x40 16-bit columns (128 bytes). The height is the size over the row length.
 * g_effect_palette_ptr keeps the section's address. */
void effect_init_palette(s16 index) {
    RECT rect;
    effect_file_header_t* base;
    effect_palette_section_t* data;
    s32 size;
    s32 raw_size;
    s32 height;

    base = g_effect_data_ptrs[index];
    data = (effect_palette_section_t*)((u8*)base + base->palette_and_texture);
    g_effect_palette_ptr = data;
    map_load_palette_data(data->primary_clut, MAP_CLUT_ROW_EFFECT, 0, 1);
    map_load_palette_data(data->secondary_clut, MAP_CLUT_ROW_EFFECT_SECOND, 0, 1);
    raw_size = data->image_size[0] + (data->image_size[1] << 8);
    raw_size = (data->image_size[2] << 16) + raw_size;
    height = raw_size;
    size = height;
    if (data->wide_image != 0) {
        height = size / 0x100;
        rect.x = 0x180;
        rect.y = 0;
        rect.w = 0x80;
        rect.h = height;
        LoadImage(&rect, data->pixels);
    } else {
        height = size;
        height = height / 0x80;
        rect.x = 0x180;
        rect.y = 0;
        rect.w = 0x40;
        rect.h = height;
        LoadImage(&rect, data->pixels);
    }
}
