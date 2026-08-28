#include "psx/types.h"

/* Submit the twelve glyph packets and three framing packets. */
void battle_menu_submit_numeric_display_frame_primitives(u8* display) {
    s32 index;
    s32 offset;

    index = 0;
    offset = 0x18;
    do {
        battle_gfx_draw_or_append_gpu_primitive(display + offset);
        index++;
        offset += 0x10;
    } while (index < 12);
    battle_gfx_draw_or_append_gpu_primitive(display);
    battle_gfx_draw_or_append_gpu_primitive(display + 0xd8);
    battle_gfx_draw_or_append_gpu_primitive(display + 0x0c);
}
