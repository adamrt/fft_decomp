#ifndef EVENT_REQUIRE_H
#define EVENT_REQUIRE_H

/* EVENT/REQUIRE.OUT: the after-battle overlay: conditions, rewards, party changes and congratulations. */

#include "fft/battle_unit.h"
#include "fft/battle_unit_encounter.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_panel.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* conditions */
/* One color record per display-condition quad; the fourth byte of each
 * CVECTOR is not consumed by the banner renderer. */
typedef struct require_display_condition_colors {
    u8 _unused_00[4];    /* 0x00 */
    CVECTOR vertices[4]; /* 0x04 */
} require_display_condition_colors_t;
typedef char
    require_display_condition_colors_size_must_be_20[sizeof(require_display_condition_colors_t) == 20 ? 1 : -1];

typedef struct require_render_condition_tpage {
    u8 _unused_00;
    u8 _unused_01;
    u8 page; /* Bits 5 and 6 select the texture page. */
    u8 _unused_03;
} require_render_condition_tpage_t;

extern u8 g_require_condition_general_quad_ranges[];
extern sprite_sheet_frame_t g_require_condition_general_quads[];
extern require_render_condition_tpage_t g_require_condition_general_tpages[];
extern require_display_condition_colors_t g_require_condition_general_vertex_colors[];
/* Per-condition first-quad indices; entry n+1 bounds entry n. */
extern u8 g_require_condition_special_quad_ranges[];
extern sprite_sheet_frame_t g_require_condition_special_quads[];
/* 0x14-byte vertex-color records, one per quad. */
extern require_display_condition_colors_t g_require_condition_special_vertex_colors[];

/* Called by the event library in both BATTLE and WORLD. */
void require_condition_dispatch(void);
void require_condition_show_ready_bugged(void);
void require_condition_run_special_thread(void);

/* rewards */
/* Double-buffered war-trophy primitives, copied when each page opens. */
typedef struct require_reward_war_trophy_primitives {
    POLY_FT4 quads[16]; /* 0x000 */
    SPRT banner;        /* 0x280 */
    DR_MODE draw_mode;  /* 0x294 */
} require_reward_war_trophy_primitives_t;
typedef char require_reward_war_trophy_primitives_size_must_be_672[sizeof(require_reward_war_trophy_primitives_t) == 672
        ? 1
        : -1];

/* g_require_reward_war_trophy_entry_types[]: what a war trophy entry's
 * value in g_require_reward_war_trophy_entry_values is. */
typedef enum require_war_trophy_entry {
    REQUIRE_WAR_TROPHY_ENTRY_ITEM = 0,        /* an item id */
    REQUIRE_WAR_TROPHY_ENTRY_BONUS_MONEY = 1, /* the gil amount */
} require_war_trophy_entry_e;

/* One 12x24 burst tile: a double-buffered flat quad plus its motion state. */
typedef struct require_reward_burst_tile {
    POLY_F4 poly[2];   /* 0x00 */
    s32 delay;         /* 0x30 */
    s32 radius;        /* 0x34 */
    s32 angle;         /* 0x38 */
    s32 x;             /* 0x3C */
    s32 y;             /* 0x40 */
    s32 initial_delay; /* 0x44 */
} require_reward_burst_tile_t;
typedef char require_reward_burst_tile_size_must_be_72[sizeof(require_reward_burst_tile_t) == 72 ? 1 : -1];

/* The mode byte of the bonus-image burst parameters: the distance that sets
 * each tile's start delay. Any other value measures along a rotated x axis. */
typedef enum require_reward_burst_mode {
    REQUIRE_REWARD_BURST_FROM_CENTER = 0,
    REQUIRE_REWARD_BURST_FROM_ORIGIN = 1, /* the top-left corner */
    REQUIRE_REWARD_BURST_HORIZONTAL = 2,  /* x only */
} require_reward_burst_mode_e;

