#include "fft/battle_text.h"
#include "fft/equip.h"
#include "psx/types.h"

typedef struct equip_stat_entry_t {
    u16 x;
    u16 y;
    s16* value;
    u16 flags;
    u16 unknown_0a;
} equip_stat_entry_t;

typedef struct equip_stat_out_t {
    s16 x;
    s16 y;
    s32 unknown_04;
    s32 unknown_08;
    u32 color;
} equip_stat_out_t;

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
        equip_text_render_decimal_value(value, (s16)entry->flags, pixels, (status_panel_text_position_t*)out);
        out->color = 0;
        entry++;
    }
}
