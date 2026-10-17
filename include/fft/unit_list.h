#ifndef UNIT_LIST_H
#define UNIT_LIST_H

/* The Unit List screen, EVENT/BUNIT.OUT (a BATTLE overlay): its unit records and sorting, the unit grid, the reorder
 * list, the job page, the status banner, help, cursors, graphics, input and text. */

#include "fft/gfx.h"
#include "fft/help.h"
#include "fft/job.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* screen */
/* g_unit_list_current_menu: what bunit_entrypoint runs. */
typedef enum unit_list_menu {
    UNIT_LIST_MENU_UNIT_LIST = -1,   /* unit_list_run_frame: the grid and its triangle menu */
    UNIT_LIST_MENU_REORDER_LIST = 0, /* unit_list_run_sort_menu, the triangle menu's first row */
    UNIT_LIST_MENU_JOB = 2,          /* unit_list_update_job_page: the selected unit's jobs and abilities */
} unit_list_menu_e;

enum { UNIT_LIST_SELECTION_RECORD_COUNT = 14 };

extern s32 g_unit_list_current_menu; /* unit_list_menu_e */
extern s16 g_unit_list_primary_result;
extern s8 g_unit_list_selected_battle_id;
extern s16 g_unit_list_comparison_index;
extern u8 g_unit_list_saved_status_banner_enabled;
extern u8 g_unit_list_saved_unit_browse_enabled;
extern clut_t g_unit_list_clut;

/* Run BUNIT's Unit List loop until it closes and return the chosen battle unit. */
s32 bunit_entrypoint(s32 unused_unit, void* unused_otag);
void unit_list_reset_results(void);
s32 unit_list_is_busy(void);

/* The event library's thread entry for the menu row that opens the Unit List: it runs the screen's transition in a
 * child thread slot and waits for it. */
void unit_list_open(void);

/* menu threads */
/* Slots of the unit list screen's own menus; thread_screen_slot_e
 * has the shared ones. */
typedef enum unit_list_thread_slot {
    UNIT_LIST_THREAD_SLOT_MENU = 5,          /* the unit list's triangle menu */
    UNIT_LIST_THREAD_SLOT_REORDER_MENU = 15, /* the reorder list */
} unit_list_thread_slot_e;

extern u8 g_unit_list_thread_running;

s32 unit_list_run_menu_thread(s32 id, menu_entry_t* desc);

/* unit records */
/* Unit List menu snapshot built from unit_stats_t, with display values and
 * the battle_id used to return the selected unit. */
typedef struct unit_list_record {
    s16 level;            /* 0x00; unit_list_get_level, _experience, _brave and _faith read the four stats */
    s16 team_kind;        /* 0x02 */
    s16 list_index;       /* 0x04 */
    s16 unit_count;       /* 0x06; shown units plus eggs in each billboard record */
    s16 experience;       /* 0x08 */
    s16 active_battle_id; /* 0x0a */
    menu_unit_status_gauge_t gauges[3]; /* 0x0c; HP, MP and CT */
    s16 formation_index;                /* 0x1e */
    u8 _unused_020[2];
    s16 name_index; /* 0x22 */
    s16 job_id;     /* 0x24; used when character_identity is 0x82 */
    s16 brave;      /* 0x26 */
    s16 faith;      /* 0x28 */
    s16 zodiac;     /* 0x2a */
    s16 battle_id;  /* 0x2C; searched by unit_list_find_by_battle_id */
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
                                  * unit_list_build_record; ability_list_build reads [0]) */
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
} unit_list_record_t;
typedef char unit_list_record_size_must_be_268[(sizeof(unit_list_record_t) == 268) ? 1 : -1];

extern unit_list_record_t* g_unit_list_pointers[];

/* 16-bit field index into unit_list_record_t, per sort key, used by
 * unit_list_sort. The name is provisional. */
extern s8 g_unit_list_sort_field_indices[];

s32 unit_list_build_records(s32 mode, s32 unit_data, s32 sort_mode);
s32 unit_list_find_by_battle_id(s16 battle_id);
void unit_list_sort(s32 sort_mode);

/* unit list */
extern u8 g_unit_list_initialized;
extern u8 g_unit_list_visible_count;
extern u8 g_unit_list_index_map[];
extern s8 g_unit_list_page_offset;
extern s8 g_unit_list_scroll_step;
extern u8 g_unit_list_browse_enabled;
extern menu_entry_t g_unit_list_triangle_menu;
extern u8 g_unit_list_triangle_menu_open;

