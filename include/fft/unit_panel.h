#ifndef UNIT_PANEL_H
#define UNIT_PANEL_H

/* The unit status panels BATTLE, WORLD, WLDCORE and the EVENT overlays draw (the status banner, the equipment and
 * ability panels and the numeric panel): the selected and comparison unit records, the panel threads, packets, status
 * icons, portraits and cell layouts, and each overlay's own panel state. */

#include "fft/battle_map.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_stack.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* selected and comparison units */
/* The unit a status panel shows, saved and restored whole by unit_panel_copy_billboard_out and
 * unit_panel_copy_billboard_in. unit_panel_load_selected fills job_id..zodiac for the unit named by battle_id; the
 * overlays' status panels keep a second record for the comparison unit (g_unit_panel_comparison_unit_data).
 * Provisional. */
typedef struct unit_panel_unit {
    /* 0x00: unit name text id less TEXT_ID_UNIT_NAME_BASE (DEBUGCHR, REQUIRE);
     * in ATTACK the roster index, a row of the names it builds at text 0x5800 */
    s16 name_id;
    s16 job_id;     /* 0x02 */
    s16 brave;      /* 0x04 */
    s16 faith;      /* 0x06 */
    s16 zodiac;     /* 0x08 */
    s16 battle_id;  /* 0x0a: selected unit */
    s16 _unused_0c; /* 0x0c */
} unit_panel_unit_t;
typedef char unit_panel_unit_size_must_be_14[(sizeof(unit_panel_unit_t) == 14) ? 1 : -1];

/* Complete status and preview record, copied whole by the billboard routines. */
typedef struct unit_panel_record {
    menu_status_panel_slot_state_t state;
    u8 _unused_38[8]; /* preserves the copied 0x40-byte record */
} unit_panel_record_t;
typedef char unit_panel_record_size_must_be_64[(sizeof(unit_panel_record_t) == 64) ? 1 : -1];

/* A unit's status summary: g_unit_panel_selected_unit_status (BATTLE's AT banner) and
 * g_unit_panel_comparison_unit_status (its status billboard), WORLD's formation records
 * g_unit_panel_selected_unit_stat_summary and g_formation_comparison_unit_stat_summary, and each overlay's pair of
 * panel billboards, each pair 0x24 apart. unit_panel_fill_billboard and unit_panel_build_unit_data fill the first 0x1e
 * bytes, unit_view_stats_panel_thread and the overlays' unit_panel_banner_thread draw it, and
 * unit_panel_copy_billboard_out saves the banner whole. Equipment previews use the HP and MP gauges' bonus
 * fields. Provisional. */
typedef struct unit_panel_summary {
    s16 level;      /* 0x00; debug_unit_editor_apply_values writes it back */
    s16 team_kind;  /* 0x02: 0 ally, 1 enemy, 2 neutral, 3 auto-battle; selects the status icon,
                       the panel's 0xc-byte geometry row and its CLUT */
    s16 list_index; /* 0x04: AT-list position + 1, negative when the unit is not listed
                       (drawn as dashes, format 0xc00) */
    s16 unit_count; /* 0x06 */
    s16 experience; /* 0x08; written back with level */
    s16 unit_index; /* 0x0a; the battle id in battle, the party index in the WORLD and WLDCORE panels */
    menu_unit_status_gauge_t gauges[3]; /* 0x0c: MENU_UNIT_STATUS_GAUGE_*; CT max is always 100 */
    s16 projected_display_value;        /* 0x1e */
    u16 projected_hit_percent;          /* 0x20 */
} unit_panel_summary_t;
typedef char unit_panel_summary_size_must_be_34[(sizeof(unit_panel_summary_t) == 34) ? 1 : -1];

extern s32 g_unit_panel_hide_numeric_values; /* non-zero: draw digits as blanks (0x48, 0x10) */

/* The selected panel unit can differ from the unit whose turn is active. Its record, status summary and
 * equipment and ability rows come first, then the comparison unit's. BATTLE, WORLD,
 * ATTACK, DEBUGCHR and REQUIRE define their own records. */
