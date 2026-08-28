#ifndef FFT_SCRIPT_VARIABLES_H
#define FFT_SCRIPT_VARIABLES_H

/* Event-script variable ids shared by the BATTLE and WORLD interpreters.
 * The calendar and location ids are also serialized into the memory-card
 * slot description by CARD 0x801c01ac. */
typedef enum event_script_variable_id {
    /* Result word tested by the conditional-jump opcodes. */
    EVENT_SCRIPT_VAR_COMPARISON_RESULT = 0x00,
    EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION = 0x18,
    /* First of the camera X, Z, Y and angle words (0x1a-0x1d) handed to the
     * camera opcodes as one block. */
    EVENT_SCRIPT_VAR_CAMERA_X = 0x1a,
    EVENT_SCRIPT_VAR_CAMERA_ANGLE = 0x1d,
    /* WORLD camera setup stores its 12-bit yaw here; the corresponding reader
     * normalizes it into 0..0xfff. */
    EVENT_SCRIPT_VAR_CAMERA_YAW = 0x1e,
    EVENT_SCRIPT_VAR_CAMERA_ROTATION = 0x1f,
    /* Reads synthesize a fresh random value; the backing word does not retain
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
    /* The Deep Dungeon location menu stores the chosen floor here. WLDCORE
     * also stages a script's pending sound effect, weather sound and music
     * track in 0x5c-0x5e and clears all three once they have played. */
    EVENT_SCRIPT_VAR_DEEP_DUNGEON_SELECTION = 0x5c,
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
    EVENT_SCRIPT_VAR_SHOP_ITEM_AVAILABILITY = 0x6f,
    /* Special misc-unit count (capped at 4) that selects the Deep Dungeon
     * map state; a change reloads the map. */
    EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT = 0x70,
    EVENT_SCRIPT_VAR_FUR_SHOP_ENABLED = 0x90,
    EVENT_SCRIPT_VAR_PROPOSITIONS_ENABLED = 0x91,
    EVENT_SCRIPT_VAR_FACTS_ENABLED = 0x92,
    EVENT_SCRIPT_VAR_OLAN_SAW_RAMZA_FUNERAL = 0xa0,
    /* When set, the interpreters skip the DismissUnit, CallFunction,
     * UnlockDate, and SetDateAdvance side effects. */
    EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS = 0x1fc,
    /* Read at interpreter start and cleared at EventEnd. */
    EVENT_SCRIPT_VAR_PENDING_STAGED_STATUS = 0x1fd,
    /* ATTACK uses this branch while selecting deployment squads, and REQUIRE
     * uses it while applying permanent Brave and Faith changes. */
    EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE = 0x1fe,
    EVENT_SCRIPT_VAR_RAMZA_MANDATORY_IN_SQUAD = 0x1ff,
} event_script_variable_id_e;

/* WORLD stores ordinary variables as whole words, then packs the remaining
 * script-variable domains as 32 one-bit values or eight four-bit values per
 * word. */
enum {
    EVENT_SCRIPT_VAR_PACKED_BIT_FIRST = 0x80,
    EVENT_SCRIPT_VAR_PACKED_NIBBLE_FIRST = 0x360,
    EVENT_SCRIPT_VAR_END = 0x400,
    EVENT_SCRIPT_PACKED_BIT_WORD_FIRST = 0x80,
    EVENT_SCRIPT_PACKED_NIBBLE_WORD_FIRST = 0x97,
};

#endif