void unit_list_init(s32 unused_selected_index);
void unit_list_build_index_map(void);
s32 unit_list_is_scroll_needed(void);

/* unit grid */
extern u8 g_unit_list_grid_selected_cell;
extern point16_t g_unit_list_grid_cursor_target;
extern u8 g_unit_list_stat_mode; /* menu_unit_readout_e */
extern s16 g_unit_list_current_stat_value;
extern s16 g_unit_list_max_stat_value;
extern menu_sprite_cell_t g_unit_list_grid_label_tiles[8];
extern menu_sprite_cell_t g_unit_list_grid_label_tiles_bottom[8];
extern menu_instruction_t g_unit_list_grid_stat_bar_script_top;
extern menu_instruction_t g_unit_list_grid_stat_bar_script_bottom;
extern menu_draw_number_command_t g_unit_list_grid_level_number;
extern menu_draw_number_command_t g_unit_list_grid_experience_number;
extern menu_draw_number_command_t g_unit_list_grid_brave_number;
extern menu_draw_number_command_t g_unit_list_grid_brave_number_wide;
extern menu_draw_number_command_t g_unit_list_grid_faith_number;
extern menu_draw_number_command_t g_unit_list_grid_faith_number_wide;
extern menu_draw_number_command_t g_unit_list_grid_stat_number;
extern menu_draw_number_command_t g_unit_list_grid_max_stat_number;
extern s8 g_unit_list_grid_gauge_offsets[];
extern u16 g_unit_list_grid_hp_gauge_clut;
extern rgb24_t g_unit_list_grid_hp_gauge_colors[4];
extern u16 g_unit_list_grid_mp_gauge_clut;
extern rgb24_t g_unit_list_grid_mp_gauge_colors[4];
extern u16 g_unit_list_grid_ct_gauge_clut;
extern rgb24_t g_unit_list_grid_ct_gauge_colors[4];
extern rgb24_t g_unit_list_grid_normal_color;
extern rgb24_t g_unit_list_grid_dimmed_color;
extern oriented_quad_t g_unit_list_dot_sprite;
extern oriented_quad_t g_unit_list_shadow_sprite;
extern s8 g_unit_list_dot_glow_direction;
extern s8 g_unit_list_dot_glow_timer;
extern menu_button_piece_t g_unit_list_slot_sprite_entries[];
extern u8 g_unit_list_sprite_cell_sizes[][2];

/* Signed 4.12 x/y scale pair per sprite entry; a negative scale flips the quad. */
extern s16 g_unit_list_button_piece_scales[][2];

s32 unit_list_update_grid(s16 allow_banner_toggle, s16 allow_browse, s16 show_stat_panel, s16 allow_stat_cycle,
    s16 stat_mode, s16 buttons, s16 locked);

/* reorder list */
extern u8 g_unit_list_reorder_list_initialized;
extern menu_entry_t g_unit_list_reorder_list_desc[];
extern s16 g_unit_list_reorder_list_entries[];
extern s16 g_unit_list_reorder_list_index;
extern s16 g_unit_list_reorder_list_redraw_request;
extern s16 g_unit_list_reorder_result;
extern u8 g_unit_list_reorderable_count;
extern u8 g_unit_list_reorderable_indices[];

void unit_list_run_sort_menu(void);
s32 unit_list_build_sortable(void);
void unit_list_load_sort_order(s32 unused, s16* entries);
void unit_list_store_sort_order(s32 unused, const s16* entries);

/* job page */
void unit_list_update_job_page(void);
s32 unit_list_build_job_list(s32 unit_id, s16* job_ids, s32 unused_2);

/* status banner */
extern u8 g_unit_list_status_banner_enabled;
extern u8 g_unit_list_status_banner_at_bottom;
extern u8 g_unit_list_status_banner_at_bottom_latch;
extern s8 g_unit_list_status_banner_slide_step;

/* help */
extern s32 g_unit_list_help_screen_id; /* help_grid_id_e */
extern help_navigation_record_t* g_unit_list_help_nodes;
extern help_navigation_record_t g_unit_list_help_nodes_unit_browse[];
extern help_navigation_record_t g_unit_list_help_nodes_banner_top[];
extern help_navigation_record_t g_unit_list_help_nodes_banner_bottom[];
extern u8 g_unit_list_help_node_index;
extern u16 g_unit_list_help_cursor_anim_state;
extern u8 g_unit_list_help_cursor_initialized;

/* Aliases of g_unit_list_help_nodes/g_unit_list_help_node_index; the target reloads them at each use
 * instead of reusing the cached copies, which separate symbols reproduce. */
