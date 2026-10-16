#ifndef MENU_AT_LIST_H
#define MENU_AT_LIST_H

/* The AT list: BATTLE's turn-order entries and the AT list screen that shows them, either the whole turn order
 * (OPTION and WORLD) or a preview with the highlighted ability's turn in it. */

#include "fft/menu.h"
#include "fft/menu_list.h"
#include "psx/types.h"

/* turn order */
/* Capacity of the AT list, the unit index that ends it, and the unit-index bits of a unit code. */
enum {
    AT_LIST_CAPACITY = 40,
    AT_END_OF_LIST = 31, /* unit index of the entry that ends the list */
    AT_UNIT_ID_MASK = 0x1f,
};

/* One 4-byte entry of the 40-entry AT list built by at_list_build and
 * kept in key order by at_list_insert_entry. Unit codes combine the unit index
 * and action flags into one byte; insertion copies complete entries. */
typedef struct at_entry {
    u8 unit_code;     /* 0x00; unit index and at_entry_flags_e */
    u8 action_id_low; /* 0x01; low byte of the ability id, or a staged item id */
    u16 sort_key;     /* 0x02; insertion key for at_list_insert_entry */
} at_entry_t;
typedef char at_entry_size_must_be_4[(sizeof(at_entry_t) == 4) ? 1 : -1];

/* at_entry_t.unit_code */
typedef enum at_entry_flags {
    AT_JUMP = 0x20,
    AT_ACTION_ENTRY = 0x40, /* charged action or staged item preview */
    AT_ABILITY_ID_HIGH_BIT = 0x80,
} at_entry_flags_e;

/* AT list */
/* Provisional: the AT-list work area at_list_build_ability_preview
 * fills, reached through g_heap_companion_overlay_load_address. The five
 * parallel 16-bit columns become the layout's text_ids/text_colors tables,
 * `text` is published through g_text_section_pointers[TEXT_SECTION_AT_LIST].
 * The main-stack preview fills all 40 AT entries; the menu displays entries
 * 1 through 39. */
typedef struct menu_at_list_work {
    s16 primary_values[AT_LIST_CAPACITY];   /* 0x000: turn-value column */
    s16 secondary_values[AT_LIST_CAPACITY]; /* 0x050: unit-name column */
    s16 entry_numbers[AT_LIST_CAPACITY];    /* 0x0a0: ordinal column */
    s16 primary_colors[AT_LIST_CAPACITY];   /* 0x0f0 */
    s16 secondary_colors[AT_LIST_CAPACITY]; /* 0x140 */
    u8 text[0x4f0];                         /* 0x190: packed 0xfe-terminated names */
    at_entry_t entries[AT_LIST_CAPACITY];   /* 0x680; entry 0 is omitted from the menu */
} at_list_work_t;
typedef char menu_at_list_work_size_must_be_1824[(sizeof(at_list_work_t) == 1824) ? 1 : -1];

extern at_entry_t g_at_list_descriptors[AT_LIST_CAPACITY];
extern menu_entry_t g_at_list_state;
extern menu_scroll_text_layout_t g_at_list_layout;
extern menu_text_layout_t g_at_list_entry_table[1];
extern u8 g_at_list_text[];
extern s16 g_at_list_indices[];
extern s16 g_at_list_primary_values[];
extern s16 g_at_list_secondary_values[];
extern s16 g_at_list_flags[];
extern menu_list_page_t g_at_list_pages[2];
/* Releases at_list_select_unit_thread once a unit picked from the list has been viewed. */
extern u16 g_at_list_selection_finished;

/* Runs the g_at_list_state menu two thread slots below and waits for it to finish. */
void at_list_start_thread(void);
/* Builds and runs the AT list screen; OPTION and WORLD each build their own body. */
void at_list_run(void);
void at_list_finish_selection(void);
/* The ability list's preview: the turn order with the highlighted ability in it, as a scrolling list. */
void at_list_ability_preview_thread(void);

#endif
