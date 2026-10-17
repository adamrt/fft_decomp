#include "psx/libetc.h"

void PadInit(int mode) {
    g_psyq_etc_pad_mode = mode;
    g_psyq_etc_pad_buttons = -1;
    ResetCallback();
    PAD_init2(0x20000001, &g_psyq_etc_pad_buttons);
    ChangeClearPAD(0);
}
