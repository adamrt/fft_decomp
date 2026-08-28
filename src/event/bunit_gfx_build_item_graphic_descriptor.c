#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Provisional result layout returned through battle_get_item_graphic_data's output pointer. */
typedef struct bunit_item_graphic_result {
    u8 pad[12];
    u8 u0;
    u8 v0;
    u16 clut;
    u8 tail[8];
} bunit_item_graphic_result_t;

/* Fill a 16x16 item icon descriptor: VRAM u/v, size, CLUT and texture page. */
void bunit_gfx_build_item_graphic_descriptor(s16* descriptor) {
    bunit_item_graphic_result_t graphic;

    /* 0x80136b10 is battle_get_item_graphic_data(SPRT*, s32) in BATTLE;
     * this caller leaves the item id register unset. */
    ((void (*)(bunit_item_graphic_result_t*))battle_get_item_graphic_data)(&graphic);
    descriptor[0] = graphic.u0;
    descriptor[1] = graphic.v0;
    descriptor[2] = 0x10;
    descriptor[3] = 0x10;
    descriptor[4] = graphic.clut;
    descriptor[5] = GetTPage(0, 0, 0x380, 0x120);
}
