/* Scenario conditionals, the music resync with map darkness, and marking other squads' units undeployable. */

#include "fft/battle_event.h"
#include "fft/battle_map_light.h"
#include "fft/battle_sound.h"
#include "fft/battle_unit_encounter.h"
#include "fft/event_attack.h"
#include "fft/main.h"
#include "fft/main_sound.h"
#include "fft/main_unit.h"
#include "fft/memory.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_party.h"
#include "psx/libgte.h"
#include "psx/types.h"

typedef struct scenario_setup {
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
    u8 portrait_unit_id; /* 0x0e */
    u8 story_entd_id[2]; /* 0x0f */
    u8 ramza_mandatory;  /* 0x11 */
    u8 next_scenario[2]; /* 0x12 */
    u8 after_event[2];   /* 0x14 */
    u8 event_script[2];  /* 0x16 */
} scenario_setup_t;

/*
 * Advances the scenario flags and loads the current scenario's battle setup.
 *
 * In SCRIPT_ATTACK_ENTRY_PRELOADED mode only the special portrait and unit of the
 * pending scenario are looked up; that path returns without a value, like the target (v0 keeps
 * whatever the last expression left). Otherwise the event id is advanced, the
 * finish-operation table is rebuilt from every scenario record, and the
 * current scenario's map, music, ENTD, squads and condition script are copied
 * into the script variables. Returns 1 when the scenario has no first squad.
 */
