#ifndef MENU_PANEL_H
#define MENU_PANEL_H

/* Status panels: the panel threads and their parameter blocks, the double-buffered packet layouts, portraits,
 * panel fades and thread indicators, and the character status, equipment, numeric panel and numeric display
 * panels and the battle Status screen. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_status.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* panel threads */
/* Flags of a unit status panel (menu_status_panel_display_thread_t and
 * the WORLD status display's menu_status_thread_t). */
typedef enum menu_status_panel_flags {
    MENU_STATUS_PANEL_SLIDE_DOWN = 0x20, /* run the slide-down table once */
    MENU_STATUS_PANEL_SLIDE_UP = 0x40,   /* run the slide-up table once */
    MENU_STATUS_PANEL_HIDDEN = 0x80,     /* draw nothing; the only flag kept when a slide ends */
} menu_status_panel_flags_e;

/* Parameter block of the task-0x3b status panel threads (WORLD's formation
 * panels and the overlays' character status and numeric panel threads): the
 * panel origin, a redraw request and the frame style read by
 * menu_set_status_panel_primitive_colors. The origin is read as whole 32-bit values (lw) for the
 * draw-area offset; the 16-bit reads that feed the s16 draw-offset stores
 * are the compiler narrowing those loads. */
typedef struct menu_status_thread {
    s32 x;              /* 0x00 */
    s32 y;              /* 0x04 */
    s32 flags;          /* 0x08: menu_status_panel_flags_e in the status display record */
    s32 redraw_request; /* 0x0c: set to 1 elsewhere to request a text/portrait rebuild; the status
                           display record also keeps the selection-slide direction here */
    s32 style;          /* 0x10: 1 selects the dimmed menu colors */
} menu_status_thread_t;
typedef char menu_status_thread_size_must_be_20[(sizeof(menu_status_thread_t) == 20) ? 1 : -1];

/* Parameter block of the editor display threads, reached through the
 * 0x400-byte battle thread table. It has the shape of menu_status_thread_t,
 * but these threads load x as a 16-bit value. The shake offset is sign-extended
 * from the s16 shake tables and narrowed to the draw-offset y. Only the members the display
 * threads use are named; the record's total size is not proven. */
typedef struct menu_status_panel_display_thread {
    u16 x;           /* 0x00 */
    u16 _padding_02; /* aligns shake_y */
    s32 shake_y;     /* 0x04; narrowed to a draw-offset 16-bit value when rendered */
    s32 flags;       /* 0x08: menu_status_panel_flags_e */
    s32 work;        /* 0x0c: redraw request flags, cleared once serviced */
    s32 dimmed;      /* 0x10: nonzero forces the dimmed CLUTs and colors; bound as
                           g_bunit_status_display_dimmed and the WORLD display globals */
} menu_status_panel_display_thread_t;

/* Parameter blocks of the selected unit's and the comparison unit's status panels, the stat preview panel and
 * the numeric panel. */
extern menu_status_thread_t g_menu_status_display_thread_params;
extern menu_status_thread_t g_menu_comparison_display_thread_params;
extern menu_status_thread_t g_menu_preview_stats_thread_params;
extern menu_status_thread_t g_menu_numeric_panel_params;

/* Each starts its panel thread when enable is set and the thread is idle, and asks it to stop otherwise. */
void menu_toggle_unit_status_thread(s32 enable);
void menu_toggle_comparison_status_thread(s32 enable);
void menu_toggle_numeric_panel_thread(s32 enable);

/* The unit status panel thread; each program that shows the panel links its own body. */
void menu_run_unit_status_thread(void);
/* Set by BUNIT and REQUIRE to ask the status banner to slide in again: 1 or 2 picks the slide direction. */
extern s32 g_status_display_redraw_request;

/* panel packets */
/* Battle menu status and character-editor panel records. The EVENT overlays
 * (ATTACK, BUNIT, DEBUGCHR, EQUIP, REQUIRE) each link their own copy of the
 * panel code, and the WORLD unit-editor panels reuse the packet layouts. Each panel keeps two copies of its
 * packets and fills the one for this frame. */

