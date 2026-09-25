#ifndef FFT_MENU_H
#define FFT_MENU_H

/* Menu, window and text records shared across modules. */

#include "fft/gfx.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

/* ability */
/* Provisional: per-skill byte tables reached through g_battle_ai_workspace_ptr and indexed
 * by the skill selected in menu entry 3 (world_menu_validate_skill_selection_thread, 0x800f474c). Only the three
 * columns that function reads are named; the table extents are unknown. */
typedef struct world_ability_skill_use_tables {
    u8 unknown_000[0x1ec];
    u8 mp_cost[0x290 - 0x1ec]; /* 0x1ec; compared against the unit's MP */
    u8 flags[0x334 - 0x290];   /* 0x290; bit 1: blocked by a status */
    u8 target[1];              /* 0x334; 0xff means none */
} world_ability_skill_use_tables_t;

/* Provisional ability-list work buffer at *g_battle_ai_workspace_ptr (0x52 rows each).
 * WORLD twin of the BATTLE ability list. */
typedef struct world_ability_list {
    u16 ids[0x52];      /* 0x000; 0xffff terminated, item/ability text-bank row flags */
    u16 values[0x52];   /* 0x0a4 */
    u16 extras[0x52];   /* 0x148 */
    u8 mp_costs[0x52];  /* 0x1ec; 0xff terminated */
    u8 bytes_23e[0x52]; /* 0x23e */
    u8 flags[0x52];     /* 0x290; bit 1 = blocked by unit flag 0x59 & 8 */
    u8 bytes_2e2[0x52]; /* 0x2e2 */
    u8 enabled[0x52];   /* 0x334 */
} world_ability_list_t;

/* item */
/* Provisional records filled by the WORLD stat-comparison helper at
 * 0x80122f9c and consumed by world_item_combine_stat_details. Armor supplies the
 * HP/MP bonuses; weapon, shield, accessory and item-attribute tables supply
 * the named detail fields. Weapon fields follow equipment-slot order. */
typedef struct world_item_stat_summary {
    u8 unknown_00[2];
    s16 team_state; /* 0x02; the wldcore_unit_status_gauges_t team_state */
    u8 unknown_04[0x0a];
    s16 hp_bonus; /* 0x0e */
    u8 unknown_10[4];
    s16 mp_bonus; /* 0x14 */
    u8 unknown_16[0x28 - 0x16];
} world_item_stat_summary_t;
typedef char world_stat_summary_size_must_be_0x28[(sizeof(world_item_stat_summary_t) == 0x28) ? 1 : -1];

typedef struct world_item_stat_detail {
    s16 move_bonus; /* Signed halfwords combined by world_item_combine_stat_details. */
    s16 speed_bonus;
    s16 jump_bonus;
    s16 right_weapon_power; /* 0x06; summed per equipment slot by world_item_sum_equipment_stat_details */
    s16 left_weapon_power;  /* 0x08 */
    s16 right_weapon_evade; /* 0x0a */
    s16 left_weapon_evade;  /* 0x0c */
    s16 generic_monster;    /* 0x0e; star icons instead of ability-type icons; panels pick labels and sprites by it */
    s16 two_hands;          /* 0x10; with generic_monster, selects the panel sprite sheet layout */
    s16 physical_attack_bonus;
    s16 physical_class_evade;     /* 0x14 */
    s16 physical_shield_evade;    /* 0x16 */
    s16 physical_accessory_evade; /* 0x18 */
    u8 unknown_1a[2];
    s16 magical_attack_bonus;
    s16 magical_class_evade;     /* 0x1e */
    s16 magical_shield_evade;    /* 0x20 */
    s16 magical_accessory_evade; /* 0x22 */
    u16 equipment[5];            /* 0x24; world_menu_run_best_fit_equipment (0x8011b554) keeps its suggested set here */
    s16 ability_ids[5];          /* 0x2e; status panel ability rows, -1 for an empty row */
    u8 unknown_38[0x40 - 0x38];
} world_item_stat_detail_t;
typedef char world_stat_detail_size_must_be_0x40[(sizeof(world_item_stat_detail_t) == 0x40) ? 1 : -1];

