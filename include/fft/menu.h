#ifndef MENU_H
#define MENU_H

/* Menu, window and text records shared across modules. */

#include "fft/data.h"
#include "fft/gfx.h"
#include "fft/map.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

struct action_command;
struct action_data;
struct unit_stats;

/* scene lifecycle */
/* 0: normal, 1: companion/editor transition, 2: AT-list selection. */
extern u16 g_menu_overlay_state;

void menu_init_palette_and_text(s32 unused_0);
void menu_init_subsystem_pointers(void);
void menu_init_subsystems(void);
s32 menu_init_attack_resources_and_threads(void);
void menu_reset_subsystems(void);
s32 menu_poll_small_load(void);

/* modal state */
/* Nonzero while a modal selection, panel or companion overlay owns the input. */
extern u16 g_menu_modal_open;

void menu_set_modal_open(void);
void menu_clear_modal_open(void);
/* Refused-command thread: queue the invalid sound and exit. */
void menu_run_invalid_choice_thread(void);

/* ability */
/* Provisional ability-list work buffer at *g_ai_workspace_ptr (82 rows each).
 * WORLD twin of the BATTLE ability list. */
typedef struct menu_ability_list {
    u16 ids[82];     /* 0x000; 0xffff terminated; the text id base of the names is ORed in */
    u16 values[82];  /* 0x0a4 */
    u16 extras[82];  /* 0x148 */
    u8 mp_costs[82]; /* 0x1ec; 0xff terminated */
    u8 ct[82];       /* 0x23e; CT */
    u8 flags[82];    /* 0x290; bit 1 = blocked by unit flag 0x59 & 8 */
    u8 turns[82];    /* 0x2e2 */
    u8 skill_ct[82]; /* 0x334 */
} menu_ability_list_t;

s32 menu_copy_u8_to_u16_terminated(u16* destination, const u8* source);
void menu_copy_u8_to_u16(u16* destination, const u8* source, s32 count);

/* ability callbacks */
extern s16 g_menu_ability_category;  /* menu_ability_category_e; BUNIT and JOBSTTS tab selection */
extern s16 g_menu_ability_entries[]; /* Packed ability IDs/flags; -1 terminates the list. */
extern s32 g_menu_ability_type;
extern ability_data_t* g_menu_selected_ability_data;
extern main_ability_secondary_pointer_t g_menu_selected_ability_secondary_data;

s32 menu_ability_is_action(s32 unused_row);
s32 menu_ability_is_action_tab(s32 unused_row);
s32 menu_ability_is_movement_tab(s32 unused_row);
s32 menu_ability_is_non_action_learned(s32 index);
s32 menu_ability_is_non_action_unlearned(s32 index);
s32 menu_ability_is_reaction_tab(s32 unused_row);
s32 menu_ability_is_support_tab(s32 unused_row);
s32 menu_ability_get_mp_cost_display_value(s32 index);
s32 menu_ability_get_ct_display_value(s32 index);
s32 menu_ability_get_jp_cost_display_value(s32 index);
s32 menu_ability_is_learned(s32 index);

/* job level callbacks */
extern s16 g_menu_job_ids[];
extern s32 g_menu_job_current_level;
extern u32 g_menu_job_next_level_jp_requirement;

s32 menu_job_calculate_next_level_jp_requirement(s32 index);
s32 menu_job_get_current_level(s32 unused_row);
s32 menu_job_get_next_level_jp_requirement(s32 unused_row);

/* frame timing */
/* Raw VSync mode; animation and script steps use 1 when this is zero. */
extern s8 g_menu_event_speed;
/* Set while a menu thread forces event speed 2; the previous speed is saved. */
extern u16 g_menu_animation_speed_forced;
extern u16 g_menu_saved_animation_speed;

s32 menu_get_event_speed(void);
void menu_set_vsync_mode_and_event_speed(s32 speed);
s32 menu_update_menus_and_event_speed(u32* otag, s32 controller_input);

/* primitives */
void menu_init_default_poly_ft4(POLY_FT4* poly);
s32 menu_lerp_fixed12(s32 start, s32 end, s32 factor);
void menu_lerp_rect_to_poly_ft4(POLY_FT4* poly, const RECT* from, const RECT* to, s32 factor);

/* palette */
/* Two sixteen-color banks: the normal menu colors followed by their dimmed forms. */
typedef struct menu_palette_pair {
    rgba5551_t colors[2 * PALETTE_COLOR_COUNT];
} menu_palette_pair_t;
typedef char menu_palette_pair_size_must_be_64[(sizeof(menu_palette_pair_t) == 64) ? 1 : -1];
extern menu_palette_pair_t g_menu_palette_colors;
extern u8 g_menu_numeric_display_palette_indices[12];
void menu_set_palette_colors(const menu_palette_pair_t* source);

/* Shared 4bpp menu glyph sheet: 32 rows of 128 packed pixel bytes, then
 * its palette. Window-frame builders copy aligned pairs of bytes. */
typedef struct menu_glyph_sheet {
    u8 pixels[4096];             /* 0x0000 */
    menu_palette_pair_t palette; /* 0x1000 */
} menu_glyph_sheet_t;
typedef char menu_glyph_sheet_size_must_be_4160[(sizeof(menu_glyph_sheet_t) == 4160) ? 1 : -1];

extern menu_glyph_sheet_t* g_menu_glyph_sheet;

void menu_build_window_image_row(const u16* tiles, u16* row, s32 count, s32 index);
u8* menu_build_window_image(s32 width, s32 height, RECT* rect, s32 mode, s32 tail_offset, s32 keep);
void menu_build_and_upload_window_image(s32 width, s32 height, RECT* rect, s32 mode, s32 tail_offset);

/* menu_window_buffer_t::icon_flags: which of the window's three icon sprites
 * are drawn. */
typedef enum menu_window_icon_flags {
    MENU_WINDOW_ICON_SCROLL_UP = 0x01,     /* icon_sprites[0]: lines above the first drawn line */
    MENU_WINDOW_ICON_SCROLL_DOWN = 0x02,   /* icon_sprites[1]: lines below the last drawn line */
    MENU_WINDOW_ICON_SCROLL_MARKER = 0x04, /* icon_sprites[2]: the scroll position */
} menu_window_icon_flags_e;

/* Six menu buffers, allocated/released as three adjacent pairs. The BATTLE
 * allocator at 0x8012e348 and WORLD counterpart establish the 0x118 stride.
 * Event ChangeDialog scans records 0, 2 and 4, compares +0x14 with operand 0,
 * and checks the thread at +0x04 for task 0x33. For DisplayMessage, the text
 * threads store the dialog type's box nibble (& 0x70), the portrait unit and
 * the scroll state, and battle/menu_draw_active_window_frames read
 * them. */
typedef struct menu_window_buffer {
    s32 active;             /* 0x00: 1 once the text thread has built the window; the frame
                               drawers skip the pair otherwise; cleared on allocation and close */
    s32 thread_id;          /* 0x04 */
    s32 message_id;         /* 0x08: active/change-dialog text entry ID */
    s32 portrait_code;      /* 0x0c: portrait texture/CLUT descriptor, -1 when unused */
    s32 portrait_battle_id; /* 0x10: battle id of the DisplayMessage unit (box type 0x10),
                               drawn as the portrait when portrait_code == -1 */
    s32 dialogue_selector;  /* 0x14 */
    s32 box_type;           /* 0x18: dialog type & 0x70; 0x10 is the portrait box */
    s32 portrait_flipped;   /* 0x1c: 1 when the tail offset is negative; event
                               instruction 0x37 toggles it */
    s32 icon_flags;         /* 0x20: menu_window_icon_flags_e */
    s32 scroll_range;       /* 0x24: scrollable lines (line - lines); 1 when freshly allocated */
    s32 scroll_position;    /* 0x28: first drawn line; the scroll marker sits at
                               scroll_position / scroll_range of its span; 1 when freshly allocated */
    /* Primitive payload established by the WORLD allocator at 0x800e2548:
     * draw mode, four window sprites, three icon sprites, two quads and a
     * trailing color. */
    DR_MODE draw_mode;    /* 0x2c */
    SPRT sprites[4];      /* 0x38 */
    SPRT icon_sprites[3]; /* 0x88 */
    POLY_FT4 quads[2];    /* 0xc4 */
    rgb24_t color;        /* 0x114; window tint */
    u8 is_message_box;    /* 0x117: set by the message-box thread; alternate frame clut and tint */
} menu_window_buffer_t;
typedef char menu_window_buffer_size_must_be_280[(sizeof(menu_window_buffer_t) == 280) ? 1 : -1];

extern s32 g_menu_packet_buffer_index;
extern menu_window_buffer_t* g_menu_window_buffer_slots[6];
extern menu_window_buffer_t g_menu_window_buffers[6];
extern u16* g_menu_window_tween_easing_tables[];
/* Twelve crop percentages, indexed by step. */
extern u16 g_menu_cursor_zoom_percentages[];

menu_window_buffer_t* menu_alloc_window_buffer_pair(void);
void menu_reset_window_buffer_slots(void);
void menu_release_window_buffer_pair(menu_window_buffer_t* buffer);
void menu_animate_window_quad_crop(menu_window_buffer_t* buffers, RECT* rect, RECT* unused_to);
/* BATTLE and WORLD each link their own body. */
void menu_draw_active_window_frames(void);
void menu_select_sprite_clut(void* packet);
void menu_select_icon_sprite_clut(SPRT* slot);
void menu_tween_window_quads(menu_window_buffer_t* buffers, RECT* to, RECT* to2, s32 table);
void menu_zoom_cursor_frame(const void* unused_source, SPRT* sprite, s32 step);