/* DR_OFFSET followed by signed GPU offsets.
 * Signed storage retains the retail negative-immediate loads. */
typedef struct menu_status_panel_draw_offset {
    DR_OFFSET packet;   /* 0x00 */
    point16_t position; /* 0x0c */
} menu_status_panel_draw_offset_t;
typedef char menu_status_panel_draw_offset_size_must_be_16[(sizeof(menu_status_panel_draw_offset_t) == 16) ? 1 : -1];

/* Double-buffered panel packets: a center tile and nine-scanline top and
 * bottom gradients, used by status indicators and menu panel fades. */
typedef struct menu_gradient_panel_prims {
    DR_MODE draw_mode; /* 0x00 */
    TILE center;       /* 0x0c */
    TILE top[9];       /* 0x1c */
    TILE bottom[9];    /* 0xac */
} menu_gradient_panel_prims_t;
typedef char menu_gradient_panel_prims_size_must_be_316[(sizeof(menu_gradient_panel_prims_t) == 316) ? 1 : -1];

/* The numeric display frame, built by menu_init_numeric_display_frame_primitives: two DR_MODE
 * packets, twelve LINE_F2 packets and one covering SPRT whose CLUT
 * menu_init_primitive_colors_normal and _dimmed set. */
typedef struct menu_palette_primitives {
    DR_MODE draw_mode;      /* 0x00: texture window g_gfx_texture_window */
    DR_MODE draw_mode_menu; /* 0x0c: texture window g_menu_numeric_display_texture_window */
    LINE_F2 lines[12];      /* 0x18 */
    SPRT sprite;            /* 0xd8 */
} menu_palette_primitives_t;
typedef char menu_palette_primitives_size_must_be_236[(sizeof(menu_palette_primitives_t) == 236) ? 1 : -1];

/* Placement of one of the twelve frame lines (menu_palette_primitives_t.lines): signed endpoint offsets from the
 * box origin and, per coordinate, a flag selecting whether the box width or height is added. The table,
 * g_menu_numeric_display_frame_offsets, is walked by byte offset (MENU_LINE_LAYOUT_*), so nothing uses
 * this type. Provisional. */
typedef struct menu_line_layout {
    s8 x0, y0, x1, y1;                 /* 0x00 */
    s8 add_w0, add_h0, add_w1, add_h1; /* 0x04 */
} menu_line_layout_t;

/* The builder walks the table with a byte offset stepping by 8 (the
 * target's induction variable), so it addresses these fields by offset. */
enum menu_line_layout_offset {
    MENU_LINE_LAYOUT_X0 = 0,
    MENU_LINE_LAYOUT_Y0 = 1,
    MENU_LINE_LAYOUT_X1 = 2,
    MENU_LINE_LAYOUT_Y1 = 3,
    MENU_LINE_LAYOUT_ADD_W0 = 4,
    MENU_LINE_LAYOUT_ADD_H0 = 5,
    MENU_LINE_LAYOUT_ADD_W1 = 6,
    MENU_LINE_LAYOUT_ADD_H1 = 7,
    MENU_LINE_LAYOUT_SIZE = 8
};

/* One 0x100-byte slot holding a panel's palette primitive set. */
typedef struct menu_palette_slot {
    menu_palette_primitives_t primitives; /* 0x00 */
    u8 _padding_ec[0x14];                 /* preserves the 0x100-byte allocation stride */
} menu_palette_slot_t;
typedef char menu_palette_slot_size_must_be_256[(sizeof(menu_palette_slot_t) == 256) ? 1 : -1];

/* Status-panel packet block (0x1d8 bytes), doubled the same way as the editor
 * block. sprites[6] doubles as the scroll cursor: the display threads write
 * its texture window from the scroll row/column. */