/* menu */
/* Six menu buffers, allocated/released as three adjacent pairs. The BATTLE
 * allocator at 0x8012e348 and WORLD counterpart establish the 0x118 stride.
 * Event ChangeDialog scans records 0, 2 and 4, compares +0x14 with operand 0,
 * and checks the thread at +0x04 for task 0x33. For DisplayMessage, the text
 * threads store the dialog type's box nibble (& 0x70), the portrait unit and
 * the scroll state, and battle/world_menu_draw_active_window_frames read
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
    s32 icon_flags;         /* 0x20: visibility bits for the three window icons */
    s32 scroll_range;       /* 0x24: scrollable lines (line - lines); 1 when freshly allocated */
    s32 scroll_position;    /* 0x28: first drawn line; the scroll marker sits at
                               scroll_position / scroll_range of its span; 1 when freshly allocated */
    /* Primitive payload established by the WORLD allocator at 0x800e2548:
     * draw mode, four window sprites, three icon sprites, two quads and a
     * trailing colour. */
    DR_MODE draw_mode;    /* 0x2c */
    SPRT sprites[4];      /* 0x38 */
    SPRT icon_sprites[3]; /* 0x88 */
    POLY_FT4 quads[2];    /* 0xc4 */
    u8 r;                 /* 0x114 */
    u8 g;                 /* 0x115 */
    u8 b;                 /* 0x116 */
    u8 is_message_box;    /* 0x117: set by the message-box thread; alternate frame clut and tint */
} menu_window_buffer_t;
typedef char menu_window_buffer_size_must_be_0x118[(sizeof(menu_window_buffer_t) == 0x118) ? 1 : -1];
typedef char menu_window_buffer_selector_must_be_0x14
    [((unsigned long)&((menu_window_buffer_t*)0)->dialogue_selector == 0x14) ? 1 : -1];

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

/* Menu line-box layout, established by the builder at 0x800e3358: two DR_MODE
 * packets, twelve LINE_F2 packets and one SPRT whose CLUT (+0xe6) the
 * palette-bank initialisers set. */
typedef struct world_menu_palette_primitives {
    DR_MODE draw_mode;      /* 0x00: texture window g_world_gfx_texture_window */
    DR_MODE draw_mode_menu; /* 0x0c: texture window g_world_menu_numeric_display_texture_window */
    LINE_F2 lines[12];      /* 0x18 */
    SPRT sprite;            /* 0xd8 */
} world_menu_palette_primitives_t;
typedef char world_menu_palette_primitives_sprite_offset_must_be_0xd8
    [((unsigned long)&((world_menu_palette_primitives_t*)0)->sprite == 0xd8) ? 1 : -1];
typedef char world_menu_palette_lines_offset_must_be_0x18
    [((unsigned long)&((world_menu_palette_primitives_t*)0)->lines == 0x18) ? 1 : -1];
typedef char world_menu_palette_footer_offset_must_be_0xe6
    [((unsigned long)&((world_menu_palette_primitives_t*)0)->sprite.clut == 0xe6) ? 1 : -1];

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

/* Per-skillset menu behavior stored at 0x80065cb4. Values select specialized
 * inventory, targeting, and ability-loading paths; they are not menu command
 * opcodes. */
typedef enum action_menu_type {
    ACTION_MENU_TYPE_DEFAULT = 0x00,
    ACTION_MENU_TYPE_ITEM_INVENTORY = 0x01,
    ACTION_MENU_TYPE_WEAPON_INVENTORY = 0x02,
    ACTION_MENU_TYPE_ARITHMETICKS = 0x03,
    ACTION_MENU_TYPE_ELEMENTS = 0x04,
    ACTION_MENU_TYPE_BLANK_05 = 0x05,
    ACTION_MENU_TYPE_MONSTER = 0x06,
    ACTION_MENU_TYPE_KATANA_INVENTORY = 0x07,
    ACTION_MENU_TYPE_ATTACK = 0x08,
    ACTION_MENU_TYPE_JUMP = 0x09,
    ACTION_MENU_TYPE_CHARGE = 0x0a,
    ACTION_MENU_TYPE_DEFEND = 0x0b,
    ACTION_MENU_TYPE_CHANGE_EQUIPMENT = 0x0c,
    ACTION_MENU_TYPE_UNKNOWN_0D = 0x0d,
    ACTION_MENU_TYPE_BLANK_0E = 0x0e,
    ACTION_MENU_TYPE_UNKNOWN_0F = 0x0f,
    ACTION_MENU_TYPE_COUNT = 0x10,
} action_menu_type_e;

/* Contextual skillset IDs synthesized by the action menu. The value 0xbc is
 * also an ordinary monster skillset when it comes from a monster unit. */
typedef enum action_menu_pseudo_skillset {
    ACTION_MENU_PSEUDO_SKILLSET_ANYTHING = 0xbc,
} action_menu_pseudo_skillset_e;

/* Seven-byte window command in the menu script streams. The length field
 * advances to the next command, which may have a different size. */
typedef struct world_menu_window_command {
    u8 opcode;
    u8 length;
    u8 rectangle_source;
    u8 x;
    u8 y;
    u8 width;
    u8 height;
} world_menu_window_command_t;

