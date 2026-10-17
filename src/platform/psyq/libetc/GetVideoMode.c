#include "psx/libetc.h"

/* LIBETC 0x8001e884-0x8001e894: cached video mode. */
s32 GetVideoMode(void) {
    return g_psyq_etc_video_mode;
}
