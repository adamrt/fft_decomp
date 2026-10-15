#ifndef EVENT_BUNIT_H
#define EVENT_BUNIT_H

/* EVENT/BUNIT.OUT: menu screen overlay that runs with BATTLE. */

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
extern u8 g_bunit_saved_status_banner_enabled;
extern u8 g_bunit_saved_unit_browse_enabled;

s32 bunit_entrypoint(s32 unused_unit, void* unused_otag);

/* unit */
/* Unit List menu snapshot built from unit_stats_t, with display values and
 * the battle_id used to return the selected unit. */
typedef struct bunit_unit {
    s16 level;      /* 0x00; getters at 0x801c4054..0x801c4163 read level, experience, brave and faith */
    s16 team_kind;  /* 0x02 */
    s16 list_index; /* 0x04 */
    s16 unit_count; /* 0x06; shown units plus eggs in each billboard record */
    s16 experience; /* 0x08 */
    s16 entd_slot;  /* 0x0a */
    menu_unit_status_gauge_t gauges[3]; /* 0x0c; HP, MP and CT */
    s16 formation_index;                /* 0x1e */
    u8 _unused_020[2];
    s16 name_index; /* 0x22 */
    s16 job_id;     /* 0x24; used when character_identity is 0x82 */
    s16 brave;      /* 0x26 */
    s16 faith;      /* 0x28 */
    s16 zodiac;     /* 0x2a */
    s16 battle_id;  /* 0x2C; searched by bunit_unit_find_index_by_battle_id */
    u8 _unused_02e[2];
    s16 move;                     /* 0x30 */
    s16 speed;                    /* 0x32 */
    s16 jump;                     /* 0x34 */
    s16 right_weapon_power;       /* 0x36 */
    s16 left_weapon_power;        /* 0x38 */
    s16 right_weapon_evade;       /* 0x3a */
    s16 left_weapon_evade;        /* 0x3c */
    s16 uses_monster_skillset;    /* 0x3e; nonzero restricts the job list to its base entry */
    s16 two_handed;               /* 0x40 */
    s16 physical_attack;          /* 0x42 */
    s16 physical_class_evade;     /* 0x44 */
    s16 physical_shield_evade;    /* 0x46 */
    s16 physical_accessory_evade; /* 0x48 */
    u8 _unused_04a[2];
    s16 magical_attack;          /* 0x4c */
    s16 magical_class_evade;     /* 0x4e */
    s16 magical_shield_evade;    /* 0x50 */
    s16 magical_accessory_evade; /* 0x52 */
    s16 equipment[5];            /* 0x54 */
    s16 abilities[5];            /* 0x5e; primary skillset, secondary, reaction, support, movement (filled by
                                  * bunit_unit_build_record_from_stats; bunit_ability_build_list reads [0]) */
    u8 _unused_068[8];           /* 0x68; spare tail of the stat-detail block */
    u8 gender_flags;             /* 0x70; Bard/Dancer restrictions */
    u8 runtime_id;               /* 0x71 */
    u8 initial_team_flags;       /* 0x72 */
    u8 mount_info;               /* 0x73; unit_stats_t.mount_info */
    u8 character_identity;       /* 0x74; special character, generic male/female/monster */
    u8 unlocked_jobs[UNIT_UNLOCKED_JOB_BYTE_COUNT]; /* 0x75; bit reader consumes Squire through Mime */
    u8 learned_abilities[UNIT_LEARNED_ABILITY_JOB_COUNT][UNIT_LEARNED_ABILITY_BYTES_PER_JOB]; /* 0x78 */
    u8 job_levels[UNIT_JOB_LEVEL_BYTE_COUNT]; /* 0xb1; two packed job-level nibbles per byte */
    u8 _padding_0bb;                          /* aligns job_points */
    unit_job_points_t job_points;             /* 0xbc; current JP followed by total JP */
} bunit_unit_t;
typedef char bunit_unit_size_must_be_268[(sizeof(bunit_unit_t) == 268) ? 1 : -1];

