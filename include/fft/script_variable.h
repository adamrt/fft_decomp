#ifndef SCRIPT_VARIABLE_H
#define SCRIPT_VARIABLE_H

/* Event script variables: their ids and value domains, the routines that read and write them, and the event ids that
 * EVENT_SCRIPT_VAR_CURRENT_EVENT holds. */

#include "fft/script_instruction.h"
#include "psx/types.h"

/* variables */
/* Logical ids shared by the BATTLE and WORLD interpreters: 0x000-0x07f
 * address 32-bit values, 0x080-0x35f packed bits, and 0x360-0x3ff packed
 * four-bit values. These are not uniform byte offsets into the backing bank.
 * The calendar and location ids are also serialized into the memory-card
 * slot description by save_build_image. */
typedef enum event_script_variable_id {
    /* 32-bit result tested by the conditional-jump opcodes. */
    EVENT_SCRIPT_VAR_COMPARISON_RESULT = 0x00,
    /* The comparison opcodes compare 32-bit variable 0 with this 32-bit variable and leave the
     * truth value in 32-bit variable 0. */
    EVENT_SCRIPT_VAR_COMPARISON_OPERAND = 0x01,
    /* Also where TEXT_STORE_VARIABLE writes until TEXT_SET_VARIABLE_BASE
     * names another variable. */
    EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION = 0x18,
    /* A 32-bit store is also copied to text substitution value 0. */
    EVENT_SCRIPT_VAR_TEXT_SUBSTITUTION_VALUE = 0x19,
    /* 32-bit variables 0x1a-0x20 mirror the live camera: the store and load routines copy
     * X -> vx, Z (height) -> vy, Y (map depth) -> vz, tilt, yaw, roll and zoom.
     * The camera opcodes take X, Z, Y and tilt (0x1a-0x1d) as one block. */
    EVENT_SCRIPT_VAR_CAMERA_X = 0x1a,
    EVENT_SCRIPT_VAR_CAMERA_Z = 0x1b,
    EVENT_SCRIPT_VAR_CAMERA_Y = 0x1c,
    EVENT_SCRIPT_VAR_CAMERA_TILT = 0x1d,
    /* WORLD camera setup stores its 12-bit yaw here; the corresponding reader
     * normalizes it into 0..0xfff. */
    EVENT_SCRIPT_VAR_CAMERA_YAW = 0x1e,
    EVENT_SCRIPT_VAR_CAMERA_ROLL = 0x1f,
    EVENT_SCRIPT_VAR_CAMERA_ZOOM = 0x20,
    /* Reads synthesize a fresh random value; the backing 32-bit value does not retain
     * the value returned by the interpreter. */
    EVENT_SCRIPT_VAR_RANDOM_VALUE = 0x21,
    EVENT_SCRIPT_VAR_WEATHER_AND_TIME = 0x22,
    EVENT_SCRIPT_VAR_WEATHER = 0x23,
    EVENT_SCRIPT_VAR_TIME_OF_DAY = 0x24,
    EVENT_SCRIPT_VAR_CURRENT_EVENT = 0x27,
    EVENT_SCRIPT_VAR_DISABLED_MENU_ACTIONS = 0x28,
    EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS = 0x29,
    EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS = 0x2a,
    EVENT_SCRIPT_VAR_WAR_FUNDS = 0x2c,
    EVENT_SCRIPT_VAR_MONTH = 0x2e,
    EVENT_SCRIPT_VAR_DAY = 0x2f,
    EVENT_SCRIPT_VAR_MAP_ARRANGEMENT = 0x30,
    EVENT_SCRIPT_VAR_LOCATION = 0x31,
    EVENT_SCRIPT_VAR_CURRENT_ENTD = 0x32,
    EVENT_SCRIPT_VAR_CURRENT_MAP = 0x33,
    EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT = 0x34,
    EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID = 0x35,
    EVENT_SCRIPT_VAR_DEPLOYMENT_SECONDARY_SQUAD_ID = 0x36,
    /* The deployment screen reads squad n's id from PRIMARY_SQUAD_ID + n and
     * keeps four squad lists; ATTACK clears these two. */
    EVENT_SCRIPT_VAR_DEPLOYMENT_THIRD_SQUAD_ID = 0x37,
    EVENT_SCRIPT_VAR_DEPLOYMENT_FOURTH_SQUAD_ID = 0x38,
    /* ATTACK stores the selected squad's unit limit here; SMALL clears it
     * during overlay setup. */
    EVENT_SCRIPT_VAR_DEPLOYMENT_UNIT_LIMIT = 0x39,
    /* Set while CallFunction 0x06 saves the game. */
    EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS = 0x51,
    EVENT_SCRIPT_VAR_DEPLOYED_UNIT_COUNT = 0x52,
    EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE = 0x53,
    EVENT_SCRIPT_VAR_DATE_ADVANCE = 0x54,
    EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT = 0x55,
    EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE = 0x56,
    /* Incremented for every glyph the dialog typewriter uploads. */
    EVENT_SCRIPT_VAR_PRINTED_CHARACTER_COUNT = 0x57,
    /* Added to the typewriter's VRAM x column (0x1c0); the dialog threads
     * clear it when they start. */
    EVENT_SCRIPT_VAR_TYPEWRITER_VRAM_X_OFFSET = 0x59,
    /* Written by text control TEXT_SET_PORTRAIT. */
    EVENT_SCRIPT_VAR_DIALOG_PORTRAIT = 0x5a,
    /* WLDCORE stages a script's pending sound effect, weather sound and
     * music track in 0x5c-0x5e and clears all three once they have played. */
    EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT = 0x5c,
    EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND = 0x5d,
    EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK = 0x5e,
    /* The player character's birthday, written by the OPEN birthday entry. */
    EVENT_SCRIPT_VAR_PLAYER_BIRTH_MONTH = 0x5f,
    EVENT_SCRIPT_VAR_PLAYER_BIRTH_DAY = 0x60,
    EVENT_SCRIPT_VAR_INJURED = 0x61,
    EVENT_SCRIPT_VAR_CASUALTIES = 0x62,
    /* Enables the contextual Anything entry in the action skillset menu. */
    EVENT_SCRIPT_VAR_ANYTHING_ACTION_ENABLED = 0x63,
    EVENT_SCRIPT_VAR_NEXT_SCENARIO = 0x64,
    /* One less than the number of Deep Dungeon floors listed. */
    EVENT_SCRIPT_VAR_DEEP_DUNGEON_LIST_LENGTH = 0x65,
    /* Scaled by 10 and added to the TEXT_SET_VARIABLE_BASE variable by
     * TEXT_STORE_VARIABLE, which then clears it. */
    EVENT_SCRIPT_VAR_TEXT_STORE_INDEX = 0x66,
    /* Incremented each March 21 and by whole years of advanced days, capped
     * at 99. */
    EVENT_SCRIPT_VAR_UNKNOWN_67 = 0x67,
    EVENT_SCRIPT_VAR_DEEP_DUNGEON_EXIT = 0x68,
    /* WLDCORE also uses this value to select the current-location marker's
     * animation on the world map. */
    EVENT_SCRIPT_VAR_TOWN_BACKGROUND = 0x69,
    /* World help message 0 stores 1 here as it ends (TEXT_STORE_VARIABLE);
     * Gariland's world script shows that message while this is clear. */
    EVENT_SCRIPT_VAR_WORLD_MAP_HELP_SHOWN = 0x6d,
    /* Main story progress counter. */
    EVENT_SCRIPT_VAR_STORY_PROGRESS = 0x6e,
    EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY = 0x6f,
    /* Special runtime-unit count (capped at 4) that selects the Deep Dungeon
     * map state; a change reloads the map. */
    EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT = 0x70,
    /* ATTACK zeroes these 32 variables each time it loads a scenario, and
     * the interpreters never refuse a write to them. */
    EVENT_SCRIPT_VAR_SCENARIO_LOCAL_FIRST = 0x70,
    EVENT_SCRIPT_VAR_SCENARIO_LOCAL_END = 0x90,
    EVENT_SCRIPT_VAR_FUR_SHOP_ENABLED = 0x90,
    EVENT_SCRIPT_VAR_PROPOSITIONS_ENABLED = 0x91,
    EVENT_SCRIPT_VAR_FACTS_ENABLED = 0x92,
    /* Set by event EVENT_ID_MALAK_REVIVED; Goug's steel ball events need it. */
    EVENT_SCRIPT_VAR_GOUG_EVENTS_UNLOCKED = 0x94,
    /* Set by event 0x071. */
    EVENT_SCRIPT_VAR_ELIDIBS_DEFEATED = 0x95,
    /* The option chosen in events 0x010 (destroy the Death Corps, or save
     * Algus), 0x07a (leave, or stay for the chocobo) and 0x08c (stay out,
     * or help Mustadio): 1 for the second. */
    EVENT_SCRIPT_VAR_ALGUS_RESCUE_CHOICE = 0x96,
    EVENT_SCRIPT_VAR_CHOCOBO_RESCUE_CHOICE = 0x97,
    EVENT_SCRIPT_VAR_MUSTADIO_RESCUE_CHOICE = 0x98,
    /* Goug's four events with Besrodio, each waiting for the one before:
     * 0x0d3 shows the steel ball, 0x0d5 starts the Steel Giant, 0x0d7 shows
     * the machine like a heavenly globe and 0x0d9 summons Cloud with it. */
    EVENT_SCRIPT_VAR_STEEL_BALL_SHOWN = 0x99,
    EVENT_SCRIPT_VAR_STEEL_GIANT_STARTED = 0x9a,
    EVENT_SCRIPT_VAR_HEAVENLY_GLOBE_SHOWN = 0x9b,
    EVENT_SCRIPT_VAR_CLOUD_SUMMONED = 0x9c,
    /* The answer in event 0x0fb to whether Ramza has read the scriptures:
     * 1 for No. */
    EVENT_SCRIPT_VAR_SCRIPTURES_READ_CHOICE = 0x9d,
    /* Set by event 0x1c9 and cleared by EVENT_ID_FUNERAL; while set, Orbonne
     * Monastery offers the fourth floor of its book storage. */
    EVENT_SCRIPT_VAR_ORBONNE_UNDERGROUND_OPEN = 0x9f,
    EVENT_SCRIPT_VAR_OLAN_SAW_RAMZA_FUNERAL = 0xa0,
    /* Zarghidas's flower girl. Event 0x1cb stores the answer (1: bought a
     * flower), 0x1cc and 0x1cd set FLOWER_GIRL_MET, and Cloud's battle there
     * (event 0x0da) needs CLOUD_SUMMONED and a bought flower; 0x0de and 0x1cc
     * close it. */
    EVENT_SCRIPT_VAR_FLOWER_CHOICE = 0xa1,
    EVENT_SCRIPT_VAR_FLOWER_GIRL_MET = 0xa2,
    EVENT_SCRIPT_VAR_ZARGHIDAS_CLOUD_BATTLE_CLOSED = 0xa3,
    /* Set by event 0x1c9, at story progress 53. */
    EVENT_SCRIPT_VAR_DEEP_DUNGEON_UNLOCKED = 0xa4,
    /* Set by event 0x1cf; Warjilis then draws the path to the dungeon and
     * its dot. */
    EVENT_SCRIPT_VAR_DEEP_DUNGEON_OPENED = 0xa5,
    /* Stored by the text of rumor 20, "Ghost of Colliery"; with the steel
     * ball shown, Lesalia then starts event 0x1d0. */
    EVENT_SCRIPT_VAR_COLLIERY_RUMOR_READ = 0xa6,
    /* Beowulf at Goland's bar: event 0x1d1 stores the answer to his
     * invitation (1: go together), 0x1d2 and 0x1d3 mark it answered, and
     * event 0x1e3 ends with Reis saved. */
    EVENT_SCRIPT_VAR_BEOWULF_ANSWERED = 0xa7,
    EVENT_SCRIPT_VAR_BEOWULF_INVITATION_CHOICE = 0xa8,
    EVENT_SCRIPT_VAR_REIS_RESCUED = 0xa9,
    /* Stored by the text of rumor 21, "Cursed Island, Nelveska"; Zeltennia
     * then draws the path to Nelveska Temple and its dot. */
    EVENT_SCRIPT_VAR_NELVESKA_RUMOR_READ = 0xaa,
    /* Set by events 0x1e7 (Worker 7 destroyed at Nelveska Temple) and 0x1e9
     * (Reis returns to human form). */
    EVENT_SCRIPT_VAR_WORKER_7_DEFEATED = 0xab,
    EVENT_SCRIPT_VAR_REIS_RESTORED = 0xac,
    /* When set, the interpreters skip the DismissUnit, CallFunction,
     * UnlockDate, and SetDateAdvance side effects. */
    EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS = 0x1fc,
    /* Set by ATTACK when a battle is loaded and cleared at EventEnd; while set,
     * statuses are staged and the turn check starts the battle-start event. */
    EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED = 0x1fd,
    /* ATTACK uses this branch while selecting deployment squads, and REQUIRE
     * uses it while applying permanent Brave and Faith changes. */
    EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE = 0x1fe,
    EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD = 0x1ff,
    /* One flag per entry of the 24-entry Brave Story list that
     * EVENT_SCRIPT_VAR_FACTS_ENABLED gates. */
    EVENT_SCRIPT_VAR_FACT_FIRST = 0x1a4,
    /* First variable of the world-map ranges WLDCORE indexes: one flag per
     * Brave Story event, location dot, route, alternate dot color, rumour,
     * treasure and unexplored land, then one nibble of state bits per proposition. */
    EVENT_SCRIPT_VAR_STORY_EVENT_FIRST = 0x1bc,
    EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST = 0x200,
    EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST = 0x22c,
    EVENT_SCRIPT_VAR_LOCATION_COLOR_FIRST = 0x267,
    /* A set flag removes one entry from a location's choice menu; a table
     * maps each choice to its flag. */
    EVENT_SCRIPT_VAR_LOCATION_CHOICE_HIDDEN_FIRST = 0x292,
    EVENT_SCRIPT_VAR_RUMOR_FIRST = 0x2d1,
    EVENT_SCRIPT_VAR_TREASURE_FIRST = 0x321,
    EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST = 0x350,
    EVENT_SCRIPT_VAR_PROPOSITION_FIRST = 0x360,
    /* One nibble per Brave Story person: zero keeps the person off the list,
     * any other value picks the profile shown. The interpreters stop writing
     * a profile that has reached 12. */
    EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST = 0x3c0,
} event_script_variable_id_e;

