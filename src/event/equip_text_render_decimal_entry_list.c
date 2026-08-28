#include "fft/battle_text.h"
#include "fft/equip.h"
#include "psx/types.h"

/* Render gauge values with sign colors, drawing a zero value as a placeholder. */
void equip_text_render_decimal_entry_list(
    s32 pixels, status_panel_gauge_entry_t* entries, status_panel_text_position_t* out, s32 count) {
    s32 i;

    for (i = 0; i < count; entries++, i++, out->color = 0) {
        s32 amount = *entries->value;
        s32 flags;
        out->x = entries->x;
        out->y = entries->y;
        flags = entries->flags;
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
        equip_text_render_decimal_value(amount, flags, (void*)pixels, out);
    }
}
