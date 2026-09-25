#include "fft/battle.h"
#include "fft/main.h"
#include "psx/types.h"

/* Apply Float and reconcile the unit's status flags and status CT.
 *
 * The byte loop retains unit-relative offsets: named status fields move the
 * pointer increment ahead of the loads and break the exact match. */
void main_status_update_unit_flags_and_ct(battle_stats_t* unit) {
    s32 status;
    /* Pins: without them the loop body is identical up to a permutation of
     * $v0/$v1/$a0-$a2 (register-only diff). */
    register u8* status_fields __asm__("$5");
    u32 innate;
    u32 immunity;
    register u32 current __asm__("$4");
    register u32 inflicted __asm__("$6");

    if (unit->movement_abilities[2] & BATTLE_MOVEMENT_SET_3_FLOAT) {
        unit->status_sets.innate[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_FLOAT)]
            |= BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FLOAT);
    }

    status = 0;
    do {
        status_fields = (u8*)unit + status;
        status++;
        innate = status_fields[0x4e];
        /* Stops cse reusing this load for `current` below. */
        __asm__("" : "=r"(innate) : "0"(innate) : "memory");
        immunity = status_fields[0x53];
        current = status_fields[0x4e];
        inflicted = status_fields[0x1bb];
        /* Keeps the innate inversion after all four loads. */
        __asm__("" : "=r"(inflicted) : "0"(inflicted) : "memory");
        innate = ~innate;
        immunity &= innate;
        current |= inflicted;
        status_fields[0x53] = immunity;
        status_fields[0x58] = current;
    } while (status < BATTLE_STATUS_BYTE_COUNT);

    for (status = BATTLE_STATUS_ID_POISON; status < BATTLE_STATUS_COUNT; status++) {
        s32 status_byte = status / 8;
        s32 status_bit = status & 7;

        if ((unit->inflicted_status[status_byte] & (0x80 >> status_bit))
            && unit->status_ct[BATTLE_STATUS_CT_INDEX(status)] == 0) {
            main_status_set_ct(unit, status, 0);
        }
    }
}
