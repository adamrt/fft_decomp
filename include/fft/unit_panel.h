#ifndef UNIT_PANEL_H
#define UNIT_PANEL_H

/* The unit status panels BATTLE, WORLD, WLDCORE and the EVENT overlays draw (the status banner, the equipment and
 * ability panels and the numeric panel): the selected and comparison unit records, the panel threads, packets, status
 * icons, portraits and cell layouts, and each overlay's own panel state. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

struct battle_map_selected_tile;
struct unit_stats;
union menu_stack_record;

/* status values */
/* Name, job and identity values copied into WORLD and EQUIP status panels.
 * The last displayed value is the roster slot, distinct from a battle id. */
typedef struct menu_unit_status_identity {
    s16 unit_index;  /* 0x00: name text 0x4000 + value (unit_panel_banner_thread) */
    s16 job_id;      /* 0x02: job name text 0x3000 + value */
    s16 brave;       /* 0x04 */
    s16 faith;       /* 0x06 */
    s16 zodiac;      /* 0x08 */
    s16 roster_slot; /* 0x0a: party slot (formation_selected_unit_name_id_get); shown as value + 1 */
    u8 _unused_0c[2];
} menu_unit_status_identity_t;
typedef char menu_unit_status_identity_size_must_be_14[sizeof(menu_unit_status_identity_t) == 14 ? 1 : -1];

/* u16 view of the -1 stored in an empty menu_status_panel_slot_state_t.ability_ids row. */
enum { MENU_STATUS_PANEL_LABEL_NONE = 0xffff };

/* Unit stats and equipment/ability rows used by status panels and previews.
 * The arrays follow the five equipment and ability slots. */
typedef struct menu_status_panel_slot_state {
    s16 move;                     /* 0x00 */
    s16 speed;                    /* 0x02 */
    s16 jump;                     /* 0x04 */
    s16 right_weapon_power;       /* 0x06 */
    s16 left_weapon_power;        /* 0x08 */
    s16 right_weapon_evade;       /* 0x0a */
    s16 left_weapon_evade;        /* 0x0c */
    s16 generic_monster;          /* 0x0e: star icons instead of ability-type icons; selects the
                                   * ability-name text section and gates the per-slot portrait
                                   * hiding pass */
    s16 two_hands;                /* 0x10: two-hands flag */
    s16 physical_attack;          /* 0x12 */
    s16 physical_class_evade;     /* 0x14 */
    s16 physical_shield_evade;    /* 0x16 */
    s16 physical_accessory_evade; /* 0x18 */
    u8 _unused_1a[2];
    s16 magical_attack;          /* 0x1c */
    s16 magical_class_evade;     /* 0x1e */
    s16 magical_shield_evade;    /* 0x20 */
    s16 magical_accessory_evade; /* 0x22 */
    s16 equipment[5];            /* 0x24: RH, LH, helmet, armor, accessory; low byte 0xff
                                  * means "no entry"; otherwise rendered through the
                                  * item-name text section and passed to
                                  * gfx_set_item_icon_sprite */
    s16 ability_ids[5];          /* 0x2e: primary/secondary skillset, reaction, support,
                                  * movement; -1 means no label; the first two rows select
                                  * the skillset-name or ability-name text section by mode,
                                  * and the remaining rows use ability names */
} menu_status_panel_slot_state_t;
typedef char menu_status_panel_slot_state_size_must_be_56[(sizeof(menu_status_panel_slot_state_t) == 56) ? 1 : -1];

enum { MENU_UNIT_STATUS_GAUGE_HP = 0, MENU_UNIT_STATUS_GAUGE_MP = 1, MENU_UNIT_STATUS_GAUGE_CT = 2 };

/* HP, MP or CT gauge, drawn 32 * value / max pixels wide. Overlay panels
 * use a four-pixel stub when max is zero; the billboard builders instead
 * store 1 for zero HP/MP maxima. Those builders clear HP/MP bonuses but leave
 * the CT bonus field untouched. */
typedef struct menu_unit_status_gauge {
    s16 value; /* 0x00 */
    s16 bonus; /* 0x02; equipment contribution or preview delta for HP/MP */
    s16 max;   /* 0x04 */
} menu_unit_status_gauge_t;
typedef char menu_unit_status_gauge_size_must_be_6[(sizeof(menu_unit_status_gauge_t) == 6) ? 1 : -1];

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
 * panel billboards, each pair 0x24 apart. unit_panel_fill_billboard and unit_panel_party_unit_prepare fill the first
 * 0x1e bytes, unit_view_stats_panel_thread and the overlays' unit_panel_banner_thread draw it, and
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
    unit_panel_record_t* billboard, struct battle_map_selected_tile* cursor_tile);
void unit_panel_copy_billboard_in(
    const unit_panel_summary_t* banner, const unit_panel_unit_t* unit, const unit_panel_record_t* billboard);

/* ATTACK's and REQUIRE's: rebuilds party slot party_index into a scratch unit and fills the selected unit's status
 * and editor records from it. */
