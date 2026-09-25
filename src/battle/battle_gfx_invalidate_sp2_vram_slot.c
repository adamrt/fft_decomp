#include "fft/battle.h"

typedef struct battle_gfx_misc_data_header {
    u8 padding[4];
    u8 entd_id;
} battle_gfx_misc_data_header_t;

void battle_gfx_invalidate_sp2_vram_slot(battle_gfx_misc_data_header_t* unit) {
    u32 invalid;
    u8* slot;
    s32 count;

    count = 0;
    invalid = 0xff;
    slot = (u8*)g_battle_gfx_vram_slots;
    do {
        u32 descriptor = *(u32*)slot;

        count++;
        if (((descriptor & 0x1f) == unit->entd_id) && ((descriptor & 0x40) != 0)) {
            *(u32*)slot = invalid;
        }
        slot += 0x7564;
    } while (count < 2);
}
