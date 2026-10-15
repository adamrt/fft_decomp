#ifndef EVENT_EQUIP_H
#define EVENT_EQUIP_H

/* EVENT/EQUIP.OUT: menu screen overlay that runs with BATTLE. */

#include "fft/battle.h"
#include "fft/bits.h"
#include "fft/card.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"

/* screen */
extern u16 g_equip_remove_all_equipment_ids[];
extern s8 g_equip_remove_all_equipment_prompt_active;
extern s8 g_equip_remove_mode_initialized;
extern s8 g_equip_type_order_ready;

s32 equip_entrypoint(s32 unit, u32* otag);

/* unit */
typedef enum equip_slot {
    EQUIP_SLOT_RIGHT_HAND = 0,
    EQUIP_SLOT_LEFT_HAND = 1,
    EQUIP_SLOT_HEAD = 2,
    EQUIP_SLOT_BODY = 3,
    EQUIP_SLOT_ACCESSORY = 4,
    EQUIP_SLOT_HAND_COUNT = 2,
    EQUIP_SLOT_COUNT = 5,
} equip_unit_equipment_slot_e;

typedef enum equip_slot_item_result {
    EQUIP_SLOT_ITEM_CURRENT_LOCKED = -2,
    EQUIP_SLOT_ITEM_CANNOT_EQUIP = -1,
    EQUIP_SLOT_ITEM_INCOMPATIBLE_PAIR = 0,
    EQUIP_SLOT_ITEM_ALLOWED = 1,
} equip_item_slot_result_e;

/* EQUIP.OUT's 0x7A-byte unit preview, built from unit_stats_t by
 * equip_unit_build_data_from_stats (0x801C6920). Field names follow
 * the unit_stats_t source; evasion_N maps to the corresponding
 * unit_stats_t equipment-stat slot. The
 * first 0x1E bytes share the unit-status-billboard layout. */
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
    u16 equipment[5];                     /* 0x54; equip_unit_equipment_slot_e order */
    u8 _unused_5e[0x12];                  /* 0x5e; ability ids and the spare tail of the stat-detail block, left zero */
    u8 equipment_categories[4];           /* 0x70 */
    u8 support_abilities[4];              /* 0x74; copy of unit_stats_t.ability_flags.support */
    u8 party_index;                       /* 0x78; unit_stats_t.formation_index */
    u8 _padding_79;                       /* tail padding to 2-byte alignment */
} equip_unit_data_t;
typedef char equip_unit_data_size_must_be_122[(sizeof(equip_unit_data_t) == 122) ? 1 : -1];

extern s8 g_equip_unit_banner_enabled;
/* Provisional: the 0x7A-byte preview records for the selected unit (index 0)
 * and its compare slot (index 1); several callers view one as raw bytes. */
extern equip_unit_data_t* g_equip_unit_data[];
extern s16 g_equip_unit_initial_equipment[];
extern equip_unit_data_t g_equip_unit_records[];
extern s16 g_equip_unit_saved_hp;
extern s16 g_equip_unit_saved_mp;
extern u8 g_equip_unit_selected_index;

void equip_unit_build_data_from_stats(struct unit_stats* stats, equip_unit_data_t* unit);
s32 equip_unit_can_equip_item_id(s16 unit_index, s16 item_entry);
void equip_unit_commit_loadout_to_stats(void);
void equip_unit_copy_data_to_compare_slot(s32 unused_selected_index, s32 unused_1);
s32 equip_unit_has_two_hands(s16 unit_index);
s32 equip_unit_has_two_swords(s16 unit_index);
s32 equip_unit_is_two_handing_weapon(const s16* slots, s32 two_hands_support);
void equip_unit_load_data_from_stats(s32 battle_id);
void equip_unit_load_selected_data(void);
s32 equip_unit_set_slot_item(s16 unit_index, s16 slot, s32 item_id);
equip_item_slot_result_e equip_unit_validate_slot_item(s16 unit_index, s16 slot, s32 item_id);

/* equipment selection */
extern u16 g_equip_equip_item_cursor_anim[];
extern s8 g_equip_equip_item_list_open;
extern s8 g_equip_equip_item_list_refresh;
extern s8 g_equip_equip_item_preview_active;
extern s16 g_equip_equip_item_previewed_cursor;
extern s8 g_equip_equip_item_slot;
extern s8 g_equip_equip_mode_initialized;

