#ifndef MENU_SELECTION_H
#define MENU_SELECTION_H

/* Menu selections: the command and list rows remembered per unit, remembered list cursors, resolved selection
 * values and the wrapping steppers that move a selection. */

#include "psx/types.h"

/* per-unit selections */
/* The selected row and scroll position of an ability list. */
typedef struct menu_list_selection {
    u8 selected_index;
    u8 scroll_offset;
} menu_list_selection_t;
typedef char menu_list_selection_size_must_be_2[sizeof(menu_list_selection_t) == 2 ? 1 : -1];

/* One remembered command/skillset selection per unit, followed by seven
 * ability-list positions. Arithmeticks uses list positions 4, 5 and 6. */
typedef struct menu_unit_selection_record {
    u8 command_index;               /* 0x00 */
    u8 _unknown_01;                 /* 0x01; reset to 0xff */
    u8 skillset_index;              /* 0x02 */
    menu_list_selection_t lists[7]; /* 0x03 */
} menu_unit_selection_record_t;
typedef char menu_unit_selection_record_size_must_be_17[(sizeof(menu_unit_selection_record_t) == 17) ? 1 : -1];

/* Remap three remembered selections to one replacement for a menu. */
typedef struct menu_selection_remap {
    u8 menu_id;
    u8 replacement;
    u8 matches[3];
} menu_selection_remap_t;
typedef char menu_selection_remap_size_must_be_5[sizeof(menu_selection_remap_t) == 5 ? 1 : -1];

extern menu_unit_selection_record_t g_menu_unit_selection_records[];
extern menu_selection_remap_t g_menu_selection_remaps[8];
/* When the idle action menu opens again for g_menu_restore_unit_id with a restore pending, it reopens the
 * skillset list and the ability list down to g_menu_restore_depth. */
extern u8 g_menu_restore_pending;
extern u8 g_menu_restore_depth;
extern u8 g_menu_restore_unit_id;

/* Clears one unit's records, only for a unit under manual control. */
void menu_reset_unit_record(s32 battle_id);
void menu_reset_unit_records(void);
/* Calls menu_reset_unit_records; nothing calls it. */
void menu_reset_unit_records_wrapper(void);
/* Restores the selected unit's command row, applying g_menu_selection_remaps. */
void menu_restore_selection_for_selected_unit(s32 menu_id);
void menu_record_selection_for_selected_unit(void);
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
extern s16 g_menu_pending_selection[4];

void menu_set_selection_record(s32 index, s32 selected_index, s32 scroll_base_index, s16* entries);
void menu_get_selection_record(s32 index, s16* selected_index, s16* scroll_base_index, s16* entries);
void menu_clear_selection_record(s32 index);
void menu_clear_selection_records(void);

/* selection values */
/* Stands for "no option" or "no item" in a resolved menu selection. */
enum { MENU_SELECTION_NONE = 0xFFFE };

/* VARIANT_MENU_SELECTION_VALUE_COUNT values. */
extern s16 g_menu_selection_values[];

void menu_clear_selection_values(void);
s32 menu_set_selection_value(u8 index, s16 value);
s32 menu_init_selection_value(u8 index, s16 value);

/* selection steppers */
/* The horizontal and vertical steppers move selection value selection_index (g_menu_selection_values) by the
 * D-pad, wrapping within entry_count, and return it; the _and_mark_change forms also queue a sound effect when
 * it moves. The grid stepper moves a grid index instead. */
s16 menu_update_wrapped_horizontal_selection(u16 entry_count, u8 selection_index, s32 input_mask);
s32 menu_update_horizontal_selection_and_mark_change(u16 entry_count, u8 selection_index, s32 input_mask, u8 sound_id);
s16 menu_update_wrapped_vertical_selection(u16 entry_count, u8 selection_index, s32 input_mask);
s32 menu_update_vertical_selection_and_mark_change(s32 entry_count, s32 selection_index, s32 input_mask, s8 sound_id);
s32 menu_update_wrapped_grid_selection(s32 columns, s32 rows, s32 last_index, s32 old_index, s32 input);

#endif