/* BONUS.BIN stores a 256x200 4bpp image, followed by eight 16-color palette slots. */
typedef struct require_reward_bonus_image_file {
    u32 pixels[200][32];   /* 0x0000; eight 4bpp pixels per 32-bit value */
    palette_t palettes[8]; /* 0x6400; four palettes per uploaded VRAM row */
    u8 _unused_6500[0x300];
} require_reward_bonus_image_file_t;
typedef char
    require_reward_bonus_image_file_size_must_be_26624[sizeof(require_reward_bonus_image_file_t) == 26624 ? 1 : -1];

extern s32 g_require_reward_war_trophy_entry_count;
extern s32 g_require_reward_war_trophy_entry_types[];
extern s32 g_require_reward_war_trophy_entry_values[];
extern u8 g_require_reward_war_trophy_item_count;
extern battle_war_result_t g_require_reward_war_trophy_search_result;
extern require_reward_war_trophy_primitives_t g_require_reward_war_trophy_primitives[2];
/* Upload rectangle for the current entry's text image. */
extern RECT g_require_reward_war_trophy_image_rect;
extern RECT g_require_reward_war_trophy_clear_rect;
extern RECT g_require_reward_screen_clip_rect_0;
extern RECT g_require_reward_screen_clip_rect_1;
extern s32 g_require_reward_money_digit_targets[6];
extern u32 g_require_reward_money_digit_positions[6];
extern u32 g_require_reward_money_digit_speeds[6];
extern u32 g_require_reward_money_digit_initial_speeds[6];
extern s32 g_require_reward_money_digit_settling[6];
extern POLY_FT4 g_require_reward_money_digit_polys[2][6];
extern POLY_FT4 g_require_reward_money_next_digit_polys[2][6];
extern POLY_FT4 g_require_reward_money_comma_polys[2];
extern POLY_FT4 g_require_reward_money_sign_polys[2];
extern RECT g_require_reward_money_clip_rect_0;
extern RECT g_require_reward_money_clip_rect_1;
extern RECT g_require_reward_bonus_palette_rect;
extern require_reward_burst_tile_t g_require_reward_burst_tiles[12][24];
extern POLY_F4 g_require_reward_burst_flash_polys[2];
extern DR_MODE g_require_reward_burst_draw_modes[2];
extern MATRIX g_require_reward_burst_matrix;
extern SVECTOR g_require_reward_burst_rotation;
extern VECTOR g_require_reward_burst_offset;
extern VECTOR g_require_reward_burst_rotated_offset;

void require_reward_load_bonus_image(s32 image_id);
void require_reward_animate_bonus_image_burst(void);

/* party */
/* What require_party_update_bio_variable_for_unit_class stores in a character's bio variables. */
typedef enum require_bio_state {
    REQUIRE_BIO_STATE_DISMISSED = 12, /* dismissed or turned away */
    REQUIRE_BIO_STATE_LEFT_LOW_BRAVE = 13,
    REQUIRE_BIO_STATE_LEFT_HIGH_FAITH = 14,
    REQUIRE_BIO_STATE_LOST_IN_BATTLE = 15,
} require_bio_state_e;

/* Three unit ids whose presence selects the event variable at +6. */
typedef struct require_party_bio_update_record {
    u16 unit_ids[3];
    u16 variable;
} require_party_bio_update_record_t;

extern menu_entry_t g_require_join_menu_descriptor[];
extern require_party_bio_update_record_t g_require_party_bio_update_records[11];
extern s32 g_require_party_affected_unit_count;
extern s32 g_require_party_affected_battle_unit_indices[];
extern u8 g_require_party_removable_flags[];
/* Six consecutive 0x3c-byte menu records; the target derives entry [4]'s
   address from entry [2], so they are one array. */
extern menu_entry_t g_require_party_removal_menus[6];
extern s16 g_require_party_removal_result;
extern RECT g_require_party_portrait_move_rect;
extern s16 g_require_party_portrait_destination;
extern RECT g_require_party_palette_move_rect;
/* x and y of the palette MoveImage destination RECT. Read as two scalar symbols: a RECT view lets
   GCC keep the struct address live in a saved register and changes the allocation. */
