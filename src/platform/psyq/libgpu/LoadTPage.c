/* LIBGPU 80022c24-80022d10. */
#include "psx/libgpu.h"

u16 LoadTPage(u32* pixels, int tp, int abr, int x, int y, int w, int h) {
    RECT rect;
    rect.x = x;
    rect.h = h;
    rect.y = y;
    switch (tp) {
    case 0:
        rect.w = w / 4;
        break;
    case 1:
        rect.w = w / 2;
        break;
    case 2:
        rect.w = w;
        break;
    }
    LoadImage(&rect, pixels);
    return GetTPage(tp, abr, x, y);
}
