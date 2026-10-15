#ifndef EVENT_SMALL_H
#define EVENT_SMALL_H

/* EVENT/SMALL.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/battle.h"
#include "fft/bits.h"
#include "fft/card.h"
#include "fft/file.h"
#include "fft/item.h"

/* text */
typedef struct small_text_name_section_offsets {
    u32 unit_names;
    u32 job_names;
} small_text_name_section_offsets_t;

extern u8 g_small_text_name_data[];
extern const small_text_name_section_offsets_t g_small_text_name_section_offsets;

void small_text_init_battle_pointers(void);

#endif