/* Menu text-entry task referenced at +0x30 of the wait parameter. */
typedef struct world_menu_text_entry_wait_task {
    void (*setup)(void);
    u16 text_parameter;
    u16 text_thread_id;
} world_menu_text_entry_wait_task_t;

/* Shared prefix of the menu record passed to text-entry wait threads. */
typedef struct world_menu_text_entry_wait_param {
    u8 unknown_00[0x1c];
    u16 text_id;
    u8 unknown_1e[0x0e];
    u16 header_id;
    u8 unknown_2e[2];
    world_menu_text_entry_wait_task_t* task;
} world_menu_text_entry_wait_param_t;

/* Six-byte menu sprite cell: screen rectangle and texture origin. */
typedef struct battle_menu_sprite_cell {
    u8 x;
    u8 y;
    u8 w;
    u8 h;
    u8 u;
    u8 v;
} battle_menu_sprite_cell_t;
typedef char battle_menu_sprite_cell_size_must_be_6[(sizeof(battle_menu_sprite_cell_t) == 6) ? 1 : -1];

/* One 0x100-byte slot holding a panel's palette primitive set. */
typedef union world_menu_palette_slot {
    world_menu_palette_primitives_t primitives;
    u8 storage[0x100];
} world_menu_palette_slot_t;

/* Provisional: record reached through world_menu_entry_t::text_binding.
 * world_menu_text_entry_thread calls the hook at +0 and starts the text thread whose
 * id is at +6 with speaker_id as its fourth parameter. */
typedef struct world_menu_text_binding {
    void (*prepare)(void); /* 0x00 */
    u16 speaker_id;        /* 0x04: unit whose portrait speaks (roster slot) */
    u16 text_thread_id;    /* 0x06 */
    u8 _pad08[8];
    s16* row_text_ids; /* 0x10: text ids indexed by entry selected_index (world_menu_announce_entry_value_thread) */
} world_menu_text_binding_t;

/* Provisional: 0x3c-byte menu entry reached through
 * g_world_menu_thread_menu_data; the same menu record as BATTLE
 * g_battle_menu_thread_menu_data and the OPTION/EQUIP menus. 0x0c/0x0e are the
 * window size (frame rect 0x0c - 0x12 by 0x0e - 0x1a in WORLD 0x8010088c and
 * option_build_at_list). The parent table at 0x24 (indexed by selected_index)
 * and the thread entry at 0x28 are known from
 * world_menu_start_entry_chain_threads. */
typedef struct world_menu_entry {
    s16 vram_x;         /* 0x00: text-image VRAM x (world_menu_redraw_text_page_on_scroll) */
    s16 vram_y;         /* 0x02: text-image VRAM y */
    s16 inner_width;    /* 0x04: text-image width; RECT.w = inner_width >> 2 (4bpp) */
    s16 inner_height;   /* 0x06: text-image height (world_menu_size_entry_to_text) */
    u16 window_x;       /* 0x08: screen x; shifted left as the width grows */
    s16 window_y;       /* 0x0a: screen y; (7 - visible rows) * 8 - 0x48 in world_menu_build_available_item_list */
    s16 window_width;   /* 0x0c: window width; scroll arrows at window_x + window_width - 0xf */
    s16 window_height;  /* 0x0e: window height; child windows open at window_y + window_height */
    s16 unknown_10[2];  /* 0x10: cleared with the size fields (open_menu_start_music_test_list_thread) */
    s16 overall_width;  /* 0x14: set with inner_width and window_width */
    s16 overall_height; /* 0x16: set with inner_height and window_height */
    s16 unknown_18[2];  /* 0x18: cleared with the size fields */
    s16 text_id;        /* 0x1c: passed to world_text_find_entry */
    s16 max_row_index;  /* 0x1e: row count - 1 (world_menu_build_skillset_entries) */
    u16 field_0x20;     /* 0x20: set from the system-function table by world_menu_run_system_function_thread */
    u8 _pad22[0x24 - 0x22];
    s16* parent_indices;        /* 0x24 */
    void (*thread_entry)(void); /* 0x28 */
    u16 header_id;              /* 0x2c: menu header; text-thread parameter (world_menu_text_entry_thread) */
    u8 _pad2e[2];
    struct world_menu_text_binding* text_binding; /* 0x30 */
    s16* value;            /* 0x34: result cell (g_world_menu_selected_option); the number-entry threads edit it */
    s16 selected_index;    /* 0x38: selected row; indexes the binding's halfword table
                              (world_menu_announce_entry_value_thread) */
    s16 select_text_table; /* 0x3a: "Index for select text": row of g_world_menu_entry_text_id_tables
                              (0x11 = the binding's row_text_ids); 2 / -1 defer to the next running
                              thread's entry. The AT-list and option-list builders park their
                              scroll row offset here instead. */
} world_menu_entry_t;
typedef char world_menu_entry_size_must_be_0x3c[(sizeof(world_menu_entry_t) == 0x3c) ? 1 : -1];

