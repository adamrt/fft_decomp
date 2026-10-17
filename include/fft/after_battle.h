#ifndef AFTER_BATTLE_H
#define AFTER_BATTLE_H

/* REQUIRE's after-battle screens: the battle conditions banners and their bursts (BONUS.BIN), the war trophies, and the
 * screens' menus, graphics, text sections and threads. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/reward.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* conditions */
/* One color record per display-condition quad; the fourth byte of each
 * CVECTOR is not consumed by the banner renderer. */
typedef struct conditions_colors {
    u8 _unused_00[4];    /* 0x00 */
    CVECTOR vertices[4]; /* 0x04 */
} conditions_colors_t;
typedef char conditions_colors_size_must_be_20[sizeof(conditions_colors_t) == 20 ? 1 : -1];

typedef struct conditions_tpage {
    u8 _unused_00;
    u8 _unused_01;
    u8 page; /* Bits 5 and 6 select the texture page. */
    u8 _unused_03;
} conditions_tpage_t;

extern u8 g_conditions_general_quad_ranges[];
extern sprite_sheet_frame_t g_conditions_general_quads[];
extern conditions_tpage_t g_conditions_general_tpages[];
extern conditions_colors_t g_conditions_general_vertex_colors[];
extern sprite_sheet_frame_t g_conditions_special_quads[];

/* Per-condition first-quad indices; entry n+1 bounds entry n. */
extern u8 g_conditions_special_quad_ranges[];

/* 0x14-byte vertex-color records, one per quad. */
extern conditions_colors_t g_conditions_special_vertex_colors[];

void conditions_show_ready_bugged(void);
void conditions_special_thread(void);
void victory_congratulations_thread(void);

/* REQUIRE's condition/reward dispatcher, called by the event library in BATTLE and WORLD. */
void require_conditions_entrypoint(void);

/* banner bursts */
/* BONUS.BIN stores a 256x200 4bpp image, followed by eight 16-color palette slots. */
typedef struct conditions_banner_file {
    u32 pixels[200][32];   /* 0x0000; eight 4bpp pixels per 32-bit value */
    palette_t palettes[8]; /* 0x6400; four palettes per uploaded VRAM row */
    u8 _unused_6500[0x300];
} conditions_banner_file_t;
typedef char conditions_banner_file_size_must_be_26624[sizeof(conditions_banner_file_t) == 26624 ? 1 : -1];

/* One 12x24 burst tile: a double-buffered flat quad plus its motion state. */
typedef struct conditions_burst_tile {
    POLY_F4 poly[2];   /* 0x00 */
    s32 delay;         /* 0x30 */
    s32 radius;        /* 0x34 */
    s32 angle;         /* 0x38 */
    s32 x;             /* 0x3C */
    s32 y;             /* 0x40 */
    s32 initial_delay; /* 0x44 */
} conditions_burst_tile_t;
typedef char conditions_burst_tile_size_must_be_72[sizeof(conditions_burst_tile_t) == 72 ? 1 : -1];

/* The mode byte of the bonus-image burst parameters: the distance that sets
 * each tile's start delay. Any other value measures along a rotated x axis. */
typedef enum conditions_burst_mode {
    CONDITIONS_BURST_FROM_CENTER = 0,
    CONDITIONS_BURST_FROM_ORIGIN = 1, /* the top-left corner */
    CONDITIONS_BURST_HORIZONTAL = 2,  /* x only */
} conditions_burst_mode_e;

extern RECT g_conditions_banner_palette_rect;
extern conditions_burst_tile_t g_conditions_burst_tiles[12][24];
extern POLY_F4 g_conditions_burst_flash_polys[2];
extern DR_MODE g_conditions_burst_draw_modes[2];
extern MATRIX g_conditions_burst_matrix;
extern SVECTOR g_conditions_burst_rotation;
extern VECTOR g_conditions_burst_offset;
extern VECTOR g_conditions_burst_rotated_offset;

void conditions_load_banner_image(s32 image_id);

/* REQUIRE's banner-burst thread, entered by DarkScreen after the overlay loads. */
void require_dark_screen_entrypoint(void);

