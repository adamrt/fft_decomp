#include "fft/attack.h"
#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "fft/script_variables.h"

/* This overlay reads BATTLE's attack entry mode unsigned (lhu). */
#define ENTRY_MODE (*(u16*)&g_battle_script_attack_entry_mode)

/* 0x18-byte scenario record in the ATTACK.OUT data at 0x801cf938.
 * Halfwords are stored little-endian and unaligned. */
typedef struct attack_scenario {
    u8 id[2];            /* 0x00 */
    u8 map_id;           /* 0x02 */
    u8 weather;          /* 0x03 */
    u8 daytime;          /* 0x04 */
    u8 music_1;          /* 0x05 */
    u8 music_2;          /* 0x06 */
    u8 entd_id[2];       /* 0x07 */
    u8 squad_1[2];       /* 0x09 */
    u8 squad_2[2];       /* 0x0b */
    u8 portrait_id;      /* 0x0d */
    u8 unit_id;          /* 0x0e */
    u8 story_entd_id[2]; /* 0x0f */
    u8 ramza_mandatory;  /* 0x11 */
    u8 next_scenario[2]; /* 0x12 */
    u8 after_event[2];   /* 0x14 */
    u8 event_script[2];  /* 0x16 */
} attack_scenario_t;

/*
 * Advances the scenario flags and loads the current scenario's battle setup.
 *
 * In entry mode 2 only the special portrait and unit of the pending scenario
 * are looked up; that path returns without a value, like the target (v0 keeps
 * whatever the last expression left). Otherwise the event id is advanced, the
 * finish-operation table is rebuilt from every scenario record, and the
 * current scenario's map, music, ENTD, squads and condition script are copied
 * into the script variables. Returns 1 when the scenario has no first squad.
 */