/* equipment display */
extern menu_instruction_t g_equip_equipment_render_commands;

/* item */
enum {
    EQUIP_ITEM_LIST_END = 0xFF,
    EQUIP_ITEM_ID_MASK = 0x3FF,
    EQUIP_ITEM_LIST_ENTRY_ITEM_ID_MASK = 0x3FF,
    EQUIP_ITEM_LIST_ENTRY_UNEQUIPPABLE = 0x4000, /* listed, but the unit cannot equip the item */
};

extern menu_entry_t* g_equip_item_action_active_menu;
extern u8 g_equip_item_inventory_totals[];
extern u8 g_equip_item_list_has_multiple_entries;
extern s16 g_equip_item_type_order[];
extern menu_entry_t* g_equip_item_type_order_active_menu;
extern u8 g_equip_item_type_order_names[];
extern u16 g_equip_remove_item_cursor_anim[];
extern s16 g_equip_remove_item_equipment[];
extern point16_t g_equip_remove_item_marker_rect;
extern s8 g_equip_remove_item_previewed_slot;

s32 equip_item_build_filtered_list(s16 unit_index, s16 sort_mode, s8 category, s16* list, u8 equip_filter);
void equip_item_build_inventory_totals(void);
void equip_item_sort_list_by_criteria(s32 mode, s16* list);
s32 equip_item_run_type_order_mode(void);

/* rearrange */

/* slot */
/* One-shot entry flag for this mode. */
extern s8 g_equip_slot_item_browser_initialized;
/* List index whose stat preview was last built; -1 forces a rebuild. */
extern s8 g_equip_slot_item_browser_previewed_index;
extern point16_t g_equip_slot_marker_rect;

/* unit panels */
extern menu_instruction_t g_equip_armor_bonus_render_commands;
extern u8 g_equip_equipment_panel_params[];
extern s32 g_equip_equipment_panel_redraw;
extern s32 g_equip_equipment_panel_style;
extern s32 g_equip_numeric_panel_redraw;
extern s32 g_equip_numeric_panel_style;
extern menu_unit_status_record_t g_equip_panel_comparison_billboard;
extern menu_status_panel_editor_packet_t g_equip_panel_comparison_editor_packets[2];
extern u8 g_equip_panel_comparison_large_number_image[];
extern u8 g_equip_panel_comparison_name_image[];
extern u8 g_equip_panel_comparison_number_image[];
extern menu_number_entry_t g_equip_panel_comparison_numeric_entries[];
extern menu_status_panel_packet_t g_equip_panel_comparison_packets[2];
extern palette_t g_equip_panel_comparison_portrait_palettes[2];
extern RECT g_equip_panel_comparison_portrait_rect;
extern menu_unit_status_identity_t g_equip_panel_comparison_unit_data;
extern image_load_parameters_t g_equip_panel_editor_label_cells[];
extern u16 g_equip_panel_editor_label_cluts[][2];
extern image_load_parameters_t g_equip_panel_editor_mode_cell[];
extern image_load_parameters_t g_equip_panel_editor_mode_cells[];
extern image_load_parameters_t g_equip_panel_editor_value_cells[];
extern u16 g_equip_panel_editor_value_cluts[][2];
extern RECT g_equip_panel_frame_rect;
extern menu_status_panel_buffer_t g_equip_panel_frames_b[];
extern CVECTOR g_equip_panel_gauge_bar_colors[];
extern image_load_parameters_t g_equip_panel_portrait_cell[];
extern menu_status_panel_editor_packet_t g_equip_panel_selected_editor_packets[2];
extern u8 g_equip_panel_selected_large_number_image[];
extern u8 g_equip_panel_selected_name_image[];
extern u8 g_equip_panel_selected_number_image[];
extern menu_number_entry_t g_equip_panel_selected_numeric_entries[];
extern menu_status_panel_packet_t g_equip_panel_selected_packets[2];
extern palette_t g_equip_panel_selected_portrait_palettes[2];
extern RECT g_equip_panel_selected_portrait_rect;
extern menu_unit_status_identity_t g_equip_panel_selected_unit_data;
extern s16 g_equip_panel_slide_down_y[];
extern s16 g_equip_panel_slide_up_y[];
extern image_load_parameters_t g_equip_panel_sprite_cells[];
/* RECT[1] (right panel) of g_menu_status_panel_origin_offsets. Kept as its own symbol: spelling it as an element lets
 * GCC derive the table base from this address and changes equip_panel_run_ability_list_thread. */