/* war trophies */
/* g_war_trophy_entry_types[]: what a war trophy entry's
 * value in g_war_trophy_entry_values is. */
typedef enum war_trophy_entry {
    WAR_TROPHY_ENTRY_ITEM = 0,        /* an item id */
    WAR_TROPHY_ENTRY_BONUS_MONEY = 1, /* the gil amount */
} war_trophy_entry_e;

/* Double-buffered war-trophy primitives, copied when each page opens. */
typedef struct war_trophy_primitives {
    POLY_FT4 quads[16]; /* 0x000 */
    SPRT banner;        /* 0x280 */
    DR_MODE draw_mode;  /* 0x294 */
} war_trophy_primitives_t;
typedef char war_trophy_primitives_size_must_be_672[sizeof(war_trophy_primitives_t) == 672 ? 1 : -1];

extern s32 g_war_trophy_entry_count;
extern s32 g_war_trophy_entry_types[];
extern s32 g_war_trophy_entry_values[];
extern u8 g_war_trophy_item_count;
extern reward_war_result_t g_war_trophy_search_result;
extern war_trophy_primitives_t g_war_trophy_primitives[2];
extern RECT g_war_trophy_clear_rect;
extern RECT g_war_trophy_screen_clip_rect_0;
extern RECT g_war_trophy_screen_clip_rect_1;
extern s32 g_war_trophy_money_digit_targets[6];
extern u32 g_war_trophy_money_digit_positions[6];
extern u32 g_war_trophy_money_digit_speeds[6];
extern u32 g_war_trophy_money_digit_initial_speeds[6];
extern s32 g_war_trophy_money_digit_settling[6];
extern POLY_FT4 g_war_trophy_money_digit_polys[2][6];
extern POLY_FT4 g_war_trophy_money_next_digit_polys[2][6];
extern POLY_FT4 g_war_trophy_money_comma_polys[2];
extern POLY_FT4 g_war_trophy_money_sign_polys[2];
extern RECT g_war_trophy_money_clip_rect_0;
extern RECT g_war_trophy_money_clip_rect_1;

/* Upload rectangle for the current entry's text image. */
extern RECT g_war_trophy_image_rect;

/* menus and graphics */
extern menu_entry_t g_after_battle_join_menu_descriptor[];
extern s16 g_after_battle_menu_selection_result;
extern s16 g_after_battle_message_menu_result;
extern u8 g_after_battle_gfx_fade_rgb[3];
extern u8 g_after_battle_gfx_formation_sprite_groups[][3];

void gfx_fade_rgb_31_frames(s32 target_0, s32 target_1, s32 target_2);

/* text */
/* Sections of REQUIRE's text file, whose offset table sits 0x80 bytes before
 * g_text_data with offsets relative to it. The after-battle routines bind
 * them over the resident sections, and the file's job names as
 * TEXT_SECTION_JOB_NAMES. */
typedef enum after_battle_text_section {
    AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_WARNINGS = 1,   /* a unit's Brave is low or its Faith high */
    AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES = 2, /* the unit leaves the party for it */
    AFTER_BATTLE_TEXT_SECTION_DISMISS_UNIT_REPLIES = 3,   /* what a unit turned away or dismissed says */
} after_battle_text_section_e;

extern char g_after_battle_text_wait_for_file_allocation_message[];

/* threads */
/* Slots of the after-battle screens' own threads; thread_screen_slot_e
 * has the shared ones. */
typedef enum after_battle_thread_slot {
    AFTER_BATTLE_THREAD_SLOT_MENU = 5,        /* selection menus, prompts and the special-case display */
    AFTER_BATTLE_THREAD_SLOT_MESSAGE = 7,     /* the icon strip, message text or menu shown with them */
    AFTER_BATTLE_THREAD_SLOT_TRANSITION = 10, /* unit_panel_transition_thread */
} after_battle_thread_slot_e;

extern u8 g_after_battle_saved_thread_block[];
extern s32 g_after_battle_thread_suspended_id;
extern u8 g_after_battle_work[]; /* no named access */

#endif
