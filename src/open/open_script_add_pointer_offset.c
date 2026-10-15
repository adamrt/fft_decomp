#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

u8* open_script_add_pointer_offset(u8* base, s32 byte_offset) {
    return base + byte_offset;
}
