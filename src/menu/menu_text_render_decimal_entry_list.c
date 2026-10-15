#include "fft/menu.h"

/* Render gauge values with sign colors, drawing a zero value as a placeholder. */
void menu_text_render_decimal_entry_list(u8* pixels, const menu_number_entry_t* entries, glyph_blit_t* out, s32 count) {
    s32 i;

    for (i = 0; i < count; entries++, i++, out->color = 0) {
        s32 amount = *entries->value;
        s32 flags;
        out->rect.x = entries->x;
        out->rect.y = entries->y;
        flags = entries->format;
        flags &= MENU_DECIMAL_SIGN_CLEAR_MASK;
        if (amount < 0) {
            amount = -amount;
            flags |= MENU_DECIMAL_SIGN_MINUS;
            out->color = 0x77777777;
        } else if (amount > 0) {
            flags |= MENU_DECIMAL_SIGN_PLUS;
            out->color = 0xbbbbbbbb;
        } else {
            out->color = 0;
            flags &= MENU_DECIMAL_FIELD_WIDTH_CLEAR_MASK;
            flags |= MENU_DECIMAL_ZERO_PLACEHOLDER_FLAGS;
        }
        menu_text_render_decimal_value(amount, flags, pixels, out);
    }
}