extern unit_panel_unit_t g_unit_panel_selected_unit;
extern unit_panel_summary_t g_unit_panel_selected_unit_status;
extern unit_panel_record_t g_unit_panel_selected_unit_fields;
extern unit_panel_summary_t g_unit_panel_selected_unit_stat_summary;

/* The unit shown beside the selected one in the comparison panels; REQUIRE
 * holds the same record. */
extern unit_panel_unit_t g_unit_panel_comparison_unit_data;
extern unit_panel_summary_t g_unit_panel_comparison_unit_status;

/* Target preview copies only generic_monster and the reaction ability here. */
extern unit_panel_record_t g_unit_panel_comparison_unit_fields;

/* The selected unit's names, as unit_panel_load_selected copies them for the panels.
 * BATTLE, WORLD, DEBUGCHR and REQUIRE define all four; ATTACK defines the skillset windows. */
extern u8 g_unit_panel_billboard_unit_name[];               /* name text window */
extern u8 g_unit_panel_billboard_job_name[];                /* job name text window */
extern u8 g_unit_panel_billboard_skillset_name[];           /* primary skillset name text window */
extern u8 g_unit_panel_billboard_secondary_skillset_name[]; /* secondary skillset name text window */

/* Fills the selected and comparison records, stats, equipment and ability names. */
void unit_panel_load_selected(void);

/* Copies the selected unit's records and the cursor tile into the caller's buffers; _in restores them without
 * the tile. */
void unit_panel_copy_billboard_out(unit_panel_summary_t* banner, unit_panel_unit_t* unit,
    unit_panel_record_t* billboard, battle_map_selected_tile_t* cursor_tile);
void unit_panel_copy_billboard_in(
    const unit_panel_summary_t* banner, const unit_panel_unit_t* unit, const unit_panel_record_t* billboard);

/* ATTACK's and REQUIRE's: rebuilds party slot party_index into a scratch unit and fills the selected unit's status
 * and editor records from it. */
void unit_panel_load_party_unit(s32 unused, s32 party_index);

/* WLDCORE's: loads party member party_index into g_unit_panel_unit and fills the status-panel records the
 * participant detail level draws. */
void unit_panel_build_unit_data(s32 party_index);

/* panel threads */
/* Parameter block of the task-0x3b status panel threads (WORLD's formation
 * panels and the overlays' character status and numeric panel threads): the
 * panel origin, a redraw request and the frame style read by
 * unit_panel_set_colors. The origin is read as whole 32-bit values (lw) for the
 * draw-area offset. Banner threads take the unsigned low half of x before
 * the GPU coordinate arithmetic; the stored field is a full 32-bit value. */
typedef struct unit_panel_thread {
    point32_t position; /* 0x00 */
    s32 flags;          /* 0x08: menu_status_panel_flags_e in the status display record */
    s32 redraw_request; /* 0x0c: set to 1 elsewhere to request a text/portrait rebuild; the status
                           display record also keeps the selection-slide direction here */
    s32 style;          /* 0x10: 1 dims equipment/numeric panels; any nonzero value dims the banner */
} unit_panel_thread_t;
typedef char unit_panel_thread_size_must_be_20[(sizeof(unit_panel_thread_t) == 20) ? 1 : -1];

/* Parameter blocks of the selected unit's and the comparison unit's status panels, the stat preview panel and
 * the numeric panel. */
extern unit_panel_thread_t g_unit_panel_status_display_thread_params;
extern unit_panel_thread_t g_unit_panel_comparison_display_thread_params;
extern unit_panel_thread_t g_unit_panel_preview_stats_thread_params;
extern unit_panel_thread_t g_unit_panel_numeric_panel_params;

/* Parameter blocks of the character status and numeric panel threads of BUNIT, DEBUGCHR and REQUIRE. */
extern unit_panel_thread_t g_unit_panel_character_status_frame_config;
extern unit_panel_thread_t g_unit_panel_numbers_thread_params;

/* Parameter blocks of WORLD's status banner, numeric panel and equipment/ability panel threads, which WLDCORE runs
 * in thread slots 8, 9 and 12. */
