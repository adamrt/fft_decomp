#include "fft/world.h"
#include "psx/types.h"

void world_menu_size_entry_to_text(world_menu_entry_t* entry, s16* width, s16* height, s32* out_pad, s32 pad_short) {
    const u8* text;
    s32 padded;
    u16 w;
    u16 h;

    text = world_text_find_entry(entry->text_id);
    *out_pad = 0;
    world_text_measure_pixels(width, height, text);
    if (pad_short != 0) {
        if (*width < 8 && *height == 1) {
            *out_pad = (8 - *width) * 5;
            *width = 8;
        }
    }
    padded = (u16)*width + 0x18;
    *width = padded + (padded & 3);
    *height = *height * 16 + 16;
    entry->window_x = 0x102 - (*width >> 1);
    w = (u16)*width;
    entry->overall_width = w;
    entry->inner_width = w;
    entry->window_width = w;
    h = (u16)*height;
    entry->overall_height = h;
    entry->inner_height = h;
    entry->window_height = h;
}