s32 attack_load_scenario_conditionals(void) {
    /* sp+0x10 squad 1, sp+0x14 squad 2, sp+0x1c story ENTD; [2] is unused.
     * The story ENTD lives in the array: as a scalar it would take a
     * callee-saved register instead of the target's stack slot. */
    s32 squads[4];
    attack_scenario_t* scenario;
    s32 i;
    s32 id;
    s32 value;
    s32 event;
    s32 map_id;
    s32 weather;
    s32 daytime;
    s32 entd;
    s32 ramza;
    s32 script;
    s16* conditions;
    u16* base;
    s32 first;
    s32 count;

    if (battle_script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) != 0) {
        if (battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == 8) {
            battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 9);
            battle_script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 0);
            g_battle_script_attack_entry_mode = 1;
            g_main_system_go_straight_to_battle = 0;
        }
    }
    if (ENTRY_MODE == 2) {
        g_main_special_portrait_wldface_id = 0;
        g_main_special_portrait_unit_id = 0;
        value = g_scenario_event_finish_operations[battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT)];
        if (value == 0) {
            return;
        }
        event = ((value & 0xc00) >> 2) + (value & 0xff);
        scenario = (attack_scenario_t*)g_attack_scenario_table;
        for (i = 0; i < 500; i++) {
            id = scenario->id[0] + (scenario->id[1] << 8);
            if (id == event || id == 0) {
                break;
            }
            scenario++;
        }
        if (id == 0) {
            return;
        }
        g_main_special_portrait_wldface_id = scenario->portrait_id;
        g_main_special_portrait_unit_id = scenario->unit_id;
        return;
    }
    g_battle_scenario_event_active = 0;
    for (i = 0x70; i < 0x90; i++) {
        battle_script_set_variable(i, 0);
    }
    battle_script_set_variable(EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS, 1);
    if (battle_script_get_variable(EVENT_SCRIPT_VAR_OLAN_SAW_RAMZA_FUNERAL) != 0
        && battle_script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) != 0
        && battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == 0x12a) {
        battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x12b);
    } else if (battle_script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) != 0) {
        id = battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
        if (id == 0x5a) {
            battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x74);
        } else if (id == 0x12a) {
            battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x148);
        } else {
            battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, id + 1);
        }
    } else if (g_main_system_go_straight_to_battle != 0) {
        battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 1);
        for (i = 1; i < 20; i++) {
            main_party_remove_unit(i);
        }
    }
    g_main_system_go_straight_to_battle = 0;
    battle_script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 0);
    event = battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
    scenario = (attack_scenario_t*)g_attack_scenario_table;
    for (i = 0; i < 500; i++) {
        id = scenario->id[0] + (scenario->id[1] << 8);
        if (id == 0) {
            break;
        }
        g_scenario_event_finish_operations[id]
            = (scenario->after_event[0] << 8) | ((scenario->next_scenario[1] << 10) + scenario->next_scenario[0]);
        scenario++;
    }
    g_scenario_event_finish_operations[0x192] = 0x8000;
    g_scenario_event_finish_operations[0x191] = 0;
    g_scenario_event_finish_operations[0x193] = 0;
    if (battle_script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
        g_scenario_event_finish_operations[0x190] = 0x8000;
        g_scenario_event_finish_operations[0x194] = 0x8000;
    } else {
        g_scenario_event_finish_operations[0x190] = 0x8200;
        g_scenario_event_finish_operations[0x194] = 0x8200;
    }
    scenario = (attack_scenario_t*)g_attack_scenario_table;
    for (i = 0; i < 500; i++) {
        id = scenario->id[0] + (scenario->id[1] << 8);
        if (id == event || id == 0) {
            break;
        }
        scenario++;
    }
    if (id == 0) {
        battle_script_set_variable(EVENT_SCRIPT_VAR_MAP_ARRANGEMENT, 0);
        battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
        return 0;
    }
    g_battle_scenario_event_active++;
    attack_load_unaligned_s16(scenario->id);
    map_id = scenario->map_id;
    weather = scenario->weather;
    daytime = scenario->daytime;
    g_battle_scenario_music_1_id = scenario->music_1;
    g_battle_scenario_music_2_id = scenario->music_2;
    entd = attack_load_unaligned_s16(scenario->entd_id);
    squads[0] = attack_load_unaligned_s16(scenario->squad_1);
    squads[1] = attack_load_unaligned_s16(scenario->squad_2);
    g_main_special_portrait_wldface_id = scenario->portrait_id;
    g_main_special_portrait_unit_id = scenario->unit_id;
    squads[3] = attack_load_unaligned_s16(scenario->story_entd_id);
    ramza = scenario->ramza_mandatory;
    attack_load_unaligned_s16(scenario->next_scenario);
    attack_load_unaligned_s16(scenario->after_event);
    script = attack_load_unaligned_s16(scenario->event_script);
    base = (u16*)g_attack_event_condition_blocks;
    conditions = (s16*)(base + (s16)((u16*)g_attack_event_condition_blocks)[script] / 2);
    first = *conditions;
    for (i = 0; i < 16; i++) {
        g_main_scenario_condition_offsets[i] = *conditions - first;
        if (*conditions == 0) {
            g_main_scenario_condition_offsets[i] = 0;
            break;
        }
        conditions++;
    }
    for (i = 0; i < 256; i++) {
        g_main_scenario_condition_data[i] = (base + ((s16)g_main_scenario_condition_offsets[0] + first) / 2)[i];
    }
    count = 0;
    for (i = 0; i < 2; i++) {
        if (squads[i] != 0) {
            count++;
        }
    }
    battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_MAP, map_id);
    battle_script_set_variable(EVENT_SCRIPT_VAR_MAP_ARRANGEMENT, weather >> 4);
    battle_script_set_variable(EVENT_SCRIPT_VAR_WEATHER, weather & 0xf);
    battle_script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, daytime);
    if (entd != 0) {
        if (battle_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
            battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD, squads[3]);
        } else {
            battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD, entd);
        }
    } else if (battle_script_get_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_EXIT) >= 5) {
        battle_script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_EXIT, 0);
    }
    g_battle_entd_selection_mode = 0;
    battle_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, count);
    battle_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT, 0);
    if (squads[0] == 0) {
        return 1;
    }
    battle_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, squads[0]);
    battle_script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID, squads[1]);
    battle_script_set_variable(0x37, 0);
    battle_script_set_variable(0x38, 0);
    battle_script_set_variable(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD, ramza);
    return 0;
}