/* Provisional: three-column menu text layout walked by world_menu_draw_text_columns (0x80109374) /
 * world_menu_draw_text_columns_2 (reached through world_menu_entry_t::text_binding when
 * that record is a layout rather than a binding). Column i uses x[i],
 * mode[i] (0 text, 1 number, 2 hidden, 3 alternate number), text_ids[i] and
 * text_colors[i], each indexed by row + row offset. */
typedef struct world_menu_text_layout {
    s16 row_count;       /* 0x00 */
    s16 hidden_rows;     /* 0x02: rows beyond the visible page (count - visible, 0 when
                            all fit); row_offset is clamped to it */
    s16 x[3];            /* 0x04 */
    s16 mode[3];         /* 0x0a */
    s16* text_ids[3];    /* 0x10 */
    s16* text_colors[3]; /* 0x1c */
} world_menu_text_layout_t;
typedef char world_menu_text_layout_size_must_be_0x28[sizeof(world_menu_text_layout_t) == 0x28 ? 1 : -1];

/* Provisional: one double-buffered menu sprite page (0xa0 bytes) built by
 * world_menu_build_sprite_page and its BATTLE twin battle_menu_build_sprite_page:
 * five DR_MODE texture-window packets followed by the five SPRTs they apply
 * to. Both modules keep two pages at 0x801cc074. */
typedef struct menu_sprite_page {
    DR_MODE modes[5]; /* 0x00 */
    SPRT sprites[5];  /* 0x3c */
} menu_sprite_page_t;
typedef char menu_sprite_page_size_must_be_0xa0[(sizeof(menu_sprite_page_t) == 0xa0) ? 1 : -1];

/* Provisional: per-line placement for the twelve menu box lines
 * (table at 0x80154cd0, indexed like world_menu_palette_primitives_t.lines):
 * signed endpoint offsets from the box origin and, per coordinate, a flag
 * selecting whether the box width/height is added. */
typedef struct world_menu_line_layout {
    s8 x0, y0, x1, y1;                 /* 0x00 */
    s8 add_w0, add_h0, add_w1, add_h1; /* 0x04 */
} world_menu_line_layout_t;

/* Icon packet head: two DR_MODE packets at +0x00/+0x0c and four SPRT packets
 * at +0x18 (CLUT ids at +0x26/+0x3a/+0x4e/+0x62). The submitter draws all
 * six packets. */
typedef struct world_menu_icon_sprites {
    DR_MODE first_draw_mode;
    DR_MODE draw_mode;
    SPRT sprites[4];
} world_menu_icon_sprites_t;
typedef char world_menu_icon_sprites_sprites_offset_must_be_0x18
    [((unsigned long)&((world_menu_icon_sprites_t*)0)->sprites == 0x18) ? 1 : -1];

/* Provisional: first thread parameter of the menu icon strip thread at
 * 0x80112704 (text image width/height at +0x04/+0x06, text id at +0x1c). */
typedef struct world_menu_icon_thread_param {
    s16 x;             /* 0x00; text image VRAM x; with y/width/height the upload rect (0x800ec108) */
    s16 y;             /* 0x02 */
    s16 width;         /* 0x04 */
    s16 height;        /* 0x06 */
    u16 icon_x;        /* 0x08; icon origin, see world_menu_update_icon_cursor_sprites */
    u16 flags;         /* 0x0a; icon y origin; bit 0 cleared by the scrolling icon list thread (0x8011241c) */
    s16 window_width;  /* 0x0c: menu record window size, set with width */
    s16 window_height; /* 0x0e */
    world_gfx_image_load_parameters_t text_image; /* 0x10; text sprite placement (0x800ec108) */
    s16 text_id;                                  /* 0x1c */
    s16 max_index; /* 0x1e; wrapping cursor bound, see world_menu_wrapping_cursor_bounds_t */
    u8 unknown_20[0x2c - 0x20];
    s16 icon_kind; /* 0x2c; 0x10 arrow, 0..4 single icon, 5..15 four-piece icon (0x800ec108) */
    u8 unknown_2e[2];
    s16* redraw_flag; /* 0x30; set to 1 to rebuild the text image */
    u8 unknown_34[4];
    s16 cursor; /* 0x38; -1 when unset */
} world_menu_icon_thread_param_t;

