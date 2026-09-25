#ifndef FFT_BATTLE_MENU_STATUS_PANEL_H
#define FFT_BATTLE_MENU_STATUS_PANEL_H

#include "fft/menu_types.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Battle menu status and character-editor panel records. The EVENT overlays
 * (ATTACK, BUNIT, DEBUGCHR, EQUIP, REQUIRE) each link their own copy of the
 * panel code, and the WORLD unit-editor panels reuse the packet layouts. */

/* libgpu DR_OFFSET packet (the DR_MODE layout) followed by the x/y pair that
 * SetDrawOffset() reads back into it. */
typedef struct battle_menu_status_panel_draw_offset {
    u32 tag;     /* 0x00 */
    u32 code[2]; /* 0x04 */
    s16 x;       /* 0x0c */
    s16 y;       /* 0x0e */
} battle_menu_status_panel_draw_offset_t;

typedef char battle_menu_status_panel_draw_offset_size_must_be_0x10
    [(sizeof(battle_menu_status_panel_draw_offset_t) == 0x10) ? 1 : -1];

/* The u16 x/y translation the status threads add to the frame tiles and
 * lines, read from the head of each overlay's panel-origin table (ATTACK
 * g_attack_character_status_frame_rect, DEBUGCHR
 * g_debugchr_character_status_frame_rect, EQUIP
 * g_equip_character_status_frame_rect, REQUIRE
 * g_require_character_status_frame_rect). Derived from the tile/line fixup
 * loops in the *_panel_run_character_status_thread routines. */
typedef struct battle_menu_status_panel_offset_pair {
    u16 x; /* 0x00 */
    u16 y; /* 0x02 */
} battle_menu_status_panel_offset_pair_t;

/* Menu primitive block written by the *_gfx_init_menu_tile_and_line_primitives
 * builders: three DR_MODE packets, two translucent 16x90 backdrop tiles and
 * eight vertical frame lines. The sprites between belong to the full status
 * buffer below and are not touched by the builder. */
typedef struct battle_menu_status_panel_menu_primitives {
    DR_MODE draw_modes[3]; /* 0x000 */
    u8 unknown_24[0x204 - 0x24];
    TILE tiles[2];    /* 0x204 */
    LINE_F2 lines[8]; /* 0x224 */
} battle_menu_status_panel_menu_primitives_t;

typedef char battle_menu_status_panel_menu_primitives_size_must_be_0x2a4
    [(sizeof(battle_menu_status_panel_menu_primitives_t) == 0x2a4) ? 1 : -1];

/* Status panel sprite view used by the *_panel_set_primitive_colors helpers:
 * a 0x10-byte head followed by 0x14-byte SPRT packets, so sprites[n + 1] is
 * battle_menu_status_panel_buffer_t.sprites[n]. */
typedef struct battle_menu_status_panel_primitives {
    u8 header[0x10];
    SPRT sprites[25]; /* 0x010 */
} battle_menu_status_panel_primitives_t;

typedef char battle_menu_status_panel_primitives_size_must_be_0x204
    [(sizeof(battle_menu_status_panel_primitives_t) == 0x204) ? 1 : -1];

/* 0x13c primitive block of the *_menu_draw_thread_status_indicators letterbox
 * panels: draw mode, the panel frame tile, and two banks of nine one-pixel
 * scanline tiles that produce the top and bottom gradients. Each overlay
 * double-buffers one block per panel ([panel][buffer]). */
typedef struct battle_menu_status_panel_indicator_prims {
    DR_MODE mode;   /* 0x00 */
    TILE frame;     /* 0x0c */
    TILE top[9];    /* 0x1c */
    TILE bottom[9]; /* 0xac */
} battle_menu_status_panel_indicator_prims_t;

typedef char battle_menu_status_panel_indicator_prims_size_must_be_0x13c
    [(sizeof(battle_menu_status_panel_indicator_prims_t) == 0x13c) ? 1 : -1];

