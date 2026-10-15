#ifndef EVENT_ATTACK_H
#define EVENT_ATTACK_H

/* EVENT/ATTACK.OUT: menu screen overlay that runs with BATTLE. */

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
extern u8 g_attack_event_condition_blocks[];

extern u8 g_attack_scenario_table[];

s32 attack_load_scenario_conditionals(void);
s16 attack_load_unaligned_s16(const u8* ptr);

/* deployment */
typedef enum attack_deployment_orientation {
    ATTACK_DEPLOYMENT_ORIENTATION_NORMAL = 0,
    ATTACK_DEPLOYMENT_ORIENTATION_ROTATE_90 = 1,
    ATTACK_DEPLOYMENT_ORIENTATION_ROTATE_180 = 2,
    ATTACK_DEPLOYMENT_ORIENTATION_ROTATE_270 = 3,
} attack_deployment_orientation_e;

enum {
    ATTACK_DEPLOYMENT_ORIENTATION_MASK = 0x0f,
};
/* The roster id of a deployment tile or placement record that holds no unit
 * (g_attack_deploy_roster_id_by_tile, g_attack_deploy_units_by_squad and
 * unit_deployment_coordinates_t.party_id). */
enum {
    ATTACK_DEPLOY_ROSTER_ID_NONE = 0xff,
};
/* Twelve-byte deployment-area record used by EVENT/ATTACK.OUT. The exact
 * roster-fieldability routine proves the stride, unit limit, and map ID;
 * valid-deployment-tile accessors establish the 5-by-5 bitmap. */
typedef struct attack_deployment_squad_data {
    u32 valid_tile_bitmap; /* 0x00; 5-by-5 deployment mask */
    s8 center_x;           /* 0x04 */
    s8 center_y;           /* 0x05 */
    u8 upper_level;        /* 0x06 */
    u8 orientation_flags;  /* 0x07; original and rotated orientations */
    u8 unit_limit;         /* 0x08 */
    u8 map_id;             /* 0x09 */
    u16 placement_id;      /* 0x0a */
} attack_deployment_squad_data_t;
typedef char attack_deployment_squad_data_size_must_be_12[(sizeof(attack_deployment_squad_data_t) == 12) ? 1 : -1];

/* Textured deployment banner followed by its two triangle packets. */
typedef struct attack_deploy_arrow_gfx {
    POLY_FT4 poly;  /* 0x00 */
    POLY_F3 tri[2]; /* 0x28 */
} attack_deploy_arrow_gfx_t;
typedef char attack_deploy_arrow_gfx_size_must_be_80[sizeof(attack_deploy_arrow_gfx_t) == 80 ? 1 : -1];

/* One 0xa2c-byte half of the double-buffered deployment-screen primitive
 * block at 0x801db650 (g_attack_deploy_render_buffers). The 5-by-5 valid-tile
 * sprites occupy its head; the two cursors each own a marker quad ([n][0],
 * bobbing above the tile) and a tile-highlight diamond ([n][1]), positioned
 * from g_attack_deploy_cursor_row/column[n]. */
typedef struct attack_deploy_render_buffer {
    SPRT tiles[5][5];                       /* 0x000 */
    POLY_FT4 portraits[10];                 /* 0x1f4 */
    POLY_FT4 status[30];                    /* 0x384 */
    attack_deploy_arrow_gfx_t screen_arrow; /* 0x834 */
    POLY_FT4 menu_cursor[4];                /* 0x884 */
    POLY_FT4 cursor[2][2];                  /* 0x924 */
    DR_MODE draw_mode_9c4;                  /* 0x9c4 */
    DR_MODE draw_mode_9d0;                  /* 0x9d0 */
    DR_MODE draw_mode_9dc;                  /* 0x9dc */
    DR_MODE draw_mode_9e8;                  /* 0x9e8 */
    DR_MODE draw_mode_9f4;                  /* 0x9f4 */
    DR_MODE menu_draw_mode;                 /* 0xa00 */
    DR_OFFSET world_offset;                 /* 0xa0c */
    point16_t world_position;               /* 0xa18; draw-offset input */
    DR_OFFSET screen_offset;                /* 0xa1c */
    point16_t screen_position;              /* 0xa28; draw-offset input */
} attack_deploy_render_buffer_t;
typedef char attack_deploy_render_buffer_size_must_be_2604[(sizeof(attack_deploy_render_buffer_t) == 2604) ? 1 : -1];

