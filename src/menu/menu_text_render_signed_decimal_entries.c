#include "fft/menu.h"

/* Set each entry's sign flags and draw its value with the corresponding color. */
void menu_text_render_signed_decimal_entries(u8* pixels, menu_number_entry_t* entries, glyph_blit_t* out, s32 count) {
    s32 i;
    s32 amount;

    for (i = 0; i < count; entries++, i++) {
        amount = *entries->value;
        out->rect.x = entries->x;
        out->rect.y = entries->y;
        entries->format &= MENU_DECIMAL_SIGN_CLEAR_MASK;
        if (amount < 0) {
            out->color = 0x88888888;
            amount = -amount;
            entries->format |= MENU_DECIMAL_SIGN_MINUS;
        } else if (amount > 0) {
            out->color = 0xcccccccc;
            entries->format |= MENU_DECIMAL_SIGN_PLUS;
        } else {
            out->color = 0;
            entries->format |= MENU_DECIMAL_SIGN_MINUS;
        }
        menu_text_render_decimal_value(amount, entries->format, pixels, out);
        out->color = 0;
    }
}
