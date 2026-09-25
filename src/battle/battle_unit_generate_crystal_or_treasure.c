#include "fft/battle.h"
#include "fft/battle_ability.h"
#include "fft/data.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "psx/types.h"

/* Crystal/treasure pickup result at 0x8006623c. */
typedef struct battle_unit_crystal_pickup_result {
    s32 result;                 /* 0x00; -1 none, 4 treasure, 2 crystal, |1 learned */
    u8 unk_04[0x14];            /* 0x04 */
    u8 learned[19][3];          /* 0x18 */
    u8 unit_index;              /* 0x51 */
    u8 treasure_item;           /* 0x52 */
    u8 learned_abilities[0x40]; /* 0x53 */
} crystal_pickup_result_t;

extern crystal_pickup_result_t g_main_crystal_pickup_result;
/* Local view of battle_stats_t with learned_abilities (0x99) as 19 rows of 3
 * bytes. The 2D indexing is what makes loop.c reduce `unit + i * 3` as the
 * outer-loop giv (target `move s6,s2` / `addiu a2,s6,0x99`). */
typedef struct {
    u8 unk_00[0x99];
    u8 learned_abilities[19][3];
} battle_stats_learned_rows_t;

/* Resolve a crystal or treasure pickup for the unit that moved onto its tile.
 *
 * The two one-trip loops around the crystal result store emit no code; they
 * count that use of the CSE'd &g_main_crystal_pickup_result pseudo two loop levels deeper
 * (8 refs), so global-alloc ranks it above `mover` and gives it s1 as in the
 * target. */
crystal_pickup_result_t* battle_unit_generate_crystal_or_treasure(battle_stats_t* mover) {
    u8 gained[3];
    u8 known[3];
    u8 offered[3];
    s32 learned_count;
    s32 mover_identity;
    s32 crystal_identity;
    s32 mover_flags;
    s32 crystal_flags;
    s32 unlocked_jobs_mask;
    battle_stats_t* crystal;
    s32 crystal_index;
    s32 job_id;
    s32 i;
    s32 j;
    s32 byte_index;
    s32 mask;
    s32 skillset;
    s32 ability;
    u16 ability_id;

    crystal_index = battle_unit_find_at_tile(mover->x, mover->position.bits.y, mover->position.bits.higher_elevation,
        BATTLE_UNIT_TILE_FILTER_CRYSTAL | BATTLE_UNIT_TILE_FILTER_TREASURE);
    if (crystal_index < 0) {
        g_main_crystal_pickup_result.result = -1;
        return &g_main_crystal_pickup_result;
    }
    g_main_crystal_pickup_result.result = 0;
    g_main_crystal_pickup_result.unit_index = crystal_index;
    crystal = &g_battle_unit_stats[crystal_index];
    if (crystal->status_sets.current[1] & 1) {
        g_main_crystal_pickup_result.result = 4;
        g_main_crystal_pickup_result.treasure_item = battle_unit_generate_treasure((u8*)crystal);
    } else {
        do {
            do {
                g_main_crystal_pickup_result.result = 2;
            } while (0);
        } while (0);
        mover_flags = mover->unit_flags;
        crystal_flags = crystal->unit_flags;
        if (!(mover_flags & 0x20) && !(crystal_flags & 0x20)) {
            learned_count = 0;
            unlocked_jobs_mask
                = (mover->unlocked_jobs[0] << 16) + (mover->unlocked_jobs[1] << 8) + mover->unlocked_jobs[2];
            mover_identity = mover->character_identity;
            crystal_identity = crystal->character_identity;
            main_util_clear_byte_data(g_main_crystal_pickup_result.learned_abilities, 0x40);
            for (i = 0; i < 19; i++) {
                for (j = 0; j < 3; j++) {
                    g_main_crystal_pickup_result.learned[i][j] = 0;
                    gained[j] = 0;
                    known[j] = ((battle_stats_learned_rows_t*)mover)->learned_abilities[i][j];
                    offered[j] = ((battle_stats_learned_rows_t*)crystal)->learned_abilities[i][j];
                }
                job_id = i + 0x4a;
                if (i == 0) {
                    if (mover_identity != crystal_identity) {
                        continue;
                    }
                    if (mover_identity < 0x80) {
                        job_id = mover_identity;
                    }
                } else if (!(unlocked_jobs_mask & (0x800000 >> i))) {
                    continue;
                }
                if (i == 0x11 && (mover_flags & 0x40)) {
                    continue;
                }
                if (i == 0x12 && (mover_flags & 0x80)) {
                    continue;
                }
                skillset = g_job_data_pointer[job_id].skillset;
                for (j = 0; j < 24; j++) {
                    byte_index = j / 8;
                    mask = 0x80 >> (j - byte_index * 8);
                    if (!(known[byte_index] & mask) && (offered[byte_index] & mask)) {
                        ability = main_ability_get_id_from_skillset(skillset, j);
                        ability_id = ability;
                        if (ability_id != 0
                            && main_util_roll_pass_fail(100, g_main_ability_data[ability_id].learn_rate) == 0) {
                            gained[byte_index] = mask | gained[byte_index];
                            learned_count++;
                            if (ability_id < 0x200) {
                                g_main_crystal_pickup_result.learned_abilities[ability_id >> 3] = (1 << (ability & 7))
                                    | g_main_crystal_pickup_result.learned_abilities[ability_id >> 3];
                            }
                        }
                    }
                }
                for (j = 0; j < 3; j++) {
                    g_main_crystal_pickup_result.learned[i][j] = gained[j];
                }
            }
            if (learned_count != 0) {
                g_main_crystal_pickup_result.result |= 1;
            }
        }
    }
    return &g_main_crystal_pickup_result;
}