/* g_attack_deploy_menu_state: what the deployment cursor is over. It selects
 * the button hint attack_deploy_build_menu_cursor_primitives draws. */
typedef enum attack_deploy_menu_state {
    ATTACK_DEPLOY_MENU_CLOSED = -1,       /* attack_deploy_run_screen is done */
    ATTACK_DEPLOY_MENU_EMPTY_TILE = 0,    /* a unit can be placed here */
    ATTACK_DEPLOY_MENU_BLOCKED_TILE = 1,  /* not a deployment tile, or the squad is full */
    ATTACK_DEPLOY_MENU_SELECTED_UNIT = 2, /* the tile of the selected roster unit */
    ATTACK_DEPLOY_MENU_OTHER_UNIT = 3,    /* another placed unit */
    ATTACK_DEPLOY_MENU_SWAP_UNIT = 4,     /* the second cursor over a unit other than the picked one */
    ATTACK_DEPLOY_MENU_HIDDEN = 5,        /* between squads and in tiles-only mode: no hint */
} attack_deploy_menu_state_e;

extern s32 g_attack_deploy_active_cursor;
extern u32 g_attack_deploy_arrow_clut_data[];
extern RECT g_attack_deploy_arrow_clut_rect;
extern s32 g_attack_deploy_arrow_position_mode;
extern s16 g_attack_deploy_confirm_result;
extern s32 g_attack_deploy_current_squad;
extern attack_deployment_squad_data_t* g_attack_deploy_current_squad_data;
extern s32 g_attack_deploy_current_squad_id;
extern POLY_FT4* g_attack_deploy_cursor_0_poly;
extern s32 g_attack_deploy_cursor_0_submitted;
extern POLY_FT4* g_attack_deploy_cursor_1_poly;
extern s32 g_attack_deploy_cursor_1_submitted;
extern palette_t g_attack_deploy_cursor_clut;
extern RECT g_attack_deploy_cursor_clut_rect;
extern s32 g_attack_deploy_cursor_column[2];
extern palette_t g_attack_deploy_cursor_palette_cycle[2];
extern const palette_t g_attack_deploy_cursor_palette_source[2];
extern s32 g_attack_deploy_cursor_row[2];
extern u32 g_attack_deploy_cursor_texture_data[];
extern s32 g_attack_deploy_deployed_unit_count;
extern s32 g_attack_deploy_fieldable_unit_count;
extern s32 g_attack_deploy_grid_slide_x;
extern s32 g_attack_deploy_grid_slide_y;
/* g_attack_event_condition_blocks help text, installed as TEXT_SECTION_OVERLAY_SCREEN */
extern u8* g_attack_deploy_help_text_section;
extern image_load_parameters_t g_attack_deploy_menu_cursor_cells[];
extern menu_entry_t g_attack_deploy_menu_entries[];
extern s32 g_attack_deploy_menu_state; /* attack_deploy_menu_state_e */