/* Six-byte option row selected by customized-options bits 9-11 in
 * 0x80106660. The first three bytes set the shared initial, primary and
 * secondary input-repeat timings. */
typedef struct world_input_timing_profile {
    u8 initial_delay;
    u8 repeat_period;
    u8 secondary_repeat_period;
    u8 scroll_accel_delay; /* copied to g_main_menu_scroll_accel_delay */
    u8 scroll_slow_step;   /* copied to g_main_menu_scroll_slow_step */
    u8 scroll_fast_step;   /* copied to g_main_menu_scroll_fast_step */
} world_input_timing_profile_t;

/* A text layout followed by the first visible row, as bound to a scrolling
 * list's world_menu_entry_t::text_binding (world_menu_scrolling_list_thread and the AT list
 * built by world_menu_build_ability_preview_at_list). */
typedef struct world_menu_scroll_text_layout {
    world_menu_text_layout_t columns; /* 0x00 */
    s16 row_offset;                   /* 0x28 */
} world_menu_scroll_text_layout_t;

/* Provisional: 12-byte textured-quad piece record (table at 0x80154e20)
 * consumed by the POLY_GT4 builder at 0x800e8d6c: texture origin, size and
 * screen position relative to g_world_menu_quad_origin. */
typedef struct world_menu_quad_piece {
    u8 u; /* 0x00 */
    u8 unknown_01;
    s8 v; /* 0x02: texture v + 0x78 */
    u8 unknown_03;
    s16 w; /* 0x04 */
    s16 h; /* 0x06 */
    s16 x; /* 0x08 */
    s16 y; /* 0x0a */
} world_menu_quad_piece_t;

/* Provisional: the four vertex colours applied by the POLY_GT4 builder at
 * 0x800e8d6c (record at 0x801557f4; the leading word is untyped). */
typedef struct world_menu_quad_colors {
    u32 unknown_00;
    CVECTOR vertex[4]; /* 0x04; one colour per POLY_GT4 corner */
} world_menu_quad_colors_t;

/* Provisional: screen origin added to every quad piece (0x80155808). */
typedef struct world_menu_quad_origin {
    s16 x; /* 0x00 */
    s16 y; /* 0x02 */
} world_menu_quad_origin_t;

/* Provisional: one 5-byte row of a WORLD menu text entry as rewritten by
 * world_menu_build_skillset_entries (0x800f3d44): the style byte at +1 is
 * 4 for a greyed row, the byte at +3 is the row separator (0xfa) or the
 * list terminator (0xff). */
typedef struct world_menu_text_row {
    u8 unknown_00;
    u8 style; /* 0x01 */
    u8 unknown_02;
    u8 terminator; /* 0x03: 0xfa continues, 0xff ends */
    u8 unknown_04;
} world_menu_text_row_t;

/* Provisional: the 0x7c-byte icon record world_menu_build_icon_record fills.
 * Its head is exactly world_menu_icon_sprites_t; the three borrowed icon
 * sprites at +0x68 and the two counters at +0x74/+0x78 are what that builder
 * adds. */
typedef struct world_menu_icon_record {
    world_menu_icon_sprites_t base; /* 0x00 */
    SPRT* icons[3];                 /* 0x68 */
    s32 unknown_74;                 /* 0x74 */
    s32 unknown_78;                 /* 0x78 */
} world_menu_icon_record_t;

/* Provisional: one of the two 0x134-byte packet pages a scrolling text list
 * alternates between frames (world_menu_scrolling_list_thread at 0x801a3d58,
 * world_build_at_list at 0x801ac6b4, world_build_at_list_2 at 0x801bfda4). */
typedef struct world_menu_list_page {
    SPRT frame;                                   /* 0x00: text image / cursor frame */
    SPRT arrows[2];                               /* 0x14: scroll arrows */
    SPRT arrow_marks[2];                          /* 0x3c */
    SPRT thumb;                                   /* 0x64: scroll-bar thumb */
    DR_MODE text_mode;                            /* 0x78 */
    DR_MODE icon_mode;                            /* 0x84 */
    world_menu_icon_record_t icons;               /* 0x90 */
    world_gfx_scaled_draw_area_pair_t draw_areas; /* 0x10c */
} world_menu_list_page_t;
typedef char world_menu_list_page_size_must_be_0x134[sizeof(world_menu_list_page_t) == 0x134 ? 1 : -1];

/* Text image placement: the upload rectangle plus the screen offsets
 * world_gfx_init_image_loading reads. */
typedef struct world_menu_text_window {
    RECT rect;
    s16 x_screen_offset;
    s16 y_screen_offset;
} world_menu_text_window_t;

