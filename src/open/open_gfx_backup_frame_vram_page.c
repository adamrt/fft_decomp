#include "psx/gpu.h"

void open_gfx_backup_frame_vram_page(void) {
    RECT source;

    source.x = 0x3c0;
    source.y = 0;
    source.w = 0x40;
    source.h = 0x100;
    MoveImage(&source, 0x3c0, 0x100);
    DrawSync(0);
}