extern s8 g_bunit_selected_battle_id;
extern s16 g_bunit_shown_unit_indices[];
extern u8 g_bunit_unit_browse_enabled;

extern s16 g_bunit_unit_comparison_index;
extern s16 g_bunit_unit_count;
extern s16 g_bunit_unit_current_stat_value;
extern bunit_unit_t* g_bunit_unit_pointers[];
extern s8 g_bunit_unit_dot_glow_direction;
extern s8 g_bunit_unit_dot_glow_timer;
extern oriented_quad_t g_bunit_unit_dot_sprite;
extern menu_draw_number_command_t g_bunit_unit_grid_brave_number;
extern menu_draw_number_command_t g_bunit_unit_grid_brave_number_wide;
extern u16 g_bunit_unit_grid_ct_gauge_clut;
extern rgb24_t g_bunit_unit_grid_ct_gauge_colors[4];
extern point16_t g_bunit_unit_grid_cursor_target;
extern rgb24_t g_bunit_unit_grid_dimmed_color;
extern menu_draw_number_command_t g_bunit_unit_grid_experience_number;
extern menu_draw_number_command_t g_bunit_unit_grid_faith_number;
extern menu_draw_number_command_t g_bunit_unit_grid_faith_number_wide;
extern s8 g_bunit_unit_grid_gauge_offsets[];
extern u16 g_bunit_unit_grid_hp_gauge_clut;
extern rgb24_t g_bunit_unit_grid_hp_gauge_colors[4];
extern menu_sprite_cell_t g_bunit_unit_grid_label_tiles[];
extern menu_sprite_cell_t g_bunit_unit_grid_label_tiles_bottom[];
extern menu_draw_number_command_t g_bunit_unit_grid_level_number;
extern menu_draw_number_command_t g_bunit_unit_grid_max_stat_number;
extern u16 g_bunit_unit_grid_mp_gauge_clut;
extern rgb24_t g_bunit_unit_grid_mp_gauge_colors[4];
extern rgb24_t g_bunit_unit_grid_normal_color;
extern u8 g_bunit_unit_grid_selected_cell;
extern menu_instruction_t g_bunit_unit_grid_stat_bar_script_bottom;
extern menu_instruction_t g_bunit_unit_grid_stat_bar_script_top;
extern menu_draw_number_command_t g_bunit_unit_grid_stat_number;
extern u8 g_bunit_unit_list_count;
extern u8 g_bunit_unit_list_index_map[];
extern u8 g_bunit_unit_list_initialized;
extern s8 g_bunit_unit_list_page_offset;
extern s8 g_bunit_unit_list_scroll_step;
extern menu_entry_t g_bunit_unit_list_triangle_menu;
extern s16 g_bunit_unit_max_stat_value;
extern bunit_unit_t g_bunit_unit_records[];
extern u8 g_bunit_unit_reorderable_count;
extern u8 g_bunit_unit_reorderable_indices[];
extern oriented_quad_t g_bunit_unit_shadow_sprite;
/* 16-bit field index, per sort key, into the 0x10C-byte formation record
 * built by bunit_unit_build_data_list. The name is provisional. */
extern s8 g_bunit_unit_sort_field_indices[];
extern u8 g_bunit_unit_stat_mode; /* menu_unit_readout_e */

void bunit_unit_build_list_index_map(void);
s32 bunit_unit_build_reorderable_index_list(void);
void bunit_unit_copy_orders_to_reorder_list(s32 unused, s16* entries);
void bunit_unit_copy_reorder_list_to_orders(s32 unused, const s16* entries);
void bunit_unit_copy_selected_data(void);
s32 bunit_unit_find_index_by_battle_id(s16 battle_id);
s32 bunit_unit_get_brave(s32 unit_id);
s32 bunit_unit_get_current_stat(s32 unit_index);
s32 bunit_unit_get_experience(s32 unit_id);
s32 bunit_unit_get_faith(s32 unit_id);
s32 bunit_unit_get_level(s32 unit_id);
s32 bunit_unit_get_max_stat(s32 unit_index);
void bunit_unit_sort_index_list_by_order_keys(s32 sort_mode);

