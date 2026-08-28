#include "fft/wldcore.h"
#include "psx/gpu.h"

void wldcore_window_load_image_record_to_vram(s32 index, u32* image) {
    s32 transformed[2];
    RECT rect;
    /* Unreferenced; reserves the target's 8-byte frame slot. */
    wldcore_window_render_bounds16_t abi_bounds;
    u32* image_data = image;
    s32 offset;
    s32* transformed_output;
    s32 x;
    s32 y;
    u32 packed_x;
    /* Pins required: unpinned, the packed y and mode loads swap v0/v1 and reorder. */
    register u32 packed_y __asm__("$5");
    u32 packed_width;
    u32 packed_height;
    register s32 mode __asm__("$4");
    /* Direct array indexing changes the target prologue schedule. Preserve the
     * byte stride, then use the typed record for every field access. */
#define IMAGE_RECORD (*(wldcore_window_render_record_t*)((u8*)g_wldcore_window_render_records + offset))

    /* Copies image into $s1 at the top of the prologue. */
    __asm__("" : "=r"(image_data) : "0"(image_data));
    offset = index * sizeof(wldcore_window_render_record_t);
    /* Makes the final shift write $s0 directly (no `move s0,v0`), ahead of
     * the &transformed setup. */
    __asm__("" : "=r"(offset) : "0"(offset));
    transformed_output = transformed;
    /* Keeps `addiu a3,sp,16` in the prologue instead of among the field loads. */
    __asm__("" : "=r"(transformed_output) : "0"(transformed_output));
    packed_x = IMAGE_RECORD.x;
    packed_width = (u16)IMAGE_RECORD.width;
    /* Keeps the x and width loads ahead of the y, mode and height loads. */
    __asm__ volatile("");
    packed_y = IMAGE_RECORD.y;
    mode = IMAGE_RECORD.coordinate_mode;
    packed_height = IMAGE_RECORD.height;
    packed_y <<= 16;
    packed_y = packed_x | packed_y;
    packed_height <<= 16;
    /* The bounds argument is a by-value struct, passed here as two packed words in a1/a2. */
    ((void (*)(s32, u32, u32, s32*))wldcore_window_transform_point_for_mode)(
        mode, packed_y, packed_width | packed_height, transformed_output);
    x = transformed[0];
    y = transformed[1];
    rect.x = x;
    rect.y = y;
    rect.w = IMAGE_RECORD.width / 4;
    rect.h = IMAGE_RECORD.height;
    LoadImage(&rect, image_data);
#undef IMAGE_RECORD
}