s32 scenario_load_conditionals(void) {
    /* sp+0x10 squad 1, sp+0x14 squad 2, sp+0x1c story ENTD; [2] is unused.
     * The story ENTD lives in the array: as a scalar it would take a
     * callee-saved register instead of the target's stack slot. */
    s32 squads[4];
    scenario_setup_t* scenario;
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
    s16* base;
    s32 first;
    s32 count;

    if (script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) != 0) {
        if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == 8) {
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 9);
            script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 0);
            g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_NORMAL;
            g_system_go_straight_to_battle = 0;
        }
    }

    if (g_script_attack_entry_mode == SCRIPT_ATTACK_ENTRY_PRELOADED) {
        g_portrait_special_wldface_id = 0;
        g_portrait_special_unit_id = 0;
        value = g_scenario_event_finish_operations[script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT)];
        if (value == 0) {
            return;
        }

        /* A finish operation keeps the next scenario's low byte in bits 0-7 and its bits 8-9 in bits 10-11. */
        event = ((value & 0xc00) >> 2) + (value & 0xff);
        /* A zero id ends the scenario table. */
        scenario = (scenario_setup_t*)g_scenario_scenario_table;
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
        g_portrait_special_wldface_id = scenario->portrait_id;
        g_portrait_special_unit_id = scenario->portrait_unit_id;
        return;
    }

    g_scenario_scenario_event_active = 0;
    for (i = EVENT_SCRIPT_VAR_SCENARIO_LOCAL_FIRST; i < EVENT_SCRIPT_VAR_SCENARIO_LOCAL_END; i++) {
        script_set_variable(i, 0);
    }
    script_set_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED, 1);

    /* With a next scenario requested, step to the next event: 0x5a skips to 0x74 and Malak's revival
     * leads to 0x12b with the funeral flag set, else 0x148. */
    if (script_get_variable(EVENT_SCRIPT_VAR_OLAN_SAW_RAMZA_FUNERAL) != 0
        && script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) != 0
        && script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == EVENT_ID_MALAK_REVIVED) {
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x12b);
    } else if (script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) != 0) {
        id = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
        if (id == 0x5a) {
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x74);
        } else if (id == EVENT_ID_MALAK_REVIVED) {
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0x148);
        } else {
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, id + 1);
        }
    } else if (g_system_go_straight_to_battle != 0) {
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 1);
        for (i = 1; i < PARTY_ROSTER_SLOT_COUNT; i++) {
            party_remove_unit(i);
        }
    }

    g_system_go_straight_to_battle = 0;
    script_set_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO, 0);
    event = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);

    /* Rebuild the finish-operation table from every scenario record (the after-event code from
     * bit 8, the next scenario encoded as above), then fix the battle-result events' entries. */
    scenario = (scenario_setup_t*)g_scenario_scenario_table;
    for (i = 0; i < 500; i++) {
        id = scenario->id[0] + (scenario->id[1] << 8);
        if (id == 0) {
            break;
        }
        g_scenario_event_finish_operations[id]
            = (scenario->after_event[0] << 8) | ((scenario->next_scenario[1] << 10) + scenario->next_scenario[0]);
        scenario++;
    }

    g_scenario_event_finish_operations[EVENT_ID_BATTLE_WON] = 0x8000;
    g_scenario_event_finish_operations[EVENT_ID_BATTLE_START] = 0;
    g_scenario_event_finish_operations[EVENT_ID_DEBUG_BATTLE_START] = 0;
    if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0) {
        g_scenario_event_finish_operations[EVENT_ID_GAME_OVER] = 0x8000;
        g_scenario_event_finish_operations[EVENT_ID_GAME_OVER_RAMZA] = 0x8000;
    } else {
        g_scenario_event_finish_operations[EVENT_ID_GAME_OVER] = 0x8200;
        g_scenario_event_finish_operations[EVENT_ID_GAME_OVER_RAMZA] = 0x8200;
    }

    scenario = (scenario_setup_t*)g_scenario_scenario_table;
    for (i = 0; i < 500; i++) {
        id = scenario->id[0] + (scenario->id[1] << 8);
        if (id == event || id == 0) {
            break;
        }
        scenario++;
    }

    if (id == 0) {
        script_set_variable(EVENT_SCRIPT_VAR_MAP_ARRANGEMENT, 0);
        script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
        return 0;
    }

    /* The id, next-scenario and after-event loads below are discarded, as in retail. */
    g_scenario_scenario_event_active++;
    mem_load_unaligned_s16(scenario->id);
    map_id = scenario->map_id;
    weather = scenario->weather;
    daytime = scenario->daytime;
    g_event_scenario_music_1_id = scenario->music_1;
    g_event_scenario_music_2_id = scenario->music_2;
    entd = mem_load_unaligned_s16(scenario->entd_id);
    squads[0] = mem_load_unaligned_s16(scenario->squad_1);
    squads[1] = mem_load_unaligned_s16(scenario->squad_2);
    g_portrait_special_wldface_id = scenario->portrait_id;
    g_portrait_special_unit_id = scenario->portrait_unit_id;
    squads[3] = mem_load_unaligned_s16(scenario->story_entd_id);
    ramza = scenario->ramza_mandatory;
    mem_load_unaligned_s16(scenario->next_scenario);
    mem_load_unaligned_s16(scenario->after_event);
    script = mem_load_unaligned_s16(scenario->event_script);

    /* The condition blocks open with a table of byte offsets; the script's list of offsets is copied
     * relative to its first entry, up to a zero. */
    base = (s16*)g_scenario_event_condition_blocks;
    conditions = base + base[script] / 2;
    first = *conditions;
    for (i = 0; i < 16; i++) {
        g_scenario_condition_offsets[i] = *conditions - first;
        if (*conditions == 0) {
            g_scenario_condition_offsets[i] = 0;
            break;
        }
        conditions++;
    }

    for (i = 0; i < 256; i++) {
        g_scenario_condition_data[i] = (base + ((s16)g_scenario_condition_offsets[0] + first) / 2)[i];
    }

    /* Publish the battle setup: map, arrangement, weather, time of day, the ENTD (the story
     * ENTD while progress effects are suppressed) and the squads. */
    count = 0;
    for (i = 0; i < 2; i++) {
        if (squads[i] != 0) {
            count++;
        }
    }

    script_set_variable(EVENT_SCRIPT_VAR_CURRENT_MAP, map_id);
    /* The weather byte's high nibble is the map arrangement. */
    script_set_variable(EVENT_SCRIPT_VAR_MAP_ARRANGEMENT, weather >> 4);
    script_set_variable(EVENT_SCRIPT_VAR_WEATHER, weather & 0xf);
    script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, daytime);
    if (entd != 0) {
        if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD, squads[3]);
        } else {
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD, entd);
        }
    } else if (script_get_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_EXIT) >= 5) {
        script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_EXIT, 0);
    }

    g_encounter_selection_mode = UNIT_ENTD_SELECTION_SCRIPT_VARIABLE;
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, count);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT, 0);

    if (squads[0] == 0) {
        return 1;
    }
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, squads[0]);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID, squads[1]);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_THIRD_SQUAD_ID, 0);
    script_set_variable(EVENT_SCRIPT_VAR_DEPLOYMENT_FOURTH_SQUAD_ID, 0);
    script_set_variable(EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD, ramza);
    return 0;
}