extern unit_panel_thread_t g_unit_panel_banner_params;
extern unit_panel_thread_t g_unit_panel_numbers_params;
extern unit_panel_thread_t g_unit_panel_panel_params;

/* Each starts its panel thread when enable is set and the thread is idle, and asks it to stop otherwise. */
void unit_panel_toggle_banner_thread(s32 enable);
void unit_panel_toggle_comparison_thread(s32 enable);
void unit_panel_toggle_numbers_thread(s32 enable);

/* The unit status panel thread; each program that shows the panel links its own body. */
void unit_panel_banner_thread(void);

/* panel packets */
/* Per-frame character status buffer (0x3d8 bytes), double buffered by both bodies of
 * unit_panel_equipment_ability_thread and the overlay list threads, which copy the first buffer
 * to the second (base + 0x3d8) before the render loop starts. Evidence:
 *  - the DR_MODE packets at +0x00/+0x0c/+0x18 are submitted individually, and
 *    the numeric threads fill the same slots with
 *    gfx_set_draw_mode_for_texture_page;
 *  - the submit loops walk the SPRTs from +0x24 with the sprite index as their
 *    counter (0..11 from +0x24, 12 at +0x114, 13 at +0x128, 14..18 from
 *    +0x13c, 19..23 from +0x1a0);
 *  - the TILE/LINE_F2 pair at +0x204/+0x224 (two translucent 16x90 backdrop
 *    tiles and eight vertical frame lines) is written, with the draw modes,
 *    by unit_panel_init_primitives, which leaves the sprites between untouched;
 *  - the two draw-offset packets follow from the SetDrawOffset(packet,
 *    position) calls at +0x2a4/+0x2b0 and +0x2b4/+0x2c0;
 *  - +0x2c4 and +0x3b0 are the arguments of
 *    status_screen_init_numbers_frame and the
 *    menu_init_scaled_draw_area_packets / menu_build_scaled_draw_area_packets
 *    pair. */
typedef struct unit_panel_buffer {
    DR_MODE frame_draw_mode;                         /* 0x000; MENU_TEXTURE_PAGE_FRAME */
    DR_MODE text_draw_mode;                          /* 0x00c; MENU_TEXTURE_PAGE_TEXT_IMAGES */
    DR_MODE image_draw_mode;                         /* 0x018; item icons or allocated images */
    SPRT sprites[24];                                /* 0x024 */
    TILE tiles[2];                                   /* 0x204 */
    LINE_F2 lines[8];                                /* 0x224 */
    menu_status_panel_draw_offset_t draw_offsets[2]; /* 0x2a4 */
    menu_palette_primitives_t numeric_frame;         /* 0x2c4 */
    scaled_draw_area_pair_t portrait;                /* 0x3b0 */
} unit_panel_buffer_t;
typedef char unit_panel_buffer_size_must_be_984[(sizeof(unit_panel_buffer_t) == 984) ? 1 : -1];

void unit_panel_init_primitives(unit_panel_buffer_t* menu);

/* The dimmed frame colors for style 1 or while the input lock mode is 1, otherwise the normal ones. */
void unit_panel_select_palette(menu_palette_primitives_t* primitives, const unit_panel_thread_t* thread);

/* The panel's sprite CLUTs and the tint of its last five sprites: dimmed and blue for style 1 or while the input
 * lock mode is 1, otherwise normal. */
void unit_panel_set_colors(unit_panel_buffer_t* primitives, const unit_panel_thread_t* state);

/* status icons */
/* ATTACK, DEBUGCHR and REQUIRE each define 76 status cells.
 * Group boundaries select spans drawn in reverse order. */
extern sprite_sheet_frame_t g_unit_panel_status_frames[76];
extern point16_t g_unit_panel_status_frame_scales[76];

/* ATTACK's status groups, drawn by its own unit_panel_build_status_groups: per-sprite first/end cell indices
 * (interleaved pairs), the visible-entry count, each entry's sprite id and fade/blink state, and a 4-byte record per
 * entry whose first 16-bit value biases the draw point's y. */
