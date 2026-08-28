#include "fft/battle.h"
#include "fft/battle_gfx.h"

/* Per-slot summary written by battle_gfx_get_vram_slot_summary. */
typedef struct battle_vram_slot_summary {
    u8 spritesheet_id; /* 0xff free, 0xfe saved, 0xfd no unit, else owner's sheet */
    u8 evtchr_state;   /* 0xff none, 0xfe saved, 0xfd loaded */
} battle_vram_slot_summary_t;

/*
 * Describe both battle VRAM slots: the owning unit's spritesheet (or the
 * free/saved marker) and the EVTCHR load marker, collapsed to 0xfd..0xff.
 *
 * A struct index into g_battle_gfx_spritesheet_slots keeps the table address out of the loop
 * invariants; the flat byte-array form hoists it into a saved register.
 */
s32 battle_gfx_get_vram_slot_summary(battle_vram_slot_summary_t* summary) {
    s32 i;
    u32 owner;
    u32 marker;

    for (i = 0; i < 2; i++) {
        owner = g_battle_gfx_vram_slots[i].owner;
        if (owner == 0xff) {
            summary[i].spritesheet_id = 0xff;
        } else if (owner == 0xfe) {
            summary[i].spritesheet_id = 0xfe;
        } else if (owner & 0x80) {
            summary[i].spritesheet_id = g_battle_gfx_spritesheet_slots[battle_unit_get_misc_data_by_misc_id(
                                                                           (u16)g_battle_gfx_vram_slots[i].owner & 0x7f)
                                                                           ->spritesheet_vram_slot]
                                            .spritesheet_id;
        } else {
            summary[i].spritesheet_id = 0xfd;
        }
        marker = g_battle_gfx_vram_slots[i].evtchr_load_marker;
        if (marker != 0xfe) {
            if (marker == 0) {
                summary[i].evtchr_state = 0xff;
                continue;
            }
            marker = 0xfd;
        }
        summary[i].evtchr_state = marker;
    }
    return 1;
}
