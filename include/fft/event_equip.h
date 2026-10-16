#ifndef EVENT_EQUIP_H
#define EVENT_EQUIP_H

/* EVENT/EQUIP.OUT: the BATTLE overlay of the Equip screen: equip, remove and the item lists. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_panel.h"
#include "fft/menu_script.h"
#include "fft/menu_status.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* screen */
/* g_equip_menu_screen_state: the index of the screen's handler in
 * g_equip_menu_screen_state_handlers. The command menu enters its picked row
 * + EQUIP: Equip, Remove and the item list are rows 0, 2 and 3 (row 1 is
 * refused). */
typedef enum equip_screen_state {
    EQUIP_SCREEN_STATE_INIT = 0, /* equip_entrypoint builds the screen */
    EQUIP_SCREEN_STATE_COMMAND_MENU = 1,
    EQUIP_SCREEN_STATE_EQUIP = 2,     /* equip_menu_run_equip_mode */
    EQUIP_SCREEN_STATE_REMOVE = 4,    /* equip_menu_run_remove_mode */
    EQUIP_SCREEN_STATE_ITEM_LIST = 5, /* menu_update_equipment */
    EQUIP_SCREEN_STATE_CLOSED = 0xff, /* equip_entrypoint returns its result */
} equip_screen_state_e;

extern u8 g_equip_menu_screen_state; /* equip_screen_state_e */
extern void (*g_equip_menu_screen_state_handlers[])(void);
extern s16 g_equip_menu_result_rows[4]; /* [2]: type-order row picked in the sub-menu, or -1 */
extern s8 g_equip_menu_ready;
extern s8 g_equip_equip_mode_initialized;
extern s8 g_equip_remove_mode_initialized;
extern s8 g_equip_type_order_ready;
extern clut_t g_equip_menu_palette_image;
/* EQUIP's 0x3c-byte menu records: the main commands, the item actions and
 * (in equip_item_run_type_order_mode) the item-type order menus. Each is
 * started through menu_start_managed_thread, which makes it the current
 * g_menu_thread_menu_data. */
extern menu_entry_t g_equip_command_menu;

s32 equip_entrypoint(s32 unit, u32* otag);
void equip_menu_init_screen(s32 battle_id);
void equip_menu_reset_selection_indices(void);

/* unit preview */
/* What equip_unit_validate_slot_item finds for an item in an equipment slot. */
typedef enum equip_item_slot_result {
    EQUIP_SLOT_ITEM_CURRENT_LOCKED = -2,
    EQUIP_SLOT_ITEM_CANNOT_EQUIP = -1,
    EQUIP_SLOT_ITEM_INCOMPATIBLE_PAIR = 0,
    EQUIP_SLOT_ITEM_ALLOWED = 1,
} equip_item_slot_result_e;

/* EQUIP.OUT's 0x7A-byte unit preview, built from unit_stats_t by
 * equip_unit_build_data_from_stats. Field names follow the unit_stats_t
 * source; the first 0x1E bytes share the unit-status-billboard layout. */