extern u8 g_unit_panel_status_group_bounds[];
extern u8 g_unit_panel_status_group_count;
extern u8 g_unit_panel_status_group_ids[];
extern u8 g_unit_panel_status_animation[];
extern sprite_draw_offset_t g_unit_panel_status_offsets[];

/* The status groups the menu library's unit_panel_build_status_groups draws in DEBUGCHR and REQUIRE. Each animated
 * group has a countdown byte in g_unit_panel_panel_status_animation; 0xff marks an inactive group. */
extern u8 g_unit_panel_panel_status_animation[];
extern u8 g_unit_panel_panel_status_group_count;
extern u8 g_unit_panel_panel_status_group_ids[];
extern u8 g_unit_panel_panel_status_group_bounds[];
extern sprite_draw_offset_t g_unit_panel_panel_status_offsets[];
extern const u16 g_unit_panel_panel_status_y_offsets[];

void unit_panel_build_status_groups(POLY_FT4* poly);

/* portraits */
extern palette_t g_unit_panel_selected_portrait_palettes[2];
extern palette_t g_unit_panel_comparison_portrait_palettes[2];

/* character status panel */
/* The selected unit's and the comparison unit's status panels, with their billboards, packets, numbers, name
 * and portrait. */
extern RECT g_unit_panel_character_status_draw_area_rect;
extern RECT g_unit_panel_frame_rect;

/* WORLD, ATTACK, BUNIT, DEBUGCHR and EQUIP each bind this down/up curve. */
extern s16 g_unit_panel_slide_y[2][6];
extern CVECTOR g_unit_panel_gauge_bar_colors[];
extern u16 g_unit_panel_editor_label_cluts[][2];
extern u16 g_unit_panel_editor_value_cluts[][2];
extern menu_status_panel_packet_t g_unit_panel_selected_packets[2];
extern menu_status_panel_editor_packet_t g_unit_panel_selected_editor_packets[2];
extern menu_number_entry_t g_unit_panel_selected_numeric_entries[];
extern u8 g_unit_panel_selected_name_image[];
extern u8 g_unit_panel_selected_number_image[];
extern u8 g_unit_panel_selected_small_number_image[];
extern RECT g_unit_panel_selected_portrait_rect;
extern menu_status_panel_packet_t g_unit_panel_comparison_packets[2];
extern menu_status_panel_editor_packet_t g_unit_panel_comparison_editor_packets[2];
extern menu_number_entry_t g_unit_panel_comparison_numeric_entries[];
extern u8 g_unit_panel_comparison_name_image[];
extern u8 g_unit_panel_comparison_number_image[];
extern u8 g_unit_panel_comparison_small_number_image[];
extern RECT g_unit_panel_comparison_portrait_rect;

/* equipment panel */
/* The selected unit's equipment and abilities, and the remove-equipment preview. */
/* WORLD, ATTACK, BUNIT, DEBUGCHR, EQUIP and REQUIRE each link these panel resources. */
extern unit_panel_buffer_t g_unit_panel_equipment_frames[2];
extern RECT g_unit_panel_equipment_frame_rects[];
extern RECT g_unit_panel_equipment_origins[];
extern const image_layout_t g_unit_panel_label_layouts_mode0[19];
extern const image_layout_t g_unit_panel_label_layouts_mode1[19];
extern const image_layout_t g_unit_panel_label_layouts_mode2[19];
extern const image_layout_t g_unit_panel_label_layouts_mode3[19];
extern const image_layout_t g_unit_panel_item_icon_layouts[MENU_EQUIPMENT_SLOT_COUNT];

/* The combined panel uses A for equipment and B for abilities; the separate panels reuse them for selected-unit
 * and item-preview text respectively. */
extern u8 g_unit_panel_text_image_a[3200];
extern u8 g_unit_panel_text_image_b[3200];
extern RECT g_unit_panel_text_upload_rect_a;
extern RECT g_unit_panel_text_upload_rect_b;

void unit_panel_equipment_ability_thread(void);

