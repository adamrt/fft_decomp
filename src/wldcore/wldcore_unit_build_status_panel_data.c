#include "fft/wldcore.h"

/* Provisional: gauge view of g_world_selected_unit_stat_summary, laid out as
 * attack_status_gauges_t. */
typedef struct wldcore_unit_status_gauges {
    s16 level;      /* 0x00 */
    s16 team_state; /* 0x02 */
    s16 list_index; /* 0x04; world_unit_status_billboard_t twin */
    s16 unit_count; /* 0x06 */
    s16 experience; /* 0x08 */
    s16 unit_index; /* 0x0a */
    u16 hp;         /* 0x0c */
    s16 hp_bonus;   /* 0x0e */
    u16 max_hp;     /* 0x10 */
    u16 mp;         /* 0x12 */
    s16 mp_bonus;   /* 0x14 */
    u16 max_mp;     /* 0x16 */
    s16 ct;         /* 0x18 */
    s16 unknown_1a;
    s16 max_ct; /* 0x1c */
} wldcore_unit_status_gauges_t;

/* Provisional: s16 view of the 0x40-byte stat detail record g_world_selected_unit_stat_detail. */
typedef struct wldcore_unit_status_detail {
    s16 move;
    s16 speed;
    s16 jump;
    s16 right_weapon_power; /* 0x06 */
    s16 left_weapon_power;  /* 0x08 */
    s16 right_weapon_evade; /* 0x0a */
    s16 left_weapon_evade;  /* 0x0c */
    s16 monster;            /* 0x0e; unit_flags & 0x20 */
    s16 unknown_10;
    s16 physical_attack;          /* 0x12 */
    s16 physical_class_evade;     /* 0x14 */
    s16 physical_shield_evade;    /* 0x16 */
    s16 physical_accessory_evade; /* 0x18 */
    s16 unknown_1a;
    s16 magic_attack;            /* 0x1c */
    s16 magical_class_evade;     /* 0x1e */
    s16 magical_shield_evade;    /* 0x20 */
    s16 magical_accessory_evade; /* 0x22 */
    s16 equipment[5];            /* 0x24 */
    s16 abilities[5];            /* 0x2e */
    u8 unknown_38[8];
} wldcore_unit_status_detail_t;

/* Loads party member party_index into the scratch unit at 0x800d467c and
 * fills the status-panel records the participant detail level draws.
 *
 * Empty hand slots fall back to the shield; empty equipment reads as -1 and
 * a zero ability id is decremented to -1. Monsters show their primary
 * skillset's first five abilities instead of their equipped ability set. */
void wldcore_unit_build_status_panel_data(s32 party_index) {
    battle_stats_t* unit;
    wldcore_unit_status_gauges_t* gauges;
    world_unit_status_identity_t* identity;
    wldcore_unit_status_detail_t* detail;
    u16* abilities;
    s32 i;
    u8 flags;
    u16 maximum_hp;
    u16 maximum_mp;

    if (main_unit_init_job_data(&g_wldcore_status_unit, party_index, 1) == -1) {
        return;
    }
    unit = &g_wldcore_status_unit;
    gauges = (wldcore_unit_status_gauges_t*)&g_world_selected_unit_stat_summary;
    identity = &g_world_selected_unit_identity;
    detail = (wldcore_unit_status_detail_t*)&g_world_selected_unit_stat_detail;
    gauges->team_state = 0;
    gauges->level = unit->level;
    flags = unit->team_flags;
    if (flags & 0x30) {
        gauges->team_state = 1;
    }
    if (!(flags & 0x38)) {
        gauges->team_state = 2;
    }
    gauges->list_index = 0;
    gauges->unit_count = 0;
    gauges->experience = unit->experience;
    maximum_hp = unit->max_hp;
    gauges->max_hp = maximum_hp;
    if (maximum_hp == 0) {
        gauges->max_hp = maximum_hp + 1;
    }
    gauges->hp_bonus = 0;
    gauges->hp = unit->hp;
    maximum_mp = unit->max_mp;
    gauges->max_mp = maximum_mp;
    if (maximum_mp == 0) {
        gauges->max_mp = maximum_mp + 1;
    }
    gauges->max_ct = 100;
    gauges->mp = unit->mp;
    gauges->ct = unit->ct;
    identity->job_id = unit->job_id;
    identity->brave = unit->brave;
    gauges->mp_bonus = 0;
    gauges->unit_index = party_index;
    identity->unit_index = party_index;
    identity->roster_slot = party_index;
    identity->faith = unit->faith;
    identity->zodiac = unit->birthday.fields.zodiac;
    detail->move = unit->move;
    detail->speed = unit->attributes[UNIT_ATTRIBUTE_SPEED];
    detail->jump = unit->jump;
    detail->right_weapon_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE];
    detail->left_weapon_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE];
    detail->right_weapon_power = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER];
    detail->left_weapon_power = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER];
    detail->physical_attack = unit->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    detail->physical_class_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE];
    detail->physical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE];
    if (unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE]
        <= unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE]) {
        detail->physical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE];
    }
    detail->physical_accessory_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE];
    detail->magic_attack = unit->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    detail->magical_class_evade = 0;
    detail->magical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE];
    if (unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE]
        <= unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE]) {
        detail->magical_shield_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE];
    }
    detail->magical_accessory_evade = unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE];
    detail->equipment[0] = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE
        ? unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]
        : unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD];
    detail->equipment[1] = unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] != ITEM_ID_NONE
        ? unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]
        : unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD];
    detail->equipment[2] = unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD];
    detail->equipment[3] = unit->equipment[UNIT_EQUIPMENT_SLOT_BODY];
    detail->equipment[4] = unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY];
    for (i = 0; i < 5; i++) {
        if (detail->equipment[i] == 0 || detail->equipment[i] == ITEM_ID_NONE) {
            detail->equipment[i] = -1;
        }
    }
    detail->monster = unit->unit_flags & UNIT_FLAG_MONSTER;
    if (detail->monster != 0) {
        abilities = main_ability_store_skillset_abilities(unit->primary_skillset, 1);
        for (i = 0; i < 5; i++) {
            detail->abilities[i] = *abilities++;
        }
    } else {
        detail->abilities[0] = unit->primary_skillset;
        detail->abilities[1] = unit->secondary_skillset;
        detail->abilities[2] = unit->reaction_ability;
        detail->abilities[3] = unit->support_ability;
        detail->abilities[4] = unit->movement_ability;
    }
    for (i = 0; i < 5; i++) {
        if (detail->abilities[i] == 0) {
            detail->abilities[i]--;
        }
    }
}
