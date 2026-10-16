#ifndef MENU_STATUS_H
#define MENU_STATUS_H

/* Unit status records: what the status panels and previews show of a unit (identity, stats, equipment and
 * abilities, gauges), the selected and comparison units, and the equipment slot order. */

#include "fft/map.h"
#include "psx/types.h"

struct unit_stats;

/* equipment slots */
/* The five equipment slots in the order the equipment screens list them (not
 * the order of a unit record's equipment array). */
typedef enum menu_equipment_slot {
    MENU_EQUIPMENT_SLOT_RIGHT_HAND = 0,
    MENU_EQUIPMENT_SLOT_LEFT_HAND = 1,
    MENU_EQUIPMENT_SLOT_HEAD = 2,
    MENU_EQUIPMENT_SLOT_BODY = 3,
    MENU_EQUIPMENT_SLOT_ACCESSORY = 4,
    MENU_EQUIPMENT_SLOT_HAND_COUNT = 2,
    MENU_EQUIPMENT_SLOT_COUNT = 5,
} menu_equipment_slot_e;

/* unit status records */
/* The unit a status panel shows (g_menu_selected_unit; DEBUGCHR's g_unit_panel_selected_unit_data, ATTACK's
 * g_unit_panel_selected_unit_data), saved and restored whole by menu_copy_billboard_data_out and
 * menu_copy_billboard_data_in. menu_load_selected_unit fills job_id..zodiac for the unit named by battle_id; the
 * overlays' status panels keep a second record for the comparison unit (g_menu_comparison_unit_data).
 * Provisional. */
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

/* Name, job and identity values copied into WORLD and EQUIP status panels.
 * The last displayed value is the roster slot, distinct from a battle id. */
typedef struct menu_unit_status_identity {
    s16 unit_index;  /* 0x00: name text 0x4000 + value (menu_run_unit_status_thread) */
    s16 job_id;      /* 0x02: job name text 0x3000 + value */
    s16 brave;       /* 0x04 */
    s16 faith;       /* 0x06 */
    s16 zodiac;      /* 0x08 */
    s16 roster_slot; /* 0x0a: party slot (formation_get_selected_unit_name_id); shown as value + 1 */
    u8 _unused_0c[2];
} menu_unit_status_identity_t;
typedef char menu_unit_status_identity_size_must_be_14[sizeof(menu_unit_status_identity_t) == 14 ? 1 : -1];

/* u16 view of the -1 stored in an empty menu_status_panel_slot_state_t.ability_ids row. */
enum {
    MENU_STATUS_PANEL_LABEL_NONE = 0xffff,
};

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

/* Complete status and preview record, copied whole by the billboard routines. */
typedef struct menu_status_panel_slot_storage {
    menu_status_panel_slot_state_t state;
    u8 _unused_38[8]; /* preserves the copied 0x40-byte record */
} menu_status_panel_slot_storage_t;
typedef char menu_status_panel_slot_storage_size_must_be_64[(sizeof(menu_status_panel_slot_storage_t) == 64) ? 1 : -1];

enum {
    MENU_UNIT_STATUS_GAUGE_HP = 0,
    MENU_UNIT_STATUS_GAUGE_MP = 1,
    MENU_UNIT_STATUS_GAUGE_CT = 2,
};

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

/* A unit's status summary: g_menu_selected_unit_status (BATTLE's AT banner) and g_menu_comparison_unit_status
 * (its status billboard), WORLD's formation records g_menu_selected_unit_stat_summary and
 * g_formation_comparison_unit_stat_summary, and each overlay's pair of panel billboards, each pair 0x24 apart.
 * menu_copy_unit_data_to_status_billboard and unit_panel_build_unit_data fill the first 0x1e bytes,
 * menu_run_unit_stats_panel and the overlays' menu_run_unit_status_thread draw it, and
 * menu_copy_billboard_data_out saves the banner whole. Equipment previews use the HP and MP gauges' bonus
 * fields. Provisional. */
typedef struct menu_unit_status_record {
    s16 level;      /* 0x00; unit_editor_apply_values writes it back */
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

/* selected and comparison units */
/* The selected panel unit can differ from the unit whose turn is active. Its record, status summary and
 * equipment and ability rows come first, then the comparison unit's. */
extern menu_active_unit_t g_menu_selected_unit;
extern menu_unit_status_record_t g_menu_selected_unit_status;
extern menu_status_panel_slot_storage_t g_menu_selected_unit_fields;
extern menu_unit_status_record_t g_menu_selected_unit_stat_summary;
/* The unit shown beside the selected one in the comparison panels; REQUIRE
 * holds the same record. */
extern menu_active_unit_t g_menu_comparison_unit_data;
extern menu_unit_status_record_t g_menu_comparison_unit_status;
/* Target preview copies only generic_monster and the reaction ability here. */
extern menu_status_panel_slot_storage_t g_menu_comparison_unit_fields;
/* The selected unit's names, as menu_load_selected_unit copies them for the panels. */
extern u8 g_menu_billboard_unit_name[];               /* name text window */
extern u8 g_menu_billboard_job_name[];                /* job name text window */
extern u8 g_menu_billboard_skillset_name[];           /* primary skillset name text window */
extern u8 g_menu_billboard_secondary_skillset_name[]; /* secondary skillset name text window */

/* The battle unit stats of g_menu_selected_unit. */
struct unit_stats* menu_get_selected_unit_stats(void);
/* Fills the selected and comparison records, stats, equipment and ability names. */
void menu_load_selected_unit(void);

/* billboards and previews */
/* Copies the selected unit's records and the cursor tile into the caller's buffers; _in restores them without
 * the tile. */
void menu_copy_billboard_data_out(menu_unit_status_record_t* banner, menu_active_unit_t* unit,
    menu_status_panel_slot_storage_t* billboard, map_selected_tile_t* cursor_tile);
void menu_copy_billboard_data_in(const menu_unit_status_record_t* banner, const menu_active_unit_t* unit,
    const menu_status_panel_slot_storage_t* billboard);
/* Clears the HP and MP preview bonuses and the equipment-detail stats. */
void menu_clear_stat_preview(menu_unit_status_record_t* summary, menu_status_panel_slot_storage_t* detail);
/* The stat changes of swapping item_before for item_after in a slot: the HP and MP bonus differences into delta
 * and the equipment-detail differences into output. */
void menu_calculate_swap_stat_delta(menu_status_panel_slot_storage_t* output, menu_unit_status_record_t* delta,
    s16 item_before, s16 item_after, s32 slot);
/* Row callbacks: the previewed HP or MP bonus, or MENU_NUMBER_VALUE_DASHES when it is 0. */
s32 menu_get_preview_hp_bonus_display(s32 unused_row);
s32 menu_get_preview_mp_bonus_display(s32 unused_row);

/* unit grid readouts */
/* What the unit grids draw below a unit (formation_draw_unit_readout,
 * unit_list_update_grid). L2/R2 step g_formation_stat_display_mode and
 * g_unit_list_stat_mode through the first five, wrapping between HP and
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

#endif
