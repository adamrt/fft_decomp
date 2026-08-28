#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Link a GPU primitive into the secondary ordering table (libgpu addPrim). */
void world_gfx_append_gpu_primitive_to_secondary_otag(u32* prim) {
    u32* entries = g_world_gfx_active_otag_entries;

    setaddr(prim, getaddr(&entries[1]));
    setaddr(&entries[1], prim);
}