typedef struct menu_status_panel_packet {
    menu_palette_primitives_t numeric_frame;       /* 0x000 */
    SPRT sprites[7];                               /* 0x0ec */
    DR_MODE draw_mode_a;                           /* 0x178 */
    DR_MODE draw_mode_b;                           /* 0x184 */
    menu_status_panel_draw_offset_t draw_offset_a; /* 0x190 */
    menu_status_panel_draw_offset_t draw_offset_b; /* 0x1a0 */
    POLY_FT4 portrait;                             /* 0x1b0 */
} menu_status_panel_packet_t;
typedef char menu_status_panel_packet_size_must_be_472[(sizeof(menu_status_panel_packet_t) == 472) ? 1 : -1];

/* Character-editor packet block (0x30c bytes) built by the overlays' menu_run_unit_status_thread. Two copies
 * sit back to back and `frame & 1` selects one, so the copy stride proves the size. The head is the numeric
 * display frame that menu_init_numeric_display_frame_primitives builds, the SPRT/POLY
 * strides come from the walks that configure and submit them, and the
 * draw-offset packets from the SetDrawOffset pairs. */
typedef struct menu_status_panel_editor_packet {
    menu_palette_primitives_t numeric_frame;       /* 0x000 */
    DR_MODE draw_mode_a;                           /* 0x0ec */
    DR_MODE draw_mode_b;                           /* 0x0f8 */
    SPRT value_sprites[4];                         /* 0x104 */
    SPRT label_sprites[7];                         /* 0x154; [6] at 0x1cc is the mode row */
    POLY_G4 bars[3];                               /* 0x1e0 */
    POLY_FT4 portrait[4];                          /* 0x24c */
    menu_status_panel_draw_offset_t draw_offset_a; /* 0x2ec */
    menu_status_panel_draw_offset_t draw_offset_b; /* 0x2fc */
} menu_status_panel_editor_packet_t;
typedef char
    menu_status_panel_editor_packet_size_must_be_780[(sizeof(menu_status_panel_editor_packet_t) == 780) ? 1 : -1];

/* Per-frame character status buffer (0x3d8 bytes), double buffered by menu_run_status_panel_thread,
 * menu_run_equipment_ability_panel_thread and the overlay list threads, which copy the first buffer
 * to the second (base + 0x3d8) before the render loop starts. Evidence:
 *  - the DR_MODE packets at +0x00/+0x0c/+0x18 are submitted individually, and
 *    the numeric threads fill the same slots with
 *    gfx_set_draw_mode_for_texture_page;
 *  - the submit loops walk the SPRTs from +0x24 with the sprite index as their
 *    counter (0..11 from +0x24, 12 at +0x114, 13 at +0x128, 14..18 from
 *    +0x13c, 19..23 from +0x1a0);
 *  - the TILE/LINE_F2 pair at +0x204/+0x224 (two translucent 16x90 backdrop
 *    tiles and eight vertical frame lines) is written, with the draw modes,
 *    by menu_init_status_panel_primitives, which leaves the sprites between untouched;
 *  - the two draw-offset packets follow from the SetDrawOffset(packet,
 *    position) calls at +0x2a4/+0x2b0 and +0x2b4/+0x2c0;
 *  - +0x2c4 and +0x3b0 are the arguments of
 *    menu_init_numeric_display_frame_primitives and the
 *    menu_init_scaled_draw_area_packets / menu_build_scaled_draw_area_packets
 *    pair. */
typedef struct menu_status_panel_buffer {
    DR_MODE frame_draw_mode;                         /* 0x000; MENU_TEXTURE_PAGE_FRAME */
    DR_MODE text_draw_mode;                          /* 0x00c; MENU_TEXTURE_PAGE_TEXT_IMAGES */
    DR_MODE image_draw_mode;                         /* 0x018; item icons or allocated images */
    SPRT sprites[24];                                /* 0x024 */
    TILE tiles[2];                                   /* 0x204 */
    LINE_F2 lines[8];                                /* 0x224 */
    menu_status_panel_draw_offset_t draw_offsets[2]; /* 0x2a4 */
    menu_palette_primitives_t numeric_frame;         /* 0x2c4 */
    scaled_draw_area_pair_t portrait;                /* 0x3b0 */
} menu_status_panel_buffer_t;
typedef char menu_status_panel_buffer_size_must_be_984[(sizeof(menu_status_panel_buffer_t) == 984) ? 1 : -1];

