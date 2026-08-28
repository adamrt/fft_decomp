#include "fft/wldcore.h"
#include "psx/types.h"

/* Copies a packed 24-bit RGB triple, applying the global per-channel tint
 * when bit 0 of the fade state word is set. Each channel is scaled by
 * ((c << 16) / 128) * factor >> 16, i.e. c * factor / 128, and clamped to
 * 255 with no lower clamp; with the factors at their 0x80 default the scale
 * is the identity. When the bit is clear the triple is copied verbatim,
 * which the 4-byte alignment-1 struct assignment emits as the unaligned
 * lwl/lwr + swl/swr quartet. */
void wldcore_gfx_copy_color_with_tint(CVECTOR* source, CVECTOR* out) {
    u32* state = (u32*)&g_wldcore_screen_fade_state;
    s32 red;
    s32 green;
    /* Pin: unpinned, blue coalesces into prod's register. */
    register s32 blue __asm__("$3");
    /* Pin: keeps the target's `mflo t0` copy, which sinks into the clamp's delay slot. */
    register s32 prod __asm__("$8");
    /* Pin: keeps the target's v0/v1 roles for the shift temp and the factor. */
    register s32 q __asm__("$2");

    if (*state & 1) {
        q = source->r << 16;
        /* Hides the non-negative range so the red scale keeps its `sra`, not `srl`. */
        __asm__("" : "=r"(q) : "0"(q));
        q = q / 128;
        prod = q * g_wldcore_screen_brightness_rgb[0];
        red = prod >> 16;
        q = source->g << 16;
        q = q / 128;
        prod = q * g_wldcore_screen_brightness_rgb[1];
        green = prod >> 16;
        q = source->b << 16;
        q = q / 128;
        prod = q * g_wldcore_screen_brightness_rgb[2];
        blue = prod >> 16;
        if (red >= 256) {
            red = 255;
        }
        if (green >= 256) {
            green = 255;
        }
        if (blue >= 256) {
            blue = 255;
        }
        /* Keeps reorg from filling the blue clamp's delay slot with the first store. */
        __asm__ volatile("");
        out->r = red;
        out->g = green;
        out->b = blue;
    } else {
        *out = *source;
    }
}