/* How many variables each of those ranges holds. The location ranges hold
 * LOCATION_COUNT and the path range WORLD_MAP_ROUTE_COUNT. */
enum {
    EVENT_SCRIPT_VAR_FACT_COUNT = 24,
    EVENT_SCRIPT_VAR_STORY_EVENT_COUNT = 64,
    EVENT_SCRIPT_VAR_RUMOR_COUNT = 64,
    EVENT_SCRIPT_VAR_TREASURE_COUNT = 47,
    EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT = 16,
    EVENT_SCRIPT_VAR_PROPOSITION_COUNT = 96,
    EVENT_SCRIPT_VAR_PERSON_PROFILE_COUNT = 64,
};

/* The Deep Dungeon location menu stores the chosen floor in the pending
 * sound effect's variable before it runs the entry script. */
enum {
    EVENT_SCRIPT_VAR_DEEP_DUNGEON_SELECTION = 0x5c,
};

/* Script variable caps: script_set_variable and each program that adds to
 * EVENT_SCRIPT_VAR_WAR_FUNDS stop the war funds at 99,999,999, and
 * unit_increase_casualty_counters stops EVENT_SCRIPT_VAR_CASUALTIES and
 * EVENT_SCRIPT_VAR_INJURED at 9999. */
enum {
    EVENT_WAR_FUNDS_MAX = 99999999,
    EVENT_CASUALTY_COUNT_MAX = 9999,
};

