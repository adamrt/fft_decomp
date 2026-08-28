#include "fft/battle_gfx.h"
#include "psx/gpu.h"
#include "psx/types.h"

void debugchr_gfx_build_gradient_grid_primitives(POLY_GT4* poly) {
    POLY_GT4* packet = poly;
    s32 row = 0;
    s32 bottom = 0x20;
    s32 lower_offset = 8;

    do {
        s32 column = 0;
        s32 top = row << 5;
        s32 right = 0xc0;
        s32 left = 0x80;
        /* The $16 byte cursor lets the 0xd8 constant reuse the GetTPage result
         * register; unpinned, or with POLY_GT4 fields, it is loaded into a fresh v0/v1. */
        register u8* fields __asm__("$16") = (u8*)packet + 0x31;

        do {
            s32 intensity;
            register s32 uv_value __asm__("$8");
            register u16 v_value __asm__("$2");
            register POLY_GT4* semi_packet __asm__("$4");
            register s32 semi_enable __asm__("$5");

            SetPolyGT4(packet);
            *(u16*)(fields - 0x23) = 0x7fbc;
            v_value = GetTPage(0, 0, 0x3c0, 0x100);

            intensity = row * 8 + 0x40;
            fields[-0x2d] = intensity;
            fields[-0x2c] = intensity;
            fields[-0x2b] = intensity;
            fields[-0x21] = intensity;
            fields[-0x20] = intensity;
            fields[-0x1f] = intensity;
            intensity = lower_offset + 0x40;
            semi_packet = packet;
            semi_enable = 1;
            /* Keeps `li a1,1` beside the semi_packet move instead of sinking past the UV stores. */
            __asm__ volatile("" : : "r"(semi_enable));

            uv_value = 0x58;
            *(u16*)(fields - 0x17) = v_value;
            fields[-0x25] = uv_value;
            v_value = 0xd8;
            fields[-0x24] = v_value;
            uv_value = 0x98;
            fields[-0x19] = uv_value;
            fields[-0x18] = v_value;
            uv_value = 0x58;
            fields[-0xd] = uv_value;
            fields[-0x15] = intensity;
            fields[-0x14] = intensity;
            fields[-0x13] = intensity;
            fields[-9] = intensity;
            fields[-8] = intensity;
            fields[-7] = intensity;
            v_value = 0xf8;
            uv_value = 0x98;

            *(s16*)(fields - 0x29) = left;
            *(s16*)(fields - 0x27) = top;
            *(s16*)(fields - 0x1d) = right;
            *(s16*)(fields - 0x1b) = top;
            *(s16*)(fields - 0x11) = left;
            *(s16*)(fields - 0xf) = bottom;
            *(s16*)(fields - 5) = right;
            *(s16*)(fields - 3) = bottom;
            fields[-0xc] = v_value;
            fields[-1] = uv_value;
            fields[0] = v_value;

            SetSemiTrans(semi_packet, semi_enable);
            SetShadeTex(packet, 0);
            battle_gfx_draw_or_append_gpu_primitive(packet);

            fields += sizeof(POLY_GT4);
            packet++;
            right += 0x40;
            column++;
            left += 0x40;
        } while (column < 4);

        bottom += 0x20;
        row++;
        lower_offset += 8;
    } while (row < 8);
}