extern help_navigation_record_t* g_unit_list_help_nodes_reload;
extern u8 g_unit_list_help_node_index_reload;
extern u8 g_unit_list_help_node_index_reload_2;

/* g_unit_list_help_nodes_unit_browse[23].help_base and [24].help_base as their own symbols: as element
 * accesses GCC derives the second store and the later table address from one base register. */
extern s16 g_unit_list_help_node_23_help_base;
extern s16 g_unit_list_help_node_24_help_base;

void unit_list_update_help_cursor(s16 mode);

/* cursors */
extern textured_quad_source_t g_unit_list_bouncing_cursor_quad;

/* Oriented-quad descriptors for the two cursor halves. */
extern oriented_quad_t g_unit_list_cursor_sprite;
extern oriented_quad_t g_unit_list_cursor_shadow_sprite;

/* Rhombus-cursor trail, oldest point first; x == -1 marks an empty slot
 * (unit_list_init_cursor_trail clears all eight). */
extern point16_t g_unit_list_cursor_trail_points[8];
extern u8 g_unit_list_cursor_trail_brightness[];

/* graphics */
extern u16 g_unit_list_gfx_draw_area_capacity;
extern u16 g_unit_list_gfx_draw_move_capacity;
extern s16 g_unit_list_gfx_line_f2_count;
extern u16 g_unit_list_gfx_background_clut;
extern u16 g_unit_list_gfx_background_tile_layout[2][8]; /* per row parity: four (x, u) column pairs */
extern s16 g_unit_list_gfx_falloff_focus_x;
extern s16 g_unit_list_gfx_falloff_focus_y;

/* Volatile capacity reads preserve the original packet-binder load scheduling. */
extern volatile u16 g_unit_list_gfx_line_g2_capacity;
extern volatile u16 g_unit_list_gfx_line_g3_capacity;

/* Base of the two frame buffers; g_menu_gfx_context alternates between them. */
extern menu_gfx_context_t* g_unit_list_gfx_context_base;

void unit_list_gfx_init_buffers(menu_gfx_context_t* buffers, u32* otags, POLY_F3* flat_triangles,
    POLY_FT3* textured_triangles, POLY_F4* flat_quads, POLY_FT4* textured_quads, POLY_G3* gouraud_triangles,
    POLY_GT3* gouraud_textured_triangles, POLY_G4* gouraud_quads, POLY_GT4* gouraud_textured_quads, LINE_F2* flat_lines,
    LINE_F3* flat_lines_3, LINE_F4* flat_lines_4, LINE_G2* gouraud_lines, LINE_G3* gouraud_lines_3,
    LINE_G4* gouraud_lines_4, TILE* tiles, TILE_1* tiles_1, TILE_8* tiles_8, TILE_16* tiles_16, SPRT* sprites,
    SPRT_8* sprites_8, SPRT_16* sprites_16, DR_MOVE* draw_moves, DR_AREA* draw_areas, DR_MODE* draw_modes);
void unit_list_gfx_submit_frame(s32 first_otag, s32 draw_otag);
void unit_list_draw_shaded_rows(s32 y);

/* input */
extern u16 g_unit_list_input_latched_button;
extern u16 g_unit_list_input_previous;
extern u32 g_unit_list_input_primary_repeat;
extern u32 g_unit_list_input_secondary_repeat;

/* Named fields hoist the address; keep the 32-bit event-input alias for scalar accesses. */
extern menu_input_state_t g_unit_list_input_event_state;

s32 unit_list_get_input_lock(void);
void unit_list_update_input(void);

/* menu commands */
/* The opcode that ends a BUNIT menu display stream. */
enum {
    UNIT_LIST_SCRIPT_END = 0x1c,
};

/* text */
extern u8 g_unit_list_help_text_state;

/* Text-section buffers: unit_list_run_sort_menu passes g_unit_list_text_reorder_list_buffer
 * straight to text_concatenate_entries as its u8* output string and
 * g_unit_list_text_menu_section as the text-section base; unit_list_init_vram_and_fade
 * installs both addresses as u8* text-section pointers. */
extern u8 g_unit_list_text_menu_section[];
extern u8 g_unit_list_text_reorder_list_buffer[];

void unit_list_start_text_thread(s32 thread_data);

/* bit streams */
extern const u8* g_unit_list_bits_cursor;
extern s32 g_unit_list_bits_index;
extern u8 g_unit_list_bits_reader_reset;

#endif