/* portrait */
/* One EVTFACE.BIN block: pixels followed by eight 16-color palettes. */
typedef struct menu_portrait_image_file {
    u32 pixels[8][48][4];  /* 0x0000; eight 32x48 4bpp portraits */
    palette_t palettes[8]; /* 0x1800; four palettes per uploaded VRAM row */
    u8 _unknown_1900[0x700];
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

enum {
    MENU_WLDFACE_SHEET_COUNT = 4,
    MENU_WLDFACE_SHEET_SIZE = 32768,
    MENU_WLDFACE_FILE_SIZE = MENU_WLDFACE_SHEET_COUNT * MENU_WLDFACE_SHEET_SIZE,
    MENU_WLDFACE_PORTRAITS_PER_SHEET = 40,
    MENU_WLDFACE_FIRST_MONSTER_PORTRAIT = 63,
    MENU_WLDFACE_MONSTER_PALETTE_ROW = 10, /* first row of the monsters' other palettes */
    MENU_WLDFACE_MONSTER_PALETTE_ROW_COUNT = 6,
};
typedef char menu_portrait_sheet_size_must_be_32768[(sizeof(menu_portrait_sheet_t) == 32768) ? 1 : -1];

extern RECT g_menu_portrait_origin;
extern u8 g_menu_portrait_transition_offsets[];
extern RECT g_menu_portrait_pixel_rect;
extern RECT g_menu_portrait_palette_rect;

/* Unreferenced in both scenes: uploads one EVTFACE.BIN portrait and its palettes. */
void menu_load_portrait_image(s32 portrait_id);
void menu_build_unflagged_portrait_poly(s32 portrait_id, POLY_FT4* poly);
void menu_build_unit_portrait_transition(const RECT* texture_rect, s32* transition, const s32* first_portrait,
    const s32* second_portrait, palette_t palettes[2], POLY_FT4* poly, s32 direction);

/* status panels */
/* Battle menu status and character-editor panel records. The EVENT overlays
 * (ATTACK, BUNIT, DEBUGCHR, EQUIP, REQUIRE) each link their own copy of the
 * panel code, and the WORLD unit-editor panels reuse the packet layouts. */
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

extern menu_gradient_panel_prims_t g_menu_panel_fade_frames[];

void menu_init_panel_fade_tiles(void);
void menu_update_panel_fade(void);

/* Menu line-box layout, established by the builder at 0x800e3358: two DR_MODE
 * packets, twelve LINE_F2 packets and one SPRT whose CLUT (+0xe6)
 * *_menu_init_primitive_colors_normal and _dimmed set. */
typedef struct menu_palette_primitives {
    DR_MODE draw_mode;      /* 0x00: texture window g_gfx_texture_window */
    DR_MODE draw_mode_menu; /* 0x0c: texture window g_menu_numeric_display_texture_window */
    LINE_F2 lines[12];      /* 0x18 */
    SPRT sprite;            /* 0xd8 */
} menu_palette_primitives_t;

typedef char menu_palette_primitives_size_must_be_236[(sizeof(menu_palette_primitives_t) == 236) ? 1 : -1];
void menu_init_primitive_colors_normal(menu_palette_primitives_t* menu);
void menu_init_primitive_colors_dimmed(menu_palette_primitives_t* menu);

void menu_init_numeric_display_frame_primitives(RECT* bounds, menu_palette_primitives_t* menu);

void menu_submit_numeric_display_frame_primitives(menu_palette_primitives_t* display);

void menu_init_sprite_array(SPRT* sprites, s32 count, s32 clut);

/* The numeric editor's two adjacent rectangles: the first is the panel frame
 * and the screen origin handed to image loading, the second places the
 * portrait. */
typedef struct battle_menu_status_panel_numeric_geometry {
    RECT texture_rect;   /* 0x00 */
    RECT draw_area_rect; /* 0x08 */
} battle_menu_status_panel_numeric_geometry_t;

/* Numeric editor panel buffer (0x2c0 bytes), double buffered by the
 * *_editor_run_numeric_thread routines (EQUIP equip_editor_run_numeric_thread copies it with
 * sizeof). The draw modes are set up with gfx_set_draw_mode_for_texture_page /
 * SetDrawMode, the 18 SPRTs by the gfx_init_image_loading walk,
 * +0x18c and +0x298 are the arguments of
 * menu_init_numeric_display_frame_primitives and
 * *_gfx_init_scaled_draw_area_packets, and the draw offsets follow from the
 * SetDrawOffset(packet, position) pairs. */
typedef struct menu_status_panel_numeric_buffer {
    DR_MODE draw_modes[3];                           /* 0x000 */
    SPRT sprites[18];                                /* 0x024 */
    menu_palette_primitives_t numeric_frame;         /* 0x18c */
    menu_status_panel_draw_offset_t draw_offsets[2]; /* 0x278 */
    scaled_draw_area_pair_t portrait;                /* 0x298 */
} menu_status_panel_numeric_buffer_t;
typedef char
    menu_status_panel_numeric_buffer_size_must_be_704[(sizeof(menu_status_panel_numeric_buffer_t) == 704) ? 1 : -1];

/* Flags of a unit status panel (menu_status_panel_display_thread_t and
 * the WORLD status display's menu_status_thread_t). */
typedef enum menu_status_panel_flags {
    MENU_STATUS_PANEL_SLIDE_DOWN = 0x20, /* run the slide-down table once */
    MENU_STATUS_PANEL_SLIDE_UP = 0x40,   /* run the slide-up table once */
    MENU_STATUS_PANEL_HIDDEN = 0x80,     /* draw nothing; the only flag kept when a slide ends */
} menu_status_panel_flags_e;

/* Parameter block of the task-0x3b status panel threads (WORLD's formation
 * panels and the overlays' character status and numeric editor threads): the
 * panel origin, a redraw request and the frame style read by
 * *_panel_set_primitive_colors. The origin is read as whole 32-bit values (lw) for the
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

/* u16 view of the -1 stored in an empty menu_status_panel_slot_state_t.ability_ids row. */
enum {
    MENU_STATUS_PANEL_LABEL_NONE = 0xffff,
};

/* Provisional 0xe-byte active-unit record (BATTLE 0x8014d080, WORLD
 * 0x8013a364, DEBUGCHR 0x801c5324, REQUIRE 0x801cb364, ATTACK 0x801ca0c8), saved and restored
 * whole by menu_copy_billboard_data_out and
 * battle_menu_copy_billboard_data_in. The load-selected-unit routines fill
 * job_id..zodiac for the unit named by battle_id; the overlays' status panels
 * keep a second record for the comparison unit 0x10 later. */
typedef struct menu_active_unit {
    /* 0x00: unit name text id less TEXT_ID_UNIT_NAME_BASE (DEBUGCHR, REQUIRE);
     * in ATTACK the roster index, a row of the names it builds at text 0x5800 */
    s16 name_id;
    s16 job_id;     /* 0x02 */
    s16 brave;      /* 0x04 */
    s16 faith;      /* 0x06 */
    s16 zodiac;     /* 0x08 */
    s16 battle_id;  /* 0x0a: selected unit */
    s16 _unused_0c; /* 0x0c */
} menu_active_unit_t;
typedef char menu_active_unit_size_must_be_14[(sizeof(menu_active_unit_t) == 14) ? 1 : -1];

/* The selected panel unit can differ from the unit whose turn is active. */
extern menu_active_unit_t g_menu_selected_unit;
extern s16 g_menu_comparison_battle_id;

struct unit_stats* menu_get_selected_unit_stats(void);

/* Name, job and identity values copied into WORLD and EQUIP status panels.
 * The last displayed value is the roster slot, distinct from a battle id. */
typedef struct menu_unit_status_identity {
    s16 unit_index;  /* 0x00: name text 0x4000 + value (world_menu_unit_status_banner_thread) */
    s16 job_id;      /* 0x02: job name text 0x3000 + value */
    s16 brave;       /* 0x04 */
    s16 faith;       /* 0x06 */
    s16 zodiac;      /* 0x08 */
    s16 roster_slot; /* 0x0a: party slot (world_menu_get_party_unit_name_id); shown as value + 1 */
    u8 _unused_0c[2];
} menu_unit_status_identity_t;
typedef char menu_unit_status_identity_size_must_be_14[sizeof(menu_unit_status_identity_t) == 14 ? 1 : -1];

/* Unit stats and equipment/ability rows used by status panels and previews.
 * The arrays follow the five character slots. */
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
                                   * hiding pass; low bit of display_mode */
    s16 two_hands;                /* 0x10: two-hands flag; high bit of display_mode */
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

/* Complete status and preview record, copied whole by the billboard routines. */
typedef struct menu_status_panel_slot_storage {
    menu_status_panel_slot_state_t state;
    u8 _unused_38[8]; /* preserves the copied 0x40-byte record */
} menu_status_panel_slot_storage_t;
typedef char menu_status_panel_slot_storage_size_must_be_64[(sizeof(menu_status_panel_slot_storage_t) == 64) ? 1 : -1];

extern menu_status_panel_slot_storage_t g_menu_selected_unit_fields;
/* Target preview copies only generic_monster and the reaction ability here. */
extern menu_status_panel_slot_storage_t g_menu_comparison_unit_fields;

/* HP, MP or CT gauge, drawn 32 * value / max pixels wide. Overlay panels
 * use a four-pixel stub when max is zero; battle billboard builders instead
 * store 1 for zero HP/MP maxima. Those builders clear HP/MP bonuses but leave
 * the CT bonus field untouched. */
typedef struct menu_unit_status_gauge {
    s16 value; /* 0x00 */
    s16 bonus; /* 0x02; equipment contribution or preview delta for HP/MP */
    s16 max;   /* 0x04 */
} menu_unit_status_gauge_t;
typedef char menu_unit_status_gauge_size_must_be_6[(sizeof(menu_unit_status_gauge_t) == 6) ? 1 : -1];

/* Provisional 0x22-byte unit status record: BATTLE's AT banner at 0x8014d038
 * and status billboard at 0x8014d05c, WORLD's scene records at
 * 0x8013a31c / 0x8013a340 and formation records at 0x8018aa98 / 0x8018aabc,
 * and each overlay's pair of panel billboards (all
 * 0x24 apart). The *_copy_unit_data_to_status_billboard builders (and
 * require_panel_copy_stats_to_gauges, wldcore_unit_build_status_panel_data)
 * fill the first 0x1e bytes, *_menu_display_hovered_unit_stats and the
 * *_render_unit_status_panel_thread editors draw it, and
 * menu_copy_billboard_data_out saves the banner whole as 0x22 bytes.
 * Equipment previews use the HP and MP gauges' bonus fields. */
typedef struct menu_unit_status_record {
    s16 level;      /* 0x00; debugchr_editor_apply_values_to_battle_unit writes it back */
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
} menu_unit_status_record_t;
typedef char menu_unit_status_record_size_must_be_34[(sizeof(menu_unit_status_record_t) == 34) ? 1 : -1];

extern menu_unit_status_record_t g_menu_selected_unit_status;
extern menu_unit_status_record_t g_menu_comparison_unit_status;
extern RECT g_menu_hovered_unit_stats_rect;
extern RECT g_menu_attack_caster_stats_rect;
extern RECT g_menu_attack_target_stats_rect;
extern RECT g_menu_right_unit_stats_rect;
extern RECT g_menu_unit_summary_panel_rect;

/* The hovered-unit status panel's two draw offsets (*_menu_display_hovered_unit_stats). */
typedef struct menu_unit_status_offsets {
    menu_status_panel_draw_offset_t a; /* 0x00 */
    menu_status_panel_draw_offset_t b; /* 0x10 */
} menu_unit_status_offsets_t;

enum {
    MENU_UNIT_STATUS_GAUGE_HP = 0,
    MENU_UNIT_STATUS_GAUGE_MP = 1,
    MENU_UNIT_STATUS_GAUGE_CT = 2,
};

/* Character-editor packet block (0x30c bytes) built by the editor display
 * threads. Two copies sit back to back and `frame & 1` selects one, so the
 * copy stride proves the size. The head is the numeric display frame that
 * menu_init_numeric_display_frame_primitives builds, the SPRT/POLY
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

/* Per-frame character status buffer (0x3d8 bytes), double buffered by the
 * *_panel_run_character_status_thread routines, which copy the first buffer
 * to the second (base + 0x3d8) before the render loop starts. Evidence, from
 * the DEBUGCHR routine and its byte-identical twins:
 *  - the DR_MODE packets at +0x00/+0x0c/+0x18 are submitted individually, and
 *    the numeric threads fill the same slots with
 *    gfx_set_draw_mode_for_texture_page;
 *  - the submit loops walk the SPRTs from +0x24 with the sprite index as their
 *    counter (0..11 from +0x24, 12 at +0x114, 13 at +0x128, 14..18 from
 *    +0x13c, 19..23 from +0x1a0);
 *  - the TILE/LINE_F2 pair at +0x204/+0x224 (two translucent 16x90 backdrop
 *    tiles and eight vertical frame lines) is written, with the draw modes,
 *    by the *_gfx_init_menu_tile_and_line_primitives builders and
 *    world_gfx_init_menu_tile_and_line_primitives (0x80110260), which leave the
 *    sprites between untouched;
 *  - the two draw-offset packets follow from the SetDrawOffset(packet,
 *    position) calls at +0x2a4/+0x2b0 and +0x2b4/+0x2c0;
 *  - +0x2c4 and +0x3b0 are the arguments of
 *    menu_init_numeric_display_frame_primitives and the
 *    *_gfx_init_scaled_draw_area_packets / *_gfx_build_scaled_draw_area_packets
 *    pair. */
typedef struct menu_status_panel_buffer {
    DR_MODE draw_modes[3];                           /* 0x000 */
    SPRT sprites[24];                                /* 0x024 */
    TILE tiles[2];                                   /* 0x204 */
    LINE_F2 lines[8];                                /* 0x224 */
    menu_status_panel_draw_offset_t draw_offsets[2]; /* 0x2a4 */
    menu_palette_primitives_t numeric_frame;         /* 0x2c4 */
    scaled_draw_area_pair_t portrait;                /* 0x3b0 */
} menu_status_panel_buffer_t;
typedef char menu_status_panel_buffer_size_must_be_984[(sizeof(menu_status_panel_buffer_t) == 984) ? 1 : -1];

/* status panel rendering */
extern u16 g_menu_gfx_draw_offset_y;
extern RECT g_menu_status_panel_draw_area_rect;
extern menu_status_panel_slot_storage_t g_menu_status_panel_fields;
extern RECT g_menu_status_panel_frame_rect[];
extern menu_status_panel_buffer_t g_menu_status_panel_frames[];
extern image_load_parameters_t g_menu_status_panel_item_icon_layouts[];
extern point16_t g_menu_status_panel_item_icon_texture[];
extern image_load_parameters_t g_menu_status_panel_label_layouts_mode0[];
extern image_load_parameters_t g_menu_status_panel_label_layouts_mode1[];
extern image_load_parameters_t g_menu_status_panel_label_layouts_mode2[];
extern image_load_parameters_t g_menu_status_panel_label_layouts_mode3[];
extern point16_t g_menu_status_panel_numeric_geometry[];
extern RECT g_menu_status_panel_origin_offsets[];
extern u8 g_menu_status_panel_text_image_a[];
extern u8 g_menu_status_panel_text_image_b[];
extern RECT g_menu_status_panel_text_upload_rect_a;
extern RECT g_menu_status_panel_text_upload_rect_b;

void menu_run_status_panel_thread(void);
void menu_run_equipment_ability_panel_thread(void);
void menu_run_numeric_editor_thread(void);
void menu_run_numeric_editor_frame_thread(void);
void menu_load_selected_unit(void);
void menu_refresh_hovered_unit_stats_display(void);
void menu_refresh_right_unit_stats_display(void);
void menu_run_unit_summary_panel_thread(void);
void menu_preview_attack_caster_stats(void);
void menu_preview_attack_target_stats(void);

void menu_copy_billboard_data_out(menu_unit_status_record_t* banner, menu_active_unit_t* unit,
    menu_status_panel_slot_storage_t* billboard, map_selected_tile_t* cursor_tile);
void menu_copy_billboard_data_in(const menu_unit_status_record_t* banner, const menu_active_unit_t* unit,
    const menu_status_panel_slot_storage_t* billboard);
/* BATTLE and DEBUGCHR share one body; WORLD links a shorter one of its own. */
void menu_copy_unit_data_to_status_billboard(struct unit_stats* unit, menu_unit_status_record_t* output);
void menu_clear_stat_preview(menu_unit_status_record_t* summary, menu_status_panel_slot_storage_t* detail);
void menu_calculate_swap_stat_delta(menu_status_panel_slot_storage_t* output, menu_unit_status_record_t* delta,
    s16 item_before, s16 item_after, s32 slot);
void menu_init_status_panel_primitives(menu_status_panel_buffer_t* menu);
void menu_subtract_scaled_stat_preview(menu_status_panel_slot_storage_t* out,
    const menu_status_panel_slot_storage_t* scaled, const menu_status_panel_slot_storage_t* base, s32 scale);

void menu_select_status_primitive_color_palette(
    menu_palette_primitives_t* primitives, const menu_status_thread_t* thread);

void menu_set_status_panel_primitive_colors(menu_status_panel_buffer_t* primitives, const menu_status_thread_t* state);

/* thread status indicators */
extern s32 g_menu_indicator_brightness[2];
extern s32 g_menu_indicator_fade_work[2];
extern s32 g_menu_indicator_packet_index[2];
extern s32 g_menu_indicator_state[2];
extern menu_gradient_panel_prims_t g_menu_thread_indicator_packets[2][2];

void menu_draw_thread_status_indicators(void);
s32 menu_thread_is_running(s32 thread_id);

/* panel drawing */
extern const u16 g_menu_draw_area_scale_percent[12];
extern RECT g_menu_draw_area_template;
extern u8 g_menu_gfx_scaled_sprite_count;
extern point16_t g_menu_gfx_scaled_sprite_scale[];
extern point16_t g_menu_gfx_scaled_sprite_texture_page;
extern point16_t g_menu_texture_origin; /* VRAM origin for menu image placement */
extern sprite_sheet_frame_t g_menu_gfx_scaled_sprite_uv_rects[];
extern u8 g_menu_panel_status_animation[];
extern u8 g_menu_panel_status_group_bounds[];
extern u8 g_menu_panel_status_group_count;
extern u8 g_menu_panel_status_group_ids[];
extern sprite_draw_offset_t g_menu_panel_status_offsets[];
extern point16_t g_menu_panel_status_scales[];
extern const point16_t g_menu_panel_status_texture;
extern sprite_sheet_frame_t g_menu_panel_status_uv_rects[];
extern const u16 g_menu_panel_status_y_offsets[];
void menu_build_scaled_draw_area_packets(scaled_draw_area_pair_t* packet, const RECT* source, s32 scale_index,
    s32 lower_half, const menu_status_thread_t* thread);
void menu_build_status_group_primitives(POLY_FT4* poly);
void menu_build_zodiac_primitives(zodiac_draw_context_t* context, POLY_FT4* poly);
void menu_init_scaled_draw_area_packets(scaled_draw_area_pair_t* data);

/* menu commands */
/* Per-skillset menu behavior stored at 0x80065cb4. Values select specialized
 * inventory, targeting, and ability-loading paths; they are not menu command
 * opcodes. */
typedef enum action_menu_type {
    ACTION_MENU_TYPE_DEFAULT = 0,
    ACTION_MENU_TYPE_ITEM_INVENTORY = 1,
    ACTION_MENU_TYPE_WEAPON_INVENTORY = 2,
    ACTION_MENU_TYPE_ARITHMETICKS = 3,
    ACTION_MENU_TYPE_ELEMENTS = 4,
    ACTION_MENU_TYPE_BLANK_05 = 5,
    ACTION_MENU_TYPE_MONSTER = 6,
    ACTION_MENU_TYPE_KATANA_INVENTORY = 7,
    ACTION_MENU_TYPE_ATTACK = 8,
    ACTION_MENU_TYPE_JUMP = 9,
    ACTION_MENU_TYPE_CHARGE = 10,
    ACTION_MENU_TYPE_DEFEND = 11,
    ACTION_MENU_TYPE_CHANGE_EQUIPMENT = 12,
    ACTION_MENU_TYPE_UNKNOWN_0D = 13,
    ACTION_MENU_TYPE_BLANK_0E = 14,
    ACTION_MENU_TYPE_UNKNOWN_0F = 15,
    ACTION_MENU_TYPE_COUNT = 16,
} action_menu_type_e;

/* Contextual skillset IDs synthesized by the action menu. The value 0xbc is
 * also an ordinary monster skillset when it comes from a monster unit. */
typedef enum action_menu_pseudo_skillset {
    ACTION_MENU_PSEUDO_SKILLSET_ANYTHING = 188,
} action_menu_pseudo_skillset_e;

/* The controller or tutorial event that drives the scene menus. */
typedef enum menu_input_mode {
    MENU_INPUT_PLAYER = 0,   /* the controller */
    MENU_INPUT_BLOCKED = 1,  /* the event frame returns early and no idle action menu is built;
                                no code stores this value */
    MENU_INPUT_TUTORIAL = 2, /* a tutorial event supplies the input; set by *_menu_start_entry_thread */
} menu_input_mode_e;

extern u16 g_menu_input_mode; /* menu_input_mode_e */

/* Modal/message ownership suppresses menu input and dims palettes. */
extern s32 g_menu_input_lock_mode;

/* The index argument of *_menu_draw_pressable_button: an on-screen button,
 * named after the pad button its callers test. */
typedef enum menu_pressable_button {
    MENU_PRESSABLE_BUTTON_L1 = 0,
    MENU_PRESSABLE_BUTTON_R1 = 1,
    MENU_PRESSABLE_BUTTON_ITEM_LIST_TRIANGLE = 2, /* formation item list */
    MENU_PRESSABLE_BUTTON_SHOP_LIST_TRIANGLE = 3, /* shop buy lists */
    MENU_PRESSABLE_BUTTON_L2 = 4,
    MENU_PRESSABLE_BUTTON_R2 = 5,
} menu_pressable_button_e;

/* State of the item list screen (g_world_item_menu_mode in the formation
 * screen, g_equip_menu_substate in EQUIP). Each state's handler returns the
 * next state. */
typedef enum menu_item_list_state {
    MENU_ITEM_LIST_STATE_CLOSE = -1,
    MENU_ITEM_LIST_STATE_BROWSE = 0,
    MENU_ITEM_LIST_STATE_REARRANGE = 1,
    MENU_ITEM_LIST_STATE_ACTION = 2, /* action strip for the selected item */
    MENU_ITEM_LIST_STATE_SORT_ORDER = 5,
    MENU_ITEM_LIST_STATE_EQUIPPED_BY = 6, /* WORLD only: the units equipping the selected item */
} menu_item_list_state_e;

/* Ability list tab of the WORLD, BUNIT and JOBSTTS ability menus
 * (g_world_ability_category and g_menu_ability_category) and the kind
 * argument of their list builders. */
typedef enum menu_ability_category {
    MENU_ABILITY_CATEGORY_ACTION = 0,
    MENU_ABILITY_CATEGORY_REACTION = 1,
    MENU_ABILITY_CATEGORY_SUPPORT = 2,
    MENU_ABILITY_CATEGORY_MOVEMENT = 3,
    MENU_ABILITY_CATEGORY_ALL = 15, /* the list builders take any other value as all */
} menu_ability_category_e;

/* The argument of option_entrypoint and world_menu_run_main_mode: which of
 * the OPTION overlay's screens to run. */
typedef enum option_menu_type {
    OPTION_MENU_GAME_OPTIONS = 0,
    OPTION_MENU_AT_LIST = 1,
    OPTION_MENU_DEAD_UNIT = 2, /* crystal and treasure panel */
} option_menu_type_e;

/* A key in the list *_item_sort_id_list / equip_item_sort_list_by_criteria
 * sorts by (g_world_sort_key_lists, g_equip_item_type_order_lists); -1 ends
 * the list. TYPE sorts ascending, the others descending. */
typedef enum item_sort_key {
    ITEM_SORT_KEY_TYPE = 0,
    ITEM_SORT_KEY_PRICE = 1,
    ITEM_SORT_KEY_RANKING = 2, /* *_item_get_ranking_value */
    ITEM_SORT_KEY_PHYSICAL_EVADE = 3,
    ITEM_SORT_KEY_MAGIC_EVADE = 4,
    ITEM_SORT_KEY_HELD_COUNT = 5, /* inventory plus equipped copies */
} item_sort_key_e;

/* The category argument of world_item_build_category_list,
 * world_shop_build_item_list and equip_item_build_filtered_list. 0 to 4
 * select what fits that equipment slot. The item menus pass their tab + 1 for
 * the first four tabs and OTHER for the fifth; the best-fit builders pass
 * WEAPON or SHIELD for the left hand. */
typedef enum item_list_filter {
    ITEM_LIST_FILTER_HAND = 0, /* weapons and shields; 1, the left hand, selects the same */
    ITEM_LIST_FILTER_HEADGEAR = 2,
    ITEM_LIST_FILTER_BODY_ARMOR = 3,
    ITEM_LIST_FILTER_ACCESSORY = 4,
    ITEM_LIST_FILTER_WEAPON = 5,
    ITEM_LIST_FILTER_SHIELD = 6,
    ITEM_LIST_FILTER_OTHER = 7,
} item_list_filter_e;

/* The last argument of the same functions: the equip check against the unit. */
typedef enum item_list_mode {
    ITEM_LIST_MODE_ALL = 0,                     /* no check */
    ITEM_LIST_MODE_MARK_UNEQUIPPABLE = 1,       /* 0x4000 on entries the unit cannot equip; taken as
                                                   EQUIPPABLE unless Show Unequippable Items is on */
    ITEM_LIST_MODE_EQUIPPABLE = 2,              /* entries the unit cannot equip are left out */
    ITEM_LIST_MODE_EQUIPPABLE_IN_INVENTORY = 3, /* world_item_build_category_list only: as
                                                   EQUIPPABLE, counting no equipped copies */
} item_list_mode_e;

/* The grid help cursor over a unit's status display. A value above the text
 * ids in the pending description id (g_world_menu_description_text_id,
 * g_bunit_text_selection_id) selects one; it is latched in
 * g_world_grid_menu_id / g_bunit_help_screen_id instead of showing a
 * description. */
typedef enum menu_grid_help_id {
    MENU_GRID_HELP_NONE = 0,
    MENU_GRID_HELP_BANNER_TOP = 0x20000, /* the status banner is at the top of the screen */
    MENU_GRID_HELP_BANNER_BOTTOM = 0x20001,
    MENU_GRID_HELP_UNIT_BROWSE = 0x20002,
    MENU_GRID_HELP_RECRUIT_PREVIEW = 0x20003, /* WORLD: the cursor skips cell 7 */
} menu_grid_help_id_e;

/* What the unit grids draw below a unit (world_formation_draw_graphics_below_sprite,
 * bunit_menu_update_unit_grid). L2/R2 step g_world_formation_stat_display_mode and
 * g_bunit_unit_stat_mode through the first five, wrapping between HP and
 * BRAVE_FAITH; WORLD picks the others per unit. */
typedef enum menu_unit_readout {
    MENU_UNIT_READOUT_HP = 0,
    MENU_UNIT_READOUT_MP = 1,
    MENU_UNIT_READOUT_CT = 2,
    MENU_UNIT_READOUT_LEVEL_EXP = 3,
    MENU_UNIT_READOUT_BRAVE_FAITH = 4,
    MENU_UNIT_READOUT_EQUIP_LABEL = 8, /* "equipping", or "can't equip" below a value of 100 */
    MENU_UNIT_READOUT_LEAVING = 9,     /* away on a proposition */
    MENU_UNIT_READOUT_EGG = 10,        /* nothing is drawn */
    MENU_UNIT_READOUT_MISSING = 11,
} menu_unit_readout_e;

/* Step of the job list menus (WORLD learn and view-abilities menus, BUNIT job
 * menu, the JOBSTTS page); each list's handler returns the next step. In
 * JOBSTTS, JOBS is jobstts_menu_update_job_list, ABILITIES is
 * jobstts_ability_update_list_menu and CLOSE makes jobstts_entrypoint close
 * the screen. */
typedef enum menu_job_list_step {
    MENU_JOB_LIST_STEP_CLOSE = -1,
    MENU_JOB_LIST_STEP_JOBS = 0,
    MENU_JOB_LIST_STEP_ABILITIES = 1,
} menu_job_list_step_e;

/* The mode argument of *_menu_build_ability_list: which list an Arithmeticks
 * (Math Skill) menu shows. Any other menu type builds its ability list. */
typedef enum menu_arithmeticks_list {
    MENU_ARITHMETICKS_LIST_ABILITIES = 0,  /* *_menu_collect_calculator_abilities */
    MENU_ARITHMETICKS_LIST_MULTIPLES = 1,  /* *_menu_load_math_skill_multiples */
    MENU_ARITHMETICKS_LIST_ATTRIBUTES = 2, /* *_menu_load_math_skill_attributes */
} menu_arithmeticks_list_e;

/* One cursor animation threshold and its signed screen offset. */
typedef struct menu_cursor_bob_entry {
    s8 threshold;
    s8 offset;
} menu_cursor_bob_entry_t;
typedef char menu_cursor_bob_entry_size_must_be_2[sizeof(menu_cursor_bob_entry_t) == 2 ? 1 : -1];

/* (threshold, offset) pairs ending at threshold 0; entry 0 holds the modulus. */
extern menu_cursor_bob_entry_t g_menu_cursor_bob_thresholds[];
extern menu_cursor_bob_entry_t g_menu_cursor_bob_active_thresholds[];

s32 menu_get_cursor_bob_offset(s32 mode);

/* menu drawing */
/* Six-byte menu sprite cell: screen rectangle and texture origin. */
typedef struct menu_sprite_cell {
    u8 x;
    u8 y;
    u8 w;
    u8 h;
    u8 u;
    u8 v;
} menu_sprite_cell_t;
typedef char menu_sprite_cell_size_must_be_6[(sizeof(menu_sprite_cell_t) == 6) ? 1 : -1];

/* One 0x100-byte slot holding a panel's palette primitive set. */
typedef struct menu_palette_slot {
    menu_palette_primitives_t primitives; /* 0x00 */
    u8 _padding_ec[0x14];                 /* preserves the 0x100-byte allocation stride */
} menu_palette_slot_t;
typedef char menu_palette_slot_size_must_be_256[(sizeof(menu_palette_slot_t) == 256) ? 1 : -1];

/* menu entries */
/* Provisional: record reached through menu_entry_t::context.
 * world_menu_text_entry_thread, world_menu_text_entry_wait_thread and the
 * overlays' *_menu_open_submenu_thread call the hook at +0 and start the text
 * thread whose id is at +6 with speaker_id as its third and fourth parameters. */
typedef struct menu_text_binding {
    void (*prepare)(void); /* 0x00 */
    u16 speaker_id;        /* 0x04: unit whose portrait speaks (roster slot) */
    u16 text_thread_id;    /* 0x06 */
    u8 _unused_08[8];
    s16* row_text_ids; /* 0x10: text ids indexed by entry selected_index (world_menu_announce_entry_value_thread) */
} menu_text_binding_t;

/* Row actions 0..0xff open a menu entry; this flag suppresses the confirm sound. */
enum { MENU_ROW_ACTION_SILENT = 0x1000 };

/* Special row action: records no selection without opening another entry. */
enum { MENU_ROW_ACTION_CLEAR_SELECTION = 0x4d2 };

/* Provisional: 60-byte menu entry used by BATTLE, WORLD and OPTION/EQUIP.
 * g_menu_thread_menu_data points to the scene's current table. 0x0c/0x0e are the
 * window size (frame rect 0x0c - 0x12 by 0x0e - 0x1a in WORLD 0x8010088c and
 * option_menu_run_at_list). The parent table at 0x24 (indexed by selected_index)
 * and the thread entry at 0x28 are known from
 * world_menu_start_entry_chain_threads. The record is also the first parameter
 * of the menu threads it starts: the icon threads read 0x00..0x07 as the text
 * image's upload rect and 0x08/0x0a as the icon origin (the scrolling icon list
 * thread clears bit 0 of 0x0a), and menu_step_wrapping_cursor_on_scroll_buttons
 * wraps the cursor at max_row_index. */
typedef struct menu_entry {
    point16_t vram_origin; /* 0x00: text-image VRAM origin passed to the draw-mode helpers */
    s16 inner_width;       /* 0x04: text-image width; RECT.w = inner_width >> 2 (4bpp) */
    s16 inner_height;      /* 0x06: text-image height (menu_size_entry_to_text) */
    RECT window;           /* 0x08: screen position and extent passed to image and draw-area helpers */
    /* 0x10: text sprite placement (0x800ec108). width/height are set with the
     * inner and window sizes; the load origin and screen offsets are cleared
     * with them. */
    image_load_parameters_t text_image;
    s16 text_id;                 /* 0x1c: passed to text_find_entry */
    s16 max_row_index;           /* 0x1e: row count - 1 (menu_build_skillset_entries) */
    s16 thread_count;            /* 0x20: threads menu_handle_cancel_input stops, -1 when there is no thread
                                    group; set from the system-function table by menu_run_system_function_thread */
    u8 _padding_22[0x24 - 0x22]; /* aligns row_actions */
    s16* row_actions;            /* 0x24 */
    void (*thread_entry)(void);  /* 0x28 */
    s16 header_id;               /* 0x2c: menu header; text-thread parameter (world_menu_text_entry_thread); the
                                    icon threads read it signed as the icon kind: 0x10 arrow, 0..4 single
                                    icon, 5..15 four-piece icon (world_menu_build_icon_record) */
    u8 _padding_2e[2];           /* aligns context */
    /* 0x30: context interpreted by the menu thread. Text threads use menu_text_binding_t
     * here, the list threads a menu_text_layout_t, the number threads a
     * world_menu_number_range_t and the icon threads the s16 redraw flag they
     * poll (set to 1 to rebuild the text image). */
    void* context;
    void* value;           /* 0x34: thread-specific value data: s16 result cell or main-window s32 substitution pair */
    s16 selected_index;    /* 0x38: selected row; indexes the binding's 16-bit table
                              (world_menu_announce_entry_value_thread) */
    s16 select_text_table; /* 0x3a: "Index for select text": row of g_world_menu_entry_text_id_tables
                              (0x11 = the binding's row_text_ids); 2 / -1 defer to the next running
                              thread's entry. The AT-list and option-list builders park their
                              scroll row offset here instead. */
} menu_entry_t;
typedef char menu_entry_size_must_be_60[(sizeof(menu_entry_t) == 60) ? 1 : -1];

/* Each scene keeps its own current table; event screens save and restore it. */
extern menu_entry_t* g_menu_thread_menu_data;
extern s32 g_menu_managed_thread_state;

s32 menu_start_managed_thread(s32 thread_id, menu_entry_t* descriptor);
void menu_request_thread_stop(s32 thread_id);
void menu_stop_managed_thread(s32 thread_id);
extern s16 g_menu_wait_row_actions[4];
extern u8 g_menu_cleared_entry_indices[11];
extern u16 g_menu_at_list_selection_finished;
void menu_init_stub(void);
void menu_confirm_action_silently(menu_entry_t* entry);
void menu_wait_thread(void);
void menu_input_thread(void);
void menu_build_main(void);
extern s32 g_menu_help_open;
extern s32 g_menu_building_thread_started;
void menu_start_building_thread(void);
/* BATTLE and WORLD each link their own bodies of the next four. */
s32 menu_can_accept_input(void);
void menu_handle_action(menu_entry_t* entry, s32 row_index);
void menu_handle_cancel_input(menu_entry_t* context);
s32 menu_should_close_thread(u32* input);
void menu_step_wrapping_cursor_on_scroll_buttons(menu_entry_t* bounds, s32* cursor);
void menu_record_selection_for_selected_unit(void);
void menu_clear_entry_selections(void);
void menu_finish_at_list_selection(void);

/* Numeric-entry and action-selection threads use value as a 16-bit result cell. */
static inline s16* menu_entry_result(menu_entry_t* entry) {
    return entry->value;
}

void menu_copy_sprite_position_uv_and_size(const menu_entry_t* entry, SPRT* sprite, s32 unused_frame);
void menu_size_entry_to_text(menu_entry_t* entry, s16* width, s16* height, s32* out_pad, s32 pad_short);

/* menu text */
/* menu_text_layout_t.mode[]: what a column of the layout draws. */
typedef enum menu_text_column_mode {
    MENU_TEXT_COLUMN_TEXT = 0,          /* text ids, drawn by *_menu_display_text_entry */
    MENU_TEXT_COLUMN_NUMBER = 1,        /* two-digit numbers, drawn by *_display_menu_number_entry */
    MENU_TEXT_COLUMN_HIDDEN = 2,        /* the column is skipped */
    MENU_TEXT_COLUMN_NUMBER_GLYPHS = 3, /* two-digit numbers, drawn by *_draw_menu_number_glyphs */
} menu_text_column_mode_e;

/* Provisional: three-column menu text layout walked by menu_draw_text_columns and its WORLD
 * screen-menu copy world_screen_menu_draw_text_columns (reached through menu_entry_t::context when
 * that record is a layout rather than a binding). Column i uses x[i],
 * mode[i] (menu_text_column_mode_e), text_ids[i] and
 * text_colors[i], each indexed by row + row offset. */
typedef struct menu_text_layout {
    s16 row_count;       /* 0x00 */
    s16 hidden_rows;     /* 0x02: rows beyond the visible page (count - visible, 0 when
                            all fit); row_offset is clamped to it */
    s16 x[3];            /* 0x04 */
    s16 mode[3];         /* 0x0a: menu_text_column_mode_e */
    s16* text_ids[3];    /* 0x10 */
    s16* text_colors[3]; /* 0x1c */
} menu_text_layout_t;
typedef char menu_text_layout_size_must_be_40[sizeof(menu_text_layout_t) == 40 ? 1 : -1];

/* menu drawing */
/* Provisional: one double-buffered menu sprite page (0xa0 bytes) built by
 * menu_build_sprite_page and its BATTLE twin battle_menu_build_sprite_page:
 * five DR_MODE texture-window packets followed by the five SPRTs they apply
 * to. Both modules keep two pages at 0x801cc074. */
typedef struct menu_sprite_page {
    DR_MODE modes[5]; /* 0x00 */
    SPRT sprites[5];  /* 0x3c */
} menu_sprite_page_t;
typedef char menu_sprite_page_size_must_be_160[(sizeof(menu_sprite_page_t) == 160) ? 1 : -1];
extern RECT g_menu_sprite_page_texture_windows[5];
extern image_load_parameters_t g_menu_sprite_page_image_params[30];
void menu_build_sprite_page(s32 page_index, RECT* base_screen);

/* Provisional: per-line placement for the twelve menu box lines
 * (table at 0x80154cd0, indexed like menu_palette_primitives_t.lines):
 * signed endpoint offsets from the box origin and, per coordinate, a flag
 * selecting whether the box width/height is added. */
typedef struct world_menu_line_layout {
    s8 x0, y0, x1, y1;                 /* 0x00 */
    s8 add_w0, add_h0, add_w1, add_h1; /* 0x04 */
} world_menu_line_layout_t;

/* Icon packet head: two DR_MODE packets at +0x00/+0x0c and four SPRT packets
 * at +0x18 (CLUT ids at +0x26/+0x3a/+0x4e/+0x62). The submitter draws all
 * six packets. */
typedef struct menu_icon_sprites {
    DR_MODE first_draw_mode;
    DR_MODE draw_mode;
    SPRT sprites[4];
} menu_icon_sprites_t;
typedef char menu_icon_sprites_size_must_be_104[(sizeof(menu_icon_sprites_t) == 104) ? 1 : -1];
void menu_select_icon_cluts(menu_icon_sprites_t* frame);

void menu_submit_icon_primitives(menu_icon_sprites_t* frame);
void menu_update_icon_cursor_sprites(menu_entry_t* param, menu_icon_sprites_t* record, s32 frame, s32 cursor);
void menu_run_icon_selection_loop(void);
void menu_auto_battle_setting_thread(void);
extern s32 g_menu_resolved_command;
void menu_store_auto_battle_main_target_id(s32 target_id, s32 battle_id);

/* Six-byte option row selected by customized-options bits 9-11 in
 * 0x80106660. The first three bytes set the shared initial, primary and
 * secondary input-repeat timings. */
typedef struct menu_input_timing_profile {
    u8 initial_delay;
    u8 repeat_period;
    u8 secondary_repeat_period;
    u8 scroll_accel_delay; /* copied to g_main_menu_scroll_accel_delay */
    u8 scroll_slow_step;   /* copied to g_main_menu_scroll_slow_step */
    u8 scroll_fast_step;   /* copied to g_main_menu_scroll_fast_step */
} menu_input_timing_profile_t;

/* A text layout followed by the first visible row, as bound to a scrolling
 * list's menu_entry_t::context (menu_run_scrolling_ability_list_thread and the AT list
 * built by menu_build_ability_preview_at_list). */
typedef struct menu_scroll_text_layout {
    menu_text_layout_t columns; /* 0x00 */
    s16 row_offset;             /* 0x28 */
} menu_scroll_text_layout_t;

/* The ability list's and the unit status list's layouts; menu entries pass
 * them as their context. */
extern menu_scroll_text_layout_t g_menu_ability_layout;
extern menu_scroll_text_layout_t g_menu_status_list_layout;
extern u16 g_menu_status_list_row_actions[8];
extern s16 g_menu_status_left_text_ids[20];
extern s16 g_menu_status_right_text_ids[20];
extern u16 g_menu_ability_display_flags[80];
extern s16 g_menu_action_type_modes[][2];
void menu_build_ability_list(s32 mode);
void menu_build_unit_status_list(void);
void menu_validate_skill_selection_thread(void);

/* Provisional: 12-byte textured-quad piece record (table at 0x80154e20)
 * consumed by the POLY_GT4 builder at 0x800e8d6c: texture origin, size and
 * screen position relative to g_menu_quad_origin. */
typedef struct menu_quad_piece {
    u8 u; /* 0x00 */
    u8 _unused_01;
    s8 v;           /* 0x02: texture v + 0x78 */
    u8 _padding_03; /* aligns w */
    s16 w;          /* 0x04 */
    s16 h;          /* 0x06 */
    s16 x;          /* 0x08 */
    s16 y;          /* 0x0a */
} menu_quad_piece_t;

/* Provisional: the four vertex colors applied by the POLY_GT4 builder at
 * 0x800e8d6c (record at 0x801557f4; the leading 32-bit value is untyped). */
typedef struct menu_quad_colors {
    u32 _unused_00;
    CVECTOR vertex[4]; /* 0x04; one color per POLY_GT4 corner */
} menu_quad_colors_t;

extern menu_quad_colors_t g_menu_quad_colors;
extern u8 g_menu_quad_group_starts[]; /* [group + 1] ends each group */
extern point16_t g_menu_quad_origin;
extern menu_quad_piece_t g_menu_quad_pieces[];

s32 menu_build_quad_pieces(s32 group, POLY_GT4* poly);

/* Provisional: one 5-byte row of a WORLD menu text entry as rewritten by
 * menu_build_skillset_entries (0x800f3d44): the style byte at +1 is
 * 4 for a grayed row, the byte at +3 is the row separator (0xfa) or the
 * list terminator (0xff). */
typedef struct menu_text_row {
    u8 _unused_00;
    u8 style; /* 0x01 */
    u8 _unused_02;
    u8 terminator; /* 0x03: 0xfa continues, 0xff ends */
    u8 _unused_04;
} menu_text_row_t;

/* Provisional: the 0x7c-byte icon record world_menu_build_icon_record fills.
 * Its head is exactly menu_icon_sprites_t; the three borrowed icon
 * sprites at +0x68 and the two state words at +0x74/+0x78 are what that builder
 * adds. */
typedef struct menu_icon_record {
    menu_icon_sprites_t base; /* 0x00 */
    SPRT* icons[3];           /* 0x68 */
    s32 _unknown_74;          /* 0x74 */
    s32 _unknown_78;          /* 0x78 */
} menu_icon_record_t;

/* Provisional: one of the two 0x134-byte packet pages a scrolling text list
 * alternates between frames (menu_run_scrolling_ability_list_thread at 0x801a3d58,
 * world_build_at_list at 0x801ac6b4, world_screen_build_at_list at 0x801bfda4). */
typedef struct menu_list_page {
    SPRT frame;                         /* 0x00: text image / cursor frame */
    SPRT arrows[2];                     /* 0x14: scroll arrows */
    SPRT arrow_marks[2];                /* 0x3c */
    SPRT thumb;                         /* 0x64: scroll-bar thumb */
    DR_MODE text_mode;                  /* 0x78 */
    DR_MODE icon_mode;                  /* 0x84 */
    menu_icon_record_t icons;           /* 0x90 */
    scaled_draw_area_pair_t draw_areas; /* 0x10c */
} menu_list_page_t;
typedef char menu_list_page_size_must_be_308[sizeof(menu_list_page_t) == 308 ? 1 : -1];

/* Text image placement: the upload rectangle plus the screen offsets
 * gfx_init_image_loading reads. */
typedef struct menu_text_window {
    RECT rect;
    s16 x_screen_offset;
    s16 y_screen_offset;
} menu_text_window_t;

/* Provisional: the double-buffered quad/sprite page at 0x80173cbc (0x144
 * bytes each): five Gouraud textured quads, two sprites and the two draw-mode
 * packets that select their texture windows. */
typedef struct menu_quad_page {
    POLY_GT4 quads[5]; /* 0x000 */
    SPRT sprites[2];   /* 0x104 */
    DR_MODE modes[2];  /* 0x12c */
} menu_quad_page_t;
typedef char menu_quad_page_size_must_be_324[sizeof(menu_quad_page_t) == 324 ? 1 : -1];

/* Nonzero while effects hide the tile height in the selected-tile display. */
extern u16 g_menu_height_display_suppressed;

void menu_enable_height_display(void);
void menu_suppress_height_display(void);
void menu_run_selected_tile_info_display_thread(void);

/* primitive queues */
extern rgb24_t g_menu_gfx_sprite_color;
extern menu_gfx_context_t* g_menu_gfx_context;
extern s16 g_menu_gfx_otag_index;
extern s16 g_menu_gfx_semitransparency;
extern u16 g_menu_atlas_texture_page;
extern u16 g_menu_gfx_poly_ft4_count;
extern u16 g_menu_gfx_poly_gt4_count;
extern u16 g_menu_gfx_draw_mode_count;
extern u16 g_menu_gfx_draw_area_count;
extern u16 g_menu_gfx_draw_move_count;
extern u16 g_menu_gfx_drawenv_y;

rgb24_t* menu_gfx_get_draw_color(void);
void menu_gfx_enqueue_draw_area(RECT* rect, s32 idx);

void menu_gfx_enqueue_draw_mode(s32 dfe, s32 dtd, s32 tpage, RECT* tw, s32 idx);

void menu_gfx_enqueue_draw_move(const RECT* rect, s32 u, s32 v, s32 otag_index);

void menu_gfx_enqueue_oriented_textured_quad(
    const oriented_quad_t* descriptor, const rgb24_t* color, s32 orientation, s32 semitrans, s32 otag_index);

void menu_gfx_enqueue_textured_quad(
    const RECT* rect, s32 u, s32 v, const rgb24_t* color, s32 semitrans, u16 texture_page, u16 clut, s32 otag_index);

void menu_gfx_enqueue_textured_quad_list(const menu_sprite_cell_t* quads, const rgb24_t* color, s32 semitrans,
    u16 texture_page, u16 clut, s32 otag_index, s32 count);

/* turn order */
enum {
    BATTLE_AT_LIST_CAPACITY = 40,
    BATTLE_AT_END_OF_LIST = 31, /* unit index of the entry that ends the list */
    BATTLE_AT_UNIT_ID_MASK = 0x1f,
};

/* One 4-byte entry of the 40-entry AT list built by action_calculate_at_list and
 * kept in key order by action_sort_at_list. Unit codes combine the unit index
 * and action flags into one byte; insertion copies complete entries. */
typedef struct battle_at_entry {
    u8 unit_code;     /* 0x00; unit index and battle_at_entry_flags_e */
    u8 action_id_low; /* 0x01; low byte of the ability id, or a staged item id */
    u16 sort_key;     /* 0x02; insertion key for action_sort_at_list */
} battle_at_entry_t;
typedef char battle_at_entry_size_must_be_4[(sizeof(battle_at_entry_t) == 4) ? 1 : -1];

/* battle_at_entry_t.unit_code */
typedef enum battle_at_entry_flags {
    BATTLE_AT_JUMP = 0x20,
    BATTLE_AT_ACTION_ENTRY = 0x40, /* charged action or staged item preview */
    BATTLE_AT_ABILITY_ID_HIGH_BIT = 0x80,
} battle_at_entry_flags_e;

/* Provisional: the AT-list work area the *_menu_build_ability_preview_at_list
 * builders fill, reached through g_main_heap_companion_overlay_load_address. The five
 * parallel 16-bit columns become the layout's text_ids/text_colors tables,
 * `text` is published through g_text_section_pointers[TEXT_SECTION_AT_LIST].
 * The main-stack preview fills all 40 AT entries; the menu displays entries
 * 1 through 39. */
typedef struct menu_at_list_work {
    s16 primary_values[BATTLE_AT_LIST_CAPACITY];        /* 0x000: turn-value column */
    s16 secondary_values[BATTLE_AT_LIST_CAPACITY];      /* 0x050: unit-name column */
    s16 entry_numbers[BATTLE_AT_LIST_CAPACITY];         /* 0x0a0: ordinal column */
    s16 primary_colors[BATTLE_AT_LIST_CAPACITY];        /* 0x0f0 */
    s16 secondary_colors[BATTLE_AT_LIST_CAPACITY];      /* 0x140 */
    u8 text[0x4f0];                                     /* 0x190: packed 0xfe-terminated names */
    battle_at_entry_t entries[BATTLE_AT_LIST_CAPACITY]; /* 0x680; entry 0 is omitted from the menu */
} menu_at_list_work_t;
typedef char menu_at_list_work_size_must_be_1824[(sizeof(menu_at_list_work_t) == 1824) ? 1 : -1];

void menu_init_at_list(menu_entry_t* entry);
void menu_build_ability_preview_at_list(void);
void menu_preview_ability_turn_at_list(void);
/* BATTLE and WORLD each link their own body. */
void menu_run_scrolling_ability_list_thread(void);

/* skillset selection */
extern s32 g_menu_selected_skill_ct;
/* -1: missing unit, 1: monster skillset, otherwise 0. */
extern s32 g_menu_skillset_build_result;
extern s16 g_menu_skillset_row_actions[];
extern u8 g_menu_skillsets[]; /* SKILLSET_ID_NONE terminates the rows. */

void menu_build_skillset_entries(void);

/* Packed 4-bit images for the status panel's three numeric groups. */
typedef struct menu_unit_stat_number_images {
    u8 mobility[20 * 36 / 2]; /* Move, Jump and Speed. */
    u8 weapons[64 * 24 / 2];  /* Weapon power and evade. */
    u8 combat[100 * 24 / 2];  /* Physical/magical attack and class/shield/accessory evade. */
} menu_unit_stat_number_images_t;
typedef char menu_unit_stat_number_images_size_must_be_2328[sizeof(menu_unit_stat_number_images_t) == 2328 ? 1 : -1];

/* Provisional 0x1ac-byte double-buffered panel record at WORLD 0x801a2bd0 and
 * BATTLE 0x80172834. */
typedef struct menu_panel_record {
    DR_MODE mode0; /* 0x000 */
    DR_MODE mode1; /* 0x00c */
    /* 0x018; menu_init_scaled_draw_area_packets */
    scaled_draw_area_pair_t draw_areas;
    SPRT sprites[18];                   /* 0x040 */
    menu_palette_primitives_t* palette; /* 0x1a8 */
} menu_panel_record_t;

void menu_run_numeric_display_panel_thread(void);

/* Provisional layout of the window frame primitive block at 0x801cc074
 * (WORLD) and OPTION's copy, which hold two identical frame records. The 0x158
 * record size is fixed by the second frame starting there and by the byte copy
 * between them. Members come from the panels' own accesses plus two helpers:
 * *_menu_init_sprite_array (0x8012e2b8) is handed the nine SPRT packets, and
 * menu_init_scaled_draw_area_packets clears rects[0].w and installs rects[1] of the draw-area pair.
 * The block is zero-filled in the overlay image, so none of it is initialized
 * data. */
typedef struct menu_panel_frame {
    DR_MODE modes[7];                 /* 0x000: one per texture window */
    SPRT sprites[9];                  /* 0x054: 5 border spans then 4 corners */
    texture_primitive_t texture_prim; /* 0x108: tpage-7 VRAM allocation record */
    /* 0x130; menu_init_scaled_draw_area_packets */
    scaled_draw_area_pair_t draw_areas;
} menu_panel_frame_t;
typedef char menu_panel_frame_size_must_be_344[sizeof(menu_panel_frame_t) == 344 ? 1 : -1];

/* Scratch in the event-overlay area at 0x801cc074, past the scene program:
 * the sprite page builder lays it out as two sprite pages, WORLD's dead-unit
 * and window-frame panels as two panel frames, and BATTLE's scrolling ability
 * list saves its five 16-color CLUTs there. */
typedef union menu_scratch_buffer {
    menu_sprite_page_t sprite_pages[2];
    menu_panel_frame_t panel_frames[2];
    u16 clut_save[5 * 16];
} menu_scratch_buffer_t;

extern menu_scratch_buffer_t g_menu_scratch_buffer;

/* 12-byte piece of a pressable button (*_menu_draw_pressable_button): source
 * rectangle, then its offset from the button origin. */
typedef struct menu_button_piece {
    u16 u;        /* 0x00 */
    u16 v;        /* 0x02 */
    s16 w;        /* 0x04 */
    s16 h;        /* 0x06 */
    u16 x_offset; /* 0x08 */
    u16 y_offset; /* 0x0a */
} menu_button_piece_t;

/* Seven menu entries form the crystal/treasure result panel. The AT-list
 * and confirmation groups each contain a message entry followed by a choice. */
typedef enum dead_unit_menu_entry {
    DEAD_UNIT_MENU_LIST = 0,
    DEAD_UNIT_MENU_AT_LIST = 1,
    DEAD_UNIT_MENU_CONFIRM = 3,
    DEAD_UNIT_MENU_MESSAGE = 5,
    DEAD_UNIT_MENU_MOVE = 6,
    DEAD_UNIT_MENU_ENTRY_COUNT = 7,
} dead_unit_menu_entry_e;

/* Crystal/treasure pickup result (g_main_crystal_pickup_result, 0x8006623c)
 * that battle_unit_generate_crystal_or_treasure builds on the main thread
 * stack call and returns; BATTLE keeps the pointer in g_battle_dead_unit_context and
 * WORLD in g_menu_dead_unit_context. The dead-unit panels pick
 * their variant from `result` and show `treasure_item`. */
typedef struct crystal_pickup_result {
    s32 result;                 /* 0x00; -1 none, 0 idle, 4 treasure, 2 crystal, |1 learned */
    u8 _unused_04[0x14];        /* 0x04 */
    u8 learned[19][3];          /* 0x18 */
    u8 unit_index;              /* 0x51 */
    u8 treasure_item;           /* 0x52; the roster/item id the panels show */
    u8 learned_abilities[0x40]; /* 0x53 */
} crystal_pickup_result_t;
extern crystal_pickup_result_t* g_menu_dead_unit_context;
s32 menu_get_dead_unit_selection(s32 battle_id);

void menu_start_dead_unit_threads(void);

/* One cursor-navigation node of the HELPMENU, BUNIT, ATTACK deployment and
 * DEBUGCHR editor screens (WORLD's help_navigation_record_t has the same
 * layout): the value or text id it refers to, its help-text base, the node
 * reached for each d-pad direction (up 0x1000, down 0x4000, right 0x2000,
 * left 0x8000) and the cursor position. */
typedef struct help_navigation_record {
    s16* value;          /* 0x00 */
    s16 help_base;       /* 0x04 */
    u8 destination[4];   /* 0x06: up, down, right, left */
    s16 x;               /* 0x0a */
    s16 y;               /* 0x0c */
    s16 vertical_cursor; /* 0x0e: nonzero: the cursor points down and bobs along y */
} help_navigation_record_t;
typedef char help_navigation_record_size_must_be_16[(sizeof(help_navigation_record_t) == 16) ? 1 : -1];

/* menu dispatch */
/* Provisional 0x14-byte system-function record, tables at BATTLE 0x801692c0
 * and WORLD 0x80156750 (*_menu_run_system_function_thread,
 * menu_dispatch_system_function). The member comments name the BATTLE
 * globals; WORLD binds its own copies. */
typedef struct menu_system_function {
    s16 menu_id;                /* 0x00; copied to g_menu_current_id */
    u16 text_id;                /* 0x02; menu entry text id */
    s16 menu_entry_index;       /* 0x04; g_menu_thread_menu_data index */
    u16 row_action;             /* 0x06; copied to g_menu_system_function_row_actions */
    u16 thread_count;           /* 0x08; menu entry thread_count */
    u8 _padding_0a[2];          /* aligns thread_entry */
    void (*thread_entry)(void); /* 0x0c; started as thread 8 */
    s16 alternate_id;           /* 0x10; replaces the id when navigation messages are not On */
    u8 _padding_12[2];          /* tail padding to 4-byte alignment */
} menu_system_function_t;
typedef char menu_system_function_size_must_be_20[(sizeof(menu_system_function_t) == 20) ? 1 : -1];

void menu_run_system_function_thread(void);
void menu_start_system_function_thread(s32 system_function);

s32 menu_dispatch_system_function(
    s32 command, s32 option, s32 unit_id, s32 parameter, s32 enabled, struct action_command* action);
s32 menu_init_system_function(s32 type, s32 option, s32 unit_id, s32 parameter, s32 enabled);

/* First argument of *_menu_init_system_function and
 * *_menu_dispatch_system_function; `option` selects the variant. */
typedef enum battle_menu_system_command {
    BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT = 1, /* prompt for the resolved menu command */
    BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK = 2,    /* option 0 closes, 1 and 4 confirm, others reject the move */
    BATTLE_MENU_SYSTEM_COMMAND_WAIT_MENU = 3,
    BATTLE_MENU_SYSTEM_COMMAND_ACTION_CONFIRM = 4,
    BATTLE_MENU_SYSTEM_COMMAND_SPELL_QUOTE = 5,
    BATTLE_MENU_SYSTEM_COMMAND_CLOSE = 6,
    BATTLE_MENU_SYSTEM_COMMAND_ABILITY_ANNOUNCEMENT = 7,
    BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK = 8, /* may start event 0x190-0x193; otherwise closes */
    BATTLE_MENU_SYSTEM_COMMAND_TARGET_CONFIRM = 9,
    BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE = 10,  /* option: text id */
    BATTLE_MENU_SYSTEM_COMMAND_ABILITY_LEARNED = 11, /* option: ability id */
    BATTLE_MENU_SYSTEM_COMMAND_CRYSTAL_OR_TREASURE = 12,
    BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM = 13,
    BATTLE_MENU_SYSTEM_COMMAND_TRAP_RESULT = 14,    /* option: trap id */
    BATTLE_MENU_SYSTEM_COMMAND_MOVE_FIND_ITEM = 15, /* option: item id */
    BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM_SILENT = 16,
    BATTLE_MENU_SYSTEM_COMMAND_POST_ACTION_RESULT = 17, /* option: action_display_image_id_e */
} battle_menu_system_command_e;

/* Result of *_menu_dispatch_system_function (and of
 * battle_menu_init_system_function, which returns it). */
typedef enum menu_system_dispatch_result {
    MENU_SYSTEM_DISPATCH_UNKNOWN_COMMAND = 0, /* the CLOSE function was started instead */
    MENU_SYSTEM_DISPATCH_DONE = 1,            /* a system function was started, or nothing had to run */
    MENU_SYSTEM_DISPATCH_EVENT = 2,           /* SCENARIO_CHECK started an event, or queued it for option 2 */
} menu_system_dispatch_result_e;

/* g_menu_current_id: the open battle menu,
 * and the index of its entry in g_menu_idle_action_entries, which is also what a row action from 0 to
 * 0xff opens. The unit menus are picked by
 * battle_menu_get_id_based_on_mount_moveable_actable. A system function
 * (menu_system_function_t) shows its text in MESSAGE or in one of the
 * choice entries and sets the current id itself; ids from 0x62 exist only
 * there and have no entry.
 *
 * Nothing in the C or in the two tables opens EMPTY_MESSAGE,
 * PERFORMING_MESSAGE, PROMPT_16, PROMPT_17 or the four NO_ACT turn menus,
 * and WAIT_YES_NO is linked only by a row action that is overwritten. */
typedef enum battle_menu_id {
    BATTLE_MENU_ID_NONE = -1,
    BATTLE_MENU_ID_TURN = 0,                /* the unit's turn: it can move and act */
    BATTLE_MENU_ID_AUTO_BATTLE = 1,         /* the Auto-battle row: the five settings */
    BATTLE_MENU_ID_AUTO_BATTLE_TARGET = 2,  /* Change target, Check target: a setting chosen again */
    BATTLE_MENU_ID_ABILITY_LIST = 3,        /* the abilities or items of the chosen SKILLSET_LIST row */
    BATTLE_MENU_ID_SKILLSET_LIST = 4,       /* the Act row: the unit's commands */
    BATTLE_MENU_ID_STATUS = 5,              /* the Status row: the unit status panel */
    BATTLE_MENU_ID_WAIT = 6,                /* the Wait row */
    BATTLE_MENU_ID_WAIT_YES_NO = 7,         /* WAIT's row action in the table; *_menu_wait_thread replaces it */
    BATTLE_MENU_ID_STATUS_LIST = 8,         /* the status panel's list of current statuses */
    BATTLE_MENU_ID_EMPTY_MESSAGE = 9,       /* a message entry with empty text 0x8000 */
    BATTLE_MENU_ID_MINI_MENU = 10,          /* *_menu_start_mini_menu_display_thread: AT, Unit List, Option */
    BATTLE_MENU_ID_OPTIONS = 11,            /* MINI_MENU's Option row: OPTION.OUT */
    BATTLE_MENU_ID_UNIT_LIST = 12,          /* MINI_MENU's Unit List row: BUNIT.OUT */
    BATTLE_MENU_ID_PERFORMING_MESSAGE = 13, /* a message entry with text 0x8002 */
    BATTLE_MENU_ID_TURN_MOVED = 14,         /* moved, or Don't Move */
    BATTLE_MENU_ID_PLAYER_UNIT = 15,        /* a player unit, not its turn */
    BATTLE_MENU_ID_OTHER_UNIT = 16,         /* any other unit, not its turn */
    /* The same two while the active unit is not under the player's control,
     * or while the status menu is open. */
    BATTLE_MENU_ID_PLAYER_UNIT_INSPECT = 17,
    BATTLE_MENU_ID_OTHER_UNIT_INSPECT = 18,
    BATTLE_MENU_ID_TURN_ACTED = 19, /* acted, or cannot act */
    BATTLE_MENU_ID_TURN_MOVED_ACTED = 20,
    BATTLE_MENU_ID_JOBSTTS = 21,   /* a STATUS_LIST row: JOBSTTS.OUT */
    BATTLE_MENU_ID_PROMPT_16 = 22, /* two equal message entries whose row opens PROMPT_YES_NO */
    BATTLE_MENU_ID_PROMPT_17 = 23,
    BATTLE_MENU_ID_PROMPT_YES_NO = 24, /* Yes, No */
    /* Yes, No under MESSAGE. As the current id: a system function that asks
     * (ride the chocobo, move here, inherit the crystal, learn the ability). */
    BATTLE_MENU_ID_MESSAGE_YES_NO = 25,
    /* The choices a system function shows without its navigation message. */
    BATTLE_MENU_ID_TARGET_KIND = 26,   /* Unit, Panel, Quit */
    BATTLE_MENU_ID_PANEL_EXECUTE = 27, /* Execute, Quit for a panel target */
    BATTLE_MENU_ID_MESSAGE = 28,       /* a system function's navigation message */
    BATTLE_MENU_ID_AT_LIST = 29,       /* MINI_MENU's AT row: OPTION.OUT's AT list */
    BATTLE_MENU_ID_EXECUTE = 30,       /* Execute, Quit */
    BATTLE_MENU_ID_ANNOUNCEMENT = 31,  /* the timed banner: an ability name or an action result */
    /* TURN and TURN_MOVED with the rows Move, Wait and Status only. */
    BATTLE_MENU_ID_TURN_NO_ACT = 32,
    BATTLE_MENU_ID_TURN_NO_ACT_MOVED = 33,
    BATTLE_MENU_ID_MOVE = 34,                       /* the Move row: *_menu_confirm_move_command_thread */
    BATTLE_MENU_ID_ACTION_RESTRICTION_MESSAGE = 35, /* why the unit cannot act or use the ability */
    BATTLE_MENU_ID_ABILITY_CONFIRM = 36,            /* an ABILITY_LIST row: *_menu_validate_skill_selection_thread */
    /* The same choices under MESSAGE. */
    BATTLE_MENU_ID_MESSAGE_TARGET_KIND = 37,
    BATTLE_MENU_ID_MESSAGE_PANEL_EXECUTE = 38,
    BATTLE_MENU_ID_AUTO_BATTLE_CONFIRM = 39, /* an AUTO_BATTLE row chosen for the first time */
    BATTLE_MENU_ID_AUTO_BATTLE_UNIT = 40,    /* Attack, Quit or Defend, Quit: the unit an auto-battle setting names */
    BATTLE_MENU_ID_MESSAGE_ATTACK_UNIT = 41, /* Attack, Quit under MESSAGE */
    BATTLE_MENU_ID_MESSAGE_DEFEND_UNIT = 42, /* Defend, Quit under MESSAGE */
    BATTLE_MENU_ID_EFFECT_MESSAGE = 43,      /* BATTLE_SYSTEM_FUNCTION_EFFECT_MESSAGE's entry */
    BATTLE_MENU_ID_DEBUGCHR = 44,            /* *_menu_open_debugchr */
    BATTLE_MENU_ID_MESSAGE_EXECUTE = 45,     /* Execute, Quit under MESSAGE */
    BATTLE_MENU_ID_RIDER_TURN = 46,          /* the TURN menus of a mounted unit, with a Stop ride row */
    BATTLE_MENU_ID_RIDER_TURN_MOVED = 47,
    BATTLE_MENU_ID_RIDER_TURN_ACTED = 48,
    BATTLE_MENU_ID_RIDER_TURN_MOVED_ACTED = 49,
    BATTLE_MENU_ID_RIDER_TURN_NO_ACT = 50,
    BATTLE_MENU_ID_RIDER_TURN_NO_ACT_MOVED = 51,
    BATTLE_MENU_ID_ABILITY_PREVIEW_AT_LIST = 52, /* the turn order with the ability under the cursor in it */
    BATTLE_MENU_ID_ABILITY_PREVIEW_MESSAGE = 53, /* shown instead when that ability gets no turn */
    BATTLE_MENU_ID_EGG = 54,                     /* replaces OTHER_UNIT and OTHER_UNIT_INSPECT for an egg */
    BATTLE_MENU_ID_EGG_STATUS = 55,              /* EGG's Status row: only the invalid sound */
    BATTLE_MENU_ID_DEAD_UNIT = 105,              /* crystal or treasure options */
} battle_menu_id_e;

extern s32 g_menu_current_id; /* battle_menu_id_e */

/* Indices of the system function table (g_menu_system_function_table and
 * its WORLD twin) that the dispatcher names; the rest are message variants. */
typedef enum battle_system_function_id {
    BATTLE_SYSTEM_FUNCTION_WAIT_MENU = 1,
    BATTLE_SYSTEM_FUNCTION_CLOSE = 8,
    BATTLE_SYSTEM_FUNCTION_ABILITY_ANNOUNCEMENT = 10,
    BATTLE_SYSTEM_FUNCTION_EFFECT_MESSAGE = 29,
    BATTLE_SYSTEM_FUNCTION_ABILITY_LEARNED = 30,
    BATTLE_SYSTEM_FUNCTION_CRYSTAL_OR_TREASURE = 31,
    BATTLE_SYSTEM_FUNCTION_MOVE_CONFIRM = 33,
    BATTLE_SYSTEM_FUNCTION_TRAP_RESULT_FIRST = 34,
    BATTLE_SYSTEM_FUNCTION_MOVE_FIND_ITEM_SOLD = 38,
    BATTLE_SYSTEM_FUNCTION_MOVE_FIND_ITEM = 39,
    BATTLE_SYSTEM_FUNCTION_MOVE_CONFIRM_SILENT = 40,
    BATTLE_SYSTEM_FUNCTION_ARITHMETICKS_ANNOUNCEMENT = 44,
    BATTLE_SYSTEM_FUNCTION_POST_ACTION_RESULT = 50,
} battle_system_function_id_e;

/* companion overlays */
/* Companion overlay requests: index into the overlay LBA, size and load offset
 * tables (g_battle_menu_companion_executable_lbas and the WORLD twins). */
typedef enum companion_overlay {
    COMPANION_OVERLAY_OPTION = 1,             /* EVENT/OPTION.OUT */
    COMPANION_OVERLAY_BUNIT = 2,              /* EVENT/BUNIT.OUT, first half */
    COMPANION_OVERLAY_HELPMENU = 3,           /* EVENT/HELPMENU.OUT */
    COMPANION_OVERLAY_EVTOOL = 4,             /* EVENT/EVTOOL.OUT; no request in retail */
    COMPANION_OVERLAY_DEBUGMAP = 5,           /* EVENT/DEBUGMAP.OUT; no request in retail */
    COMPANION_OVERLAY_REQUIRE = 6,            /* EVENT/REQUIRE.OUT */
    COMPANION_OVERLAY_JOBSTTS = 7,            /* EVENT/JOBSTTS.OUT */
    COMPANION_OVERLAY_SMALL = 8,              /* EVENT/SMALL.OUT */
    COMPANION_OVERLAY_EQUIP = 9,              /* EVENT/EQUIP.OUT, first half */
    COMPANION_OVERLAY_EQUIP_SECOND_HALF = 10, /* loaded above the heap reservation */
    COMPANION_OVERLAY_DEBUGCHR = 11,          /* EVENT/DEBUGCHR.OUT */
    COMPANION_OVERLAY_ETC = 12,               /* EVENT/ETC.OUT */
    COMPANION_OVERLAY_ATTACK = 13,            /* EVENT/ATTACK.OUT */
    COMPANION_OVERLAY_CARD = 14,              /* EVENT/CARD.OUT */
    COMPANION_OVERLAY_BUNIT_SECOND_HALF = 15, /* loaded above the heap reservation */
} companion_overlay_e;

/* Companion data file requests (the second LBA and size tables). */
typedef enum battle_companion_file {
    BATTLE_COMPANION_FILE_WIN001 = 2,    /* EVENT/WIN001.BIN */
    BATTLE_COMPANION_FILE_BONUS = 3,     /* EVENT/BONUS.BIN */
    BATTLE_COMPANION_FILE_SPELL_MES = 4, /* EVENT/SPELL.MES */
} battle_companion_file_e;
void menu_run_companion_overlay_stub(void);
extern u8* g_menu_companion_data_file_buffer;
extern s32 g_menu_companion_data_file_loading;
extern s32 g_menu_companion_data_file_lbas[];
extern s32 g_menu_companion_data_file_sizes[];
extern s32 g_menu_companion_executable_loading;
extern s32 g_menu_companion_executable_lbas[];
extern s32 g_menu_companion_executable_sizes[];
extern s32 g_menu_companion_executable_offsets[];
void menu_open_companion_data_file(void);
void menu_open_companion_executable(void);
extern s32 g_menu_companion_overlay_state;

/* g_battle_companion_overlay_state / g_menu_companion_overlay_state: the overlay
 * entry the frame driver calls, cleared when the overlay returns. */
typedef enum companion_overlay_state {
    COMPANION_OVERLAY_STATE_IDLE = 0,
    COMPANION_OVERLAY_STATE_STUB = 1,      /* an empty function */
    COMPANION_OVERLAY_STATE_BUNIT = 2,     /* bunit_entrypoint */
    COMPANION_OVERLAY_STATE_JOBSTTS = 3,   /* jobstts_entrypoint */
    COMPANION_OVERLAY_STATE_EQUIP = 4,     /* equip_entrypoint */
    COMPANION_OVERLAY_STATE_CARD_SAVE = 5, /* card_save_run_menu */
} companion_overlay_state_e;

extern char g_menu_wait_for_file_allocation_message[];
extern s32 g_menu_companion_executable_request_index;
extern s32 g_menu_companion_data_file_request_index;

void menu_open_option_at_list(void);
void menu_open_option_dead_unit_panel(void);
void menu_open_option_game_options(void);

void menu_free_high_overlay(void);
void menu_reserve_high_overlay(u32 size);
void menu_request_open_companion_executable(s32 request_index);
void menu_request_open_small(void);
void menu_request_open_companion_data_file(s32 request_index);

/* g_*_menu_indicator_state[]: state of each of the two translucent panels
 * *_menu_draw_thread_status_indicators draws behind the unit status banners. */
typedef enum menu_indicator_state {
    MENU_INDICATOR_STATE_HIDDEN = 0,     /* not drawn */
    MENU_INDICATOR_STATE_FADING_IN = 1,  /* brightness rises by 0xc a frame up to 0x78 and holds */
    MENU_INDICATOR_STATE_FADING_OUT = 2, /* brightness falls by 0xc a frame; at zero the state returns to HIDDEN */
} menu_indicator_state_e;

/* g_*_menu_panel_fade_mode: what *_menu_update_panel_fade does to the panel's
 * dimming tiles. */
typedef enum menu_panel_fade_mode {
    MENU_PANEL_FADE_NONE = 0, /* not drawn */
    MENU_PANEL_FADE_IN = 1,   /* intensity rises by 8 a frame up to 80 and holds */
    MENU_PANEL_FADE_OUT = 2,  /* intensity falls by 8 a frame; at zero the mode returns to NONE */
} menu_panel_fade_mode_e;

/* Second argument of *_gfx_set_draw_mode_for_texture_page: which of the menu
 * code's five 4-bit texture pages a draw mode selects, named by what the game
 * keeps there. Any value above 3 selects the last one. */
typedef enum menu_texture_page {
    MENU_TEXTURE_PAGE_FRAME = 0,            /* VRAM (960, 256): FRAME.BIN, the window frames and UI sprites */
    MENU_TEXTURE_PAGE_ALLOCATED_IMAGES = 1, /* VRAM (448, 0): the grid *_gfx_alloc_tpage7_vram hands out */
    MENU_TEXTURE_PAGE_TEXT_IMAGES = 2,      /* VRAM (384, 0): rendered message and list text */
    MENU_TEXTURE_PAGE_UNIT_SPRITES = 3,     /* VRAM (832, 256): the first unit spritesheet column; no caller */
    MENU_TEXTURE_PAGE_ITEM_ICONS = 4,       /* VRAM (896, 256): ITEM.BIN; the code passes y = 288 */
} menu_texture_page_e;

/* Last argument of *_menu_build_and_upload_window_frame_image. The UPLOAD
 * modes load the image into VRAM and free the buffer; the BUILD modes return
 * the buffer for the caller to draw into and upload. The ALTERNATE modes use
 * the frame pattern with sixteen-row top and bottom borders. */
typedef enum menu_window_frame_mode {
    MENU_WINDOW_FRAME_UPLOAD = 0,
    MENU_WINDOW_FRAME_BUILD = 1,
    MENU_WINDOW_FRAME_ALTERNATE_UPLOAD = 2,
    MENU_WINDOW_FRAME_ALTERNATE_BUILD = 3,
} menu_window_frame_mode_e;

/* The selected row and scroll offset saved across list transitions. */
typedef struct menu_cursor_state {
    s32 selected_index; /* 0x00 */
    s32 scroll_offset;  /* 0x04 */
} menu_cursor_state_t;
typedef char menu_cursor_state_size_must_be_8[sizeof(menu_cursor_state_t) == 8 ? 1 : -1];

/* A remembered list cursor, scroll base and entry id. The menu restores the
 * cursor by entry id when the live list has changed. */
/* menu selection */
typedef struct menu_selection_record {
    s16 selected_index;    /* 0x00 */
    s16 scroll_base_index; /* 0x02 */
    s16 entry_id;          /* 0x04; low ten bits of the list entry */
} menu_selection_record_t;
typedef char menu_selection_record_size_must_be_6[sizeof(menu_selection_record_t) == 6 ? 1 : -1];

void menu_set_selection_record(s32 index, s32 selected_index, s32 scroll_base_index, s16* entries);

/* The selected row and scroll position of an ability list. */
typedef struct menu_list_selection {
    u8 selected_index;
    u8 scroll_offset;
} menu_list_selection_t;
typedef char menu_list_selection_size_must_be_2[sizeof(menu_list_selection_t) == 2 ? 1 : -1];

/* One remembered command/skillset selection per unit, followed by seven
 * ability-list positions. Arithmeticks uses list positions 4, 5 and 6. */
typedef struct menu_unit_selection_record {
    u8 command_index;               /* 0x00 */
    u8 _unknown_01;                 /* 0x01; reset to 0xff */
    u8 skillset_index;              /* 0x02 */
    menu_list_selection_t lists[7]; /* 0x03 */
} menu_unit_selection_record_t;
typedef char menu_unit_selection_record_size_must_be_17[(sizeof(menu_unit_selection_record_t) == 17) ? 1 : -1];
extern menu_unit_selection_record_t g_menu_unit_selection_records[];
void menu_reset_unit_record(s32 record_index);

/* Remap three remembered selections to one replacement for a menu. */
typedef struct menu_selection_remap {
    u8 menu_id;
    u8 replacement;
    u8 matches[3];
} menu_selection_remap_t;
typedef char menu_selection_remap_size_must_be_5[sizeof(menu_selection_remap_t) == 5 ? 1 : -1];

/* selection state */
extern u8 g_menu_restore_pending;
extern s16 g_menu_pending_selection[4];

void menu_mini_menu_cancel_delay_thread(void);
void menu_confirm_sound_delay_thread(void);
void menu_check_action_restrictions(void);
void menu_check_action_slot_restrictions(s32 menu, s32 value, s32 check_unit);
void menu_reset_unit_records(void);
void menu_reset_unit_records_wrapper(void);
void menu_restore_selection_for_selected_unit(s32 menu_id);
void menu_run_skillset_thread(void);
void menu_select_unit_action_slots_thread(void);

/* horizontal selection */
extern s16 g_menu_selection_values[];
s16 menu_update_wrapped_horizontal_selection(u16 entry_count, u8 selection_index, s32 input_mask);
s32 menu_update_wrapped_grid_selection(s32 columns, s32 rows, s32 last_index, s32 old_index, s32 input);

/* One row of the 31-entry menu table at g_battle_menu_id_records; its first 12 rows are the
 * menus whose selection is remembered per unit.
 * menu_restore_selection_for_selected_unit and battle_menu_record_selection_for_selected_unit
 * prove the 8-byte stride and read only the leading menu id;
 * battle_menu_resolve_selection walks all 31 rows. */
typedef struct menu_command_map {
    u16 menu_id;
    u16 _padding_02; /* aligns map */
    /* 0x04: per-menu command remap table. map[0] handles the 0xFF selection
     * and map[n + 1] any other value; named by battle_menu_resolve_selection,
     * its only reader. The other readers use menu_id alone. */
    u8* map;
} menu_command_map_t;
typedef char menu_command_map_size_must_be_8[(sizeof(menu_command_map_t) == 8) ? 1 : -1];
extern menu_command_map_t g_menu_command_maps[31];
extern s32 g_menu_active_menu; /* active menu */
extern s16 g_menu_result;      /* menu result: -4 pending, -1 canceled */
extern s16 g_menu_selected_option;
extern s32 g_menu_used_skillset_id;
extern s32 g_menu_used_item_id;
extern u16 g_menu_anything_ability_id;
extern s32 g_menu_dead_unit_action;
extern s32 g_menu_dead_unit_result;
s32 menu_resolve_selection(void);

/* memory */
/* Both menu arenas track sixteen allocations in 256-byte pages. */
enum {
    MENU_BUFFER_ALLOCATION_COUNT = 16,
    MENU_BUFFER_PAGE_SIZE = 256,
};

/* Buffer is -1 when the slot is free; page_count is the allocated run length. */
typedef struct menu_allocation {
    u8* buffer;     /* 0x00 */
    s32 page_count; /* 0x04 */
} menu_allocation_t;
typedef char menu_allocation_size_must_be_8[(sizeof(menu_allocation_t) == 8) ? 1 : -1];
/* BATTLE and WORLD each link their own allocator over their own arena. */
void* menu_alloc_memory(s32 size);
void menu_free_memory(void* buffer);
void* menu_alloc_buffer(s32 bytes);

/* input */
/* Pressed, held and previous buttons, followed by the menu repeat counters. */
typedef struct menu_input_state {
    u32 new_buttons;              /* 0x00 */
    u32 current_buttons;          /* 0x04 */
    u32 previous_buttons;         /* 0x08 */
    u32 initial_repeat_counter;   /* 0x0c */
    u32 repeat_counter;           /* 0x10 */
    u32 secondary_repeat_counter; /* 0x14 */
} menu_input_state_t;
typedef char menu_input_state_size_must_be_24[sizeof(menu_input_state_t) == 24 ? 1 : -1];

/* Shared window record whose signed rectangle the window-frame and tiled
 * rectangle commands use for source 2 (pointer at 0x801cd720). */
typedef struct menu_window_rect_source {
    u8 _unused_00[6];
    RECT rect; /* 0x06 */
} menu_window_rect_source_t;

/* Destination rectangle and source cell for a decimal digit. */
typedef struct menu_digit_glyph {
    RECT rect;
    s16 u;
    s16 v;
    s16 w;
    u16 h;
} menu_digit_glyph_t;
typedef char menu_digit_glyph_size_must_be_16[sizeof(menu_digit_glyph_t) == 16 ? 1 : -1];

/* Texture rectangle, CLUT and texture page returned by menu icon callbacks. */
typedef struct menu_icon_source {
    s16 u;     /* 0x00 */
    s16 v;     /* 0x02 */
    s16 w;     /* 0x04 */
    s16 h;     /* 0x06 */
    u16 clut;  /* 0x08 */
    u16 tpage; /* 0x0a */
} menu_icon_source_t;
typedef char menu_icon_source_size_must_be_12[sizeof(menu_icon_source_t) == 12 ? 1 : -1];

extern menu_icon_source_t g_menu_item_type_icon_source;

void menu_build_item_icon_source(menu_icon_source_t* output, u32 item_id);
menu_icon_source_t* menu_build_item_type_icon_source(s32 index);
void menu_draw_weapon_hand_icons(s16 weapon_id);

/* One 12-byte positioned number: a text position, a pointer to the live
 * value and the decimal format. *_menu_draw_numeric_display_entries draws
 * tables of them (kind 0 and 1 select the plain and colored number
 * renderers), as do the status panels' *_text_render_decimal_entry_list and
 * *_text_render_signed_decimal_entries, which leave kind unset. The 0xc stride
 * is proved by g_menu_numeric_display_entries's three sublists at +0, +0x24
 * and +0x54 (entries 0, 3 and 7). */
typedef struct menu_number_entry {
    u16 x;      /* 0x00 */
    u16 y;      /* 0x02 */
    s16* value; /* 0x04 */
    s16 format; /* 0x08; low byte digit count, high byte sign/marker flags
                   (0x400/0x800/0x1000; 0xc00/0xe00 draw dashes) */
    s16 kind;   /* 0x0a */
} menu_number_entry_t;
typedef char menu_number_entry_size_must_be_12[(sizeof(menu_number_entry_t) == 12) ? 1 : -1];

extern s32 g_menu_hide_numeric_values;   /* non-zero: draw digits as blanks (0x48, 0x10) */
extern glyph_blit_t g_menu_number_glyph; /* source rectangle in the number/sign font */
extern glyph_blit_t g_menu_stacked_glyph_lower;
extern glyph_blit_t g_menu_stacked_glyph_upper;

void menu_draw_number_glyphs(s32 value, s32 digits, u8* pixels, glyph_blit_t* pos);
void menu_draw_stacked_glyph_pair(u8* pixels, glyph_blit_t* position);
void menu_draw_numeric_display_entries(u8* buffer, menu_number_entry_t* entries, glyph_blit_t* position, s32 count);
void menu_display_text_string(u8* image, glyph_blit_t* origin, u8* text);
void menu_display_text_entry_past_choice_marks(s32 text_id, u8* image, glyph_blit_t* origin);
void menu_display_text_entry(s32 text_id, u8* image, glyph_blit_t* origin);
void menu_display_number_entry(s32 value, u8 digit_count, u8* image, glyph_blit_t* origin);

void menu_display_hovered_unit_stats(
    RECT* frame_rect, s32 mode, menu_number_entry_t* entries, menu_unit_status_record_t* status);

/* The scene-local menu text state at BATTLE 0x80165f9c and
 * WORLD 0x80153280: the script-variable pointer,
 * then the pen the glyph renderers receive. */
typedef struct menu_text_state {
    s32* script_variables; /* 0x00; also bound as g_script_variables in each scene */
    /* 0x04: rect.x/rect.y are the text origin (menu_set_text_origin) and
     * the rect's size is not used; stride (0x0c) is the rect width * 4
     * (the inner window width); color (0x10) is the glyph blit fill,
     * 0x44444444 while text color 4 is drawn (*_menu_draw_text_columns). */
    glyph_blit_t pen;
} menu_text_state_t;

extern menu_text_state_t g_menu_text_state;

/* Added to glyph palette indices while menu text is drawn. */
extern s32 g_menu_text_palette_offset;

void menu_set_text_origin(s32 x, s32 y);

/* list threads */
extern u32* g_menu_input_controller;
void menu_draw_text_columns(menu_entry_t* entry, s32* row_offset, u8* buffer);
extern s16 g_menu_confirm_stored_unit_row_actions;
extern s16 g_menu_set_auto_battle_row_actions;

void menu_icon_linked_entry_thread(void);
void menu_confirm_move_command_thread(void);
void menu_confirm_auto_battle_selection_thread(void);
void menu_icon_list_thread(void);
void menu_icon_strip_thread(void);
void menu_icon_timed_thread(void);
void menu_text_entry_wait_thread(void);

/* options */
/*
 * Live 32-bit game-options value at 0x800473ac. The OPTION overlay unpacks these
 * fields in menu order. Toggle settings encode On as 0 and Off as 1; the
 * paired high bit participates in the menu's Customize/Initialize state.
 */
enum {
    GAME_OPTIONS_NAVIGATION_MESSAGES_MASK = 0x00018000,
    GAME_OPTIONS_MAX_EQUIP_AT_JOB_CHANGE_MASK = 0x60000000,
    GAME_OPTIONS_NAVIGATION_MESSAGES_OFF_BITS = 0x00008000,
};

typedef enum game_option_index {
    GAME_OPTION_CURSOR_MOVEMENT = 0,
    GAME_OPTION_CURSOR_REPEAT_SPEED = 1,
    GAME_OPTION_MULTI_HEIGHT_CURSOR_SPEED = 2,
    GAME_OPTION_FINGER_CURSOR_REPEAT_SPEED = 3,
    GAME_OPTION_MESSAGE_DISPLAY_SPEED = 4,
    GAME_OPTION_NAVIGATION_MESSAGES = 5,
    GAME_OPTION_ABILITY_NAMES = 6,
    GAME_OPTION_EFFECT_MESSAGES = 7,
    GAME_OPTION_DISPLAY_GAINED_EXP_JP = 8,
    GAME_OPTION_TARGET_FLASHING = 9,
    GAME_OPTION_SHOW_UNEQUIPPABLE_ITEMS = 10,
    GAME_OPTION_MAX_EQUIP_AT_JOB_CHANGE = 11,
    GAME_OPTION_SOUND_MODE = 12,
    GAME_OPTION_COUNT = 13,
    GAME_OPTION_UNCHANGED = 13,
} game_option_index_e;

typedef enum game_option_toggle {
    GAME_OPTION_ON = 0,
    GAME_OPTION_OFF = 1,
} game_option_toggle_e;

typedef enum game_cursor_movement {
    GAME_CURSOR_MOVEMENT_TYPE_A = 0,
    GAME_CURSOR_MOVEMENT_TYPE_B = 1,
} game_cursor_movement_e;

typedef enum game_cursor_repeat_speed {
    GAME_CURSOR_REPEAT_SPEED_FAST = 0,
    GAME_CURSOR_REPEAT_SPEED_REGULAR = 1,
    GAME_CURSOR_REPEAT_SPEED_SLOW = 2,
} game_cursor_repeat_speed_e;

typedef enum game_multi_height_cursor_speed {
    GAME_MULTI_HEIGHT_CURSOR_SPEED_FAST = 0,
    GAME_MULTI_HEIGHT_CURSOR_SPEED_REGULAR = 1,
    GAME_MULTI_HEIGHT_CURSOR_SPEED_SLOW = 2,
    GAME_MULTI_HEIGHT_CURSOR_SPEED_STOP = 3,
} game_multi_height_cursor_speed_e;

typedef enum game_finger_cursor_repeat_speed {
    GAME_FINGER_CURSOR_REPEAT_SPEED_FASTEST = 0,
    GAME_FINGER_CURSOR_REPEAT_SPEED_FASTER = 1,
    GAME_FINGER_CURSOR_REPEAT_SPEED_FAST = 2,
    GAME_FINGER_CURSOR_REPEAT_SPEED_REGULAR = 3,
    GAME_FINGER_CURSOR_REPEAT_SPEED_SLOW = 4,
    GAME_FINGER_CURSOR_REPEAT_SPEED_SLOWER = 5,
    GAME_FINGER_CURSOR_REPEAT_SPEED_SLOWEST = 6,
} game_finger_cursor_repeat_speed_e;

typedef enum game_message_display_speed {
    GAME_MESSAGE_DISPLAY_SPEED_FAST = 0,
    GAME_MESSAGE_DISPLAY_SPEED_REGULAR = 1,
    GAME_MESSAGE_DISPLAY_SPEED_SLOW = 2,
} game_message_display_speed_e;

typedef struct game_options_fields {
    u32 cursor_movement : 3;
    u32 cursor_repeat_speed : 3;
    u32 multi_height_cursor_speed : 3;
    u32 finger_cursor_repeat_speed : 3;
    u32 message_display_speed : 3;
    u32 navigation_messages : 2;
    u32 ability_names : 2;
    u32 effect_messages : 2;
    u32 sound_mode : 2;
    u32 show_unequippable_items : 2;
    u32 display_gained_exp_jp : 2;
    u32 target_flashing : 2;
    u32 max_equip_at_job_change : 2;
    u32 _unused_03_bit7 : 1;
} game_options_fields_t;

/* The reset loads and stores this four-byte record as a scalar; a struct copy
 * moves the defaults load ahead of the input-timing stores. */
typedef union game_options {
    u32 value;
    game_options_fields_t fields;
} game_options_t;
typedef char game_options_size_must_be_4[sizeof(game_options_t) == 4 ? 1 : -1];

/* Provisional: one of the two 0x190-byte options-menu packet pages at WORLD
 * 0x801ae894 and OPTION 0x801cbc5c, alternated by frame. */
typedef struct options_menu_page {
    SPRT title;                         /* 0x000: window title image */
    SPRT value;                         /* 0x014: selected value image */
    scaled_draw_area_pair_t title_area; /* 0x028 */
    scaled_draw_area_pair_t value_area; /* 0x050 */
    DR_MODE text_mode;                  /* 0x078 */
    DR_MODE window_mode;                /* 0x084 */
    DR_MODE icon_mode;                  /* 0x090 */
    menu_icon_record_t icons;           /* 0x09c */
    SPRT frame;                         /* 0x118: cursor frame */
    SPRT arrows[2];                     /* 0x12c: scroll arrows */
    SPRT arrow_marks[2];                /* 0x154 */
    SPRT thumb;                         /* 0x17c: scroll-bar thumb */
} options_menu_page_t;
typedef char options_menu_page_size_must_be_400[(sizeof(options_menu_page_t) == 400) ? 1 : -1];

/* cursor animation */
extern s8 g_menu_cursor_forward_offset_keyframes[];
extern s8 g_menu_cursor_reverse_offset_keyframes[];
extern u16 g_menu_cursor_texture_page;

void menu_update_and_draw_animated_cursor(const point16_t* anchor, u16* state, s32 mode);

/* input and palette state */
extern u16 g_menu_active_clut_back;
extern u16 g_menu_active_clut_front;
extern u16 g_menu_alternate_number_clut;
extern u16 g_menu_cursor_mode0_background_clut;
extern u16 g_menu_cursor_mode0_foreground_clut;
extern u16 g_menu_cursor_mode1_background_clut;
extern u16 g_menu_cursor_mode1_foreground_clut;
extern u16 g_menu_disabled_number_clut;
extern u8 g_menu_input_activation_timer;
extern u16 g_menu_input_latched_button;
extern u16 g_menu_input_newly_pressed;
extern u16 g_menu_input_previous;
extern u32 g_menu_input_primary_repeat;
extern s32 g_menu_input_repeat_counters[16];
extern u32 g_menu_input_secondary_repeat;
extern u16 g_menu_palette_clut;
extern u16 g_menu_text_clut_0_mode0;
extern u16 g_menu_text_clut_0_mode1;
extern u16 g_menu_text_clut_1_mode0;
extern u16 g_menu_text_clut_1_mode1;
extern u16 g_menu_text_clut_2_mode0;
extern u16 g_menu_text_clut_2_mode1;
extern u16 g_menu_text_clut_3_mode0;
extern u16 g_menu_text_clut_3_mode1;
extern rgb24_t g_menu_text_color;
extern u16 g_menu_window_clut;
void menu_input_update_controller(void);
void menu_set_palette_mode(s32 mode);

/* command palette state */
extern u16 g_menu_gfx_clut_id;
extern u16 g_menu_gfx_texture_page;
extern u8 g_menu_use_alternate_palette;

/* text */
/* The font, EVENT/FONT.BIN: 2200 glyphs of 14 rows of 10 two-bit pixels. A
 * byte from TEXT_EXTENDED_GLYPH_PREFIX_FIRST selects a page of glyphs and
 * the byte after it the glyph: (prefix & 0xf) * TEXT_GLYPHS_PER_PREFIX + byte. */
enum {
    TEXT_GLYPH_BYTES = 35,
    TEXT_GLYPHS_PER_PREFIX = 208,
};

/* Encoded menu-string controls. The 0xe2-0xfd meanings come from the dialog
 * interpreter branches (battle_text_run_dialog_thread and its WORLD
 * twin) and, for 0xe7, the menu text-image renderers; codes with an operand
 * consume the following byte. F5/F6 are the {0xF5xx}{0xF6xx} "Alter String"
 * controls. The 0xe0-0xeb substitution codes are text_format_code_e. */
typedef enum text_control {
    TEXT_EXTENDED_GLYPH_PREFIX_FIRST = 0xd0,
    TEXT_EXTENDED_GLYPH_PREFIX_LAST = 0xdf,
    TEXT_BACKREFERENCE_PREFIX_FIRST = 0xf0, /* 0xf0-0xf3 start a three-byte back-reference into earlier text */
    TEXT_BACKREFERENCE_PREFIX_LAST = 0xf3,
    TEXT_SET_GLYPH_DELAY = 0xe2,     /* operand: typewriter delay */
    TEXT_SET_PALETTE = 0xe3,         /* operand: text palette */
    TEXT_PRINT_NEXT_VALUE = 0xe4,    /* prints the next substitution value */
    TEXT_PRINT_INDEXED_VALUE = 0xe6, /* operand: substitution value index */
    TEXT_SET_X_POSITION = 0xe7,      /* operand: pen x relative to the text rect */
    TEXT_SET_NUMBER_WIDTH = 0xe8,    /* operand: digit count of the next number */
    TEXT_SET_PORTRAIT = 0xec,        /* operand: portrait, also script variable 0x5a */
    TEXT_PAGE_BREAK = 0xf4,
    TEXT_SET_VARIABLE_BASE = 0xf5, /* operand: script variable for 0xf6 */
    TEXT_STORE_VARIABLE = 0xf6,    /* operand: value stored to that variable */
    TEXT_NEWLINE = 0xf8,
    TEXT_RELEASE_WAITING_THREADS = 0xf9,
    TEXT_SPACE = 0xfa, /* 4-pixel blank glyph */
    TEXT_CHOICE_MARK = 0xfb,
    TEXT_CHOICE_SELECT = 0xfc,
    TEXT_WAIT_FOR_RESUME = 0xfd,
    TEXT_END_WAIT_FOR_CONFIRM = 0xfe,
    TEXT_END_AUTO_CLOSE = 0xff,
} text_control_e;

/* Box type: the 0x70 bits of a DisplayMessage dialog type byte. */
typedef enum dialog_box_type {
    DIALOG_BOX_TYPE_CENTERED = 0x00,
    DIALOG_BOX_TYPE_PORTRAIT = 0x10, /* three lines with a portrait */
    DIALOG_BOX_TYPE_CHECK = 0x20,    /* one to four lines titled "Check" */
    DIALOG_BOX_TYPE_HELP = 0x30,     /* one to four lines titled "Help" */
    DIALOG_BOX_TYPE_TWO_LINE = 0x40,
    DIALOG_BOX_TYPE_EIGHT_LINE = 0x50,
} dialog_box_type_e;

/* The low two dialog-type bits place the window vertically. */
typedef enum dialog_position {
    DIALOG_POSITION_AUTO = 0, /* portrait tail: choose above/below from projected speakers */
    DIALOG_POSITION_TOP = 1,
    DIALOG_POSITION_BOTTOM = 2,
    DIALOG_POSITION_CENTER = 3,
} dialog_position_e;

/* Fields of a DisplayMessage dialog type byte
 * (text_dialog_record_t::dialog_type). */
enum {
    DIALOG_TYPE_POSITION_MASK = 0x03,
    DIALOG_TYPE_OPTIONS_MASK = 0x0c,
    DIALOG_TYPE_BOX_TYPE_MASK = 0x70, /* dialog_box_type_e */
    DIALOG_TYPE_NO_BOX = 0x80,
};

/* The DIALOG_TYPE_OPTIONS_MASK field of a portrait box, as
 * *_menu_build_window_image and the text thread read it. 0x0c is tested with
 * the Help box only and has no name here. */
typedef enum dialog_options {
    DIALOG_OPTIONS_TAIL = 0x00,           /* a tail pointing at the speaker */
    DIALOG_OPTIONS_ALTERNATE_TAIL = 0x04, /* the second tail graphic, 16 rows tall */
    DIALOG_OPTIONS_NO_TAIL = 0x08,        /* plain frame, not placed at the speaker */
} dialog_options_e;

typedef enum menu_decimal_sign_flag {
    MENU_DECIMAL_SIGN_PLUS = 0x0400,
    MENU_DECIMAL_SIGN_MINUS = 0x0800,
} menu_decimal_sign_flag_e;

/* The other format flags *_text_render_decimal_value tests. */
typedef enum menu_decimal_format_flag {
    MENU_DECIMAL_DOTS_PREFIX = 0x0100,        /* a two-row dotted glyph before the number */
    MENU_DECIMAL_WIDEN_FOR_100 = 0x0200,      /* a value of 100 is drawn with three digits */
    MENU_DECIMAL_THREE_COLUMN_FIELD = 0x2000, /* right-aligned in three digit columns */
    MENU_DECIMAL_LEADING_ZEROS = 0x4000,      /* every digit of the count is drawn */
    MENU_DECIMAL_SLASH_PREFIX = 0x8000,       /* a "/" glyph before the number */
} menu_decimal_format_flag_e;

enum {
    MENU_DECIMAL_FIELD_WIDTH_MASK = 0x000f,
    MENU_DECIMAL_FIELD_WIDTH_CLEAR_MASK = 0xfff0,
    MENU_DECIMAL_SIGN_MASK = 0x0c00,
    MENU_DECIMAL_SIGN_CLEAR_MASK = 0xf3ff,
    MENU_DECIMAL_ZERO_PLACEHOLDER_FLAGS = 0x0804,
};

enum {
    TEXT_SECTION_COUNT = 32,
    TEXT_ID_SECTION_MASK = 0xf800,
    TEXT_ID_SECTION_SHIFT = 11,
    TEXT_ID_ENTRY_MASK = 0x07ff,
};

/* Text ids of the first entry of the text sections that text resolution and unit-name loading use.
 * Unit-name classes reserve one 0x100-entry page each. */
typedef enum text_id_base {
    TEXT_ID_SKILLSET_NAME_BASE = 0x1000,
    TEXT_ID_JOB_NAME_BASE = 0x3000,
    TEXT_ID_ITEM_NAME_BASE = 0x3800,
    TEXT_ID_UNIT_NAME_BASE = 0x4000,
    TEXT_ID_UNIT_NAME_SPECIAL_BASE = TEXT_ID_UNIT_NAME_BASE,
    TEXT_ID_UNIT_NAME_GENERIC_MALE_BASE = 0x4100,
    TEXT_ID_UNIT_NAME_GENERIC_FEMALE_BASE = 0x4200,
    TEXT_ID_UNIT_NAME_GENERIC_MONSTER_BASE = 0x4300,
    TEXT_ID_ABILITY_NAME_BASE = 0x7000,
    /* By world_location_id_e. ATTACK binds its deployment messages to this
     * section instead. */
    TEXT_ID_LOCATION_NAME_BASE = 0x9000,
    TEXT_ID_SKILLSET_NAME_SOURCE_BASE = 0xb000, /* section 22, which SMALL also binds as section 2 */
    /* The sections of EVENT/WORLD.LZW that only the world map uses, each
     * indexed by the id its name gives unless noted. */
    TEXT_ID_WORLD_HELP_MESSAGE_BASE = 0x5800,
    TEXT_ID_COMPLETED_PROPOSITION_NAME_BASE = 0x6000, /* the Brave Story's Job list, by proposition */
    TEXT_ID_PROPOSITION_REWARD_NAME_BASE = 0x7800,
    TEXT_ID_TIP_NAME_BASE = 0x8000,
    TEXT_ID_WORLD_MESSAGE_BASE = 0x8800, /* the block of WORLD/WLDMES.BIN that is loaded */
    TEXT_ID_FACT_NAME_BASE = 0x9800,
    TEXT_ID_MAP_NAME_BASE = 0xa800,   /* by map_id_e less one */
    TEXT_ID_WORLD_MENU_BASE = 0xb800, /* wldcore_text_id_e */
    TEXT_ID_TUTORIAL_NAME_BASE = 0xc000,
    TEXT_ID_RUMOR_NAME_BASE = 0xc800,
    TEXT_ID_PROPOSITION_NAME_BASE = 0xd000,
    TEXT_ID_UNEXPLORED_LAND_NAME_BASE = 0xd800,
    TEXT_ID_TREASURE_NAME_BASE = 0xe000,
    TEXT_ID_STORY_EVENT_NAME_BASE = 0xe800,         /* the Brave Story's Record list */
    TEXT_ID_PERSON_PROFILE_NAME_BASE = 0xf000,      /* by person * 16 + profile */
    TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE = 0xf800, /* by wldcore_proposition_fields_t::name_text less one */
} text_id_base_e;

/* Index into g_text_section_pointers: a text id's top five bits. Only the
 * sections the code binds or reads by a fixed role are named. A screen
 * rebinds a slot to its own text while it is up: help_text_section_e here,
 * world_text_section_e and require_text_section_e map those bindings. */
typedef enum text_section {
    TEXT_SECTION_EVENT_MESSAGES = 0,      /* the loaded event's text; BUNIT binds its menu text here */
    TEXT_SECTION_SKILLSET_NAMES_COPY = 2, /* TEXT_ID_SKILLSET_NAME_BASE; a copy of section 22 */
    TEXT_SECTION_JOB_NAMES = 6,           /* TEXT_ID_JOB_NAME_BASE */
    TEXT_SECTION_ITEM_NAMES = 7,          /* TEXT_ID_ITEM_NAME_BASE */
    TEXT_SECTION_ROSTER_UNIT_NAMES = 8,   /* TEXT_ID_UNIT_NAME_BASE */
    TEXT_SECTION_UNIT_NAMES = 9,
    TEXT_SECTION_PARTY_UNIT_NAMES = 11, /* names by party slot, which ATTACK writes for the deployment screen */
    TEXT_SECTION_ABILITY_NAMES = 14,    /* TEXT_ID_ABILITY_NAME_BASE */
    TEXT_SECTION_MESSAGES = 17,
    TEXT_SECTION_LOCATION_NAMES = 18,
    TEXT_SECTION_UNIT_MESSAGE = 21,         /* SPELL.MES while the spell quote thread runs */
    TEXT_SECTION_SKILLSET_NAMES = 22,       /* TEXT_ID_SKILLSET_NAME_SOURCE_BASE */
    TEXT_SECTION_FORMATION_MESSAGES = 24,   /* WORLD formation and name-entry messages; CARD binds its own copy */
    TEXT_SECTION_COMPOSED_TEXT = 25,        /* a buffer the screen fills: a sort or reorder list, the entered name */
    TEXT_SECTION_DEPLOYMENT_UNIT_HELP = 26, /* ATTACK: eleven help strings per party slot */
    TEXT_SECTION_OVERLAY_SCREEN = 27,       /* each overlay screen binds its own text here */
    TEXT_SECTION_AT_LIST = 31,              /* the AT list's text, built when the list opens */
} text_section_e;

/* The help text file, which HELPMENU and WORLD's help menu bind section by
 * section while a help window is open. */
typedef enum help_text_section {
    HELP_TEXT_SECTION_UNIT_STATUS = 1,   /* the fields of the unit status panels */
    HELP_TEXT_SECTION_MENU_COMMANDS = 2, /* the rows of each command menu */
    HELP_TEXT_SECTION_TERRAIN = 11,      /* by the cursor tile's surface type */
    HELP_TEXT_SECTION_JOBS = 12,         /* by job id */
    HELP_TEXT_SECTION_ITEMS = 13,        /* by item id */
    HELP_TEXT_SECTION_ABILITIES = 15,    /* by ability id */
    HELP_TEXT_SECTION_SKILLSETS = 19,    /* by skillset id */
    HELP_TEXT_SECTION_STATUSES = 20,     /* by status id */
} help_text_section_e;

/* Substitution opcodes shared by battle messages and WORLD event text. */
typedef enum text_format_code {
    TEXT_FORMAT_FIRST = 0xe0,
    TEXT_FORMAT_RAMZA_NAME = 0xe0,
    TEXT_FORMAT_UNIT_NAME = 0xe1,
    TEXT_FORMAT_LOCATION_NAME = 0xe5,
    TEXT_FORMAT_ITEM_NAME = 0xe9,
    TEXT_FORMAT_ABILITY_NAME = 0xea,
    TEXT_FORMAT_EXPLICIT_ID = 0xeb,
} text_format_code_e;

/* Three-byte backreference: five length bits and a base-254 backward offset. */
typedef struct text_backref {
    u8 length_high : 2;
    u8 opcode : 6;
    u8 offset_high : 4;
    u8 _unused_01 : 1;
    u8 length_low : 3;
    u8 offset_low;
} text_backref_t;
typedef char text_backref_size_must_be_3[sizeof(text_backref_t) == 3 ? 1 : -1];

/* Text-stream cursor state used by measurement and menu rendering. The
 * measurers use scratchpad 0x1f800000; menu rendering uses 0x1f800200. */
typedef struct text_reader_state {
    s32 command;         /* 0x00; current text byte */
    s32 column;          /* 0x04; characters on the current line */
    s32 max_column;      /* 0x08; widest line so far */
    s32 row;             /* 0x0c; completed lines */
    s32 value;           /* 0x10; substitution value / text id */
    s32 scratch;         /* 0x14; intermediate byte-count / back-offset */
    s32 remaining_bytes; /* 0x18; bytes left before returning to return_cursor */
    u8* return_cursor;   /* 0x1c; cursor to resume at, -1 when none */
    u8 _unused_20[4];
    s32* value_cursor; /* 0x24; next substitution value */
    s32 pending_width; /* 0x28; explicit digit width from 0xE8, 0xFFFF when none */
    s32 glyph_page;    /* 0x2c; pending 0xDx page-prefix byte, 0 otherwise (world_measure_text_pixels) */
} text_reader_state_t;

/* Text window record of the dialog and message-box threads, rendered by the
 * BATTLE and WORLD *_menu_render_text_image_at_* functions: they build an
 * image buffer (the window builder when box_type is 0x10), draw `text` into
 * it and upload `rect`. */
typedef struct text_dialog_record {
    u8* buffer;        /* 0x00 */
    u16 width;         /* 0x04 */
    u16 height;        /* 0x06 */
    RECT rect;         /* 0x08 */
    s32 text;          /* 0x10 */
    u16 color;         /* 0x14 */
    u16 dialog_type;   /* 0x16: DisplayMessage Dialog Type byte */
    u16 box_type;      /* 0x18: dialog_type & 0x70 */
    u16 position;      /* 0x1a */
    u16 options;       /* 0x1c: dialog_type & 0x0c (dialog_options_e) */
    u16 no_box;        /* 0x1e */
    u16 line;          /* 0x20 */
    u16 first_line;    /* 0x22 */
    u16 last_line;     /* 0x24 */
    u16 arrow_x;       /* 0x26 */
    u16 arrow_y;       /* 0x28 */
    s16 _unknown_2a;   /* 0x2a */
    s16 mark;          /* 0x2c: measured width, later the 0xFB cursor mark */
    s16 lines;         /* 0x2e */
    s16 tail_offset;   /* 0x30: dialog tail offset from the window center */
    u8 _padding_32[2]; /* 0x32; aligns x */
    s32 x;             /* 0x34 */
    s32 y;             /* 0x38 */
    s32 portrait;      /* 0x3c */
    u16 origin_x;      /* 0x40 */
    u16 origin_y;      /* 0x42 */
    s32 arrow_offset;  /* 0x44: DisplayMessage arrow position, added to offset */
    s32 x_offset;      /* 0x48: DisplayMessage X coordinate, added to x */
    s32 y_offset;      /* 0x4c: DisplayMessage Y coordinate, added to y */
} text_dialog_record_t;

void menu_render_text_image_at_record_origin(text_dialog_record_t* record);

/* One pending text glyph: BATTLE's dialogue renderer state at 0x8016dad4
 * (filled by battle_text_run_dialog_thread, drawn by
 * text_draw_glyph_with_typewriter_delay), WORLD's typewriter request at 0x8019a338
 * (text_draw_glyph_with_typewriter_delay) and message-box glyph at
 * 0x801c3474 (world_text_message_box_thread, ..._delay_2). The menu text state
 * and the LoadImage RECT follow it in memory but are separate globals. */
typedef struct text_glyph_state {
    u16 x;            /* 0x00: pen x; the low two bits select the sub-byte pixel phase */
    s16 y;            /* 0x02: pen y */
    u16 glyph;        /* 0x04: character code; 0xfa is the blank that skips the upload */
    u8 _unused_06[6]; /* 0x06 */
    s16 mode;         /* 0x0c: 0 blanks the image, 0x10 draws it; the message box also accepts 0x20 */
    s16 palette;      /* 0x0e */
} text_glyph_state_t;
typedef char text_glyph_state_size_must_be_16[(sizeof(text_glyph_state_t) == 16) ? 1 : -1];

/* Text runtime buffers bound to the resident BATTLE or WORLD copy. */
/* Player-name substitution, padded to 20 bytes with TEXT_END_WAIT_FOR_CONFIRM. */
extern u8 g_text_player_name_buffer[20];
/* Values $00-$1F; indexed substitutions can read past the named 32 entries. */
extern s32 g_text_substitution_values[32];
/* Pixel width of each line from the last text_measure_pixels call. */
extern s32 g_text_line_widths[];
/* Glyph pixel widths, TEXT_GLYPHS_PER_PREFIX entries per prefix page. */
extern const u8 g_text_glyph_widths[];
/* Text IDs select a section in the upper five bits and an entry in the lower eleven. */
extern u8* g_text_section_pointers[TEXT_SECTION_COUNT];
extern u8* g_text_saved_section_pointers[TEXT_SECTION_COUNT];
extern s32 g_text_section_backup_active;

void text_restore_section_pointers(void);
void text_save_section_pointers(void);

/* typewriter glyphs */
extern u8* g_text_glyph_bitmap_data;
extern s32 g_text_glyphs_per_wait;
extern s32 g_text_slot_owner_thread_ids[3];
void menu_release_current_thread_slot(void);
extern s32 g_text_slot_glyph_counts[3];
extern u8 g_text_slot_source_columns[];
extern u8 g_text_slot_row_pitches[];
extern text_glyph_state_t g_text_typewriter_glyph;
extern menu_text_state_t g_text_typewriter_state;
extern RECT g_text_typewriter_vram_rect;
extern u8 g_text_typewriter_glyph_images[3][8][0x54];
extern u8 g_text_typewriter_previous_images[3][0x54];
extern u8 g_text_typewriter_column_scratch[14];

void text_draw_glyph_with_typewriter_delay(s32 pen_x, s32 pen_y, s32 delay);

/* text threads */
extern s32 g_menu_text_active_key;

void menu_text_start_help_thread(s32 thread_data);
void menu_text_start_thread_if_idle(s32 thread_id, s32 first, s32 second, s32 third);
s32 menu_text_start_or_poll_thread(s32 thread_id, s32 parameter_0, s32 key, s32 parameter_2, s32 y_offset);

/* menu bitstreams */
/* Read-only secondary stream; CARD uses it to count save-buffer parity bits. */
extern s32 g_menu_bits_secondary_index;
extern u8 g_menu_bits_secondary_reset;
extern const u8* g_menu_bits_secondary_source;

void menu_bits_init_secondary_reader(const u8* data);
u32 menu_bits_read_secondary(s32 count);

/* encoded text entries */
const u8* menu_text_find_entry_by_index(const u8* data, s16 count, s32 unused_2);

/* decimal text */
extern glyph_blit_t g_menu_decimal_glyph;
void menu_text_render_decimal_entry_list(u8* pixels, const menu_number_entry_t* entries, glyph_blit_t* out, s32 count);
void menu_text_render_signed_decimal_entries(u8* pixels, menu_number_entry_t* entries, glyph_blit_t* out, s32 count);
void menu_text_render_decimal_value(s32 value, s32 flags, u8* pixels, glyph_blit_t* position);

void text_measure(s16* columns, s16* rows, const u8* text);
void text_measure_pixels(s16* width, s16* rows, const u8* text);
u8* text_find_entry(u32 text_id);

u8* text_advance_cursor_with_backreferences(text_reader_state_t* state, u8* cursor);
/* The menu renderer has a separate linked copy of the same cursor helper. */
u8* menu_text_advance_cursor_with_backreferences(text_reader_state_t* state, u8* cursor);
u8* text_advance_cursor_with_separate_backreference_state(s32* remaining_bytes, u8* cursor, u8** return_cursor);
const u8* text_skip_to_entry(const u8* text, s32 entry_index);
s32 text_count_decimal_digits(s32 value);
void text_fill_field_with_terminators(u8* text);
s32 text_resolve_format_string_id(s32 format_code, s32 value);
extern const char g_text_decimal_format[];
extern glyph_blit_t g_menu_text_pen;
extern s32 g_text_glyph_first_row;
extern s32 g_text_glyph_row_limit;
extern u32 g_text_decimal_divisors[];
extern s32 g_menu_text_param_22; /* line limits copied from the text record (+0x22, +0x24) */
extern s32 g_menu_text_param_24;
void text_print_fnt_value_and_next(s32 value);
void text_relocate_pointer_table(const u32* offsets);
/* BATTLE and WORLD each link their own body. */
void text_init_section_pointers(void);
void text_build_unit_name_list(void);
void text_build_display_message(s32 offset, s32 flags, s32* out_width);
s32 text_is_spell_quote_exception_skillset(s32 skillset_id);
void text_select_ability_announcement(struct action_command* action, s32 unit_id, s32 enabled);

/* unit */
/* Provisional 0x2d0-byte double-buffered frame of the unit editor panel
 * (WORLD 0x801a3494, BATTLE 0x801730f8). The first 0x2a4 bytes are laid out
 * as menu_status_panel_buffer_t (draw modes, sprites, tiles and lines),
 * and the trailing draw-area pair follows menu_panel_frame_t. */
typedef struct menu_unit_editor_frame {
    DR_MODE draw_modes[3];              /* 0x000: image pages 0, 2 and 4 */
    SPRT sprites[24];                   /* 0x024: 19 panel sprites, then 5 item icons */
    TILE tiles[2];                      /* 0x204 */
    LINE_F2 lines[8];                   /* 0x224 */
    menu_palette_primitives_t* palette; /* 0x2a4 */
    scaled_draw_area_pair_t draw_areas; /* 0x2a8 */
} menu_unit_editor_frame_t;
typedef char menu_unit_editor_frame_size_must_be_720[sizeof(menu_unit_editor_frame_t) == 720 ? 1 : -1];

void menu_run_unit_editor_panel_thread(void);

/* Color and endpoint offsets of the unit editor panel's eight frame lines,
 * relative to the panel box, seven bytes per line (WORLD 0x80156198, BATTLE
 * 0x80168d0c). The loop walks the table with a byte offset stepping by 7 (the
 * target's induction variable and exit test), so the fields are addressed by
 * offset, as for world_menu_line_layout_t. */
enum menu_unit_editor_line_layout_offset {
    MENU_UNIT_EDITOR_LINE_R = 0,
    MENU_UNIT_EDITOR_LINE_G = 1,
    MENU_UNIT_EDITOR_LINE_B = 2,
    MENU_UNIT_EDITOR_LINE_X0 = 3,
    MENU_UNIT_EDITOR_LINE_Y0 = 4,
    MENU_UNIT_EDITOR_LINE_X1 = 5,
    MENU_UNIT_EDITOR_LINE_Y1 = 6,
    MENU_UNIT_EDITOR_LINE_SIZE = 7
};

/* Provisional 0x2ec-byte double-buffered page of the unit status panel
 * (pairs at 0x801a25f8, first page per mode from g_menu_unit_status_first_page_by_mode). */
typedef struct menu_unit_status_page {
    POLY_G4 bars[3];                   /* 0x000: HP/MP/CT gauges */
    DR_MODE draw_mode_1;               /* 0x06c */
    DR_MODE draw_mode_0;               /* 0x078 */
    menu_palette_primitives_t palette; /* 0x084 */
    POLY_FT4 quads[5];                 /* 0x170 */
    SPRT sprites[7];                   /* 0x238 */
    POLY_FT4 portrait;                 /* 0x2c4 */
} menu_unit_status_page_t;

/* Provisional 0xF0-byte double-buffered frame of the unit summary panel. */
typedef struct menu_unit_summary_frame {
    SPRT sprites[7];                    /* 0x00: [0] name/job, [1]-[3] numbers, [6] zodiac */
    DR_MODE draw_mode_0;                /* 0x8c */
    DR_MODE draw_mode_1;                /* 0x98 */
    menu_palette_primitives_t* palette; /* 0xa4 */
    DR_OFFSET draw_offset_a;            /* 0xa8 */
    DR_OFFSET draw_offset_b;            /* 0xb4 */
    point16_t offset_a;                 /* 0xc0 */
    point16_t offset_b;                 /* 0xc4 */
    POLY_FT4 portrait;                  /* 0xc8 */
} menu_unit_summary_frame_t;

/* First argument of *_unit_start_view_thread and value of g_*_unit_view_mode:
 * the row of g_*_unit_view_thread_sets the supervisor keeps running. Each
 * mode from 1 up runs the selected-tile info panel plus the panels named. */
typedef enum unit_view_mode {
    UNIT_VIEW_MODE_NONE = 0,           /* no panel */
    UNIT_VIEW_MODE_TILE_INFO = 1,      /* the tile info panel alone */
    UNIT_VIEW_MODE_HOVERED_UNIT = 2,   /* hovered-unit stats */
    UNIT_VIEW_MODE_UNIT_SUMMARY = 3,   /* unit summary panel and hovered-unit stats */
    UNIT_VIEW_MODE_RIGHT_UNIT = 4,     /* right-side unit stats */
    UNIT_VIEW_MODE_ATTACK_PREVIEW = 5, /* attack caster and attack target stats */
} unit_view_mode_e;

/* Second argument of *_menu_display_hovered_unit_stats: which of the four
 * unit stats panels the thread draws; each value has one caller. */
typedef enum unit_stats_panel {
    UNIT_STATS_PANEL_HOVERED_UNIT = 0,  /* *_menu_refresh_hovered_unit_stats_display */
    UNIT_STATS_PANEL_ATTACK_CASTER = 1, /* *_menu_preview_attack_caster_stats */
    UNIT_STATS_PANEL_ATTACK_TARGET = 2, /* *_menu_preview_attack_target_stats; shows the reaction ability name */
    UNIT_STATS_PANEL_RIGHT_UNIT = 3,    /* *_menu_refresh_right_unit_stats_display */
} unit_stats_panel_e;

/* Provisional: 0x30-byte unit-view thread set at 0x801564d0, indexed by the
 * view mode. Each of the four slots names a thread entry (or -1), the
 * scheduler slot it runs in, and the task id it must carry. */
typedef struct menu_unit_view_thread_set {
    void (*entries[4])(void); /* 0x00 */
    s32 thread_ids[4];        /* 0x10 */
    s32 task_ids[4];          /* 0x20 */
} menu_unit_view_thread_set_t;
typedef char menu_unit_view_thread_set_size_must_be_48[(sizeof(menu_unit_view_thread_set_t) == 48) ? 1 : -1];

extern menu_unit_view_thread_set_t g_menu_unit_view_thread_sets[];
extern s32 g_menu_unit_view_mode;
extern u16 g_menu_unit_view_cached_battle_id;
extern u16 g_menu_unit_view_cached_comparison_id;

void menu_run_unit_view_supervisor_thread(void);
void menu_start_unit_view_thread(s32 mode, s32 unit_a, s32 unit_b);

/* menu screen starters */
void menu_open_debugchr(void);
void menu_open_debugchr_unit_editor(void);
void menu_open_bunit(void);
void menu_start_mini_menu_display_thread(void);
void menu_start_entry_thread(void);
void menu_run_triangle_selection(void);
void menu_run_help_menu_thread(void);
void menu_run_equip_thread(void);
void menu_run_jobstts_thread(void);
void menu_run_bunit_return_thread(void);
void menu_run_bunit_transition_thread(void);

/* Unit portrait transition primitives. */

/* text substitutions */
/* Text variables $7E/$7F: the four 16-bit values text_set_substitution_values_7e_7f stores
 * (BATTLE 0x801660ec, table index 0x7e from 0x80165ef4; WORLD 0x801533d0 from 0x801531d8). */
extern RECT g_text_substitution_rect;

void text_set_substitution_values_7e_7f(s32 x, s32 y, s32 width, s32 height);
void text_configure_sprite_vram(RECT* rect, s32 width, s32 height, void* packet, s32 clut_offset);
extern u16 g_text_message_duration_frames;
void text_set_message_duration_frames(u16 frames);

/* menu state */
extern s32 g_menu_companion_overlay_primitives;
/* Array view: as a scalar, GCC hoists the menu-index reload above its store
 * in menu_run_system_function_thread. */
extern menu_system_function_t g_menu_system_function_table[];
extern s32 g_menu_system_function_thread_busy;
extern menu_entry_t g_menu_idle_action_entries[];
extern s16 g_menu_action_slot_row_actions[6];
extern s16 g_menu_action_slot_selected_option;
extern u8 g_menu_restriction_message_ids[];
extern s32 g_menu_panel_fade_mode; /* menu_panel_fade_mode_e */
extern s32 g_menu_panel_fade_intensity;
extern u8 g_menu_panel_fade_modes[];
extern u8 g_menu_restore_depth;
extern u8 g_menu_restore_unit_id;
extern u16 g_menu_transition_state;
extern u16 g_menu_hovered_stats_frame_count;
extern s16 g_menu_auto_battle_option_colors[5]; /* text color per option */
/* Row actions of the auto-battle setting menu (menu entry 0x80166b4c, +0x24). */
extern s16 g_menu_auto_battle_row_actions[8];
extern menu_scroll_text_layout_t g_menu_at_list_layout;
/* Tail glyph source and destination records for menu_build_window_image. */
extern glyph_blit_t g_menu_window_sprite_glyph;
extern glyph_blit_t g_menu_window_sprite_position;
extern s32 g_menu_system_function_command;
extern s32 g_menu_system_function_option;
extern u16 g_menu_system_function_row_actions[];
extern menu_selection_remap_t g_menu_selection_remaps[8];
/* Handed to the dead-unit panel thread as its second parameter. Nothing stores to it and the thread
 * does not read the parameter. */
extern s32 g_menu_dead_unit_panel_parameter;
extern menu_palette_pair_t g_menu_default_palette_colors;
extern u16* const* g_menu_help_active_text_id_tables;
/* Source image used when menus render glyphs. */
extern menu_glyph_sheet_t g_menu_glyph_image_data;
extern s32 g_menu_formation_display_mode;
extern game_options_t g_menu_saved_game_options;

/* spell quotes and glyph data */
extern s32 g_text_spell_quote_last_ability_id;
extern u8 g_text_spell_quote_exception_skillsets[];
extern u8 g_text_glyph_bitmaps[];

/* unit panel data */
extern u8 g_menu_billboard_job_name[];                /* job name text window */
extern u8 g_menu_billboard_unit_name[];               /* name text window */
extern u8 g_menu_billboard_secondary_skillset_name[]; /* secondary skillset name text window */
extern u8 g_menu_billboard_skillset_name[];           /* primary skillset name text window */
/* Entries 5..7 of g_menu_sprite_page_image_params; a separate alias preserves the original
 * template base. */
extern image_load_parameters_t g_menu_sprite_page_image_param_bases[];
extern menu_number_entry_t g_menu_attack_caster_stats_setup[8];
extern menu_number_entry_t g_menu_attack_target_stats_setup[8];
extern menu_number_entry_t g_menu_hovered_unit_stats_setup[10];
extern menu_number_entry_t g_menu_right_unit_stats_setup[10];
extern struct action_data* g_menu_preview_target_action;
extern menu_quad_page_t g_menu_height_display_pages[2];
extern RECT g_menu_tile_info_image_location;
extern image_load_parameters_t g_menu_tile_info_image_params[2];
extern menu_palette_slot_t g_menu_unit_summary_palettes[];
extern image_load_parameters_t g_menu_unit_summary_sprite_image_params[];

/* numeric display data */
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

/* unit editor panel data */
extern menu_unit_editor_frame_t g_menu_unit_editor_frames[2];
extern RECT g_menu_unit_editor_cursor_rect; /* cursor source */
extern image_load_parameters_t g_menu_unit_editor_item_icon_image_params[5];
extern u8 g_menu_unit_editor_line_colors[8 * MENU_UNIT_EDITOR_LINE_SIZE];
extern menu_palette_primitives_t g_menu_unit_editor_palette;
extern RECT g_menu_unit_editor_panel_rect; /* panel box */
extern image_load_parameters_t g_menu_unit_editor_row_image_params_a[4];
extern image_load_parameters_t g_menu_unit_editor_row_image_params_b[4];
extern u16 g_menu_unit_editor_sprite_clut_pairs[19][2];
extern image_load_parameters_t g_menu_unit_editor_sprite_image_params_a[19];
extern image_load_parameters_t g_menu_unit_editor_sprite_image_params_b[19];
extern RECT g_menu_unit_editor_text_rect_a; /* first text image */
extern RECT g_menu_unit_editor_text_rect_b; /* second text image */

/* unit status panel data */
extern u16 g_menu_unit_status_bounce_step;
extern u16 g_menu_unit_status_bounce_offsets[];
extern u8 g_menu_unit_status_first_page_by_mode[];
extern CVECTOR g_menu_unit_status_gauge_colors[];
extern image_load_parameters_t g_menu_unit_status_icon_image_params[];
extern menu_unit_status_page_t g_menu_unit_status_panel_pages[];
extern u16 g_menu_unit_status_quad_clut_pairs[][2];
extern image_load_parameters_t* g_menu_unit_status_quad_image_params_by_mode[];
extern u16 g_menu_unit_status_sprite_clut_pairs[][2];
extern image_load_parameters_t* g_menu_unit_status_sprite_image_params_by_mode[];

/* glyph rendering */
extern glyph_blit_t g_text_large_number_glyph_rect; /* glyph source rectangle in the large number font */

void menu_clear_render_buffer(void* buffer, u32 bytes);
void text_render_glyph_to_4bpp_image(const u8* glyph_bitmap, u8* image, glyph_blit_t* origin, s32 palette_offset);
void text_blit_glyph(const u8* source, u8* pixels, const glyph_blit_t* glyph, const glyph_blit_t* position);
void text_draw_large_number_glyphs(s32 value, s32 digits, u8* pixels, glyph_blit_t* pos);
/* The caller leaves 0x16f in $a1, but the body never reads $a1. */
void gfx_mirror_4bpp_rect(u8* data, const glyph_blit_t* region);

#endif
