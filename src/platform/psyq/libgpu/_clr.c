#include "psx/libgpu.h"

/* LIBGPU, retail 0x80025c44-0x80025e5c. Selects fill or tile clearing and restores drawing state. */
int _clr(RECT* rect, u32 color) {
    extern volatile psyq_gpu_environment_t g_psyq_gpu_environment; /* An unqualified RAM view removes native 16-bit
                                                                      normalization in the bounds clamps. */
    limitRange(rect->w, 0, g_psyq_gpu_environment.vram_width - 1);
    limitRange(rect->h, 0, g_psyq_gpu_environment.vram_height - 1);
    if ((rect->x & 63) || (rect->w & 63)) {
        /* A tile under a full draw area and zero offset, then a second packet that restores the three settings. */
        g_psyq_gpu_clear_packet.tile.tag = ((u32)&g_psyq_gpu_clear_restore & PSYQ_GPU_DMA_ADDRESS_MASK)
            | (PSYQ_GPU_PACKET_WORDS(psyq_gpu_clear_tile_packet_t) << 24);
        g_psyq_gpu_clear_packet.tile.draw_area_top_left = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_AREA_TOP_LEFT);
        g_psyq_gpu_clear_packet.tile.draw_area_bottom_right
            = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_AREA_BOTTOM_RIGHT) | PSYQ_GPU_PARAMETER_MASK;
        g_psyq_gpu_clear_packet.tile.draw_offset = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_OFFSET);
        g_psyq_gpu_clear_packet.tile.draw_mask = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_MASK);
        g_psyq_gpu_clear_packet.tile.tile_color
            = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_TILE) | (color & PSYQ_GPU_RGB_MASK);
        /* Whole RECT assignment emits unaligned pairs instead of the native 32-bit GPU transfers. */
        g_psyq_gpu_clear_packet.tile.xy = *(u32*)&rect->x;
        g_psyq_gpu_clear_packet.tile.wh = *(u32*)&rect->w;
        g_psyq_gpu_clear_restore.area.tag
            = (PSYQ_GPU_PACKET_WORDS(psyq_gpu_clear_restore_packet_t) << 24) | PSYQ_GPU_DMA_END;
        g_psyq_gpu_clear_restore.area.code[0]
            = _param(PSYQ_GPU_INFO_DRAW_AREA_TOP_LEFT) | PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_AREA_TOP_LEFT);
        g_psyq_gpu_clear_restore.area.code[1] = _param(PSYQ_GPU_INFO_DRAW_AREA_BOTTOM_RIGHT)
            | PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_AREA_BOTTOM_RIGHT);
        g_psyq_gpu_clear_restore.draw_offset
            = _param(PSYQ_GPU_INFO_DRAW_OFFSET) | PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_OFFSET);
    } else {
        g_psyq_gpu_clear_packet.fill.tag
            = (PSYQ_GPU_PACKET_WORDS(psyq_gpu_clear_fill_packet_t) << 24) | PSYQ_GPU_DMA_END;
        g_psyq_gpu_clear_packet.fill.draw_mask = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_DRAW_MASK);
        g_psyq_gpu_clear_packet.fill.fill_color
            = PSYQ_GPU_COMMAND_WORD(PSYQ_GPU_CODE_BLOCK_FILL) | (color & PSYQ_GPU_RGB_MASK);
        /* Whole RECT assignment emits unaligned pairs instead of the native 32-bit GPU transfers. */
        g_psyq_gpu_clear_packet.fill.xy = *(u32*)&rect->x;
        g_psyq_gpu_clear_packet.fill.wh = *(u32*)&rect->w;
    }
    _cwc((u32*)&g_psyq_gpu_clear_packet);
    return 0;
}
