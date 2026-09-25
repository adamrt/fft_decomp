#include "fft/menu_types.h"
#include "psx/types.h"

/* Submit the twelve glyph packets and three framing packets. */
void battle_menu_submit_numeric_display_frame_primitives(world_menu_palette_primitives_t* display) {
    s32 index;

    index = 0;
    do {
        battle_gfx_draw_or_append_gpu_primitive(&display->lines[index]);
        index++;
    } while (index < 12);
    battle_gfx_draw_or_append_gpu_primitive(&display->draw_mode);
    battle_gfx_draw_or_append_gpu_primitive(&display->sprite);
    battle_gfx_draw_or_append_gpu_primitive(&display->draw_mode_menu);
}
