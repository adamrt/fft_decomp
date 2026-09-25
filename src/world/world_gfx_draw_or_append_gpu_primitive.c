#include "fft/world.h"
#include "psx/gpu.h"

/* Merge the caller's display word with the active display word. */
void world_gfx_draw_or_append_gpu_primitive(void* primitive) {
    s32* word = primitive;
    s32* ptr;
    s32 value;
    s32 active;
    if (g_world_gfx_draw_primitives_immediately != 0) {
        DrawPrim(word);
        return;
    }
    ptr = (s32*)g_world_gfx_active_otag_entries;
    value = *word;
    active = *ptr;
    *word = (value & 0xff000000) | (active & 0x00ffffff);
    *ptr = ((*ptr) & 0xff000000) | (((s32)word) & 0x00ffffff);
}
