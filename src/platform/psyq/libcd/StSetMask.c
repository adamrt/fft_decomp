/* SCUS_942.21 0x800212cc..0x800212eb. */
#include "psx/libcd.h"

void StSetMask(u32 mask, u32 start_frame, u32 end_frame) {
    g_psyq_cd_stream_start_mask = mask;
    g_psyq_cd_stream_start_frame = start_frame;
    g_psyq_cd_stream_end_frame = end_frame;
}