void unit_panel_load_party_unit(s32 unused, s32 party_index);

/* WLDCORE's: loads party member party_index into g_unit_panel_unit and fills the status-panel records the
 * participant detail level draws. */
void unit_panel_party_unit_prepare(s32 party_index);

/* The battle unit stats of g_unit_panel_selected_unit. */
struct unit_stats* menu_selected_unit_stats_get(void);

/* panel threads */
/* Flags of the unit status panel parameter records. */
typedef enum menu_status_panel_flags {
    MENU_STATUS_PANEL_SLIDE_DOWN = 0x20, /* run the slide-down table once */
    MENU_STATUS_PANEL_SLIDE_UP = 0x40,   /* run the slide-up table once */
    MENU_STATUS_PANEL_HIDDEN = 0x80      /* draw nothing; the only flag kept when a slide ends */
} menu_status_panel_flags_e;

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

/* The numeric display frame, built by status_screen_init_numbers_frame: two DR_MODE
 * packets, twelve LINE_F2 packets and one covering SPRT whose CLUT
 * menu_init_primitive_colors_normal and _dimmed set. */
typedef struct menu_numeric_frame {
    DR_MODE draw_mode;      /* 0x00: texture window g_gfx_texture_window */
    DR_MODE draw_mode_menu; /* 0x0c: texture window g_status_screen_numbers_texture_window */
    LINE_F2 lines[12];      /* 0x18 */
    SPRT sprite;            /* 0xd8 */
} menu_numeric_frame_t;
typedef char menu_numeric_frame_size_must_be_236[(sizeof(menu_numeric_frame_t) == 236) ? 1 : -1];

/* Status-panel packet block (0x1d8 bytes), doubled the same way as the editor
 * block. sprites[6] doubles as the scroll cursor: the display threads write
 * its texture window from the scroll row/column. */
typedef struct menu_status_panel_packet {
    menu_numeric_frame_t numeric_frame;            /* 0x000 */
    SPRT sprites[7];                               /* 0x0ec */
    DR_MODE draw_mode_a;                           /* 0x178 */
    DR_MODE draw_mode_b;                           /* 0x184 */
    menu_status_panel_draw_offset_t draw_offset_a; /* 0x190 */
    menu_status_panel_draw_offset_t draw_offset_b; /* 0x1a0 */
    POLY_FT4 portrait;                             /* 0x1b0 */
} menu_status_panel_packet_t;
typedef char menu_status_panel_packet_size_must_be_472[(sizeof(menu_status_panel_packet_t) == 472) ? 1 : -1];

/* Character-editor packet block (0x30c bytes) built by the overlays' unit_panel_banner_thread. Two copies
 * sit back to back and `frame & 1` selects one, so the copy stride proves the size. The head is the numeric
 * display frame that status_screen_init_numbers_frame builds, the SPRT/POLY
 * strides come from the walks that configure and submit them, and the
 * draw-offset packets from the SetDrawOffset pairs. */
typedef struct menu_status_panel_editor_packet {
    menu_numeric_frame_t numeric_frame;            /* 0x000 */
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

/* Draw modes, sprites and border packets shared by the equipment and Status frames.
 * The initializer and color helper use this prefix; draw offsets, numeric frames and
 * draw areas belong to their complete frame records. */
typedef struct unit_panel_frame {
    DR_MODE frame_draw_mode; /* 0x000; MENU_TEXTURE_PAGE_FRAME */
    DR_MODE text_draw_mode;  /* 0x00c; MENU_TEXTURE_PAGE_TEXT_IMAGES */
    DR_MODE image_draw_mode; /* 0x018; item icons or allocated images */
    SPRT sprites[24];        /* 0x024 */
    TILE tiles[2];           /* 0x204 */
    LINE_F2 lines[8];        /* 0x224 */
} unit_panel_frame_t;
typedef char unit_panel_frame_size_must_be_676[sizeof(unit_panel_frame_t) == 676 ? 1 : -1];

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
    unit_panel_frame_t frame;                        /* 0x000 */
    menu_status_panel_draw_offset_t draw_offsets[2]; /* 0x2a4 */
    menu_numeric_frame_t numeric_frame;              /* 0x2c4 */
    scaled_draw_area_pair_t portrait;                /* 0x3b0 */
} unit_panel_buffer_t;
typedef char unit_panel_buffer_size_must_be_984[(sizeof(unit_panel_buffer_t) == 984) ? 1 : -1];

/* The frame lines in the normal or dimmed menu colors and the covering sprite in the matching CLUT. */
void menu_init_primitive_colors_normal(menu_numeric_frame_t* menu);
void menu_init_primitive_colors_dimmed(menu_numeric_frame_t* menu);

void unit_panel_init_primitives(unit_panel_frame_t* menu);

/* The dimmed frame colors for style 1 or while the input lock mode is 1, otherwise the normal ones. */
void unit_panel_select_palette(menu_numeric_frame_t* primitives, const unit_panel_thread_t* thread);

