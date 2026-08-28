#include "fft/battle.h"
#include "psx/gpu.h"

/* Link a GPU primitive into the current ordering table entry (libgpu addPrim). */
void battle_gfx_append_gpu_primitive_to_secondary_otag(u32* prim) {
    u32* entries = g_current_otag_entry;

    setaddr(prim, getaddr(&entries[1]));
    setaddr(&entries[1], prim);
}
