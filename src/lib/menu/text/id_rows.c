/* Rendering a list of text ids as 16-pixel rows and uploading them to VRAM. */

#include "fft/gfx_image.h"
#include "fft/text.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Flagged rows draw eight filler glyphs: bars in Unit List/Status, dots in Card/Equip. */
#if defined(FFT_EVENT_BUNIT) || defined(FFT_EVENT_JOBSTTS)
enum { TEXT_ID_ROW_FILL_GLYPH = 0xe7 };
#else
enum { TEXT_ID_ROW_FILL_GLYPH = 0xeb };
#endif

/* Render each id of a -1 terminated text-id list as one 16-pixel row and
 * upload the rows to VRAM from the top of `destination` down. */
void text_id_rows_to_vram_render(const u8* text_table, const s16* text_ids, const RECT* destination, s32 flags) {
    u8 image[0x800];
    text_image_bounds_t bounds;
    RECT upload_rect;
    s16 row_text_ids[2];

    bounds.x = 0;
    bounds.y = 0;
    bounds.row_stride = destination->w;
    bounds.line_height = 0x10;
    upload_rect.x = destination->x;
    upload_rect.y = destination->y;
    upload_rect.w = destination->w;
    upload_rect.h = 0x10;

    while (*text_ids != -1) {
        row_text_ids[0] = *text_ids;
        row_text_ids[1] = -1;
        memset(image, 0, 0x800);
        menu_text_render_encoded_ids_to_image(
            image, &bounds, 0xA, 0x64, text_table, row_text_ids, 0x64, TEXT_ID_ROW_FILL_GLYPH, flags);
        gfx_load_image_and_wait(&upload_rect, image);
        upload_rect.y += 0x10;
        text_ids++;
    }
}
