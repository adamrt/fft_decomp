#include "fft/battle_text.h"
#include "fft/world.h"
#include "psx/types.h"

/* Render signed gauge values with their positive, negative, or zero color. */
void world_text_render_signed_gauge_entry_list(
    s32 pixels, world_menu_number_entry_t* entries, world_glyph_blit_t* out, s32 count) {
    s32 i;
    s32 amount;

    for (i = 0; i < count; entries++, i++) {
        amount = *entries->value;
        out->rect.x = entries->x;
        out->rect.y = entries->y;
        entries->param &= MENU_DECIMAL_SIGN_CLEAR_MASK;
        if (amount < 0) {
            out->color = 0x88888888;
            amount = -amount;
            entries->param |= MENU_DECIMAL_SIGN_MINUS;
        } else if (amount > 0) {
            out->color = 0xcccccccc;
            entries->param |= MENU_DECIMAL_SIGN_PLUS;
        } else {
            out->color = 0;
            entries->param |= MENU_DECIMAL_SIGN_MINUS;
        }
        world_text_render_decimal_value(amount, entries->param, (void*)pixels, out);
        out->color = 0;
    }
}