/* ability */
extern s8 g_bunit_ability_list_confirm_prompt_state;

extern s16 g_bunit_ability_list_job_name_row[];
extern u8 g_bunit_ability_list_ready;

/* job */
extern u8 g_bunit_job_list_initialized;

extern s16 g_bunit_job_selected_generic_index;

s32 bunit_job_build_unit_job_list(s32 unit_id, s16* job_ids, s32 unused_2);

/* comparison */
extern s32 g_bunit_comparison_display_flags;
extern s32 g_bunit_comparison_display_offset_y;
extern s32 g_bunit_comparison_display_redraw_request;

/* character status */
extern u8 g_bunit_character_status_ability_text_image[];
extern RECT g_bunit_character_status_ability_text_rect;
extern s32 g_bunit_character_status_dimmed;
extern u8 g_bunit_character_status_equipment_text_image[];
extern RECT g_bunit_character_status_equipment_text_rect;
extern menu_status_thread_t g_bunit_character_status_frame_config;
extern RECT g_bunit_character_status_frame_rect;
extern menu_status_panel_buffer_t g_bunit_character_status_frames[2];
extern image_load_parameters_t g_bunit_character_status_item_icon_layout[5];
extern image_load_parameters_t g_bunit_character_status_layout_mode0[19];
extern image_load_parameters_t g_bunit_character_status_layout_mode1[19];
extern image_load_parameters_t g_bunit_character_status_layout_mode2[19];
extern image_load_parameters_t g_bunit_character_status_layout_mode3[19];
extern RECT g_bunit_character_status_sprite_origin;

/* unit panels */
extern palette_t g_bunit_panel_comparison_portrait_palettes[2];
extern s16 g_bunit_panel_comparison_unit_data[];
extern palette_t g_bunit_panel_selected_portrait_palettes[2];
extern s16 g_bunit_panel_selected_unit_data[];
extern s32 g_bunit_status_display_dimmed;
extern u32 g_bunit_status_display_flags; /* menu_status_panel_flags_e */
extern s32 g_bunit_status_display_offset_y;

void bunit_panel_toggle_equipment_and_numeric_editor_threads(s32 enable);
void bunit_unit_build_record_from_stats(const struct unit_stats* unit, bunit_unit_t* record, s32 unused_slot);
s32 bunit_unit_build_data_list(s32 mode, s32 unit_data, s32 sort_mode);

/* numeric editor */
extern s32 g_bunit_numeric_editor_dimmed;
extern menu_status_thread_t g_bunit_numeric_editor_thread_params;

/* menus */

/* Display opcodes use .value for predicates and numbers, .icon for sprite sources. */
enum { BUNIT_MENU_SELECTION_RECORD_COUNT = 14 };
/* g_bunit_menu_current_menu: what bunit_entrypoint runs. */
typedef enum bunit_menu {
    BUNIT_MENU_UNIT_LIST = -1,   /* bunit_menu_run_unit_list_frame: the grid and its triangle menu */
    BUNIT_MENU_REORDER_LIST = 0, /* bunit_menu_run_reorder_list, the triangle menu's first row */
    BUNIT_MENU_JOB = 2,          /* bunit_menu_update_job: the selected unit's jobs and abilities */
} bunit_menu_e;

extern menu_instruction_t g_bunit_ability_list_menu_script;
extern menu_instruction_t g_bunit_job_list_menu_script;
extern s8 g_bunit_job_menu_active;
extern s8 g_bunit_job_menu_phase; /* menu_job_list_step_e */
/* Signed 4.12 x/y scale pair per sprite entry; a negative scale flips the quad. */
extern s16 g_bunit_menu_button_piece_scales[][2];
extern clut_t g_bunit_menu_clut;
extern s32 g_bunit_menu_current_menu; /* bunit_menu_e */
extern u16 g_bunit_menu_list_cursor_anim_state;