/* The frame lines in the normal or dimmed menu colors and the covering sprite in the matching CLUT. */
void menu_init_primitive_colors_normal(menu_palette_primitives_t* menu);
void menu_init_primitive_colors_dimmed(menu_palette_primitives_t* menu);
void menu_init_numeric_display_frame_primitives(RECT* bounds, menu_palette_primitives_t* menu);
void menu_submit_numeric_display_frame_primitives(menu_palette_primitives_t* display);
void menu_init_status_panel_primitives(menu_status_panel_buffer_t* menu);
/* The dimmed frame colors for style 1 or while the input lock mode is 1, otherwise the normal ones. */
void menu_select_status_primitive_color_palette(
    menu_palette_primitives_t* primitives, const menu_status_thread_t* thread);
/* The panel's sprite CLUTs and the tint of its last five sprites: dimmed and blue for style 1 or while the input
 * lock mode is 1, otherwise normal. */
void menu_set_status_panel_primitive_colors(menu_status_panel_buffer_t* primitives, const menu_status_thread_t* state);

/* number images */
/* Three 16x10 packed 4bpp images for the numbers below a unit's name:
 * Brave, Faith and the roster slot. */
typedef struct menu_unit_number_images {
    u8 brave[16 * 10 / 2];
    u8 faith[16 * 10 / 2];
    u8 roster_slot[16 * 10 / 2];
} menu_unit_number_images_t;
typedef char menu_unit_number_images_size_must_be_240[sizeof(menu_unit_number_images_t) == 240 ? 1 : -1];

/* Packed 4-bit images for the status panel's three numeric groups. */
typedef struct menu_unit_stat_number_images {
    u8 mobility[20 * 36 / 2]; /* Move, Jump and Speed. */
    u8 weapons[64 * 24 / 2];  /* Weapon power and evade. */
    u8 combat[100 * 24 / 2];  /* Physical/magical attack and class/shield/accessory evade. */
} menu_unit_stat_number_images_t;
typedef char menu_unit_stat_number_images_size_must_be_2328[sizeof(menu_unit_stat_number_images_t) == 2328 ? 1 : -1];

/* scaled draw areas */
/* A panel that zooms open draws through a pair of draw areas: the panel's rectangle at the thread's position,
 * cropped about its center to g_menu_draw_area_scale_percent[scale_index] percent, and the frame buffer's own
 * area. */
extern const u16 g_menu_draw_area_scale_percent[12];
extern RECT g_menu_draw_area_template;

/* Clears the first draw area's width and installs the default second draw area. */
void menu_init_scaled_draw_area_packets(scaled_draw_area_pair_t* data);
void menu_build_scaled_draw_area_packets(scaled_draw_area_pair_t* packet, const RECT* source, s32 scale_index,
    s32 lower_half, const menu_status_thread_t* thread);

/* status icons and zodiac */
/* Each animated status group's countdown byte; 0xff marks an inactive group. */
extern u8 g_menu_panel_status_animation[];
extern u8 g_menu_panel_status_group_count;
extern u8 g_menu_panel_status_group_ids[];
extern u8 g_menu_panel_status_group_bounds[];
extern sprite_draw_offset_t g_menu_panel_status_offsets[];
extern point16_t g_menu_panel_status_scales[];
extern const point16_t g_menu_panel_status_texture;
extern sprite_sheet_frame_t g_menu_panel_status_uv_rects[];
extern const u16 g_menu_panel_status_y_offsets[];
extern u8 g_menu_gfx_scaled_sprite_count;
extern point16_t g_menu_gfx_scaled_sprite_scale[];
extern point16_t g_menu_gfx_scaled_sprite_texture_page;
extern sprite_sheet_frame_t g_menu_gfx_scaled_sprite_uv_rects[];

void menu_build_status_group_primitives(POLY_FT4* poly);
/* The scaled zodiac sprites and their backdrop. */
void menu_build_zodiac_primitives(zodiac_draw_context_t* context, POLY_FT4* poly);