/* Scaled draw-area packet pair at +0x3b0 of the status buffer: the same
 * layout as world_gfx_scaled_draw_area_pair_t. The *_gfx_build_scaled_draw_area_packets
 * helpers call SetDrawArea(areas[0], rects[0]) and SetDrawArea(areas[1], rects[1]);
 * the *_gfx_init_scaled_draw_area_packets helpers clear rects[0].w and rects[1].h
 * and copy the menu template into rects[1]. */
typedef struct battle_menu_status_panel_portrait_primitive_tail {
    DR_AREA areas[2]; /* 0x00 */
    RECT rects[2];    /* 0x18 */
} battle_menu_status_panel_portrait_primitive_tail_t;

typedef char battle_menu_status_panel_portrait_primitive_tail_size_must_be_0x28
    [(sizeof(battle_menu_status_panel_portrait_primitive_tail_t) == 0x28) ? 1 : -1];

/* Per-frame character status buffer (0x3d8 bytes), double buffered by the
 * *_panel_run_character_status_thread routines, which copy the first buffer
 * to the second (base + 0x3d8) before the render loop starts. Evidence, from
 * the DEBUGCHR routine and its byte-identical twins:
 *  - the DR_MODE packets at +0x00/+0x0c/+0x18 are submitted individually, and
 *    the numeric threads fill the same slots with
 *    battle_gfx_set_draw_mode_for_texture_page;
 *  - the submit loops walk the SPRTs from +0x24 with the sprite index as their
 *    counter (0..11 from +0x24, 12 at +0x114, 13 at +0x128, 14..18 from
 *    +0x13c, 19..23 from +0x1a0);
 *  - the TILE/LINE_F2 pair at +0x204/+0x224 is the one
 *    battle_menu_status_panel_menu_primitives_t describes;
 *  - the two draw-offset packets follow from the SetDrawOffset(packet,
 *    position) calls at +0x2a4/+0x2b0 and +0x2b4/+0x2c0;
 *  - +0x2c4 and +0x3b0 are the arguments of
 *    battle_menu_init_numeric_display_frame_primitives and the
 *    *_gfx_init_scaled_draw_area_packets / *_gfx_build_scaled_draw_area_packets
 *    pair. */
typedef struct battle_menu_status_panel_buffer {
    DR_MODE draw_modes[3];                                       /* 0x000 */
    SPRT sprites[24];                                            /* 0x024 */
    TILE tiles[2];                                               /* 0x204 */
    LINE_F2 lines[8];                                            /* 0x224 */
    battle_menu_status_panel_draw_offset_t draw_offsets[2];      /* 0x2a4 */
    world_menu_palette_primitives_t numeric_frame;               /* 0x2c4 */
    battle_menu_status_panel_portrait_primitive_tail_t portrait; /* 0x3b0 */
} battle_menu_status_panel_buffer_t;

typedef char
    battle_menu_status_panel_buffer_size_must_be_0x3d8[(sizeof(battle_menu_status_panel_buffer_t) == 0x3d8) ? 1 : -1];

/* Numeric editor panel buffer (0x2c0 bytes), double buffered by the
 * *_editor_run_numeric_thread routines (EQUIP equip_editor_run_numeric_thread copies it with
 * sizeof). The draw modes are set up with battle_gfx_set_draw_mode_for_texture_page /
 * SetDrawMode, the 18 SPRTs by the battle_gfx_init_image_loading walk,
 * +0x18c and +0x298 are the arguments of
 * battle_menu_init_numeric_display_frame_primitives and
 * *_gfx_init_scaled_draw_area_packets, and the draw offsets follow from the
 * SetDrawOffset(packet, position) pairs. */
typedef struct battle_menu_status_panel_numeric_buffer {
    DR_MODE draw_modes[3];                                       /* 0x000 */
    SPRT sprites[18];                                            /* 0x024 */
    world_menu_palette_primitives_t numeric_frame;               /* 0x18c */
    battle_menu_status_panel_draw_offset_t draw_offsets[2];      /* 0x278 */
    battle_menu_status_panel_portrait_primitive_tail_t portrait; /* 0x298 */
} battle_menu_status_panel_numeric_buffer_t;

