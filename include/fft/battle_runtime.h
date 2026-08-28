#ifndef FFT_BATTLE_RUNTIME_H
#define FFT_BATTLE_RUNTIME_H

#include "psx/types.h"

/* Battle heap block header: blocks are counted in 8-byte units and linked
 * either on the circular free list (g_battle_heap_rover) or on one of the 16
 * per-owner lists in g_battle_heap_owner_lists. */
typedef struct battle_heap_node {
    struct battle_heap_node* next;
    u16 size; /* in 8-byte blocks */
    s16 owner;
} battle_heap_node_t;

typedef struct battle_heap_owner_list {
    battle_heap_node_t* head;
    s32 unk4;
} battle_heap_owner_list_t;

extern battle_heap_node_t* g_battle_heap_base;
extern s16 g_battle_heap_block_count;
extern battle_heap_owner_list_t g_battle_heap_owner_lists[];
extern battle_heap_node_t* g_battle_heap_rover;

/* BATTLE-resident state that the EVENT, EFFECT and WORLD overlays also read. */
extern void* g_battle_ai_workspace_ptr; /* pointer cell reloaded per subsystem */
extern void* g_current_effect_work;
extern u16 g_event_input_suppression_frames;
extern s32 g_event_mode;
extern u16 g_option_menu_open;
extern u16 g_option_menu_submenu_state;
extern u16 g_option_menu_transition_finished;

/* copy */
/* BATTLE 0x8014bf54..0x8014bf7c: destination first. This do-while loop
 * requires a positive count; it is not a general-purpose memcpy. */
void battle_copy_bytes(void* destination, const void* source, s32 count);

/* heap */
void* battle_heap_alloc_block(u32 size, s32 kind);
void battle_heap_free_block(void* ptr);
void battle_heap_init(battle_heap_node_t* buffer, u32 buffer_size);
void battle_heap_print_stats(void);

/* map */
void battle_map_freeze(void);
void battle_map_unfreeze(void);

/* menu */
void battle_menu_clear_option_menu_open(void);
void battle_menu_open_bunit(void);
void battle_menu_run_bunit_transition_thread(void);
void battle_menu_set_option_menu_open(void);

/* return */
s32 battle_return_zero(void);

/* script */
void battle_script_pulse_tutorial_wait_value(s32 value);
void battle_script_set_specialized_map_destroyed(void);

/* state */
void battle_state_run_deployment(void);

/* unit */
void battle_unit_clear_misc_units(void);

extern u32 g_battle_heap_end_address;
extern s32 g_battle_heap_min_largest_free; /* Lowest largest-free_node-block size seen, in 8-byte units. */

#endif
