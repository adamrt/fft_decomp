#include "psx/libetc.h"

s32 SetVideoMode(s32 mode) {
    s32 previous = g_psyq_etc_video_mode;
    g_psyq_etc_video_mode = mode;
    return previous;
}