typedef char battle_menu_status_panel_numeric_buffer_size_must_be_0x2c0
    [(sizeof(battle_menu_status_panel_numeric_buffer_t) == 0x2c0) ? 1 : -1];

/* Parameter block of the character status thread: the panel origin, a redraw
 * request and the frame style read by *_panel_set_primitive_colors. */
typedef struct battle_menu_status_panel_frame_config {
    u16 origin_x; /* 0x00 */
    u8 unknown_02[2];
    u16 origin_y; /* 0x04 */
    u8 unknown_06[6];
    s32 redraw_request; /* 0x0c: set to 1 elsewhere to request a text/portrait rebuild */
    s32 style;          /* 0x10 */
} battle_menu_status_panel_frame_config_t;

typedef char battle_menu_status_panel_frame_config_size_must_be_0x14
    [(sizeof(battle_menu_status_panel_frame_config_t) == 0x14) ? 1 : -1];

/* u16 view of the -1 stored in an empty battle_menu_status_panel_slot_state_t.ability_ids row. */
enum {
    BATTLE_MENU_STATUS_PANEL_LABEL_NONE = 0xffff,
};

/* Per-slot editor state (0x38 bytes). Both arrays are indexed by the five
 * character slots; the status threads walk them with 2-byte cursors. The
 * layout is the lower billboard record (0x801ca0e8). */
typedef struct battle_menu_status_panel_slot_state {
    u8 unknown_00[0x0e];
    s16 generic_monster; /* 0x0e: star icons instead of ability-type icons; selects the
                          * ability-name text bank and gates the per-slot portrait
                          * hiding pass; low bit of display_mode */
    s16 two_hands;       /* 0x10: two-hands flag; high bit of display_mode */
    u8 unknown_12[0x12];
    u16 equipment[5];   /* 0x24: RH, LH, helmet, armor, accessory; low byte 0xff
                         * means "no entry"; otherwise rendered through the
                         * item-name text bank and passed to
                         * battle_get_item_graphic_data */
    s16 ability_ids[5]; /* 0x2e: primary/secondary skillset, reaction, support,
                         * movement; -1 means no label; the first two rows select
                         * the skillset-name or ability-name text bank by mode,
                         * and the remaining rows use ability names */
} battle_menu_status_panel_slot_state_t;

typedef char
    battle_menu_status_panel_slot_state_size_must_be_0x38[(sizeof(battle_menu_status_panel_slot_state_t) == 0x38) ? 1
                                                                                                                  : -1];

/* Unit gauge record (0x1e bytes) copied from battle_stats_t for the status
 * panel; the same shape as the first 0x1e bytes of battle_unit_status_record_t. */
typedef struct battle_menu_status_panel_gauges {
    s16 level;      /* 0x00 */
    s16 team_state; /* 0x02 */
    s16 _04;
    s16 _06;
    s16 experience; /* 0x08 */
    s16 unit_index; /* 0x0a */
    u16 hp;         /* 0x0c */
    s16 _0e;
    u16 max_hp; /* 0x10 */
    u16 mp;     /* 0x12 */
    s16 _14;
    u16 max_mp; /* 0x16 */
    s16 ct;     /* 0x18 */
    s16 _1a;
    s16 max_ct; /* 0x1c */
} battle_menu_status_panel_gauges_t;

typedef char
    battle_menu_status_panel_gauges_size_must_be_0x1e[(sizeof(battle_menu_status_panel_gauges_t) == 0x1e) ? 1 : -1];

/* One value bar: the Gouraud bar is drawn value/limit wide. */
typedef struct battle_menu_status_panel_bar {
    s16 value;
    s16 unknown_02;
    s16 limit; /* 0x04; a zero limit draws the fixed 4-pixel stub */
} battle_menu_status_panel_bar_t;

typedef char battle_menu_status_panel_bar_size_must_be_6[(sizeof(battle_menu_status_panel_bar_t) == 6) ? 1 : -1];