/* The panel's sprite CLUTs and the tint of its last five sprites: dimmed and blue for style 1 or while the input
 * lock mode is 1, otherwise normal. */
void unit_panel_set_colors(unit_panel_frame_t* primitives, const unit_panel_thread_t* state);

/* status icons */
/* ATTACK, DEBUGCHR and REQUIRE each define 76 status cells.
 * Group boundaries select spans drawn in reverse order. */
extern sprite_sheet_frame_t g_unit_panel_status_frames[76];
extern point16_t g_unit_panel_status_frame_scales[76];

/* ATTACK, DEBUGCHR and REQUIRE each define the animated status groups.
 * Each group has a countdown byte; 0xff marks an inactive group. */
extern u8 g_unit_panel_status_animation[];
extern u8 g_unit_panel_status_group_count;

void unit_panel_build_status_groups(POLY_FT4* poly);

/* portraits */
extern RECT g_menu_portrait_origin;

extern palette_t g_unit_panel_selected_portrait_palettes[2];
extern palette_t g_unit_panel_comparison_portrait_palettes[2];

/* thread status indicators */
/* g_menu_panel_backdrop_state[] (and EQUIP's g_equipment_indicator_state[]): state of each of the two translucent
 * panels menu_draw_status_panel_backdrops draws behind the unit status banners. */
typedef enum menu_indicator_state {
    MENU_INDICATOR_STATE_HIDDEN = 0,    /* not drawn */
    MENU_INDICATOR_STATE_FADING_IN = 1, /* brightness rises by 0xc a frame up to 0x78 and holds */
    MENU_INDICATOR_STATE_FADING_OUT = 2 /* brightness falls by 0xc a frame; at zero the state returns to HIDDEN */
} menu_indicator_state_e;

void menu_draw_status_panel_backdrops(void);

/* character status panel */
/* The selected unit's and the comparison unit's status panels, with their billboards, packets, numbers, name
 * and portrait. */
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

/* numeric panel */
/* The numeric panel's two adjacent rectangles: the first is the panel frame
 * and the screen origin handed to image loading, the second places the
 * portrait. */
typedef struct menu_status_panel_numeric_geometry {
    RECT texture_rect;   /* 0x00 */
    RECT draw_area_rect; /* 0x08 */
} menu_status_panel_numeric_geometry_t;

/* Numeric panel buffer (0x2c0 bytes), double buffered by unit_panel_numbers_thread, which copies it with sizeof. The
 * draw modes are set up with gfx_set_draw_mode_for_texture_page / SetDrawMode, the 18 SPRTs by the
 * gfx_set_image_primitive_layout walk, +0x18c and +0x298 are the arguments of status_screen_init_numbers_frame and
 * menu_init_scaled_draw_area_packets, and the draw offsets follow from the SetDrawOffset(packet, position) pairs. */
typedef struct menu_status_panel_numeric_buffer {
    DR_MODE frame_draw_mode;                         /* 0x000; MENU_TEXTURE_PAGE_FRAME */
    DR_MODE text_draw_mode;                          /* 0x00c; MENU_TEXTURE_PAGE_TEXT_IMAGES */
    DR_MODE image_draw_mode;                         /* 0x018; item icons or allocated images */
    SPRT sprites[18];                                /* 0x024 */
    menu_numeric_frame_t numeric_frame;              /* 0x18c */
    menu_status_panel_draw_offset_t draw_offsets[2]; /* 0x278 */
    scaled_draw_area_pair_t portrait;                /* 0x298 */
} menu_status_panel_numeric_buffer_t;
typedef char
    menu_status_panel_numeric_buffer_size_must_be_704[(sizeof(menu_status_panel_numeric_buffer_t) == 704) ? 1 : -1];

extern menu_status_panel_numeric_geometry_t g_unit_panel_editor_numeric_table;
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

/* Numeric panel thread: ATTACK/REQUIRE and BUNIT/WORLD share bodies; DEBUGCHR and EQUIP each define their own. */
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
extern unit_panel_unit_t g_unit_panel_panel_comparison_unit_data;

void unit_panel_toggle_equipment_and_numbers(s32 enable);

/* panel state (EQUIP) */
extern unit_panel_thread_t g_unit_panel_unit_status_panel_params;

void unit_panel_toggle_equipment_threads(s32 enable);
void unit_panel_toggle_item_numbers_thread(s32 enable);

/* panel state (REQUIRE) */
void unit_panel_set_transition(s32 value);
void unit_panel_transition_thread(void);

/* help levels (WLDCORE) */
/* The window-layout request that opens a MENU_STACK_LEVEL_UNIT_PANEL_HELP level. help_cursor_move reads the
 * first three fields. */
typedef struct unit_panel_help_level {
    s32 window_index;
    s32 layout; /* 0x04; row of g_unit_panel_help_rows */
    s32 cached_layout_kind;
    s32 saved_state; /* 0x0c; previous g_menu_stack_ot_offset, saved by the push */
} unit_panel_help_level_t;

void unit_panel_help_open(void);

#endif