/* ATTACK, BUNIT, DEBUGCHR and REQUIRE share yield-and-exit stubs; EQUIP and WORLD each build panel bodies. */
void unit_panel_ability_thread(void);
void unit_panel_equipment_thread(void);

/* numeric panel */
extern menu_status_panel_numeric_geometry_t g_unit_panel_editor_numeric_table;
extern menu_status_panel_numeric_buffer_t g_unit_panel_editor_numeric_state[2];
extern menu_status_panel_numeric_buffer_t g_unit_panel_editor_numeric_state_a[2];
extern menu_status_panel_numeric_buffer_t g_unit_panel_editor_numeric_state_b[2];
extern menu_number_entry_t g_unit_panel_editor_numeric_descriptor_a[];
extern menu_number_entry_t g_unit_panel_editor_numeric_descriptor_b[];
extern RECT g_unit_panel_editor_numeric_entries_a;
extern RECT g_unit_panel_editor_numeric_entries_b;
extern RECT g_unit_panel_editor_numeric_entries_c;
extern RECT g_unit_panel_editor_numeric_entries_d;
extern RECT g_unit_panel_editor_numeric_entries_e;
extern RECT g_unit_panel_editor_numeric_entries_f;
extern menu_unit_stat_number_images_t g_unit_panel_editor_numeric_text_a;
extern menu_unit_stat_number_images_t g_unit_panel_editor_numeric_text_b;
extern struct unit_stats g_unit_panel_editor_party_unit_stats;

void unit_panel_numbers_polarity_thread(void);

/* BUNIT and WORLD share a source; DEBUGCHR and EQUIP each build their own body. */
void unit_panel_numbers_thread(void);

/* cell layouts */
extern image_layout_t g_unit_panel_editor_label_cells[];
extern image_layout_t g_unit_panel_editor_mode_cell[];
extern image_layout_t g_unit_panel_editor_mode_cells[];
extern image_layout_t g_unit_panel_editor_value_cells[];
extern point16_t g_unit_panel_item_icon_texture_origin;
extern image_layout_t g_unit_panel_portrait_cell[];
extern image_layout_t g_unit_panel_sprite_cells[];
extern point16_t g_unit_panel_texture_origin;
extern image_layout_t g_unit_panel_editor_numeric_texture[];

/* panel state (BUNIT) */
extern unit_panel_unit_t g_unit_panel_panel_selected_unit_data;
extern palette_t g_unit_panel_panel_selected_portrait_palettes[2];
extern unit_panel_unit_t g_unit_panel_panel_comparison_unit_data;
extern palette_t g_unit_panel_panel_comparison_portrait_palettes[2];

void unit_panel_toggle_equipment_and_numbers(s32 enable);

/* panel state (EQUIP) */
extern unit_panel_buffer_t g_unit_panel_frames_b[];
extern unit_panel_thread_t g_unit_panel_unit_status_panel_params;

void unit_panel_toggle_equipment_threads(s32 enable);
void unit_panel_toggle_item_numbers_thread(s32 enable);

/* panel state (REQUIRE) */

/* Scalar x aliases of the three panels' origins; they preserve the retail
 * transition-store scheduling. */
extern s32 g_unit_panel_status_display_origin_x;
extern s32 g_unit_panel_character_status_origin_x;
extern s32 g_unit_panel_numbers_origin_x;

void unit_panel_set_transition(s32 value);
void unit_panel_transition_thread(void);

/* help levels (WLDCORE) */
/* The window-layout request that opens a MENU_STACK_LEVEL_UNIT_PANEL_HELP level. unit_panel_move_help_cursor reads the
 * first three fields. */
typedef struct unit_panel_help_level {
    s32 window_index;
    s32 layout; /* 0x04; row of g_unit_panel_help_rows */
    s32 cached_layout_kind;
    s32 saved_state; /* 0x0c; previous g_menu_stack_ot_offset, saved by the push */
} unit_panel_help_level_t;

void unit_panel_push_help_level(void);
void unit_panel_resume_help_level(menu_stack_record_t* level);

#endif
