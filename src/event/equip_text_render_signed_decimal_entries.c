#include "fft/event_equip.h"
#include "psx/types.h"

/* Render signed gauge values with their positive, negative, or zero color. */
void equip_text_render_signed_decimal_entries(
    void* pixels, equip_stat_entry_t* entry, equip_stat_out_t* out, s32 count) {
    s32 i;
    s32 value;

    for (i = 0; i < count; i++) {
        value = *entry->value;
        out->x = entry->x;
        out->y = entry->y;
        entry->flags &= MENU_DECIMAL_SIGN_CLEAR_MASK;
        if (value < 0) {
            out->color = 0x88888888;
            value = -value;
            entry->flags |= MENU_DECIMAL_SIGN_MINUS;
        } else if (value > 0) {
            out->color = 0xcccccccc;
            entry->flags |= MENU_DECIMAL_SIGN_PLUS;
        } else {
            out->color = 0;
            entry->flags |= MENU_DECIMAL_SIGN_MINUS;
        }
        equip_text_render_decimal_value(
            value, (s16)entry->flags, pixels, (battle_menu_status_panel_text_position_t*)out);
        out->color = 0;
        entry++;
    }
}