/* Provisional: the double-buffered quad/sprite page at 0x80173cbc (0x144
 * bytes each): five Gouraud textured quads, two sprites and the two draw-mode
 * packets that select their texture windows. */
typedef struct world_menu_quad_page {
    POLY_GT4 quads[5]; /* 0x000 */
    SPRT sprites[2];   /* 0x104 */
    DR_MODE modes[2];  /* 0x12c */
} world_menu_quad_page_t;
typedef char world_menu_quad_page_size_must_be_0x144[sizeof(world_menu_quad_page_t) == 0x144 ? 1 : -1];

/* options */
/*
 * Live game-options word at 0x800473ac. The OPTION overlay unpacks these
 * fields in menu order. Toggle settings encode On as 0 and Off as 1; the
 * paired high bit participates in the menu's Customize/Initialize state.
 */
enum {
    GAME_OPTIONS_THREE_BIT_VALUE_MASK = 0x7,
    GAME_OPTIONS_TWO_BIT_VALUE_MASK = 0x3,
    GAME_OPTIONS_CURSOR_MOVEMENT_SHIFT = 0,
    GAME_OPTIONS_CURSOR_MOVEMENT_MASK = 0x00000007,
    GAME_OPTIONS_CURSOR_REPEAT_SPEED_SHIFT = 3,
    GAME_OPTIONS_CURSOR_REPEAT_SPEED_MASK = 0x00000038,
    GAME_OPTIONS_MULTI_HEIGHT_CURSOR_SPEED_SHIFT = 6,
    GAME_OPTIONS_MULTI_HEIGHT_CURSOR_SPEED_MASK = 0x000001c0,
    GAME_OPTIONS_FINGER_CURSOR_REPEAT_SPEED_SHIFT = 9,
    GAME_OPTIONS_FINGER_CURSOR_REPEAT_SPEED_MASK = 0x00000e00,
    GAME_OPTIONS_MESSAGE_DISPLAY_SPEED_SHIFT = 12,
    GAME_OPTIONS_MESSAGE_DISPLAY_SPEED_MASK = 0x00007000,
    GAME_OPTIONS_NAVIGATION_MESSAGES_SHIFT = 15,
    GAME_OPTIONS_NAVIGATION_MESSAGES_MASK = 0x00018000,
    GAME_OPTIONS_ABILITY_NAMES_SHIFT = 17,
    GAME_OPTIONS_ABILITY_NAMES_MASK = 0x00060000,
    GAME_OPTIONS_EFFECT_MESSAGES_SHIFT = 19,
    GAME_OPTIONS_EFFECT_MESSAGES_MASK = 0x00180000,
    GAME_OPTIONS_SOUND_MODE_SHIFT = 21,
    GAME_OPTIONS_SOUND_MODE_MASK = 0x00600000,
    GAME_OPTIONS_SHOW_UNEQUIPPABLE_ITEMS_SHIFT = 23,
    GAME_OPTIONS_SHOW_UNEQUIPPABLE_ITEMS_MASK = 0x01800000,
    GAME_OPTIONS_DISPLAY_GAINED_EXP_JP_SHIFT = 25,
    GAME_OPTIONS_DISPLAY_GAINED_EXP_JP_MASK = 0x06000000,
    GAME_OPTIONS_TARGET_FLASHING_SHIFT = 27,
    GAME_OPTIONS_TARGET_FLASHING_MASK = 0x18000000,
    GAME_OPTIONS_MAX_EQUIP_AT_JOB_CHANGE_SHIFT = 29,
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
    u32 unused_31 : 1;
} game_options_fields_t;

typedef union game_options {
    u32 value;
    game_options_fields_t fields;
} game_options_t;
typedef char game_options_size_must_be_4[sizeof(game_options_t) == 4 ? 1 : -1];

/* text */
/* Encoded menu-string controls. The 0xe2-0xfd meanings come from the dialog
 * interpreter branches (battle_text_character_handling_thread and its WORLD
 * twin) and, for 0xe7, the menu text-image renderers; codes with an operand
 * consume the following byte. F5/F6 are the {0xF5xx}{0xF6xx} "Alter String"
 * controls. The 0xe0-0xeb substitution codes are text_format_code_e. */
