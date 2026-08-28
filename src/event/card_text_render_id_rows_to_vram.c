#include "fft/card.h"
#include "fft/text.h"
#include "psx/gpu.h"
#include "psx/types.h"

void card_text_render_id_rows_to_vram(glyph_data, text_ids, destination, flags) void* glyph_data;
const u16* text_ids;
const RECT* destination;
s32 flags;
{
    u8 image[0x800];
    card_text_image_bounds_t bounds;
    RECT upload_rect;
    u16 row_text_ids[2];
    u16 text_id;
    s32 terminator;
    void* saved_glyph_data;

    bounds.x = 0;
    bounds.y = 0;
    bounds.row_stride = destination->w;
    bounds.line_height = 0x10;

    upload_rect.x = destination->x;
    upload_rect.y = destination->y;
    upload_rect.w = destination->w;
    upload_rect.h = 0x10;

    text_id = *text_ids;
    if ((s16)*text_ids == -1) {
        return;
    }
    saved_glyph_data = glyph_data;
    terminator = -1;
    do {
        row_text_ids[0] = text_id;
        row_text_ids[1] = terminator;
        memset(image, 0, sizeof(image));
        card_text_render_encoded_ids_to_image(
            image, &bounds, 0x0a, 0x64, saved_glyph_data, row_text_ids, 0x64, TEXT_FORMAT_EXPLICIT_ID, flags);
        card_gfx_load_image_and_wait(&upload_rect, (u32*)image);
        text_ids++;
        upload_rect.y += 0x10;
        text_id = *text_ids;
    } while ((s16)*text_ids != terminator);
}