/* Editor state record (0x24 bytes) of the editor display threads. DEBUGCHR's
 * two instances, g_debugchr_panel_selected_billboard and
 * g_debugchr_panel_comparison_billboard, are 0x24 apart. `mode` selects
 * one of the 0xc-byte geometry rows and the panel's CLUT; `bars` holds the
 * three value/limit triples the Gouraud bars are drawn from. */
typedef struct battle_menu_status_panel_editor_state {
    u8 level; /* 0x00; debugchr_editor_apply_values_to_battle_unit writes it back */
    u8 unknown_01;
    s16 mode;       /* 0x02 */
    s16 list_index; /* 0x04: AT-list position; drawn as dashes (format 0xc00)
                       when negative */
    s16 unknown_06;
    u8 experience; /* 0x08; written back with level */
    u8 unknown_09;
    s16 unit;                               /* 0x0a */
    battle_menu_status_panel_bar_t bars[3]; /* 0x0c */
    s16 unknown_1E[3];
} battle_menu_status_panel_editor_state_t;

typedef char battle_menu_status_panel_editor_state_size_must_be_0x24
    [(sizeof(battle_menu_status_panel_editor_state_t) == 0x24) ? 1 : -1];

/* One numeric-display entry (0xc bytes). battle_menu_draw_numeric_display_entries
 * takes a base and a count of these; the display threads write only the
 * halfword at +8, choosing the code the value is drawn with. */
typedef struct battle_menu_status_panel_numeric_entry {
    u8 unknown_00[8];
    s16 format; /* 0x08: digits argument; low byte digit count, high byte sign/marker
                   flags (0xc00/0xe00 draw dashes) */
    u8 unknown_0A[2];
} battle_menu_status_panel_numeric_entry_t;

typedef char battle_menu_status_panel_numeric_entry_size_must_be_0xc
    [(sizeof(battle_menu_status_panel_numeric_entry_t) == 0xc) ? 1 : -1];

/* Character-editor packet block (0x30c bytes) built by the editor display
 * threads. Two copies sit back to back and `frame & 1` selects one, so the
 * copy stride proves the size. The head is the numeric display frame that
 * battle_menu_init_numeric_display_frame_primitives builds, the SPRT/POLY
 * strides come from the walks that configure and submit them, and the
 * draw-offset packets from the SetDrawOffset pairs. */
typedef struct battle_menu_status_panel_editor_packet {
    u8 unknown_000[0xec];                                 /* numeric display frame */
    DR_MODE draw_mode_a;                                  /* 0x0ec */
    DR_MODE draw_mode_b;                                  /* 0x0f8 */
    SPRT value_sprites[4];                                /* 0x104 */
    SPRT label_sprites[7];                                /* 0x154; [6] at 0x1cc is the mode row */
    POLY_G4 bars[3];                                      /* 0x1e0 */
    POLY_FT4 portrait[4];                                 /* 0x24c */
    battle_menu_status_panel_draw_offset_t draw_offset_a; /* 0x2ec */
    battle_menu_status_panel_draw_offset_t draw_offset_b; /* 0x2fc */
} battle_menu_status_panel_editor_packet_t;

typedef char battle_menu_status_panel_editor_packet_size_must_be_0x30c
    [(sizeof(battle_menu_status_panel_editor_packet_t) == 0x30c) ? 1 : -1];

/* Status-panel packet block (0x1d8 bytes), doubled the same way as the editor
 * block. sprites[6] doubles as the scroll cursor: the display threads write
 * its texture window from the scroll row/column. */
typedef struct battle_menu_status_panel_packet {
    u8 unknown_000[0xec];                                 /* numeric display frame */
    SPRT sprites[7];                                      /* 0x0ec */
    DR_MODE draw_mode_a;                                  /* 0x178 */
    DR_MODE draw_mode_b;                                  /* 0x184 */
    battle_menu_status_panel_draw_offset_t draw_offset_a; /* 0x190 */
    battle_menu_status_panel_draw_offset_t draw_offset_b; /* 0x1a0 */
    POLY_FT4 portrait;                                    /* 0x1b0 */
} battle_menu_status_panel_packet_t;