extern s16 g_bunit_menu_list_scroll_entry_ids[];
extern s16 g_bunit_menu_primary_result;
extern menu_entry_t g_bunit_menu_reorder_list_desc[];
extern s16 g_bunit_menu_reorder_list_entries[];
extern s16 g_bunit_menu_reorder_list_index;
extern u8 g_bunit_menu_reorder_list_initialized;

extern s16 g_bunit_menu_reorder_list_redraw_request;
extern s16 g_bunit_menu_reorder_result;
extern menu_button_piece_t g_bunit_slot_sprite_entries[];
/* Text-section buffers: bunit_menu_run_reorder_list passes g_bunit_text_reorder_list_buffer
 * straight to bunit_text_concatenate_entries as its u8* output string and
 * g_bunit_text_menu_section as the text-section base; bunit_gfx_init_vram_and_start_fade
 * installs both addresses as u8* text-section pointers. */
extern u8 g_bunit_text_menu_section[];
extern u8 g_bunit_unit_list_triangle_menu_open;

void bunit_menu_draw_pressable_button(s16 slot, s16 x, s16 y, s16 pressed, s16 busy, u16 sound_id);
void bunit_menu_init_unit_list(s32 unused_selected_index);
s32 bunit_menu_is_active_or_transitioning(void);
s32 bunit_menu_is_selected_unit_in_upper_rows(s32 page_offset);
s32 bunit_menu_is_unit_list_scroll_needed(void);
void bunit_menu_reset_results(void);
s32 bunit_menu_run_descriptor_thread(s32 id, menu_entry_t* desc);
void bunit_menu_run_reorder_list(void);
s32 bunit_menu_run_unit_list_frame(void);
void bunit_menu_update_job(void);
void bunit_menu_update_node_cursor(s16 mode);
void bunit_menu_update_unit_browser(void);
s32 bunit_menu_update_unit_grid(s16 allow_banner_toggle, s16 allow_browse, s16 show_stat_panel, s16 allow_stat_cycle,
    s16 stat_mode, s16 buttons, s16 locked);
void bunit_menu_update_unit_list_scroll(s8* step, s8* offset);

/* help */
extern u16 g_bunit_help_cursor_anim_state;
extern u8 g_bunit_help_cursor_initialized;
/* g_bunit_help_nodes_unit_browse[23].help_base and [24].help_base as their own symbols: as element
 * accesses GCC derives the second store and the later table address from one base register. */
extern s16 g_bunit_help_node_23_help_base;
extern s16 g_bunit_help_node_24_help_base;
extern u8 g_bunit_help_node_index;
extern u8 g_bunit_help_node_index_reload;
extern u8 g_bunit_help_node_index_reload_2;
extern help_navigation_record_t* g_bunit_help_nodes;
extern help_navigation_record_t g_bunit_help_nodes_banner_bottom[];
extern help_navigation_record_t g_bunit_help_nodes_banner_top[];
/* Aliases of g_bunit_help_nodes/g_bunit_help_node_index; the target reloads them at each use
 * instead of reusing the cached copies, which separate symbols reproduce. */
extern help_navigation_record_t* g_bunit_help_nodes_reload;
extern help_navigation_record_t g_bunit_help_nodes_unit_browse[];
extern s32 g_bunit_help_screen_id; /* menu_grid_help_id_e */

/* cursor */
/* Oriented-quad descriptors for the two cursor halves. */
extern oriented_quad_t g_bunit_cursor_shadow_sprite;
extern oriented_quad_t g_bunit_cursor_sprite;
extern u8 g_bunit_cursor_trail_brightness[];
/* Rhombus-cursor trail, oldest point first; x == -1 marks an empty slot
 * (bunit_gfx_init_rhombus_cursor_tpages clears all eight). */
extern point16_t g_bunit_cursor_trail_points[8];

/* graphics */
extern textured_quad_source_t g_bunit_bouncing_cursor_quad;
extern u16 g_bunit_gfx_background_clut;
extern u16 g_bunit_gfx_background_tile_layout[2][8]; /* per row parity: four (x, u) column pairs */

