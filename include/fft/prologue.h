#ifndef PROLOGUE_H
#define PROLOGUE_H

/* OPEN's new-game prologue: text pages over crossfading pictures. */

#include "psx/types.h"

/* New-game prologue entry: a packed line count, texture-page position and image operation, followed by four-byte
 * texture-row/width records. */
typedef struct prologue_entry {
    u32 header;
    u8 line_data[0]; /* variable number of encoded four-byte line records */
} prologue_entry_t;
typedef char prologue_entry_size_must_be_4[sizeof(prologue_entry_t) == 4 ? 1 : -1];

void prologue_start(void);
void prologue_init_sequence(s32 target, s32 ot_layer, prologue_entry_t** entry_table);
s32 prologue_update(void);
void prologue_finished_stub(void);
void prologue_text_pages_load(void);

#endif
