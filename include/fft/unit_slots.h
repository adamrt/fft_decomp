#ifndef FFT_UNIT_SLOTS_H
#define FFT_UNIT_SLOTS_H

/* The battle simulation owns 21 unit records. Battle-slot search routines use
 * the first value beyond that array as their no-unit sentinel. */
enum {
    BATTLE_UNIT_SLOT_COUNT = 21,
    BATTLE_UNIT_ID_NONE = BATTLE_UNIT_SLOT_COUNT,
};

#endif
