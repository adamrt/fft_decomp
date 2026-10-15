#include "fft/menu.h"
#include "psx/types.h"

/* Size a message window to its text and center it on x = 0x102. */
void menu_size_entry_to_text(menu_entry_t* entry, s16* width, s16* height, s32* out_pad, s32 pad_short) {
    const u8* text;
    u16 padded;
    u16 w;
    u16 h;

    text = text_find_entry(entry->text_id);
    *out_pad = 0;
    text_measure_pixels(width, height, text);
    if (pad_short != 0) {
        if (*width < 8 && *height == 1) {
            *out_pad = (8 - *width) * 5;
            *width = 8;
        }
    }
    padded = *width + 0x18;
    *width = padded + (padded & 3);
    *height = *height * 16 + 16;
    entry->window.x = 0x102 - (*width >> 1);
    w = *width;
    entry->text_image.width = w;
    entry->inner_width = w;
    entry->window.w = w;
    h = *height;
    entry->text_image.height = h;
    entry->inner_height = h;
    entry->window.h = h;
}
