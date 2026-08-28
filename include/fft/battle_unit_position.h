#ifndef FFT_BATTLE_UNIT_POSITION_H
#define FFT_BATTLE_UNIT_POSITION_H

#include "psx/types.h"

typedef enum battle_unit_spell_quote_skillset_flags {
    BATTLE_UNIT_SPELL_QUOTE_SECONDARY_SKILLSET = 1,
    BATTLE_UNIT_SPELL_QUOTE_PRIMARY_SKILLSET = 2,
} battle_unit_spell_quote_skillset_flags_e;

enum {
    BATTLE_UNIT_POSITION_STEPPING_STONE = 0x4000,
};

/*
 * Packed Y and facing data shared by ENTD definitions and runtime battle
 * records.  The US target copies the named bit groups independently.
 */
typedef union battle_unit_position {
    u16 raw;
    struct {
        u16 y : 8;
        u16 facing : 4;
        u16 spell_quote_skillsets : 2;
        u16 stepping_stone : 1;
        u16 higher_elevation : 1;
    } bits;
    u8 bytes[2]; /* bytes[0] is y; battle_unit_modify_entd_unit saves it with lbu/sb */
} battle_unit_position_t;

typedef char battle_unit_position_size_must_be_2[(sizeof(battle_unit_position_t) == 2) ? 1 : -1];

typedef union entd_unit_position {
    u16 raw;
    struct {
        u16 y : 8;
        u16 facing : 2;
        u16 _bits10_11 : 2;
        u16 spell_quote_skillsets : 2;
        u16 stepping_stone : 1;
        u16 higher_elevation : 1;
    } bits;
} entd_unit_position_t;

typedef char entd_unit_position_size_must_be_2[(sizeof(entd_unit_position_t) == 2) ? 1 : -1];
#endif
