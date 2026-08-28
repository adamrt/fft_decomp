#include "fft/battle.h"
#include "fft/battle_text.h"
#include "psx/types.h"

/* Draw each entry of a menu number table at its own position, dispatching on
 * the entry kind. */
void battle_menu_draw_numeric_display_entries(
    s32 buffer, menu_number_entry_t* entries, menu_number_position_t* position, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        position->x = entries->x;
        position->y = entries->y;
        if (entries->kind == 0) {
            battle_text_draw_large_number_glyphs(
                *entries->value, entries->digits, (void*)buffer, (battle_rect_t*)position);
        } else if (entries->kind == 1) {
            battle_draw_menu_number_glyphs(*entries->value, entries->digits, (void*)buffer, (battle_rect_t*)position);
        }
        entries++;
    }
}
