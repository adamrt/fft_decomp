#ifndef EQUIPMENT_H
#define EQUIPMENT_H

/* Equip: EQUIP's battle Equip screen, and the equipment previews, item list rows and item icons it shares with
 * Formation's item menu. */

#include "fft/menu.h"
#include "fft/unit_panel.h"
#include "psx/libgpu.h"
#include "psx/types.h"

struct menu_gfx_context;

/* screen */
/* g_equipment_screen_state: the index of the screen's handler in
 * g_equipment_screen_state_handlers. The command menu enters its picked row
 * + EQUIP: Equip, Remove and the item list are rows 0, 2 and 3 (row 1 is
 * refused). */
typedef enum equipment_state {
    EQUIPMENT_STATE_INIT = 0, /* equip_entrypoint builds the screen */
    EQUIPMENT_STATE_COMMAND_MENU = 1,
    EQUIPMENT_STATE_EQUIP = 2,     /* equip_update */
    EQUIPMENT_STATE_REMOVE = 4,    /* remove_update */
    EQUIPMENT_STATE_ITEM_LIST = 5, /* formation_items_update */
    EQUIPMENT_STATE_CLOSED = 0xff  /* equip_entrypoint returns its result */
} equipment_state_e;

extern u8 g_equipment_screen_state; /* equipment_state_e */
extern void (*g_equipment_screen_state_handlers[])(void);
extern s16 g_equipment_result_rows[4]; /* [2]: type-order row picked in the sub-menu, or -1 */

void equipment_thread(void);

/* Advance EQUIP one frame; return its ordering table while open, then the equipment-change result. */
s32 equip_entrypoint(s32 unit, u32* otag);
void equipment_init(s32 battle_id);
void equipment_reset_selection(void);

/* unit preview */
/* What equipment_validate_slot_item finds for an item in an equipment slot. */
typedef enum equipment_slot_result {
    EQUIPMENT_SLOT_ITEM_CURRENT_LOCKED = -2,
    EQUIPMENT_SLOT_ITEM_CANNOT_EQUIP = -1,
    EQUIPMENT_SLOT_ITEM_INCOMPATIBLE_PAIR = 0,
    EQUIPMENT_SLOT_ITEM_ALLOWED = 1
} equipment_slot_result_e;

/* EQUIP.OUT's 0x7A-byte unit preview, built from unit_stats_t by
 * equipment_build_unit_preview. Field names follow the unit_stats_t
 * source; the first 0x1E bytes share the unit-status-billboard layout. */
typedef struct equipment_unit_preview {
    s16 level;                            /* 0x00; capped at 99 */
    s16 team_kind;                        /* 0x02; 1 enemy, 2 neutral */
    s16 list_index;                       /* 0x04; AT-list lookup + 1, or -1 */
    s16 unit_count;                       /* 0x06; units still present, set by equipment_load_unit */
    s16 experience;                       /* 0x08; capped at 99 */
    s16 unit_index;                       /* 0x0a; unit_stats_t.active_battle_id */
    menu_unit_status_gauge_t gauges[3];   /* 0x0c; HP, MP and CT */
    s16 battle_id;                        /* 0x1e */
    s16 _unused_20;                       /* 0x20 */
    menu_unit_status_identity_t identity; /* 0x22 */
    s16 move;                             /* 0x30 */
    s16 speed;                            /* 0x32 */
    s16 jump;                             /* 0x34 */
    s16 right_weapon_power;               /* 0x36 */
    s16 left_weapon_power;                /* 0x38 */
    s16 right_weapon_evade;               /* 0x3a */
    s16 left_weapon_evade;                /* 0x3c */
    s16 _unused_3e;                       /* 0x3e; monster-skillset flag slot */
    s16 two_handing;                      /* 0x40; unit_is_two_handing_weapon result */
    s16 physical_attack;                  /* 0x42 */
    s16 physical_class_evade;             /* 0x44 */
    s16 physical_shield_evade;            /* 0x46 */
    s16 _unused_48;                       /* 0x48; accessory evade slot */
    s16 _unused_4a;                       /* 0x4a */
    s16 magic_attack;                     /* 0x4c */
    s16 magical_class_evade;              /* 0x4e; cleared (lower billboard M-C-Ev) */
    s16 magical_shield_evade;             /* 0x50 */
    s16 magical_accessory_evade;          /* 0x52 */
    u16 equipment[5];                     /* 0x54; menu_equipment_slot_e order */
    u8 _unused_5e[0x12];                  /* 0x5e; ability ids and the spare tail of the stat-detail block, left zero */
    u8 equipment_categories[4];           /* 0x70 */
    u8 support_abilities[4];              /* 0x74; copy of unit_stats_t.ability_flags.support */
    u8 party_index;                       /* 0x78; unit_stats_t.formation_index */
    u8 _padding_79;                       /* tail padding to 2-byte alignment */
} equipment_unit_preview_t;
typedef char equipment_unit_preview_size_must_be_122[(sizeof(equipment_unit_preview_t) == 122) ? 1 : -1];

extern u8 g_equipment_unit_selected_index;

/* Provisional: the 0x7A-byte preview records for the selected unit (index 0)
 * and its compare slot (index 1); several callers view one as raw bytes. */
extern equipment_unit_preview_t* g_equipment_unit_data[2];

void equipment_load_unit(s32 battle_id);
void equipment_load_selected_unit(void);
void equipment_copy_unit_to_comparison(s32 unused_selected_index, s32 unused_1);
s32 equipment_set_slot_item(s16 unit_index, s16 slot, s32 item_id);
void equipment_commit_loadout(void);

/* EQUIP and WORLD each build their own body. */
s32 equipment_item_compatibility_check(s16 unit_index, s16 item_id);

