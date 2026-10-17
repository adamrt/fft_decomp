#include "psx/libetc.h"

u32 PadRead(s32 id) {
    PAD_dr();
    return ~g_psyq_etc_pad_buttons;
}
