/* A VRAM rectangle copy that waits for the copy to finish. */

#include "psx/libgpu.h"
#include "psx/types.h"

/* Copy a VRAM rectangle to (x, y) in VRAM and wait for the copy to finish. */
void gfx_move_image_and_wait(RECT* rect, s16 x, s16 y) {
    MoveImage(rect, x, y);
    while (DrawSync(1) != 0) { }
}
