#ifndef FFT_TEXT_H
#define FFT_TEXT_H

enum {
    TEXT_SECTION_COUNT = 32,
    TEXT_ID_SECTION_MASK = 0xf800,
    TEXT_ID_SECTION_SHIFT = 11,
    TEXT_ID_ENTRY_MASK = 0x07ff,
};

/* String-table bank bases used by text resolution and unit-name loading.
 * Unit-name classes reserve one 0x100-entry page each. */
typedef enum text_id_base {
    TEXT_ID_SKILLSET_NAME_BASE = 0x1000,
    TEXT_ID_JOB_NAME_BASE = 0x3000,
    TEXT_ID_ITEM_NAME_BASE = 0x3800,
    TEXT_ID_UNIT_NAME_BASE = 0x4000,
    TEXT_ID_UNIT_NAME_SPECIAL_BASE = TEXT_ID_UNIT_NAME_BASE,
    TEXT_ID_UNIT_NAME_GENERIC_MALE_BASE = 0x4100,
    TEXT_ID_UNIT_NAME_GENERIC_FEMALE_BASE = 0x4200,
    TEXT_ID_UNIT_NAME_GENERIC_MONSTER_BASE = 0x4300,
    TEXT_ID_ABILITY_NAME_BASE = 0x7000,
    TEXT_ID_SECTION_9000_BASE = 0x9000,
} text_id_base_e;

/* Substitution opcodes shared by battle messages and WORLD event text. */
typedef enum text_format_code {
    TEXT_FORMAT_FIRST = 0xe0,
    TEXT_FORMAT_RAMZA_NAME = 0xe0,
    TEXT_FORMAT_UNIT_NAME = 0xe1,
    TEXT_FORMAT_SECTION_9000 = 0xe5,
    TEXT_FORMAT_ITEM_NAME = 0xe9,
    TEXT_FORMAT_ABILITY_NAME = 0xea,
    TEXT_FORMAT_EXPLICIT_ID = 0xeb,
} text_format_code_e;

#endif