/* portraits */
/* One EVTFACE.BIN block: pixels followed by eight 16-color palettes. */
typedef struct menu_portrait_image_file {
    u32 pixels[8][48][4];  /* 0x0000; eight 32x48 4bpp portraits */
    palette_t palettes[8]; /* 0x1800; four palettes per uploaded VRAM row */
    u8 _unused_1900[0x700];
} menu_portrait_image_file_t;
typedef char menu_portrait_image_file_size_must_be_8192[sizeof(menu_portrait_image_file_t) == 8192 ? 1 : -1];

/* EVENT/WLDFACE.BIN is four of these sheets: forty 32x48 portraits at 4 bits
 * per pixel, eight across and five down, then sixteen rows of four palettes.
 * Palette n is portrait n's; rows 10-15 hold the other palettes of the monster
 * portraits, one row per monster with the palette number as the column. */
typedef struct menu_portrait_sheet {
    u8 pixels[240][128];       /* 0x0000; 4bpp, 256 pixels per row */
    palette_t palettes[16][4]; /* 0x7800; four 16-color palettes per row */
} menu_portrait_sheet_t;
typedef char menu_portrait_sheet_size_must_be_32768[(sizeof(menu_portrait_sheet_t) == 32768) ? 1 : -1];

enum {
    MENU_WLDFACE_SHEET_COUNT = 4,
    MENU_WLDFACE_SHEET_SIZE = 32768,
    MENU_WLDFACE_FILE_SIZE = MENU_WLDFACE_SHEET_COUNT * MENU_WLDFACE_SHEET_SIZE,
    MENU_WLDFACE_PORTRAITS_PER_SHEET = 40,
    MENU_WLDFACE_FIRST_MONSTER_PORTRAIT = 63,
    MENU_WLDFACE_MONSTER_PALETTE_ROW = 10, /* first row of the monsters' other palettes */
    MENU_WLDFACE_MONSTER_PALETTE_ROW_COUNT = 6,
};

/* Portrait code bits 8-9: either set means the code is not a unit portrait, so no portrait CLUT is built. */
enum { MENU_PORTRAIT_CLUT_SUPPRESS_MASK = 0x300 };

extern RECT g_menu_portrait_origin;
extern u8 g_menu_portrait_transition_offsets[];
extern RECT g_menu_portrait_pixel_rect;
extern RECT g_menu_portrait_palette_rect;
extern u16 g_portrait_index_by_sprite_set[];
extern RECT g_portrait_sheet_rect_0;
extern RECT g_portrait_sheet_rect_1;
extern RECT g_portrait_sheet_rect_2;
extern RECT g_portrait_sheet_rect_3;
extern RECT g_special_portrait_rect;
extern RECT g_special_portrait_palette_rect;

/* Unreferenced in both scenes: uploads one EVTFACE.BIN portrait and its palettes. */
void menu_load_portrait_image(s32 portrait_id);
/* ATTACK, REQUIRE, WORLD and the EVENT library each build their own body. */
void menu_build_portrait_poly(s32 portrait_id, POLY_FT4* poly);
/* BATTLE's body takes a battle id and textures the quad from the unit's spritesheet page; WORLD's takes a
 * portrait code into the WLDFACE.BIN sheets. */
void menu_build_unit_portrait_poly(POLY_FT4* poly, s32 id);

/* panel fades */
/* g_menu_panel_fade_mode: what menu_update_panel_fade does to the panel's
 * dimming tiles. */
typedef enum menu_panel_fade_mode {
    MENU_PANEL_FADE_NONE = 0, /* not drawn */
    MENU_PANEL_FADE_IN = 1,   /* intensity rises by 8 a frame up to 80 and holds */
    MENU_PANEL_FADE_OUT = 2,  /* intensity falls by 8 a frame; at zero the mode returns to NONE */
} menu_panel_fade_mode_e;

extern s32 g_menu_panel_fade_mode; /* menu_panel_fade_mode_e */
extern s32 g_menu_panel_fade_intensity;
extern u8 g_menu_panel_fade_modes[];
extern menu_gradient_panel_prims_t g_menu_panel_fade_frames[];