extern s32 g_attack_deploy_pending_roster_index;
extern RECT g_attack_deploy_rect_09c;
extern RECT g_attack_deploy_rect_0ac;
extern RECT g_attack_deploy_rect_0cc;
extern attack_deploy_render_buffer_t g_attack_deploy_render_buffers[2];
extern u8 g_attack_deploy_roster_id_by_tile[5][5];
extern s32 g_attack_deploy_roster_navigation_direction;
extern u8 g_attack_deploy_roster_unit_deployable[PARTY_ROSTER_SLOT_COUNT];
/* Alias of render_buffers[0].screen_arrow, addressed separately by the constructor. */
extern attack_deploy_arrow_gfx_t g_attack_deploy_screen_arrow;
extern s32 g_attack_deploy_second_pass;
extern help_navigation_record_t g_attack_deploy_select_menu_nodes[];
extern s32 g_attack_deploy_selected_roster_index;
extern s32 g_attack_deploy_selected_unit_deployed;
extern palette_t g_attack_deploy_semitrans_clut_buffer;
extern s32 g_attack_deploy_slot_offsets[5];
extern attack_deployment_squad_data_t* g_attack_deploy_squad_data;
extern image_load_parameters_t g_attack_deploy_text_graphic_params[9];
extern POLY_FT4 g_attack_deploy_text_polys[2][9];
extern s32 g_attack_deploy_text_reveal_heights[9];
extern s32 g_attack_deploy_text_reveal_steps[9];
extern RECT g_attack_deploy_text_screen_origin;
extern point16_t g_attack_deploy_texture_location[];
extern rgba5551_t g_attack_deploy_tile_clut_buffer[48];
extern const palette_t g_attack_deploy_tile_clut_data[3];
extern RECT g_attack_deploy_tile_clut_rect;
extern s32 g_attack_deploy_tiles_only_mode;
extern u8 g_attack_deploy_unit_fieldable[PARTY_ROSTER_SLOT_COUNT];
/* g_attack_scenario_table string arena, installed as TEXT_SECTION_DEPLOYMENT_UNIT_HELP */
extern u8* g_attack_deploy_unit_text_section;
extern u8 g_attack_deploy_units_by_squad[4][25];
extern u8 g_attack_deploy_valid_tiles[5][5];
extern RECT g_attack_deploy_vram_copy_rect;
extern s16 g_attack_deploy_vram_copy_x;
extern s16 g_attack_deploy_vram_copy_y;
/* The brightness byte of g_attack_deploy_zodiac_draw_context (+4) as its own s32 symbol: the fade decrements the whole
 * 32-bit value (lw/addiu/sw), which a u8 member access would not reproduce. The array form lets GCC hoist its address
 * out of the fade loop, as the target does. */
extern s32 g_attack_deploy_zodiac_brightness_word[1];
extern zodiac_draw_context_t g_attack_deploy_zodiac_draw_context;
extern unit_deployment_coordinates_t g_attack_deployed_unit_map_coordinates[12];

void attack_deploy_find_fieldable_units(void);
unit_deployment_coordinates_t* attack_deploy_get_coordinate_slots(void);
s32 attack_deploy_has_roster_id_on_tiles(s32 roster_id);
s32 attack_deploy_is_roster_unit_deployed(s32 roster_id);
void attack_deploy_mark_other_squad_units_undeployable(void);
void attack_deploy_run_screen(void);
void attack_deploy_run_select_menu(void);
void attack_text_build_deployment_strings(void);

/* formation */
extern RECT g_attack_formation_sheet_rect;
extern RECT g_attack_formation_sheet_source_rect;
extern u8 g_attack_formation_sprite_staging[];
extern u8 g_attack_formation_sprite_tall_cell[];

void attack_gfx_build_formation_sprites(void);
void attack_gfx_prepare_formation_sprites(void);

/* map */
extern s32 g_attack_map_title_closing_past_midpoint;
extern s32 g_attack_map_title_image_rect;
extern s32 g_attack_map_title_palette_data;
extern s32 g_attack_map_title_palette_rect;
/* Two buffers of the four shaded title-panel strips. */
extern POLY_GT4 g_attack_map_title_sprites[2][4];

/* Scenario interpreter interfaces shared by the BATTLE and WORLD
 * script_execute_event_variant bodies. Signatures are as the interpreters bind them;
 * unverified names stay provisional. */
void attack_map_load_title_graphic(void);
void attack_map_show_title(void);

/* unit */
void attack_gfx_prepare_party_portrait_textures(void);

/* character status */
extern u8 g_attack_character_status_frame_config[];