/* EVENT_SCRIPT_VAR_TIME_OF_DAY values. Map loading puts the night bit in bit 15 of the GNS map
 * state. */
enum {
    TIME_OF_DAY_DAY = 0,
    TIME_OF_DAY_NIGHT = 1,
};

/* Ordinary variables are 32-bit values; the remaining script-variable domains
 * are packed as 32 one-bit values or eight four-bit values per 32-bit value. */
enum {
    EVENT_SCRIPT_VAR_PACKED_BIT_FIRST = 0x80,
    EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST = 0x360,
    EVENT_SCRIPT_VAR_END = 0x400,
    EVENT_SCRIPT_PACKED_BIT_WORD_FIRST = 0x80,
    EVENT_SCRIPT_PACKED_NIBBLE_WORD_FIRST = 0x97,
};

/* Each scene points to the resident script-variable bank. */
extern s32* g_script_variables;

/* Suppress recursive write filtering while getters use comparison variable 0. */
extern u8 g_script_variable_write_guard;

/* Resident 1,024-byte bank: 128 words, 736 packed bits and 160 packed nibbles
 * occupy the first 684 bytes. Save images also preserve the remaining bytes. */
extern s32 g_script_variable_storage[0x100];

void script_set_variable(s32 variable_id, s32 value);
void script_run_condition(event_opcode_e opcode);

