#ifndef EVENT_SMALL_H
#define EVENT_SMALL_H

/* EVENT/SMALL.OUT: the small BATTLE overlay that binds the unit and job name text. */

#include "psx/types.h"

/* text */
/* Offsets of the two name sections in g_small_text_name_data. */
typedef struct small_text_name_section_offsets {
    u32 unit_names;
    u32 job_names;
} small_text_name_section_offsets_t;

extern u8 g_small_text_name_data[];
extern const small_text_name_section_offsets_t g_small_text_name_section_offsets;

void small_text_init_battle_pointers(void);

#endif