/* unit panels */
extern RECT g_attack_deploy_cursor_texture_rect;
extern palette_t g_attack_panel_comparison_portrait_palettes[2];
extern menu_active_unit_t g_attack_panel_comparison_unit_data;
extern palette_t g_attack_panel_selected_portrait_palettes[2];
extern menu_active_unit_t g_attack_panel_selected_unit_data;
extern u8 g_attack_panel_status_animation[];
/* Per-sprite first/end cell indices (interleaved pairs), the visible-entry
 * count, each entry's sprite id and fade/blink state, and a 4-byte record per
 * entry whose first 16-bit value biases the draw point's y. */
extern u8 g_attack_panel_status_group_bounds[];
extern u8 g_attack_panel_status_group_count;
extern u8 g_attack_panel_status_group_ids[];
extern sprite_draw_offset_t g_attack_panel_status_offsets[];
extern point16_t g_attack_panel_status_scales[];
extern sprite_sheet_frame_t g_attack_panel_status_uv_rects[];
extern u16 g_attack_panel_status_y_offsets[][2];
extern s32 g_attack_status_display_offset_y[];
extern s32 g_attack_status_display_thread_params;

/* numeric editor */
/* Five name ids and five ability/skillset ids of the unit loaded into the
 * editor (g_attack_editor_unit_fields + 0x24). The target walks them from this
 * standalone address: indexing the editor record instead folds 0x24 into each
 * load and allocates the walk from the record base. */
extern s16 g_attack_editor_item_and_ability_ids[10];
extern u8 g_attack_numeric_editor_thread_params[];

void attack_editor_load_party_unit(s32 mode, s32 roster_index);

/* menus */

/* graphics */
extern POLY_FT4 g_attack_gfx_frame_ft4[];
/* Map darkness uses RGB and ignores the fourth byte. */
extern const CVECTOR g_attack_map_darkness_color;

/* portraits */
extern RECT g_attack_portrait_palette_upload_rect;
extern u8 g_attack_portrait_staging[];
extern RECT g_attack_portrait_upload_rect;

void attack_gfx_load_portraits(void);

/* text */
/* Overlay text file: offsets are relative to the data following the header. */
typedef struct attack_text_file {
    s32 section_offsets[32];
    u8 section_data[0];
} attack_text_file_t;
typedef char attack_text_file_header_size_must_be_128[(sizeof(attack_text_file_t) == 128) ? 1 : -1];

extern s32 g_attack_text_job_names_offset;
/* Entries [1] and [2] of the unit-name text file's section-offset table (0x801cd2b0, s32[32], ending at
 * g_attack_text_data). */
extern s32 g_attack_text_roster_unit_names_offset;

/* input */

/* threads */
/* Slots of the deployment screen's own threads; the screen itself runs in
 * THREAD_BORROWED_SLOT_DEPLOYMENT and its panels in the
 * thread_screen_slot_e slots (unit status 8, character status 9,
 * numeric editor 12). */
typedef enum attack_thread_slot {
    ATTACK_THREAD_SLOT_SELECT_MENU = 3,      /* attack_deploy_run_select_menu */
    ATTACK_THREAD_SLOT_ROSTER_STEP = 4,      /* attack_deploy_select_next/previous_roster_unit */
    ATTACK_THREAD_SLOT_NOTICE = 13,          /* the notice shown for an invalid deployment input */
    ATTACK_THREAD_SLOT_SCREEN_GRAPHICS = 14, /* attack_deploy_update_screen_graphics, then the map title */
    ATTACK_THREAD_SLOT_DEPLOY_RENDER = 15,   /* attack_deploy_run_render_thread */
} attack_thread_slot_e;

/* The same slots while the confirm prompt or the unit detail view is up. */
typedef enum attack_prompt_thread_slot {
    ATTACK_PROMPT_THREAD_SLOT_CONFIRM_MENU_CHILD = 3, /* the child of the confirm menu */
    ATTACK_PROMPT_THREAD_SLOT_CONFIRM_MENU = 4,       /* the menu that sets g_attack_deploy_confirm_result */
    ATTACK_PROMPT_THREAD_SLOT_DETAIL_MARKER = 13,     /* a wait-forever thread while the unit detail panels are open */
} attack_prompt_thread_slot_e;

/* sound */
void attack_sound_resync_scenario_music_and_apply_map_darkness(void);

#endif
