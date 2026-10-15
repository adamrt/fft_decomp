#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Copy a menu entry's window position and text-image UV and size to its sprite. */
void menu_copy_sprite_position_uv_and_size(const menu_entry_t* entry, SPRT* sprite, s32 unused_frame) {
    sprite->x0 = entry->window.x;
    sprite->y0 = entry->window.y;
    sprite->u0 = (entry->vram_origin.x & 0x3f) * 4;
    sprite->v0 = entry->vram_origin.y;
    sprite->w = entry->text_image.width;
    sprite->h = entry->text_image.height;
}
