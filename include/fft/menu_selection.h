#ifndef MENU_SELECTION_H
#define MENU_SELECTION_H

/* Menu selections: the command and list rows remembered per unit, remembered list cursors, resolved selection values
 * and the wrapping steppers that move a selection. */

#include "psx/types.h"

/* per-unit selections */
/* When the idle action menu opens again for g_command_restore_unit_id with a restore pending, it reopens the
 * skillset list and the ability list down to g_menu_restore_depth. */
extern u8 g_menu_restore_pending;
extern u8 g_menu_restore_depth;

void menu_clear_entry_selections(void);

/* remembered list cursors */
/* The selected row and scroll offset saved across list transitions. */
typedef struct menu_cursor_state {
    s32 selected_index; /* 0x00 */
    s32 scroll_offset;  /* 0x04 */
} menu_cursor_state_t;
typedef char menu_cursor_state_size_must_be_8[sizeof(menu_cursor_state_t) == 8 ? 1 : -1];

/* A remembered list cursor, scroll base and entry id. The menu restores the
 * cursor by entry id when the live list has changed. */
typedef struct menu_selection_record {
    s16 selected_index;    /* 0x00 */
    s16 scroll_base_index; /* 0x02 */
    s16 entry_id;          /* 0x04; low ten bits of the list entry */
} menu_selection_record_t;
typedef char menu_selection_record_size_must_be_6[sizeof(menu_selection_record_t) == 6 ? 1 : -1];

/* VARIANT_MENU_SELECTION_RECORD_COUNT records: 14 in BUNIT and WORLD, 5 in EQUIP and JOBSTTS. */
extern menu_selection_record_t g_menu_selection_records[];

void menu_selection_record_set(s32 index, s32 selected_index, s32 scroll_base_index, const s16* entries);
void menu_selection_record_restore(s32 index, s16* selected_index, s16* scroll_base_index, const s16* entries);
void menu_selection_record_clear(s32 index);
void menu_selection_records_clear(void);

/* selection values */
/* Stands for "no option" or "no item" in a resolved menu selection. */
enum { MENU_SELECTION_NONE = 0xFFFE };

/* VARIANT_MENU_SELECTION_VALUE_COUNT values. */
extern s16 g_menu_selection_values[];

void menu_selection_values_clear(void);
s32 menu_selection_value_set(u8 index, s16 value);
s32 menu_selection_value_init(u8 index, s16 value);

/* selection steppers */
/* The horizontal and vertical steppers move selection value selection_index (g_menu_selection_values) by the
 * D-pad, wrapping within entry_count, and return it; the _sound_step forms also queue a sound effect when
 * it moves. The grid stepper moves a grid index instead. */
s16 menu_selection_horizontal_step(u16 entry_count, u8 selection_index, s32 input_mask);
s32 menu_selection_horizontal_sound_step(u16 entry_count, u8 selection_index, s32 input_mask, u8 sound_id);
s16 menu_selection_vertical_step(u16 entry_count, u8 selection_index, s32 input_mask);
s32 menu_selection_vertical_sound_step(s32 entry_count, s32 selection_index, s32 input_mask, s8 sound_id);
s32 menu_grid_selection_step(s32 columns, s32 rows, s32 last_index, s32 old_index, s32 input);

#endif