/* A black semi-transparent 256x50 center tile with nine one-line tiles above and below for the graded edges. */
void menu_init_panel_fade_tiles(void);
void menu_update_panel_fade(void);

/* thread status indicators */
/* g_menu_indicator_state[] (and EQUIP's g_equip_menu_indicator_state[]): state of each of the two translucent
 * panels menu_draw_thread_status_indicators draws behind the unit status banners. */
typedef enum menu_indicator_state {
    MENU_INDICATOR_STATE_HIDDEN = 0,     /* not drawn */
    MENU_INDICATOR_STATE_FADING_IN = 1,  /* brightness rises by 0xc a frame up to 0x78 and holds */
    MENU_INDICATOR_STATE_FADING_OUT = 2, /* brightness falls by 0xc a frame; at zero the state returns to HIDDEN */
} menu_indicator_state_e;

extern s32 g_menu_indicator_state[2];
extern s32 g_menu_indicator_brightness[2];
extern s32 g_menu_indicator_fade_work[2];
extern s32 g_menu_indicator_packet_index[2];
extern menu_gradient_panel_prims_t g_menu_thread_indicator_packets[2][2];

void menu_draw_thread_status_indicators(void);

/* character status panel */
/* The selected unit's and the comparison unit's status panels, with their billboards, packets, numbers, name
 * and portrait. */
extern s32 g_character_status_redraw_request;
extern RECT g_character_status_draw_area_rect;
extern RECT g_menu_status_panel_frame_rect[];
extern RECT g_panel_frame_rect;
extern menu_status_panel_buffer_t g_panel_frames_a[];
extern RECT g_panel_origin_offsets[];
extern s16 g_panel_slide_down_y[];
extern s16 g_panel_slide_up_y[];
extern CVECTOR g_panel_gauge_bar_colors[];
extern u16 g_panel_editor_label_cluts[][2];
extern u16 g_panel_editor_value_cluts[][2];
extern u8 g_panel_text_image_a[];
extern u8 g_panel_text_image_b[];
extern RECT g_panel_text_upload_rect_a;
extern RECT g_panel_text_upload_rect_b;
extern menu_unit_status_record_t g_panel_selected_billboard;
extern menu_status_panel_packet_t g_panel_selected_packets[2];
extern menu_status_panel_editor_packet_t g_panel_selected_editor_packets[2];
extern menu_number_entry_t g_panel_selected_numeric_entries[];
extern u8 g_panel_selected_name_image[];
extern u8 g_panel_selected_number_image[];
extern u8 g_panel_selected_small_number_image[];
extern RECT g_panel_selected_portrait_rect;
extern menu_unit_status_record_t g_panel_comparison_billboard;
extern menu_status_panel_packet_t g_panel_comparison_packets[2];
extern menu_status_panel_editor_packet_t g_panel_comparison_editor_packets[2];
extern menu_number_entry_t g_panel_comparison_numeric_entries[];
extern u8 g_panel_comparison_name_image[];
extern u8 g_panel_comparison_number_image[];
extern u8 g_panel_comparison_small_number_image[];
extern RECT g_panel_comparison_portrait_rect;

void menu_run_status_panel_thread(void);

/* equipment panel */
/* The selected unit's equipment and abilities, and the remove-equipment preview. */
extern RECT g_menu_equipment_panel_origin;
extern RECT g_menu_equipment_panel_frame_rect;
extern menu_status_panel_buffer_t g_menu_equipment_panel_frames[2];
extern image_load_parameters_t g_menu_equipment_panel_icon_params[MENU_EQUIPMENT_SLOT_COUNT];
extern u8 g_menu_equipment_panel_item_text_image[];
extern RECT g_menu_equipment_panel_item_text_rect;
extern u8 g_menu_equipment_panel_ability_text_image[];
extern RECT g_menu_equipment_panel_ability_text_rect;
extern image_load_parameters_t g_menu_equipment_panel_sprite_params_0[19];
extern image_load_parameters_t g_menu_equipment_panel_sprite_params_1[19];
extern image_load_parameters_t g_menu_equipment_panel_sprite_params_2[19];
extern image_load_parameters_t g_menu_equipment_panel_sprite_params_3[19];
extern u16 g_remove_all_equipment_ids[5]; /* weapon, shield, head, body, accessory */
extern s8 g_remove_all_equipment_prompt_active;
extern s16 g_remove_item_equipment[5];
extern s8 g_remove_item_previewed_slot;

