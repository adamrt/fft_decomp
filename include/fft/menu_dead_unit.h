#ifndef MENU_DEAD_UNIT_H
#define MENU_DEAD_UNIT_H

/* Crystals and treasure: the pickup result BATTLE builds when a unit moves onto a fallen unit's crystal or
 * treasure, and the dead-unit panel (OPTION and WORLD) that offers it. */

#include "fft/menu.h"
#include "psx/types.h"

/* crystals and treasure */
/* Crystal/treasure pickup result (g_main_crystal_pickup_result) that battle_unit_generate_crystal_or_treasure
 * builds on the main thread stack and returns; each program keeps the pointer in g_menu_dead_unit_context. The
 * dead-unit panels pick their variant from `result` and show `treasure_item`. */
typedef struct crystal_pickup_result {
    s32 result;                 /* 0x00; -1 none, 0 idle, 4 treasure, 2 crystal, |1 learned */
    u8 _unused_04[0x14];        /* 0x04 */
    u8 learned[19][3];          /* 0x18 */
    u8 unit_index;              /* 0x51 */
    u8 treasure_item;           /* 0x52; the roster/item id the panels show */
    u8 learned_abilities[0x40]; /* 0x53 */
} crystal_pickup_result_t;

extern crystal_pickup_result_t* g_menu_dead_unit_context;

/* Runs battle_unit_generate_crystal_or_treasure for the unit on the main stack, keeps the result in
 * g_menu_dead_unit_context and returns the treasure item when there is one, otherwise -1. */
s32 menu_get_dead_unit_selection(s32 battle_id);

/* dead-unit panel */
/* Seven menu entries form the crystal/treasure result panel. The AT-list
 * and confirmation groups each contain a message entry followed by a choice. */
typedef enum dead_unit_menu_entry {
    DEAD_UNIT_MENU_LIST = 0,
    DEAD_UNIT_MENU_AT_LIST = 1,
    DEAD_UNIT_MENU_CONFIRM = 3,
    DEAD_UNIT_MENU_MESSAGE = 5,
    DEAD_UNIT_MENU_MOVE = 6,
    DEAD_UNIT_MENU_ENTRY_COUNT = 7,
} dead_unit_menu_entry_e;

extern menu_entry_t g_dead_unit_menu[DEAD_UNIT_MENU_ENTRY_COUNT];
extern menu_text_layout_t g_dead_unit_menu_entries;
extern s16 g_dead_unit_menu_flags[];
extern u32* g_dead_unit_controller_input;
/* Handed to the dead-unit panel thread as its second parameter. Nothing stores to it and the thread
 * does not read the parameter. */
extern s32 g_menu_dead_unit_panel_parameter;
extern s16 g_menu_dead_unit_menu_result;
extern s16 g_menu_dead_unit_move_result;
/* The command the panel resolves to (4 for a treasure, 0 for nothing) and the treasure item. */
extern s32 g_menu_dead_unit_action;
extern s32 g_menu_dead_unit_result;

/* Runs the crystal/treasure result panel and its list and choice threads. */
void menu_start_dead_unit_threads(void);

#endif
