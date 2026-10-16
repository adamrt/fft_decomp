/* LIBGPU 80025a04-80025a88. */
#include "psx/libgpu.h"

/* Build the GP0 texture-window command from rect in 8-pixel units (the mask comes from the negated
 * width and height); 0 for no rect. */
u32 get_tw(RECT* rect) {
    /* Scalar locals remove the native stack stores and reloads of these command components. */
    struct {
        u32 x_offset;
        u32 y_offset;
        u32 x_mask;
        u32 y_mask;
    } window;
    if (rect) {
        window.x_offset = (rect->x & 0xff) >> 3;
        window.x_mask = (-rect->w & 0xff) >> 3;
        window.y_offset = (rect->y & 0xff) >> 3;
        window.y_mask = (-rect->h & 0xff) >> 3;
        return PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_TEXTURE_WINDOW) | (window.y_offset << 15) | (window.x_offset << 10)
            | (window.y_mask << 5) | window.x_mask;
    }
    return 0;
}