extern RECT g_equip_right_panel_frame_rect;
extern u8 g_equip_status_label_image[];
extern RECT g_equip_status_label_image_rect;
extern s32 g_equip_unit_status_panel_flags;
extern u8 g_equip_unit_status_panel_params[];
extern s32 g_equip_unit_status_panel_redraw;

void equip_panel_toggle_equipment_threads(s32 enable);
void equip_panel_toggle_item_numeric_thread(s32 enable);
void equip_panel_toggle_unit_status_thread(s32 enable);

/* numeric editor */
extern menu_number_entry_t g_equip_editor_numeric_descriptor_a[];
extern menu_number_entry_t g_equip_editor_numeric_descriptor_b[];
extern RECT g_equip_editor_numeric_entries_a;
extern RECT g_equip_editor_numeric_entries_b;
extern RECT g_equip_editor_numeric_entries_c;
extern RECT g_equip_editor_numeric_entries_d;
extern RECT g_equip_editor_numeric_entries_e;
extern RECT g_equip_editor_numeric_entries_f;
extern menu_status_panel_numeric_buffer_t g_equip_editor_numeric_state_a[2];
extern menu_status_panel_numeric_buffer_t g_equip_editor_numeric_state_b[2];
/* Per-thread-slot window rect, upload rects, sprite descriptors and number
 * entries; the first set belongs to thread 12. */
extern battle_menu_status_panel_numeric_geometry_t g_equip_editor_numeric_table;
extern menu_unit_stat_number_images_t g_equip_editor_numeric_text_a;
extern menu_unit_stat_number_images_t g_equip_editor_numeric_text_b;
extern image_load_parameters_t g_equip_editor_numeric_texture[];

/* menus */
/* g_equip_menu_screen_state: the index of the screen's handler in
 * g_equip_menu_screen_state_handlers. The command menu enters its picked row
 * + EQUIP: Equip, Remove and the item list are rows 0, 2 and 3 (row 1 is
 * refused). */
typedef enum equip_screen_state {
    EQUIP_SCREEN_STATE_INIT = 0, /* equip_entrypoint builds the screen */
    EQUIP_SCREEN_STATE_COMMAND_MENU = 1,
    EQUIP_SCREEN_STATE_EQUIP = 2,     /* equip_menu_run_equip_mode */
    EQUIP_SCREEN_STATE_REMOVE = 4,    /* equip_menu_run_remove_mode */
    EQUIP_SCREEN_STATE_ITEM_LIST = 5, /* equip_menu_update_equipment */
    EQUIP_SCREEN_STATE_CLOSED = 0xff, /* equip_entrypoint returns its result */
} equip_screen_state_e;
typedef s32 (*equip_row_callback_t)(s32 row);

extern menu_instruction_t g_equip_equip_item_menu_script;
extern s8 g_equip_item_action_menu_active;
extern menu_entry_t g_equip_item_action_menu_single_item;
extern point16_t g_equip_menu_clear_all_prompt_draw_params;
extern volatile u16 g_equip_menu_clear_all_prompt_frame;

extern s32 g_equip_menu_indicator_brightness[2];
/* Double-buffer index, current level, fade level and state, one pair each for
 * the two status panels. */
extern s32 g_equip_menu_indicator_buffer_index[2];
extern s32 g_equip_menu_indicator_edge_brightness[2];
extern s32 g_equip_menu_indicator_state[2];
extern u16 g_equip_menu_list_cursor_anim_state[];

extern s16 g_equip_menu_list_scroll_entry_ids;
extern u16 g_equip_menu_list_scroll_progress;
extern clut_t g_equip_menu_palette_image;
extern s8 g_equip_menu_ready;

extern s16 g_equip_menu_result_rows[4]; /* [2]: type-order row picked in the sub-menu, or -1 */
extern u8 g_equip_menu_screen_state;    /* equip_screen_state_e */
extern void (*g_equip_menu_screen_state_handlers[])(void);

extern oriented_quad_t g_equip_menu_secondary_slot_marker;
extern oriented_quad_t g_equip_menu_slot_marker_rects[];
extern menu_instruction_t g_equip_menu_slot_marker_window;
extern u8 g_equip_menu_slot_marker_x[];
extern s8 g_equip_menu_substate; /* menu_item_list_state_e */
extern s16 g_equip_menu_text_redraw_flag;