void menu_run_equipment_ability_panel_thread(void);

/* numeric panel */
/* The numeric panel's two adjacent rectangles: the first is the panel frame
 * and the screen origin handed to image loading, the second places the
 * portrait. */
typedef struct menu_status_panel_numeric_geometry {
    RECT texture_rect;   /* 0x00 */
    RECT draw_area_rect; /* 0x08 */
} menu_status_panel_numeric_geometry_t;

/* Numeric panel buffer (0x2c0 bytes), double buffered by menu_run_numeric_panel_frame_thread, which
 * copies it with sizeof. The draw modes are set up with gfx_set_draw_mode_for_texture_page /
 * SetDrawMode, the 18 SPRTs by the gfx_init_image_loading walk,
 * +0x18c and +0x298 are the arguments of
 * menu_init_numeric_display_frame_primitives and
 * menu_init_scaled_draw_area_packets, and the draw offsets follow from the
 * SetDrawOffset(packet, position) pairs. */
typedef struct menu_status_panel_numeric_buffer {
    DR_MODE frame_draw_mode;                         /* 0x000; MENU_TEXTURE_PAGE_FRAME */
    DR_MODE text_draw_mode;                          /* 0x00c; MENU_TEXTURE_PAGE_TEXT_IMAGES */
    DR_MODE image_draw_mode;                         /* 0x018; item icons or allocated images */
    SPRT sprites[18];                                /* 0x024 */
    menu_palette_primitives_t numeric_frame;         /* 0x18c */
    menu_status_panel_draw_offset_t draw_offsets[2]; /* 0x278 */
    scaled_draw_area_pair_t portrait;                /* 0x298 */
} menu_status_panel_numeric_buffer_t;
typedef char
    menu_status_panel_numeric_buffer_size_must_be_704[(sizeof(menu_status_panel_numeric_buffer_t) == 704) ? 1 : -1];

extern s32 g_numeric_panel_redraw_request;
extern menu_status_panel_numeric_geometry_t g_editor_numeric_table;
extern menu_status_panel_numeric_buffer_t g_editor_numeric_state[2];
extern menu_status_panel_numeric_buffer_t g_editor_numeric_state_a[2];
extern menu_status_panel_numeric_buffer_t g_editor_numeric_state_b[2];
extern menu_number_entry_t g_editor_numeric_descriptor_a[];
extern menu_number_entry_t g_editor_numeric_descriptor_b[];
extern RECT g_editor_numeric_entries_a;
extern RECT g_editor_numeric_entries_b;
extern RECT g_editor_numeric_entries_c;
extern RECT g_editor_numeric_entries_d;
extern RECT g_editor_numeric_entries_e;
extern RECT g_editor_numeric_entries_f;
extern menu_unit_stat_number_images_t g_editor_numeric_text_a;
extern menu_unit_stat_number_images_t g_editor_numeric_text_b;
extern struct unit_stats g_editor_party_unit_stats;
extern u8 g_editor_primary_skillset_name[];
extern u8 g_editor_secondary_skillset_name[];

void menu_run_numeric_panel_thread(void);
/* EQUIP builds its own body. */
void menu_run_numeric_panel_frame_thread(void);

/* numeric display */
/* The Status screen's numeric side panel: its fifteen numbers are drawn into three images once, then the frame and
 * eighteen sprites every frame. */

/* Double-buffered 0x1ac-byte record of the numeric display panel (g_menu_numeric_display_panel_records).
 * Provisional. */
