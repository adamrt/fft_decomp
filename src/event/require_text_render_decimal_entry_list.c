#include "fft/battle_text.h"
#include "fft/require.h"
#include "psx/types.h"

/* Render gauge values with an explicit sign, colored by sign, and a zero placeholder. */
void require_text_render_decimal_entry_list(s32 pixels, battle_menu_status_panel_gauge_entry_t* entries,
    battle_menu_status_panel_text_position_t* output, s32 count) {
    s32 i;

    for (i = 0; i < count; entries++, i++, output->color = 0) {
        s32 value = *entries->value;
        s32 flags;
        output->x = entries->x;
        output->y = entries->y;
        flags = entries->flags;
        flags &= MENU_DECIMAL_SIGN_CLEAR_MASK;
        if (value < 0) {
            value = -value;
            flags |= MENU_DECIMAL_SIGN_MINUS;
            output->color = 0x77777777;
        } else if (value > 0) {
            flags |= MENU_DECIMAL_SIGN_PLUS;
            output->color = 0xbbbbbbbb;
        } else {
            output->color = 0;
            flags &= MENU_DECIMAL_FIELD_WIDTH_CLEAR_MASK;
            flags |= MENU_DECIMAL_ZERO_PLACEHOLDER_FLAGS;
        }
        require_text_render_decimal_value(value, flags, (void*)pixels, output);
    }
}
