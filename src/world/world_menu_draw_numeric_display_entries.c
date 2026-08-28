#include "fft/world.h"
#include "psx/types.h"

/* Draw positioned menu-number entries using the renderer selected by type. */
void world_menu_draw_numeric_display_entries(
    s32 arg, world_menu_number_entry_t* entries, world_glyph_blit_t* position, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        position->rect.x = entries->x;
        position->rect.y = entries->y;
        if (entries->type == 0) {
            world_text_render_decimal_value_or_dashes(*entries->value, entries->param, (void*)arg, position);
        } else if (entries->type == 1) {
            world_draw_menu_number_glyphs(*entries->value, entries->param, (void*)arg, position);
        }
        entries++;
    }
}