typedef struct menu_panel_record {
    DR_MODE mode0; /* 0x000 */
    DR_MODE mode1; /* 0x00c */
    /* 0x018; menu_init_scaled_draw_area_packets */
    scaled_draw_area_pair_t draw_areas;
    SPRT sprites[18];                   /* 0x040 */
    menu_palette_primitives_t* palette; /* 0x1a8 */
} menu_panel_record_t;

extern menu_panel_record_t g_menu_numeric_display_panel_records[2];
extern menu_number_entry_t g_menu_numeric_display_entries[15];
extern RECT g_menu_numeric_display_frame_rect;
extern image_load_parameters_t g_menu_numeric_display_image_params[18];
extern menu_palette_slot_t g_menu_numeric_display_palettes[];
extern RECT g_menu_numeric_display_text_rect_a;
extern RECT g_menu_numeric_display_text_rect_b;
extern RECT g_menu_numeric_display_text_rect_c;
extern RECT g_menu_numeric_display_panel_texture_window;
extern RECT g_menu_numeric_display_texture_window; /* texture window for the menu page */

void menu_run_numeric_display_panel_thread(void);

/* status screen */
/* Double-buffered 0x2d0-byte frame of the Status screen panel (g_menu_status_screen_frames). The first 0x2a4
 * bytes are laid out as menu_status_panel_buffer_t (draw modes, sprites, tiles and lines),
 * and the trailing draw-area pair follows menu_panel_frame_t. Provisional. */
typedef struct menu_status_screen_frame {
    DR_MODE frame_draw_mode;            /* 0x000; MENU_TEXTURE_PAGE_FRAME */
    DR_MODE text_draw_mode;             /* 0x00c; MENU_TEXTURE_PAGE_TEXT_IMAGES */
    DR_MODE image_draw_mode;            /* 0x018; MENU_TEXTURE_PAGE_ITEM_ICONS */
    SPRT sprites[24];                   /* 0x024: 19 panel sprites, then 5 item icons */
    TILE tiles[2];                      /* 0x204 */
    LINE_F2 lines[8];                   /* 0x224 */
    menu_palette_primitives_t* palette; /* 0x2a4 */
    scaled_draw_area_pair_t draw_areas; /* 0x2a8 */
} menu_status_screen_frame_t;
typedef char menu_status_screen_frame_size_must_be_720[sizeof(menu_status_screen_frame_t) == 720 ? 1 : -1];

/* Color and endpoint offsets of the Status screen panel's eight frame lines, relative to the panel box, seven bytes
 * per line (g_menu_status_screen_line_colors). The loop walks the table with a byte offset stepping by 7 (the
 * target's induction variable and exit test), so the fields are addressed by
 * offset, as for menu_line_layout_t. */
enum menu_status_screen_line_layout_offset {
    MENU_STATUS_SCREEN_LINE_R = 0,
    MENU_STATUS_SCREEN_LINE_G = 1,
    MENU_STATUS_SCREEN_LINE_B = 2,
    MENU_STATUS_SCREEN_LINE_X0 = 3,
    MENU_STATUS_SCREEN_LINE_Y0 = 4,
    MENU_STATUS_SCREEN_LINE_X1 = 5,
    MENU_STATUS_SCREEN_LINE_Y1 = 6,
    MENU_STATUS_SCREEN_LINE_SIZE = 7
};

extern menu_status_screen_frame_t g_menu_status_screen_frames[2];
extern RECT g_menu_status_screen_panel_rect;  /* panel box */
extern RECT g_menu_status_screen_cursor_rect; /* cursor source */
extern menu_palette_primitives_t g_menu_status_screen_palette;
extern RECT g_menu_status_screen_text_rect_a; /* first text image */
extern RECT g_menu_status_screen_text_rect_b; /* second text image */
extern image_load_parameters_t g_menu_status_screen_row_image_params_a[4];
extern image_load_parameters_t g_menu_status_screen_row_image_params_b[4];
extern image_load_parameters_t g_menu_status_screen_sprite_image_params_a[19];
extern image_load_parameters_t g_menu_status_screen_sprite_image_params_b[19];
extern image_load_parameters_t g_menu_status_screen_item_icon_image_params[5];

void menu_run_status_screen_thread(void);

#endif