typedef struct equip_unit_data {
    s16 level;                            /* 0x00; capped at 99 */
    s16 team_kind;                        /* 0x02; 1 enemy, 2 neutral */
    s16 list_index;                       /* 0x04; AT-list lookup + 1, or -1 */
    s16 unit_count;                       /* 0x06; units still present, set by equip_unit_load_data_from_stats */
    s16 experience;                       /* 0x08; capped at 99 */
    s16 unit_index;                       /* 0x0a; unit_stats_t.entd_slot */
    menu_unit_status_gauge_t gauges[3];   /* 0x0c; HP, MP and CT */
    s16 battle_id;                        /* 0x1e */
    s16 _unused_20;                       /* 0x20 */
    menu_unit_status_identity_t identity; /* 0x22 */
    s16 move;                             /* 0x30 */
    s16 speed;                            /* 0x32 */
    s16 jump;                             /* 0x34 */
    s16 right_weapon_power;               /* 0x36 */
    s16 left_weapon_power;                /* 0x38 */
    s16 right_weapon_evade;               /* 0x3a */
    s16 left_weapon_evade;                /* 0x3c */
    s16 _unused_3e;                       /* 0x3e; monster-skillset flag slot */
    s16 two_handing;                      /* 0x40; equip_unit_is_two_handing_weapon result */
    s16 physical_attack;                  /* 0x42 */
    s16 physical_class_evade;             /* 0x44 */
    s16 physical_shield_evade;            /* 0x46 */
    s16 _unused_48;                       /* 0x48; accessory evade slot */
    s16 _unused_4a;                       /* 0x4a */
    s16 magic_attack;                     /* 0x4c */
    s16 magical_class_evade;              /* 0x4e; cleared (lower billboard M-C-Ev) */
    s16 magical_shield_evade;             /* 0x50 */
    s16 magical_accessory_evade;          /* 0x52 */
    u16 equipment[5];                     /* 0x54; menu_equipment_slot_e order */
    u8 _unused_5e[0x12];                  /* 0x5e; ability ids and the spare tail of the stat-detail block, left zero */
    u8 equipment_categories[4];           /* 0x70 */
    u8 support_abilities[4];              /* 0x74; copy of unit_stats_t.ability_flags.support */
    u8 party_index;                       /* 0x78; unit_stats_t.formation_index */
    u8 _padding_79;                       /* tail padding to 2-byte alignment */
} equip_unit_data_t;
typedef char equip_unit_data_size_must_be_122[(sizeof(equip_unit_data_t) == 122) ? 1 : -1];

extern equip_unit_data_t g_equip_unit_records[];
/* Provisional: the 0x7A-byte preview records for the selected unit (index 0)
 * and its compare slot (index 1); several callers view one as raw bytes. */
extern equip_unit_data_t* g_equip_unit_data[];
extern u8 g_equip_unit_selected_index;
extern s16 g_equip_unit_initial_equipment[];
extern s16 g_equip_unit_saved_hp;
extern s16 g_equip_unit_saved_mp;
extern s8 g_equip_unit_banner_enabled;

void equip_unit_load_data_from_stats(s32 battle_id);
void equip_unit_load_selected_data(void);
void equip_unit_copy_data_to_compare_slot(s32 unused_selected_index, s32 unused_1);
s32 equip_unit_set_slot_item(s16 unit_index, s16 slot, s32 item_id);
s32 equip_unit_has_two_hands(s16 unit_index);
s32 equip_unit_has_two_swords(s16 unit_index);
void equip_unit_commit_loadout_to_stats(void);

/* equipment slots */
extern point16_t g_equip_slot_marker_rect;
extern point16_t g_equip_remove_item_marker_rect;
extern u16 g_equip_equip_item_cursor_anim[];
extern u16 g_equip_remove_item_cursor_anim[];
extern oriented_quad_t g_equip_menu_slot_marker_rects[];
extern oriented_quad_t g_equip_menu_secondary_slot_marker;
extern menu_instruction_t g_equip_menu_slot_marker_window;
extern u8 g_equip_menu_slot_marker_x[];
/* One-shot entry flag for this mode. */
extern s8 g_equip_slot_item_browser_initialized;
/* List index whose stat preview was last built; -1 forces a rebuild. */
extern s8 g_equip_slot_item_browser_previewed_index;
extern point16_t g_equip_menu_clear_all_prompt_draw_params;
extern volatile u16 g_equip_menu_clear_all_prompt_frame;

void equip_menu_draw_equipment_slot_marker(s32 unused, s32 slot, s32 override);

/* item lists */
extern s8 g_equip_item_slot;
extern s8 g_equip_item_list_open;
extern s8 g_equip_item_list_refresh;
extern u8 g_equip_item_list_has_multiple_entries;
extern s8 g_equip_item_preview_active;
extern s16 g_equip_item_previewed_cursor;
extern u8 g_equip_item_inventory_totals[];
extern s8 g_equip_menu_substate; /* menu_item_list_state_e */
extern menu_instruction_t g_equip_item_menu_script;
extern menu_instruction_t g_equip_equipment_render_commands;
extern menu_instruction_t g_equip_armor_bonus_render_commands;

