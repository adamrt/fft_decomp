#include "psx/gpu.h"
#include "psx/types.h"

void world_gfx_move_image_and_wait(RECT* rect, s16 x, s16 y) {
    MoveImage(rect, x, y);
    while (DrawSync(1) != 0) { }
}
