#include "fft/event_attack.h"
#include "psx/types.h"

/* Render signed gauge values with their positive, negative, or zero color. */
void attack_text_render_signed_decimal_entries(s32 pixels, battle_menu_status_panel_gauge_entry_t* entries,
    battle_menu_status_panel_text_position_t* out, s32 count) {
    s32 i;
    s32 amount;

    for (i = 0; i < count; entries++, i++) {
        amount = *entries->value;
        out->x = entries->x;
        out->y = entries->y;
        entries->flags &= MENU_DECIMAL_SIGN_CLEAR_MASK;
        if (amount < 0) {
            out->color = 0x88888888;
            amount = -amount;
            entries->flags |= MENU_DECIMAL_SIGN_MINUS;
        } else if (amount > 0) {
            out->color = 0xcccccccc;
            entries->flags |= MENU_DECIMAL_SIGN_PLUS;
        } else {
            out->color = 0;
            entries->flags |= MENU_DECIMAL_SIGN_MINUS;
        }
        attack_text_render_decimal_value(amount, entries->flags, (void*)pixels, out);
        out->color = 0;
    }
}
