#include "psx/types.h"

/* Store `base - scale * scaled` for each equipment-stat halfword of an item detail record. */
void equip_item_subtract_scaled_stats(u8* out, u8* scaled, u8* base, s32 scale) {
    *(s16*)(out + 0x0) = *(u16*)(base + 0x0) - (scale * *(s16*)(scaled + 0x0));
    *(s16*)(out + 0x2) = *(u16*)(base + 0x2) - (scale * *(s16*)(scaled + 0x2));
    *(s16*)(out + 0x4) = *(u16*)(base + 0x4) - (scale * *(s16*)(scaled + 0x4));
    *(s16*)(out + 0x6) = *(u16*)(base + 0x6) - (scale * *(s16*)(scaled + 0x6));
    *(s16*)(out + 0x8) = *(u16*)(base + 0x8) - (scale * *(s16*)(scaled + 0x8));
    *(s16*)(out + 0xa) = *(u16*)(base + 0xa) - (scale * *(s16*)(scaled + 0xa));
    *(s16*)(out + 0xc) = *(u16*)(base + 0xc) - (scale * *(s16*)(scaled + 0xc));
    *(s16*)(out + 0x12) = *(u16*)(base + 0x12) - (scale * *(s16*)(scaled + 0x12));
    *(s16*)(out + 0x14) = *(u16*)(base + 0x14) - (scale * *(s16*)(scaled + 0x14));
    *(s16*)(out + 0x16) = *(u16*)(base + 0x16) - (scale * *(s16*)(scaled + 0x16));
    *(s16*)(out + 0x18) = *(u16*)(base + 0x18) - (scale * *(s16*)(scaled + 0x18));
    *(s16*)(out + 0x1c) = *(u16*)(base + 0x1c) - (scale * *(s16*)(scaled + 0x1c));
    *(s16*)(out + 0x1e) = *(u16*)(base + 0x1e) - (scale * *(s16*)(scaled + 0x1e));
    *(s16*)(out + 0x20) = *(u16*)(base + 0x20) - (scale * *(s16*)(scaled + 0x20));
    *(s16*)(out + 0x22) = *(u16*)(base + 0x22) - (scale * *(s16*)(scaled + 0x22));
}