/* Base of the two frame buffers; g_menu_gfx_context alternates between them. */
extern menu_gfx_context_t* g_bunit_gfx_context_base;
extern u16 g_bunit_gfx_draw_area_capacity;
extern u16 g_bunit_gfx_draw_move_capacity;
extern s16 g_bunit_gfx_falloff_focus_x;
extern s16 g_bunit_gfx_falloff_focus_y;
extern s16 g_bunit_gfx_line_f2_count;
extern s16 g_bunit_gfx_poly_f4_count;
extern u8 g_bunit_sprite_cell_sizes[][2];

s32 bunit_gfx_calculate_distance_falloff(s32 x, s32 y, s32 base_value, s32 minimum);
void bunit_gfx_draw_falloff_shaded_rows(s32 y);
void bunit_gfx_draw_unit_sprite_and_name_plate(s16 unit, s32 x, s32 y);
void bunit_gfx_init_double_packet_buffers(menu_gfx_context_t* buffers, u32* otags, POLY_F3* flat_triangles,
    POLY_FT3* textured_triangles, POLY_F4* flat_quads, POLY_FT4* textured_quads, POLY_G3* gouraud_triangles,
    POLY_GT3* gouraud_textured_triangles, POLY_G4* gouraud_quads, POLY_GT4* gouraud_textured_quads, LINE_F2* flat_lines,
    LINE_F3* flat_lines_3, LINE_F4* flat_lines_4, LINE_G2* gouraud_lines, LINE_G3* gouraud_lines_3,
    LINE_G4* gouraud_lines_4, TILE* tiles, TILE_1* tiles_1, TILE_8* tiles_8, TILE_16* tiles_16, SPRT* sprites,
    SPRT_8* sprites_8, SPRT_16* sprites_16, DR_MOVE* draw_moves, DR_AREA* draw_areas, DR_MODE* draw_modes);
void bunit_gfx_init_packet_pools(menu_gfx_context_t* buffer);
void bunit_gfx_init_rhombus_cursor_tpages(void);
void bunit_gfx_init_vram_and_start_fade(void);
void bunit_gfx_submit_frame_and_swap_buffers(s32 first_otag, s32 draw_otag);
void bunit_gfx_update_rhombus_cursor_trail(point16_t* target);

/* input */
/* Named fields hoist the address; keep the 32-bit event-input alias for scalar accesses. */
extern menu_input_state_t g_bunit_input_event_state;
extern u16 g_bunit_input_latched_button;
extern u16 g_bunit_input_previous;
extern u32 g_bunit_input_primary_repeat;
extern u32 g_bunit_input_secondary_repeat;

s32 bunit_input_get_lock_timer(void);
s32 bunit_input_read_pad1_unless_locked(void);
void bunit_input_update_controller(void);
void bunit_input_update_lock_timer(void);

/* menu commands */
/* The opcode that ends a BUNIT menu display stream. */
enum {
    BUNIT_CMD_END = 0x1c,
};

/* text */
extern u8 g_bunit_help_text_state;
extern const char g_bunit_text_decimal_format[];
extern u8 g_bunit_text_reorder_list_buffer[];

void bunit_text_start_selection_thread(s32 thread_data);

/* bit streams */
extern const u8* g_bunit_bits_cursor;
extern s32 g_bunit_bits_index;
extern u8 g_bunit_bits_reader_reset;

void bunit_bits_init_reader(const u8* data);
u32 bunit_bits_read(s32 bit_count);

/* threads */
/* Slots of the unit list screen's own menus; thread_screen_slot_e
 * has the shared ones. */
typedef enum bunit_thread_slot {
    BUNIT_THREAD_SLOT_MENU = 5,          /* the unit list's triangle menu */
    BUNIT_THREAD_SLOT_REORDER_MENU = 15, /* the reorder list */
} bunit_thread_slot_e;

extern u8 g_bunit_menu_thread_running;

void bunit_menu_update_event_thread_state(void);

/* sound */

/* status */
extern u8 g_bunit_status_banner_at_bottom;
extern u8 g_bunit_status_banner_at_bottom_latch;
extern u8 g_bunit_status_banner_enabled;
extern s8 g_bunit_status_banner_slide_step;

#endif