extern menu_entry_t g_equip_type_order_menu_hand_body;
extern menu_entry_t g_equip_type_order_menu_other_slots;

void equip_menu_draw_equipment_slot_marker(s32 unused, s32 slot, s32 override);
s32 equip_menu_get_preview_hp_bonus(s32 unused_row);
s32 equip_menu_get_preview_mp_bonus(s32 unused_row);
void equip_menu_init_screen(s32 battle_id);
void equip_menu_load_images_and_reset_lists(void);
void equip_menu_reset_selection_indices(void);
void equip_menu_reset_state_flags(void);
void menu_restore_list_selection(s32 index, s16* selected_index, s16* scroll_base_index, s16* entries);
s32 equip_menu_run_equip_mode(void);
s32 equip_menu_run_remove_mode(void);
s32 equip_menu_update_clear_all_equipment_prompt(void);
s32 equip_menu_update_item_action_state(void);

/* help */
extern u32 g_equip_help_text_table[];

/* graphics */
/* The scratch marker rect this routine drives; uncataloged, but the same
 * descriptor equip_menu_draw_equipment_slot_marker feeds to the oriented quad
 * enqueue. */
extern oriented_quad_t g_equip_gfx_blank_cell_rect;
extern menu_gfx_context_t* g_equip_gfx_context_base;
extern menu_gfx_context_t g_equip_gfx_contexts[];
extern u32* g_equip_gfx_current_ot;
extern DR_AREA g_equip_gfx_draw_area_buffer[];
extern u16 g_equip_gfx_draw_area_capacity;
extern DR_MODE g_equip_gfx_draw_mode_buffer[];
extern u16 g_equip_gfx_draw_mode_capacity;
extern DR_MOVE g_equip_gfx_draw_move_buffer[];
extern u16 g_equip_gfx_draw_move_capacity;
extern LINE_F2 g_equip_gfx_line_f2_buffer[];
extern u16 g_equip_gfx_line_f2_capacity;
extern u16 g_equip_gfx_line_f2_count;

extern u16 g_equip_gfx_line_f3_capacity;
extern u16 g_equip_gfx_line_f3_count;

extern u16 g_equip_gfx_line_f4_capacity;
extern u16 g_equip_gfx_line_f4_count;
extern volatile u16 g_equip_gfx_line_g2_capacity; /* LINE_G2 capacity; load order matters in 0x801cda80 */
extern u16 g_equip_gfx_line_g2_count;
extern volatile u16 g_equip_gfx_line_g3_capacity; /* LINE_G3 capacity; load order matters in 0x801cda80 */
extern u16 g_equip_gfx_line_g3_count;
extern u16 g_equip_gfx_line_g4_capacity;
extern u16 g_equip_gfx_line_g4_count;
extern u32 g_equip_gfx_otag_buffer[];
extern u16 g_equip_gfx_otag_length;
extern u16 g_equip_gfx_poly_f3_capacity;
extern u16 g_equip_gfx_poly_f3_count;
extern u16 g_equip_gfx_poly_f4_capacity;
extern u16 g_equip_gfx_poly_f4_count;
extern u16 g_equip_gfx_poly_ft3_capacity;
extern u16 g_equip_gfx_poly_ft3_count;
extern POLY_FT4 g_equip_gfx_poly_ft4_buffer[];
extern u16 g_equip_gfx_poly_ft4_capacity;
extern u16 g_equip_gfx_poly_g3_capacity;
extern u16 g_equip_gfx_poly_g3_count;
extern u16 g_equip_gfx_poly_g4_capacity;
extern u16 g_equip_gfx_poly_g4_count;
extern u16 g_equip_gfx_poly_gt3_capacity;
extern u16 g_equip_gfx_poly_gt3_count;
extern u16 g_equip_gfx_poly_gt4_capacity;
extern u16 g_equip_gfx_poly_gt4_count;
extern u16 g_equip_gfx_sprite_16_capacity;
extern u16 g_equip_gfx_sprite_16_count;
extern u16 g_equip_gfx_sprite_8_capacity;
extern u16 g_equip_gfx_sprite_8_count;
extern u16 g_equip_gfx_sprite_capacity;
extern u16 g_equip_gfx_sprite_count;
extern u16 g_equip_gfx_tile_16_capacity;
extern u16 g_equip_gfx_tile_16_count;
extern u16 g_equip_gfx_tile_1_capacity;
extern u16 g_equip_gfx_tile_1_count;
extern u16 g_equip_gfx_tile_8_capacity;
extern u16 g_equip_gfx_tile_8_count;
extern TILE g_equip_gfx_tile_buffer[];
extern u16 g_equip_gfx_tile_capacity;
extern s16 g_equip_gfx_window_zoom_percent[];

