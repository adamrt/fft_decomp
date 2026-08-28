#ifndef FFT_UNIT_FLAGS_H
#define FFT_UNIT_FLAGS_H

/* Shared by ENTD definitions, persistent party records, and runtime battle
 * units. The byte combines persistence controls with unit-kind flags. */
typedef enum unit_flags {
    UNIT_FLAG_SAVE_FORMATION = 0x01,
    UNIT_FLAG_EGG = 0x04,
    UNIT_FLAG_LOAD_FORMATION = 0x08,
    UNIT_FLAG_JOIN_AFTER_EVENT = 0x10,
    UNIT_FLAG_MONSTER = 0x20,
    UNIT_FLAG_FEMALE = 0x40,
    UNIT_FLAG_MALE = 0x80,
} unit_flags_e;

#endif
