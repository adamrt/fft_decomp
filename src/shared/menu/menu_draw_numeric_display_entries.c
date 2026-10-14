#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/menu.h"

void menu_draw_numeric_display_entries(u8* buffer, menu_number_entry_t* entries, glyph_blit_t* position, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        position->rect.x = entries->x;
        position->rect.y = entries->y;
        if (entries->kind == 0) {
            text_draw_large_number_glyphs(*entries->value, entries->format, buffer, position);
        } else if (entries->kind == 1) {
            menu_draw_number_glyphs(*entries->value, entries->format, buffer, position);
        }
        entries++;
    }
}
