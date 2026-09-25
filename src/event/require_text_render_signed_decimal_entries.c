#include "fft/battle_text.h"
#include "fft/require.h"
#include "psx/types.h"

/* Render signed gauge values with their positive, negative, or zero color. */
void require_text_render_signed_decimal_entries(s32 pixels, battle_menu_status_panel_gauge_entry_t* entries,
    battle_menu_status_panel_text_position_t* output, s32 count) {
    s32 i;

    for (i = 0; i < count;) {
        s32 value = *entries->value;
        s32 flags;
        output->x = entries->x;
        output->y = entries->y;
        entries->flags &= MENU_DECIMAL_SIGN_CLEAR_MASK;
        if (value < 0) {
            output->color = 0x88888888;
            value = -value;
            entries->flags |= MENU_DECIMAL_SIGN_MINUS;
        } else if (value > 0) {
            output->color = 0xcccccccc;
            entries->flags |= MENU_DECIMAL_SIGN_PLUS;
        } else {
            output->color = 0;
            entries->flags |= MENU_DECIMAL_SIGN_MINUS;
        }
        flags = entries->flags;
        entries++;
        i++;
        require_text_render_decimal_value(value, flags, (void*)pixels, output);
        output->color = 0;
    }
}