s32 equip_item_build_filtered_list(s16 unit_index, s16 sort_mode, s8 category, s16* list, u8 equip_filter);
void equip_item_sort_id_list(s32 mode, s16* list);
void equip_item_build_inventory_totals(void);
s32 equip_item_run_category_menu(void);
void equip_menu_restore_list_selection(s32 index, s16* selected_index, s16* scroll_base_index, s16* entries);
s32 equip_menu_get_preview_hp_bonus(s32 unused_row);
s32 equip_menu_get_preview_mp_bonus(s32 unused_row);

/* item actions */
extern s8 g_equip_item_action_menu_active;
extern menu_entry_t* g_equip_item_action_active_menu;
extern menu_entry_t g_equip_item_action_menu_single_item;
extern s16 g_equip_menu_text_redraw_flag;

s32 equip_menu_update_item_action_state(void);

/* item type order */
extern s16 g_equip_item_type_order[];
extern u8 g_equip_item_type_order_names[];
extern menu_entry_t* g_equip_item_type_order_active_menu;
extern menu_entry_t g_equip_type_order_menu_hand_body;
extern menu_entry_t g_equip_type_order_menu_other_slots;

s32 equip_item_run_type_order_mode(void);

/* unit panels */
extern menu_unit_status_identity_t g_equip_panel_selected_unit_data;
extern palette_t g_equip_panel_selected_portrait_palettes[2];
extern menu_unit_status_identity_t g_equip_panel_comparison_unit_data;
extern palette_t g_equip_panel_comparison_portrait_palettes[2];
extern menu_status_panel_buffer_t g_equip_panel_frames_b[];
/* RECT[1] (right panel) of g_panel_origin_offsets. Kept as its own symbol: spelling it as an element lets
 * GCC derive the table base from this address and changes menu_run_ability_list_thread. */
extern RECT g_equip_right_panel_frame_rect;
extern u8 g_equip_unit_status_panel_params[];
extern s32 g_equip_unit_status_panel_flags;
extern s32 g_equip_unit_status_panel_redraw;
extern u8 g_equip_equipment_panel_params[];
extern s32 g_equip_equipment_panel_style;
extern s32 g_equip_equipment_panel_redraw;
extern s32 g_equip_numeric_panel_style;
extern s32 g_equip_numeric_panel_redraw;
extern u8 g_equip_status_label_image[];
extern RECT g_equip_status_label_image_rect;

void equip_panel_toggle_unit_status_thread(s32 enable);
void equip_panel_toggle_equipment_threads(s32 enable);
void equip_panel_toggle_item_numeric_thread(s32 enable);

/* status indicators */
/* Double-buffer index, current level, fade level and state, one pair each for
 * the two status panels. */
extern s32 g_equip_menu_indicator_buffer_index[2];
extern s32 g_equip_menu_indicator_brightness[2];
extern s32 g_equip_menu_indicator_edge_brightness[2];
extern s32 g_equip_menu_indicator_state[2];
extern menu_gradient_panel_prims_t g_equip_thread_indicator_packets[2][2];

void equip_menu_draw_thread_status_indicators(void);

/* graphics */
extern menu_gfx_context_t* g_equip_gfx_context_base;
extern menu_gfx_context_t g_equip_gfx_contexts[];
extern u32* g_equip_gfx_current_ot;
extern u16 g_equip_gfx_draw_area_capacity;
extern u16 g_equip_gfx_draw_move_capacity;
/* LINE_G2 and LINE_G3 capacities; volatile keeps their load order in equip_gfx_init_contexts. */
extern volatile u16 g_equip_gfx_line_g2_capacity;
extern volatile u16 g_equip_gfx_line_g3_capacity;
extern u16 g_equip_gfx_poly_f4_count;
extern u16 g_equip_gfx_poly_g4_count;
extern LINE_F2 g_equip_gfx_line_f2_buffer[];
extern TILE g_equip_gfx_tile_buffer[];
extern DR_MODE g_equip_gfx_draw_mode_buffer[];