extern s16 g_require_party_palette_destination;
extern s16 g_require_party_palette_destination_y;

s32 require_party_update_after_battle(void);
void require_party_report_brave_faith_changes(void);
void require_party_remove_low_brave_high_faith_units(void);
void require_party_update_bio_variable_for_unit_class(s32 unit_class, s32 value);
s32 require_party_store_equipment_or_sell_overflow(s32 unit_id);
s32 require_party_sell_equipment_over_limit(s32 unit_id);

/* unit panels */
extern palette_t g_require_panel_selected_portrait_palettes[2];
extern palette_t g_require_panel_comparison_portrait_palettes[2];
extern s32 g_require_panel_dim_a;
extern s32 g_require_panel_dim_b;
extern s32 g_require_panel_dim_c;
extern menu_status_thread_t g_require_status_display_thread_params;
extern menu_status_thread_t g_require_character_status_frame_config;
extern menu_status_thread_t g_require_numeric_editor_thread_params;
/* Scalar x aliases of the three panels' origins; they preserve the retail
 * transition-store scheduling. */
extern s32 g_require_status_display_origin_x;
extern s32 g_require_character_status_origin_x;
extern s32 g_require_numeric_editor_origin_x;

void require_panel_set_transition_value(s32 value);

/* portraits */
extern u8 g_require_portrait_cell_pixels[];
extern RECT g_require_portrait_cell_rect;
extern RECT g_require_portrait_palette_rect;

void require_gfx_prepare_party_portrait_textures(void);

/* menus */
extern s16 g_require_menu_selection_result;
extern s16 g_require_message_menu_result;

void require_menu_wait_selection_threads(void);

/* graphics */
extern u8 g_require_gfx_fade_rgb[3];
extern u8 g_require_gfx_formation_sprite_groups[][3];

void require_gfx_build_scaled_draw_area_packets(scaled_draw_area_pair_t* packet, const RECT* source, s32 scale_index,
    s32 lower_half, const menu_status_thread_t* thread);
void require_gfx_fade_rgb_31_frames(s32 target_0, s32 target_1, s32 target_2);

/* text */
/* Sections of REQUIRE's text file, whose offset table sits 0x80 bytes before
 * g_text_data with offsets relative to it. The after-battle routines bind
 * them over the resident sections, and the file's job names as
 * TEXT_SECTION_JOB_NAMES. */
typedef enum require_text_section {
    REQUIRE_TEXT_SECTION_BRAVE_FAITH_WARNINGS = 1,   /* a unit's Brave is low or its Faith high */
    REQUIRE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES = 2, /* the unit leaves the party for it */
    REQUIRE_TEXT_SECTION_DISMISS_UNIT_REPLIES = 3,   /* what a unit turned away or dismissed says */
} require_text_section_e;

extern char g_require_text_wait_for_file_allocation_message[];

void require_text_show_battle_congratulations(void);

/* input */
void require_input_wait_frames_or_skip(s32 frames);

/* threads */
/* Slots of the after-battle screens' own threads; thread_screen_slot_e
 * has the shared ones. */
typedef enum require_thread_slot {
    REQUIRE_THREAD_SLOT_MENU = 5,        /* selection menus, prompts and the special-case display */
    REQUIRE_THREAD_SLOT_MESSAGE = 7,     /* the icon strip, message text or menu shown with them */
    REQUIRE_THREAD_SLOT_TRANSITION = 10, /* require_gfx_run_suspended_thread_transition */
} require_thread_slot_e;

extern u8 g_require_saved_thread_block[];
extern s32 g_require_thread_suspended_id;

void require_gfx_run_suspended_thread_transition(void);
void require_thread_exit_current(void);

/* unknown state */
extern u8 g_require_work[]; /* no named access */

#endif
