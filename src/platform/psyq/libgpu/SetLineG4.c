/* LIBGPU 80023e6c-80023e8c. */
#include "psx/libgpu.h"

void SetLineG4(LINE_G4* primitive) {
    setlen(primitive, PSYQ_GPU_PACKET_WORDS(LINE_G4));
    setcode(primitive, PSYQ_GPU_CODE_LINE_G4);
    primitive->pad = PSYQ_GPU_POLYLINE_END;
}