void equip_gfx_enqueue_textured_quad_current_ot(const RECT* rect, s32 u, s32 v, const rgb24_t* color, s32 semitrans,
    u16 texture_page, u16 clut, s32 unused_priority);
void equip_gfx_init_contexts(menu_gfx_context_t* base, u32* otag, POLY_F3* poly_f3, POLY_FT3* poly_ft3,
    POLY_F4* poly_f4, POLY_FT4* textured_quads, POLY_G3* poly_g3, POLY_GT3* poly_gt3, POLY_G4* poly_g4,
    POLY_GT4* poly_gt4, LINE_F2* line_f2, LINE_F3* line_f3, LINE_F4* line_f4, LINE_G2* line_g2, LINE_G3* line_g3,
    LINE_G4* line_g4, TILE* tiles, TILE_1* tiles_1, TILE_8* tiles_8, TILE_16* tiles_16, SPRT* sprites,
    SPRT_8* sprites_8, SPRT_16* sprites_16, DR_MOVE* draw_moves, DR_AREA* draw_areas, DR_MODE* draw_modes);
void equip_gfx_init_primitive_buffers(menu_gfx_context_t* context);
void equip_gfx_swap_context_and_clear_otag(s32 first_otag, s32 unused);

/* input */
extern u8 g_equip_input_lock_timer;
extern u8 g_equip_input_page_scroll_disabled;
extern u16 g_equip_input_page_scroll_hold_frames;

s32 equip_input_read_page_scroll_direction(void);

/* menu commands */
/* The opcode that ends an EQUIP menu display stream. */
enum {
    EQUIP_CMD_END = 0x19,
};

/* EQUIP's 0x3c-byte menu records: the main commands, the item actions and
 * (in equip_item_run_type_order_mode) the item-type order menus. Each is
 * started through equip_thread_start_managed, which makes it the current
 * g_menu_thread_menu_data. */
extern menu_entry_t g_equip_command_menu;

menu_instruction_t* menu_cmd_draw_line_f2_handler(menu_instruction_t* instruction);
menu_instruction_t* equip_cmd_draw_translucent_tile_handler(menu_instruction_t* instruction);

/* text */
extern u8 g_equip_text_data[];
/* The "%d" format string at the head of the overlay. */
extern const char g_equip_text_decimal_format[];
extern u8 g_equip_text_restore_pending;

void equip_text_show_message_with_sound(s32 message_id, s32 sound_id);

/* bit streams */

/* threads */
/* Slots of the equip screen's own threads. The WORLD formation screen runs
 * the same threads in other slots (world_thread_slot_e and
 * thread_screen_slot_e); here BATTLE's suspended menu threads stay in 7 and 8. */
typedef enum equip_thread_slot {
    EQUIP_THREAD_SLOT_POPUP_MENU = 3,         /* the item action and type order menus */
    EQUIP_THREAD_SLOT_COMMAND_MENU_CHILD = 4, /* a wait-forever thread while a command's mode runs */
    EQUIP_THREAD_SLOT_COMMAND_MENU = 5,       /* menu_icon_list_thread */
    EQUIP_THREAD_SLOT_EQUIPMENT_LIST = 10,    /* equip_panel_run_equipment_list_thread */
    EQUIP_THREAD_SLOT_ITEM_NUMERIC = 11,      /* the numeric editor thread of the item panel */
    EQUIP_THREAD_SLOT_UNIT_STATUS = 13,       /* equip_panel_run_unit_status_thread */
    EQUIP_THREAD_SLOT_COMPARISON_STATUS = 14, /* the second banner slot the indicator draw watches */
} equip_thread_slot_e;

extern s32 g_equip_item_numeric_thread_enabled;
extern s8 g_equip_menu_thread_running;
extern s16 g_equip_text_message_thread_active;
extern u8 g_equip_text_thread_running_state;
extern u8 g_equip_thread_idle_stop_pending;
extern menu_gradient_panel_prims_t g_equip_thread_indicator_packets[2][2];

/* sound */

extern u8 g_equip_sound_suppress_queued;

/* unknown */
extern s16 D_801d86ac;

#endif