typedef enum text_control {
    TEXT_EXTENDED_GLYPH_PREFIX_FIRST = 0xd0,
    TEXT_EXTENDED_GLYPH_PREFIX_LAST = 0xdf,
    TEXT_SET_GLYPH_COLOR = 0xe2,     /* operand: glyph colour argument */
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

typedef enum menu_decimal_sign_flag {
    MENU_DECIMAL_SIGN_PLUS = 0x0400,
    MENU_DECIMAL_SIGN_MINUS = 0x0800,
} menu_decimal_sign_flag_e;

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

/* String-table bank bases used by text resolution and unit-name loading.
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
    TEXT_ID_SECTION_9000_BASE = 0x9000,
} text_id_base_e;

/* Substitution opcodes shared by battle messages and WORLD event text. */
typedef enum text_format_code {
    TEXT_FORMAT_FIRST = 0xe0,
    TEXT_FORMAT_RAMZA_NAME = 0xe0,
    TEXT_FORMAT_UNIT_NAME = 0xe1,
    TEXT_FORMAT_SECTION_9000 = 0xe5,
    TEXT_FORMAT_ITEM_NAME = 0xe9,
    TEXT_FORMAT_ABILITY_NAME = 0xea,
    TEXT_FORMAT_EXPLICIT_ID = 0xeb,
} text_format_code_e;

/* Provisional: text-stream cursor state shared by the two WORLD copies of
 * the back-reference cursor stepper (battle twin: 0x80130718). world_measure_text
 * keeps it in the scratchpad at 0x1f800000 and names the remaining fields. */
typedef struct world_text_backreference_state {
    s32 command;         /* 0x00; current text byte */
    s32 column;          /* 0x04; characters on the current line */
    s32 max_column;      /* 0x08; widest line so far */
    s32 row;             /* 0x0c; completed lines */
    s32 value;           /* 0x10; substitution value / text id */
    s32 scratch;         /* 0x14; intermediate byte-count / back-offset */
    s32 remaining_bytes; /* 0x18; bytes left before returning to return_cursor */
    u8* return_cursor;   /* 0x1c; cursor to resume at, -1 when none */
    u8 unknown_20[4];
    s32* value_cursor; /* 0x24; next substitution value */
    s32 pending_width; /* 0x28; explicit digit width from 0xE8, 0xFFFF when none */
    s32 glyph_page;    /* 0x2c; pending 0xDx page-prefix byte, 0 otherwise (world_measure_text_pixels) */
} world_text_backreference_state_t;

/* Dialog record handed to world_menu_render_text_image_at_record_origin;
 * the leading fields follow world_menu_text_image_t. */
typedef struct world_text_dialog_record {
    void* buffer;     /* 0x00 */
    u16 width;        /* 0x04 */
    u16 height;       /* 0x06 */
    RECT rect;        /* 0x08 */
    s32 text;         /* 0x10 */
    u16 color;        /* 0x14 */
    u16 dialog_type;  /* 0x16: DisplayMessage Dialog Type byte */
    u16 box_type;     /* 0x18: dialog_type & 0x70 */
    u16 position;     /* 0x1a */
    u16 options;      /* 0x1c */
    u16 no_box;       /* 0x1e */
    u16 line;         /* 0x20 */
    u16 first_line;   /* 0x22 */
    u16 last_line;    /* 0x24 */
    u16 arrow_x;      /* 0x26 */
    u16 arrow_y;      /* 0x28 */
    s16 unk_2a;       /* 0x2a */
    s16 mark;         /* 0x2c: measured width, later the 0xFB cursor mark */
    s16 lines;        /* 0x2e */
    s16 offset;       /* 0x30 */
    u8 unk_32[2];     /* 0x32 */
    s32 x;            /* 0x34 */
    s32 y;            /* 0x38 */
    s32 portrait;     /* 0x3c */
    u16 origin_x;     /* 0x40 */
    u16 origin_y;     /* 0x42 */
    s32 arrow_offset; /* 0x44: DisplayMessage arrow position, added to offset */
    s32 x_offset;     /* 0x48: DisplayMessage X coordinate, added to x */
    s32 y_offset;     /* 0x4c: DisplayMessage Y coordinate, added to y */
} world_text_dialog_record_t;

/* unit */
/* Provisional 0x2d0-byte double-buffered frame of the unit editor panel
 * (WORLD 0x801a3494, BATTLE 0x801730f8). The prefix is
 * battle_menu_status_panel_menu_primitives_t (0x80110260 fills the same draw modes, tiles
 * and lines); the sprites fill its unknown gap, and the trailing cursor
 * mode/sprite/window follow world_panel_frame_t. */
typedef struct world_unit_editor_frame {
    DR_MODE draw_modes[3];                    /* 0x000: image pages 0, 2 and 4 */
    SPRT sprites[24];                         /* 0x024: 19 panel sprites, then 5 item icons */
    TILE tiles[2];                            /* 0x204 */
    LINE_F2 lines[8];                         /* 0x224 */
    world_menu_palette_primitives_t* palette; /* 0x2a4 */
    DR_MODE cursor_mode;                      /* 0x2a8 */
    SPRT cursor;                              /* 0x2b4 */
    RECT cursor_texture_window;               /* 0x2c8 */
} world_unit_editor_frame_t;
typedef char world_unit_editor_frame_size_must_be_0x2d0[sizeof(world_unit_editor_frame_t) == 0x2d0 ? 1 : -1];

/* Colour and endpoint offsets of the unit editor panel's eight frame lines,
 * relative to the panel box, seven bytes per line (WORLD 0x80156198, BATTLE
 * 0x80168d0c). The loop walks the table with a byte offset stepping by 7 (the
 * target's induction variable and exit test), so the fields are addressed by
 * offset, as for world_menu_line_layout_t. */
enum world_unit_editor_line_layout_offset {
    WORLD_UNIT_EDITOR_LINE_R = 0,
    WORLD_UNIT_EDITOR_LINE_G = 1,
    WORLD_UNIT_EDITOR_LINE_B = 2,
    WORLD_UNIT_EDITOR_LINE_X0 = 3,
    WORLD_UNIT_EDITOR_LINE_Y0 = 4,
    WORLD_UNIT_EDITOR_LINE_X1 = 5,
    WORLD_UNIT_EDITOR_LINE_Y1 = 6,
    WORLD_UNIT_EDITOR_LINE_SIZE = 7
};

/* Provisional view of the edited unit's panel data (WORLD 0x8013a392, BATTLE
 * 0x8014d0ae). The target addresses the flag and the id table from one base
 * register, so they are one object. */
typedef struct world_unit_editor_panel_data {
    s16 flag;   /* 0x00: selects label texts and sprite layout */
    s16 layout; /* 0x02: selects the parameter rows copied below */
    u8 unknown_04[0x12];
    s16 item_ids[5];       /* 0x16; -1 for an empty slot */
    s16 label_text_ids[5]; /* 0x20; -1 for an empty row */
} world_unit_editor_panel_data_t;
typedef char world_unit_editor_panel_data_item_ids_offset_must_be_0x16
    [((unsigned long)&((world_unit_editor_panel_data_t*)0)->item_ids == 0x16) ? 1 : -1];

/* Provisional 0x2ec-byte double-buffered page of the unit status panel
 * (pairs at 0x801a25f8, first page per mode from g_world_unit_status_first_page_by_mode). */
typedef struct world_unit_status_page {
    POLY_G4 bars[3];                         /* 0x000: HP/MP/CT gauges */
    DR_MODE draw_mode_1;                     /* 0x06c */
    DR_MODE draw_mode_0;                     /* 0x078 */
    world_menu_palette_primitives_t palette; /* 0x084 */
    POLY_FT4 quads[5];                       /* 0x170 */
    SPRT sprites[7];                         /* 0x238 */
    POLY_FT4 portrait;                       /* 0x2c4 */
} world_unit_status_page_t;

/* Provisional 0xF0-byte double-buffered frame of the unit summary panel. */
typedef struct world_unit_summary_frame {
    SPRT sprites[7];                          /* 0x00: [0] name/job, [1]-[3] numbers, [6] zodiac */
    DR_MODE draw_mode_0;                      /* 0x8c */
    DR_MODE draw_mode_1;                      /* 0x98 */
    world_menu_palette_primitives_t* palette; /* 0xa4 */
    u32 draw_offset_a[3];                     /* 0xa8 */
    u32 draw_offset_b[3];                     /* 0xb4 */
    s16 offset_a[2];                          /* 0xc0 */
    s16 offset_b[2];                          /* 0xc4 */
    POLY_FT4 portrait;                        /* 0xc8 */
} world_unit_summary_frame_t;

/* Provisional: 0x30-byte unit-view thread set at 0x801564d0, indexed by the
 * view mode. Each of the four slots names a thread entry (or -1), the
 * scheduler slot it runs in, and the task id it must carry. */
typedef struct world_unit_view_thread_set {
    void (*entries[4])(void); /* 0x00 */
    s32 thread_ids[4];        /* 0x10 */
    s32 task_ids[4];          /* 0x20 */
} world_unit_view_thread_set_t;
typedef char world_unit_view_thread_set_size_must_be_0x30[(sizeof(world_unit_view_thread_set_t) == 0x30) ? 1 : -1];

/* other */
/* One selectable save-file icon: 16-colour CLUT plus 16x16 4bpp image. */
typedef struct card_save_icon {
    u8 clut[0x20];  /* 0x00 */
    u8 image[0x80]; /* 0x20 */
} card_save_icon_t;

#endif