/* Re-syncs the two battle scenario-music slots with the pending track pair,
 * then applies the map darkness record.
 *
 * The music step is skipped when g_scenario_scenario_event_active is clear or the current
 * event is in 410..425. When the playing track is one of the pending pair, only the idle
 * slot ((index ^ 1) + 1) is reloaded, with the pair's track for the playing index;
 * otherwise both tracks restart.
 * `playing` is s32 rather than u16 because a u16 merge of the two lhu results
 * adds an andi 0xffff that the target does not have. */
void scenario_restore_music_and_darkness(void) {
    CVECTOR darkness = g_deploy_map_darkness_color;
    s32 scenario;
    s32 playing;
    u16 replacement;

    scenario = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
    if (g_scenario_scenario_event_active != 0 && (scenario < 410 || scenario > 425)) {
        if (g_sound_music_track_1_id == 0 && g_sound_music_track_2_id == 0) {
            g_sound_current_music_track_index = 0;
            music_play_tracks(g_event_scenario_music_1_id, g_event_scenario_music_2_id);
        } else {
            if (g_sound_current_music_track_index == 0) {
                playing = g_sound_music_track_1_id;
                replacement = g_event_scenario_music_1_id;
            } else {
                playing = g_sound_music_track_2_id;
                replacement = g_event_scenario_music_2_id;
            }

            if (playing != g_event_scenario_music_1_id && playing != g_event_scenario_music_2_id) {
                g_sound_current_music_track_index = 0;
                music_play_tracks(g_event_scenario_music_1_id, g_event_scenario_music_2_id);
            } else {
                g_thread_call_target = (void (*)(void))music_unload_scenario_slot;
                thread_call_on_main_stack((g_sound_current_music_track_index ^ 1) + 1);
                g_thread_call_target = (void (*)(void))music_load_into_slot;
                thread_call_on_main_stack(replacement, (g_sound_current_music_track_index ^ 1) + 1);
                music_wait_for_load();
            }
        }

        g_sound_music_track_1_id = g_event_scenario_music_1_id;
        g_sound_music_track_2_id = g_event_scenario_music_2_id;
    }

    battle_map_light_state_command(BATTLE_MAP_LIGHT_COMMAND_SET_DARKNESS_COLOR, &darkness);
    text_build_unit_name_list();
}

/* Rebuild the "deployable" flags of the 20 roster slots for the current
 * squad: every fieldable unit, minus the units already placed in one of the
 * other three squads. */
void deploy_find_deployable_units(void) {
    s32 squad_id;
    s32 i;
    s32 unit_id;

    mem_copy_bytes_nonempty(g_deploy_roster_unit_deployable, g_deploy_unit_fieldable, PARTY_ROSTER_SLOT_COUNT);
    for (squad_id = 0; squad_id < 4; squad_id++) {
        if (squad_id != g_deploy_current_squad) {
            for (i = 0; i < 25; i++) {
                unit_id = g_deploy_units_by_squad[squad_id][i];
                if (unit_id != DEPLOY_ROSTER_ID_NONE) {
                    g_deploy_roster_unit_deployable[unit_id] = 0;
                }
            }
        }
    }
}