typedef char
    battle_menu_status_panel_packet_size_must_be_0x1d8[(sizeof(battle_menu_status_panel_packet_t) == 0x1d8) ? 1 : -1];

/* Parameter block of the editor display threads, reached through the
 * 0x400-byte battle thread table. The shake offset is written as a word from
 * the s16 shake tables and read back as its low halfword, which is how the
 * target loads it into the draw-offset y. Only the members the display threads
 * use are named; the record's total size is not proven. */
typedef struct battle_menu_status_panel_display_thread {
    u16 x; /* 0x00 */
    u16 unknown_02;
    union {
        s32 word; /* 0x04 */
        u16 low;
    } shake_y;
    s32 flags;     /* 0x08: 0x20 and 0x40 start the two shake tables, 0x80
                      suppresses all drawing */
    void* work;    /* 0x0c: redraw request, cleared once serviced */
    s32 highlight; /* 0x10: nonzero forces the alternate (highlight) CLUTs; bound as
                      g_bunit_status_display_highlight and the WORLD display globals */
} battle_menu_status_panel_display_thread_t;

/* Item graphic load descriptor filled by *_gfx_build_item_graphic_descriptor. */
typedef struct battle_menu_status_panel_graphic_descriptor {
    s16 x_load_location;
    s16 y_load_location;
    s16 width;
    s16 height;
    u16 clut;
    s16 tpage;
} battle_menu_status_panel_graphic_descriptor_t;

typedef char battle_menu_status_panel_graphic_descriptor_size_must_be_0xc
    [(sizeof(battle_menu_status_panel_graphic_descriptor_t) == 0xc) ? 1 : -1];

/* Ordering-tag/intensity/scale record read by the
 * *_gfx_build_scaled_sprite_primitive_chain builders. */
typedef struct battle_menu_status_panel_scaled_sprite {
    u32* ordering_tag;
    u8 intensity;
    u8 pad_05[3];
    s32 link_primitive;
    s16 scale_x;
    s16 scale_y;
    u8 pad_10[8];
    s16 offset[6];
} battle_menu_status_panel_scaled_sprite_t;

typedef char battle_menu_status_panel_scaled_sprite_size_must_be_0x24
    [(sizeof(battle_menu_status_panel_scaled_sprite_t) == 0x24) ? 1 : -1];

/* Glyph source rectangle used by the *_text_render_decimal_value renderers. */
typedef struct battle_menu_status_panel_glyph {
    s16 source_x;
    s16 source_y;
    s16 width;
    s16 height;
} battle_menu_status_panel_glyph_t;

/* One gauge value for the *_text_render_decimal_entry_list and
 * *_text_render_signed_decimal_entries renderers: the text position, a
 * pointer to the value and the decimal-format flags. */
typedef struct battle_menu_status_panel_gauge_entry {
    u16 x;
    u16 y;
    s16* value;
    s16 flags;
    u16 unknown_0a;
} battle_menu_status_panel_gauge_entry_t;

typedef char
    battle_menu_status_panel_gauge_entry_size_must_be_0xc[(sizeof(battle_menu_status_panel_gauge_entry_t) == 0xc) ? 1
                                                                                                                  : -1];

/* Text cursor passed to *_text_render_decimal_value. The entry-list renderers
 * set the word at +0x0c to a 4bpp colour fill pattern (0x77777777 or
 * 0x88888888 for negative values, 0xbbbbbbbb or 0xcccccccc for positive ones)
 * and clear it after each value; the value renderer clears it while it draws
 * the sign glyphs. */
typedef struct battle_menu_status_panel_text_position {
    u16 x;
    u16 y;
    u8 unknown_04[4];
    u16 row_stride; /* 0x08 */
    u16 unknown_0a;
    u32 color; /* 0x0c */
} battle_menu_status_panel_text_position_t;

typedef char battle_menu_status_panel_text_position_size_must_be_0x10
    [(sizeof(battle_menu_status_panel_text_position_t) == 0x10) ? 1 : -1];

#endif