void equip_gfx_init_contexts(menu_gfx_context_t* base, u32* otag, POLY_F3* poly_f3, POLY_FT3* poly_ft3,
    POLY_F4* poly_f4, POLY_FT4* textured_quads, POLY_G3* poly_g3, POLY_GT3* poly_gt3, POLY_G4* poly_g4,
    POLY_GT4* poly_gt4, LINE_F2* line_f2, LINE_F3* line_f3, LINE_F4* line_f4, LINE_G2* line_g2, LINE_G3* line_g3,
    LINE_G4* line_g4, TILE* tiles, TILE_1* tiles_1, TILE_8* tiles_8, TILE_16* tiles_16, SPRT* sprites,
    SPRT_8* sprites_8, SPRT_16* sprites_16, DR_MOVE* draw_moves, DR_AREA* draw_areas, DR_MODE* draw_modes);
void equip_gfx_swap_context_and_clear_otag(s32 first_otag, s32 unused);
void equip_gfx_enqueue_textured_quad_current_ot(const RECT* rect, s32 u, s32 v, const rgb24_t* color, s32 semitrans,
    u16 texture_page, u16 clut, s32 unused_priority);

/* input */
s32 equip_input_read_page_scroll_direction(void);
void equip_menu_input_update_with_message_state(void);

/* menu commands */
/* The opcode that ends an EQUIP menu display stream. */
enum {
    EQUIP_CMD_END = 0x19,
};

/* A g_menu_row_callbacks getter, as equip_menu_opcode_0d_draw_formatted_number calls it. */
typedef s32 (*equip_row_callback_t)(s32 row);

extern u16 g_equip_menu_list_cursor_anim_state[];
extern s16 g_equip_menu_list_scroll_entry_ids;

menu_instruction_t* equip_menu_opcode_07_draw_line(menu_instruction_t* instruction);
menu_instruction_t* equip_menu_opcode_03_draw_tile(menu_instruction_t* instruction);
menu_instruction_t* equip_menu_opcode_17_draw_right_aligned_number(menu_instruction_t* instruction);
menu_instruction_t* equip_menu_opcode_0d_draw_formatted_number(menu_instruction_t* instruction);
menu_instruction_t* equip_menu_opcode_10_draw_scrollable_list(menu_instruction_t* instruction);

/* text */
/* The "%d" format string at the head of the overlay. */
extern const char g_equip_text_decimal_format[];
extern u8 g_equip_text_restore_pending;

void equip_text_show_message_with_sound(s32 message_id, s32 sound_id);

/* threads */
/* Slots of the equip screen's own threads. The WORLD formation screen runs
 * the same threads in other slots (world_thread_slot_e and
 * thread_screen_slot_e); here BATTLE's suspended menu threads stay in 7 and 8. */
typedef enum equip_thread_slot {
    EQUIP_THREAD_SLOT_POPUP_MENU = 3,         /* the item action and type order menus */
    EQUIP_THREAD_SLOT_COMMAND_MENU_CHILD = 4, /* a wait-forever thread while a command's mode runs */
    EQUIP_THREAD_SLOT_COMMAND_MENU = 5,       /* menu_icon_list_thread */
    EQUIP_THREAD_SLOT_EQUIPMENT_LIST = 10,    /* menu_run_equipment_list_thread */
    EQUIP_THREAD_SLOT_ITEM_NUMERIC = 11,      /* the numeric editor thread of the item panel */
    EQUIP_THREAD_SLOT_UNIT_STATUS = 13,       /* menu_run_unit_status_thread */
    EQUIP_THREAD_SLOT_COMPARISON_STATUS = 14, /* the second banner slot the indicator draw watches */
} equip_thread_slot_e;

extern s8 g_equip_menu_thread_running;
extern s16 g_equip_text_message_thread_active;
extern s32 g_equip_item_numeric_thread_enabled;
extern u8 g_equip_thread_idle_stop_pending;

/* sound */
extern u8 g_equip_sound_suppress_queued;

/* unknown state */
extern s16 g_equip_input_unread_value;

#endif
