#include "fft/wldcore.h"
#include "fft/world.h"

/* One random-encounter tier: the story-progress threshold that selects it,
 * the percentage chance an encounter happens, and the high-bit-first mask of
 * which of the entry's eight ENTDs may be drawn. */
typedef struct wldcore_encounter_tier {
    u8 progress_threshold; /* 0x00 */
    u8 encounter_chance;   /* 0x01 */
    u8 encounter_mask;     /* 0x02 */
} wldcore_encounter_tier_t;

/* One road into a world-map location. Three per location, 19 locations
 * (ids 24..42), reached through the pointer cell g_wldcore_random_battle_data. */
typedef struct wldcore_encounter_entry {
    u8 route;                          /* 0x00 */
    u8 deployment_squad_id;            /* 0x01: deployment zone id = this + 0x200 */
    wldcore_encounter_tier_t tiers[4]; /* 0x02 */
    u8 entd_ids[8];                    /* 0x0e */
    u8 battle_map_id;                  /* 0x16 */
    u8 script_variable_id;             /* 0x17 */
} wldcore_encounter_entry_t;

extern wldcore_encounter_entry_t (*g_wldcore_random_battle_data)[3];

#define ENTRY_AT(byte_offset)   ((wldcore_encounter_entry_t*)((byte_offset) + base))
#define ENTRY_BASE(byte_offset) ((wldcore_encounter_entry_t*)(base + (byte_offset)))

s32 wldcore_map_roll_random_encounter(s32 location, s32 route) {
    s32 base;
    s32 offset;
    wldcore_encounter_entry_t* row;
    wldcore_encounter_entry_t* rolled;
    s32 progress;
    s32 chance;
    s32 mask;
    s32 count;
    s32 pick;

    location -= 24;
    if ((u32)location >= 19) {
        return 0;
    }
    base = (s32)g_wldcore_random_battle_data;
    offset = location * 72;
    for (location = 0; location < 3; location++) {
        if (ENTRY_BASE(offset + location * 24)->route == route) {
            break;
        }
    }
    if (location == 3) {
        return 0;
    }
    offset += location * 24;
    row = ENTRY_AT(offset);
    g_wldcore_random_battle_squad_id = row->deployment_squad_id + 0x200;
    progress = world_script_get_variable(row->script_variable_id);
    for (location = 0; location < 4; location++) {
        if (ENTRY_AT(offset + location * 3)->tiers[0].progress_threshold >= progress) {
            break;
        }
    }
    if (location == 4) {
        return 0;
    }
    rolled = ENTRY_AT(location * 3 + offset);
    chance = rolled->tiers[0].encounter_chance;
    if (chance == 0) {
        return 0;
    }
    pick = rand() * 100 >> 15;
    if (chance >= pick) {
        mask = rolled->tiers[0].encounter_mask;
        count = 0;
        for (location = 0; location < 8; location++) {
            if ((mask >> location) & 1) {
                count++;
            }
        }
        pick = (rand() * count >> 15) + 1;
        for (location = 0; location < 8; location++) {
            if ((mask >> location) & 1) {
                pick--;
                if (pick == 0) {
                    break;
                }
            }
        }
        /* entd_ids[7 - location] as a flat byte index: the typed form folds the
         * 0x0e field offset differently and breaks the match. */
        g_wldcore_random_battle_entd_id = ((u8*)base)[offset + 21 - location];
        g_wldcore_random_battle_map_id = ENTRY_AT(offset)->battle_map_id;
        return 1;
    }
    return 0;
}