/* equipment slots */
extern u16 g_equipment_remove_all_equipment_ids[5]; /* weapon, shield, head, body, accessory */
extern s8 g_equipment_remove_all_equipment_prompt_active;
extern s16 g_equipment_remove_item_equipment[5];
extern s8 g_equipment_remove_item_previewed_slot;

void equipment_draw_slot_marker(s32 unused, s32 slot, s32 override);

/* stat previews */
s32 equipment_get_preview_hp_bonus(s32 unused_row);
s32 equipment_get_preview_mp_bonus(s32 unused_row);

/* Row callbacks: the previewed HP or MP bonus, or MENU_NUMBER_VALUE_DASHES when it is 0. */
s32 equipment_get_preview_hp_bonus_display(s32 unused_row);
s32 equipment_get_preview_mp_bonus_display(s32 unused_row);

/* Clears the HP and MP preview bonuses and the equipment-detail stats. */
void equipment_stat_preview_clear(unit_panel_summary_t* summary, unit_panel_record_t* detail);

/* Sums weapon power and equipment evade from five items; other detail fields are unchanged. */
void equipment_weapon_evade_totals_build(unit_panel_record_t* total, const u16* equipment);

/* The stat changes of swapping item_before for item_after in a slot: the HP and MP bonus differences into delta
 * and the equipment-detail differences into output. */
void equipment_swap_delta_calculate(
    unit_panel_record_t* output, unit_panel_summary_t* delta, s16 item_before, s16 item_after, s32 slot);

/* Sums the stat changes of swapping each of the five slots from from_items to to_items. */
void equipment_loadout_delta_calculate(
    unit_panel_record_t* detail_total, unit_panel_summary_t* summary_total, const u16* from_items, const u16* to_items);

/* item lists */
extern u8 g_equipment_item_inventory_totals[];

s32 equipment_build_item_list(s16 unit_index, s16 sort_mode, s8 category, s16* list, u8 equip_filter);

/* EQUIP and WORLD each build their own bodies of these two. */
s32 formation_items_update(void);
s32 formation_items_browse_update(void);

/* Row callbacks of the item lists. */
menu_icon_source_t* equipment_get_row_icon_source(s32 row);
s32 equipment_get_row_item_count(s32 row);
s32 equipment_get_row_equipped_count(s32 row);

/* item actions */
s32 equipment_update_item_actions(void);

/* item type order */
s32 equipment_type_order_update(void);

/* unit panels */
extern menu_unit_status_identity_t g_equipment_selected_unit_data;
extern unit_panel_thread_t g_equipment_panel_params;

/* graphics */
extern u32* g_equipment_gfx_current_ot;
extern u16 g_equipment_gfx_draw_area_capacity;
extern u16 g_equipment_gfx_draw_move_capacity;

/* LINE_G2 and LINE_G3 capacities; volatile keeps their load order in equipment_gfx_init_contexts. */
extern volatile u16 g_equipment_gfx_line_g2_capacity;
extern volatile u16 g_equipment_gfx_line_g3_capacity;

void equipment_gfx_init_contexts(struct menu_gfx_context* base, u32* otag, POLY_F3* poly_f3, POLY_FT3* poly_ft3,
    POLY_F4* poly_f4, POLY_FT4* textured_quads, POLY_G3* poly_g3, POLY_GT3* poly_gt3, POLY_G4* poly_g4,
    POLY_GT4* poly_gt4, LINE_F2* line_f2, LINE_F3* line_f3, LINE_F4* line_f4, LINE_G2* line_g2, LINE_G3* line_g3,
    LINE_G4* line_g4, TILE* tiles, TILE_1* tiles_1, TILE_8* tiles_8, TILE_16* tiles_16, SPRT* sprites,
    SPRT_8* sprites_8, SPRT_16* sprites_16, DR_MOVE* draw_moves, DR_AREA* draw_areas, DR_MODE* draw_modes);
void equipment_gfx_swap_context(s32 first_otag, s32 unused);

/* input */
s32 equipment_input_read_page_scroll_direction(void);

/* menu commands */
/* The opcode that ends an EQUIP menu display stream. */
enum { EQUIPMENT_SCRIPT_END = 0x19 };

/* messages and sound */
extern u8 g_equipment_sound_suppress_queued;

void equipment_show_message(s32 message_id, s32 sound_id);

/* threads */
/* Slots of the equip screen's own threads. The WORLD formation screen runs
 * the same threads in other slots (screen_thread_slot_e and
 * thread_screen_slot_e); here BATTLE's suspended menu threads stay in 7 and 8. */
typedef enum equipment_thread_slot {
    EQUIPMENT_THREAD_SLOT_POPUP_MENU = 3,         /* the item action and type order menus */
    EQUIPMENT_THREAD_SLOT_COMMAND_MENU_CHILD = 4, /* a wait-forever thread while a command's mode runs */
    EQUIPMENT_THREAD_SLOT_COMMAND_MENU = 5,       /* menu_icon_list_thread */
    EQUIPMENT_THREAD_SLOT_EQUIPMENT_LIST = 10,    /* unit_panel_equipment_thread */
    EQUIPMENT_THREAD_SLOT_ITEM_NUMERIC = 11,      /* the numeric panel thread of the item panel */
    EQUIPMENT_THREAD_SLOT_UNIT_STATUS = 13,       /* unit_panel_banner_thread */
    EQUIPMENT_THREAD_SLOT_COMPARISON_STATUS = 14  /* the second banner slot the indicator draw watches */
} equipment_thread_slot_e;

extern s16 g_equipment_text_message_thread_active;
extern s32 g_equipment_item_numeric_thread_enabled;

#endif