/* Read through the resident scene's variable commands, preserving the comparison
 * result. Reading weather/time first rebuilds it from the separate variables. */
s32 script_get_variable(s32 variable_id);

/* The fourth argument is supplied by callers but is not read. */
void script_run_variable_command(s32 opcode, s32 destination_id, s32 source, s32 unused);

/* event ids */
/* Event ids: EVENT_SCRIPT_VAR_CURRENT_EVENT, the 0x2000-byte block of
 * EVENT/TEST.EVT that holds the event's script and text, and the index of
 * g_scenario_event_finish_operations. The game names only the 64 Brave
 * Story record scenes (g_brave_story_event_ids lists their setup events);
 * the ones the code singles out are named here for what their own script and
 * text show. */
typedef enum event_id {
    /* CallFunction JOIN_UNITS_SILENTLY also restores the new-game inventory
     * in the events below this one. */
    EVENT_ID_NEW_GAME_INVENTORY_END = 0x4e,
    /* A Zodiac Stone speaks in these two. Their centered messages start at
     * the box's left edge while ETC's graphics are off. */
    EVENT_ID_WIEGRAF_PACT_WITH_VELIUS = 0xf9, /* Brave Story record "Velius" (setup 0xf8) */
    /* Also the event the ending leaves current, with the next scenario
     * requested, so that ATTACK steps on from it. */
    EVENT_ID_MALAK_REVIVED = 0x12a, /* Brave Story record "The lying heart" (setup 0x129) */
    /* King Delita and Ovelia. Its save does not request the next scenario. */
    EVENT_ID_EPILOGUE = 0x12c, /* Brave Story record "Things obtained" (setup 0x12b) */
    /* REQUIRE congratulates the player on completing the game after it. */
    EVENT_ID_FINAL_BATTLE_WON = 0x145,
    /* script_run_event_frame returns SCRIPT_EVENT_FRAME_ENDING when it ends. */
    EVENT_ID_FUNERAL = 0x147, /* Brave Story record "Reunion and beyond..." (setup 0x146) */
    /* A unit the event draws also gets its status graphics. */
    EVENT_ID_LIMBERRY_CEMETERY_BATTLE = 0x18c,
    /* The 0x1a events no scenario names, through TUTORIAL_LAST:
     * SCENARIO_CHECK starts the first four, and the sixteen tutorials follow. */
    EVENT_ID_COMMON_FIRST = 0x190,
    EVENT_ID_GAME_OVER = 0x190,    /* the battle is lost */
    EVENT_ID_BATTLE_START = 0x191, /* while EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED is set */
    EVENT_ID_BATTLE_WON = 0x192,
    EVENT_ID_DEBUG_BATTLE_START = 0x193, /* BATTLE_START under EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE */
    /* The same script as GAME_OVER; while it runs, every unit selector
     * resolves to the first unit that is a Ramza. */
    EVENT_ID_GAME_OVER_RAMZA = 0x194,
    EVENT_ID_TUTORIAL_FIRST = 0x19a,
    EVENT_ID_TUTORIAL_LAST = 0x1a9,
    /* These two scenes carry on in the event that follows; the EVTCHR sprites
     * stay loaded after them. */
    EVENT_ID_OLAN_WOUNDED = 0x1ad, /* Brave Story record "Delita's betrayal" (setup 0x1ac) */
    EVENT_ID_BEOWULF_INVITATION = 0x1d1,
} event_id_e;

#endif
