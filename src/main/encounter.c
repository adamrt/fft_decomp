/* Resident ENTD loading and roster construction from an encounter. */

#include "fft/encounter.h"
#include "fft/file.h"
#include "fft/memory.h"
#include "fft/party.h"
#include "fft/unit_entd.h"
#include "psx/libetc.h"

/* Start loading the ENTD file that holds g_encounter_current_id into a new
 * 0x14000-byte heap block (g_encounter_set). The four files cover 128 encounters
 * each.
 *
 * Returns 1 once the read is queued, and also for encounter 0, which loads nothing.
 * Returns 0 with the block freed when the loader is busy; the caller retries. */
int encounter_file_open(void) {
    int lba;
    entd_file_t* entd_set;

    if (g_encounter_current_id != 0) {
        if (g_encounter_current_id < ENTD2_FIRST_ENCOUNTER) {
            entd_set = heap_alloc(sizeof(entd_file_t));
            g_encounter_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD1_ENT;
        } else if (g_encounter_current_id < ENTD3_FIRST_ENCOUNTER) {
            entd_set = heap_alloc(sizeof(entd_file_t));
            g_encounter_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD2_ENT;
        } else if (g_encounter_current_id < ENTD4_FIRST_ENCOUNTER) {
            entd_set = heap_alloc(sizeof(entd_file_t));
            g_encounter_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD3_ENT;
        } else {
            entd_set = heap_alloc(sizeof(entd_file_t));
            g_encounter_set = entd_set;
            lba = DISC_SECTOR_BATTLE_ENTD4_ENT;
        }

        if (file_bytes_read_request(lba, sizeof(entd_file_t), entd_set) != 0) {
            heap_block_release(g_encounter_set);
            return 0;
        }
    }
    return 1;
}

/* Return the selected encounter in the loaded ENTD file: -1 when there
 * is no selected encounter, NULL while the file is still loading. Encounter ids 0-127
 * index the file directly; each further range of 128 ids (the last one
 * open-ended) indexes it from its own base. */
const entd_encounter_t* encounter_current_get(void) {
    int* encounter_id = &g_encounter_current_id;
    int id;

    if (*encounter_id == 0) {
        return (const entd_encounter_t*)-1;
    }
    if (file_load_state_get() == 0) {
        id = *encounter_id;
        if (id < ENTD2_FIRST_ENCOUNTER) {
            return &g_encounter_set->encounters[id];
        }
        return &g_encounter_set->encounters[id < ENTD3_FIRST_ENCOUNTER ? id - ENTD2_FIRST_ENCOUNTER
                : id < ENTD4_FIRST_ENCOUNTER                           ? id - ENTD3_FIRST_ENCOUNTER
                                                                       : id - ENTD4_FIRST_ENCOUNTER];
    }
    return 0;
}

/* Rebuild the party roster from an ENTD encounter: load its file (blocking),
 * empty the roster, save its 16 unit slots through party_encounter_unit_save,
 * and free the file. */
void encounter_party_init(int encounter_id) {
    const entd_encounter_t* entd;
    int unit_id;

    g_encounter_current_id = encounter_id;
    while (encounter_file_open() == 0) {
        VSync(0);
        file_load_poll(&g_file_cd_state);
    }

    for (;;) {
        entd = encounter_current_get();
        if (entd == (const entd_encounter_t*)-1) {
            break;
        }
        if (entd != 0) {
            party_clear();
            for (unit_id = 0; unit_id < ENTD_UNITS_PER_ENCOUNTER; unit_id++) {
                party_encounter_unit_save(entd, unit_id, 0, 1);
            }
            heap_block_release(g_encounter_set);
            break;
        }
        VSync(0);
        file_load_poll(&g_file_cd_state);
    }
}
