#include "fft/main_runtime.h"
#include "psx/types.h"

/* The three VRAM uploads read consecutive regions of FRAME.BIN after its
 * 0x1000-byte prefix. Keep the regions word-aligned for LoadImage. */
typedef struct main_gfx_frame_bin {
    u8 unknown_0000[0x1000];
    u32 image[0x8000 / sizeof(u32)];
    u32 palette[0x200 / sizeof(u32)];
    u32 palette_tail[0x600 / sizeof(u32)];
} main_gfx_frame_bin_t;

void main_gfx_load_frame_bin_into_vram(void) {
    main_gfx_frame_bin_t* frame = main_file_get_bin_as_tim(0xe68, 0x9800);

    LoadImage(&g_main_gfx_frame_image_rect, frame->image);
    LoadImage(&g_main_gfx_frame_palette_rect, frame->palette);
    LoadImage(&g_main_gfx_frame_palette_tail_rect, frame->palette_tail);
    DrawSync(0);
    main_heap_call_free(frame);
}
