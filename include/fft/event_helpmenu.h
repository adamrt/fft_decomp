#ifndef FFT_EVENT_HELPMENU_H
#define FFT_EVENT_HELPMENU_H

/* EVENT/HELPMENU.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/battle.h"

extern s16 g_helpmenu_menu_cursor_tile[];
extern u8 g_helpmenu_menu_text_id_tables[];
void helpmenu_menu_run_require_help(void);
void helpmenu_menu_close(void);

/* Menu descriptor for a pending help request. It is reached through the
 * native thread record's first function-parameter word, so its offsets are
 * its own and do not overlap native_thread_t. WORLD's help menu uses the same
 * layout as world_help_request_table_t / world_help_request_t. */
typedef struct help_request_table {
    u8 _unused_00[0x10];
    s16* primary_ids;   /* 0x10 */
    s16* secondary_ids; /* 0x14 */
    s16* mode_flag;     /* 0x18 */
} help_request_table_t;
typedef char help_request_table_mode_flag_offset_must_be_0x18
    [((unsigned long)&((help_request_table_t*)0)->mode_flag == 0x18) ? 1 : -1];

typedef struct help_request {
    u8 _unused_00[0x30];
    help_request_table_t* table; /* 0x30 */
    s16* value;                  /* 0x34 */
    s16 selected_index;          /* 0x38 */
    s16 kind;                    /* 0x3a */
} help_request_t;
typedef char help_request_kind_offset_must_be_0x3a[((unsigned long)&((help_request_t*)0)->kind == 0x3a) ? 1 : -1];

extern u8 g_helpmenu_active_banner[];
extern u8 g_helpmenu_active_unit_data[];

/* 0x40-byte billboard filled by battle_action_copy_at_and_cursor_to; +0x0e is the monster-skillset flag. */
extern battle_menu_status_panel_slot_state_t g_helpmenu_billboard;
extern help_navigation_record_t g_helpmenu_require_navigation[];

/* 0x80 bytes of saved g_battle_text_section_pointers, i.e. 32 pointers. */
extern u8* g_helpmenu_text_saved_pointers[32];
extern s16 g_helpmenu_selected_unit_panel_mode;

/* Section offsets of the help text file, relative to g_helpmenu_text_data (0x80 bytes before it). */
extern s32 g_helpmenu_text_section_offsets[32];
extern u8 g_helpmenu_text_data[];
extern s32* g_helpmenu_controller_state_ptr;
extern u16* g_helpmenu_current_text_ids;
extern help_navigation_record_t g_helpmenu_view_mode_2_navigation[];
extern help_navigation_record_t g_helpmenu_view_mode_3_navigation[];
extern help_navigation_record_t g_helpmenu_view_mode_4_navigation[];
s32 helpmenu_thread_find_running_from_4(void);
void helpmenu_run_battle_help_menu(void);

#endif
