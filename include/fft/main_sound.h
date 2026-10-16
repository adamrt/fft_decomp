#ifndef MAIN_SOUND_H
#define MAIN_SOUND_H

/* Resident sound: music ids and slots, tunes, sound-effect ids and the request wrappers. */

#include "fft/main_file.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* music */
/* Music ids: the index of a MUSIC_NN.SMD file in
 * g_music_scenario_smd_files (MUSIC_COUNT entries). Only the ids
 * the code passes by number are named, after the screen that plays them;
 * event scripts and scenario data choose the others. The music test offers
 * ids 1-96 (g_music_test_music_ids). */
typedef enum music_id {
    MUSIC_NONE = 0,       /* the *_sound_play_music_tracks functions and the WLDCORE audio queue skip it */
    MUSIC_ALTIMA = 19,    /* EVENT_CALL_FUNCTION_PLAY_ALTIMA_MUSIC */
    MUSIC_MILESTONE = 20, /* proposition_step_discovery_level, in the bar */
    MUSIC_TUTORIAL = 26,  /* the world menu's tutorial screen */
    MUSIC_WORLD_MAP = 27,
    MUSIC_SHOP = 28,                         /* shop_run_screen: the item shop */
    MUSIC_SOLDIER_OFFICE = 29,               /* shop_run_screen */
    MUSIC_FUR_SHOP = 30,                     /* shop_run_screen */
    MUSIC_FORMATION = 31,                    /* formation_run_screen */
    MUSIC_BRAVE_STORY = 32,                  /* the Brave Story screens */
    MUSIC_BAR = 33,                          /* the bar of a town */
    MUSIC_NEW_GAME = 34,                     /* name and birthday entry of a new game */
    MUSIC_PROPOSITION_REPORT = 35,           /* a finished proposition's report */
    MUSIC_SAVE = 41,                         /* the memory card screens */
    MUSIC_DEPLOYMENT = 42,                   /* the ATTACK unit placement screen */
    MUSIC_CONGRATULATIONS = 43,              /* victory_congratulations_thread */
    MUSIC_CONGRATULATIONS_FINAL_BATTLE = 44, /* the same screen after event 0x145 */
    MUSIC_GAME_OVER = 50,                    /* game_over_show */
    MUSIC_COUNT = 100,
} music_id_e;

/* Scenario-music playback state. current_music is the Suzuki music record of
 * the playing scenario music: music_stop hands it straight
 * to suzuki_smd_stop_music, which takes a suzuki_music_t*. */
typedef struct music_state {
    s32 current_slot;                 /* 0x00: slot of the playing scenario music */
    suzuki_music_t* current_music;    /* 0x04 */
    suzuki_smd_header_t* current_smd; /* 0x08: SMD data of the playing music */
    s32 volume;                       /* 0x0c */
} music_state_t;

/* Music slots 1..2 hold scenario music; slots 3..7 hold the five permanent
 * tunes. Both arrays store slot n at index n - 1. */
typedef struct music_slots {
    s32 tune;                    /* 0x00: pending tune (id | 0x80) or playing tune id */
    s32 loading_slot;            /* 0x04: slot whose SMD load is in flight, or zero */
    suzuki_music_t* music[7];    /* 0x08: music records returned by suzuki_smd_create_music */
    suzuki_smd_header_t* smd[7]; /* 0x24: loaded SMD data */
} music_slots_t;

/* The music state and slot table are one object: sound_open_generic_sfx
 * derives the slot pointers from the &state.current_smd base register, and the
 * tune/slot helpers reach across tune, handles and smd from single bases. */
typedef struct music {
    music_state_t state; /* 0x00 */
    music_slots_t slots; /* 0x10 */
} music_t;

/* Tunes of music_play_tune: short pieces that fade the scenario music
 * out, play once and fade it back in (music_update_tunes). Tune t is
 * MUSIC_(44 + t).SMD, kept loaded in music slot 2 + t. Tune 4 has no caller
 * in the code; EVENT_OPCODE_PLAY_TUNE passes its operand. */
typedef enum music_tune {
    MUSIC_TUNE_NONE = 0,
    MUSIC_TUNE_LEVEL_UP = 1,
    MUSIC_TUNE_ITEM_FOUND = 2, /* an item found by moving, a poach, a trained monster */
    MUSIC_TUNE_JOB_LEVEL_UP = 3,
    MUSIC_TUNE_JOB_CHANGE = 5,
} music_tune_e;

/* Packed tune-slot id and pending flag. PLAYING_MASK drops the pending flag once the tune starts. */
enum {
    MUSIC_TUNE_ID_MASK = 0x3f,
    MUSIC_TUNE_PLAYING_MASK = 0x7f,
    MUSIC_TUNE_PENDING = 0x80,
};

extern music_t g_music;
extern file_extent_t g_music_scenario_smd_files[];

s32 music_load_into_slot(s32 music_id, s32 slot);
s32 music_open_into_slot(s32 music_id, s32 slot);
s32 music_open_into_free_slot(s32 music_id);
s32 music_start_load_into_free_slot(s32 music_id);
s32 music_poll_scenario_load(void);
s32 music_wait_for_scenario_load(s32 music_id);
s32 music_is_loading_stub(void);
suzuki_music_t* music_get_handle(s32 slot);
suzuki_music_t* music_switch_track(s32 slot, s32 volume, s32 fade);
s32 music_get_current_scenario(void);
void music_set_track(s32 music_id);
s32 music_set_target(s32 volume, s32 time);
s32 music_set_volume(s32 volume, s32 time);
s32 music_set_volume_timed(s32 time);
s32 music_set_balance_shift_timed(s32 value, s32 time);
s32 music_set_pitch_shift_timed(s32 value, s32 time);
s32 music_set_tempo_scale_timed(s32 value, s32 time);
s32 music_stop(void);
void music_unload_current_scenario(void);
s32 music_unload_scenario_slot(s32 slot);
void music_unload_scenario_and_tunes(void);
s32 music_play_tune(s32 tune);
s32 music_set_tune_volume(s32 time);
s32 music_update_tunes(void);

/* sound effects */
/* Sound resources. A sound id is SUZUKI_SOUND_ID(resource, sound): the id of a
 * loaded suzuki_sound_resource_t in the high 16 bits and the index of a
 * sound in it in the low. Each battle effect file brings a resource of its
 * own (battle_effect_load_sound_section). */
typedef enum sound_resource {
    SOUND_RESOURCE_SYSTEM = 0,        /* SOUND/SYSTEM.SED: sound_sfx_e */
    SOUND_RESOURCE_ENV = 1,           /* SOUND/ENV.SED: sound_env_sfx_e */
    SOUND_RESOURCE_SOUND_NOVEL = 225, /* g_sound_novel_sound_resource, 81 sounds */
} sound_resource_e;

/* Sound-effect ids passed to sound_play_sfx and the module queues that
 * feed it (sound_play_effect, screen_sound_dispatch_effect and the
 * g_*_sound_*effect_id globals); every path reaches suzuki_sound_start_sfx with
 * SOUND_RESOURCE_SYSTEM in the high 16 bits, so this is one id space.
 * SYSTEM.SED holds SOUND_SFX_COUNT sounds; the ones named here are named
 * from their users in the code, the others are played only by event and
 * animation script data. The event frames store -1 in the pending globals
 * for "no request"; the dispatchers turn a negative id into SOUND_SFX_NONE. */
typedef enum sound_sfx {
    /* Nothing queued. SYSTEM.SED entry 0 has no streams, so playing it stops
     * the channel pair sound_play_sfx uses;
     * world_map_step_path_level plays it when the path animation ends. */
    SOUND_SFX_NONE = 0,
    SOUND_SFX_CONFIRM = 1,
    SOUND_SFX_CANCEL = 2,
    SOUND_SFX_CURSOR_MOVE = 3,
    SOUND_SFX_GIL_TICKER = 4, /* looping count-up of the REQUIRE gil reward */
    SOUND_SFX_INVALID = 5,
    SOUND_SFX_PAGE_SWITCH = 6, /* L1/R1 unit and page switches */
    SOUND_SFX_UNEQUIP = 7,     /* remove equipment/abilities */
    SOUND_SFX_RED_ORBS = 8,    /* battle_effect_red_orbs_secondary_handler starts */
    SOUND_SFX_DISMISS = 9,     /* dismiss unit */
    SOUND_SFX_EQUIP = 10,      /* equip, set ability, best fit */
    SOUND_SFX_CAMERA_ROTATE_LEFT = 11,
    SOUND_SFX_CAMERA_ROTATE_RIGHT = 12,
    SOUND_SFX_CAMERA_ROTATE_STOP = 13, /* the rotation reached its target */
    /* Paired zoom sounds: battle_camera_toggle_zoom and battle_camera_zoom_map
     * play 0x11 with zoom action 4 and 0xe with action 2; the in/out reading
     * follows the zoom-target comment in battle_camera_toggle_zoom. The
     * ATTACK deploy screen also plays 0x11 when a placed unit is moved. */
    SOUND_SFX_CAMERA_ZOOM_IN = 14,
    /* L1/R1 on the world map (world_map_step_cursor_level): 0x10 when
     * the toggle sets the zoomed-out bit of g_world_map_zoom, 0xf
     * when it clears it. */
    SOUND_SFX_WORLD_MAP_ZOOM_IN = 15,
    SOUND_SFX_WORLD_MAP_ZOOM_OUT = 16,
    SOUND_SFX_CAMERA_ZOOM_OUT = 17,
    SOUND_SFX_WINDOW_OPEN = 18, /* help, message or text window opens */
    /* The weapon sounds of battle_sound_play_weapon_sfx, by the weapon's
     * item_type_e: g_battle_sound_weapon_swing_sfx_ids (the swing, played
     * before the hit is known), g_battle_sound_weapon_hit_sfx_ids and
     * g_battle_sound_shield_deflect_sfx_ids. */
    SOUND_SFX_HIT_BLADE = 19,    /* knife, ninja blade, sword, knight sword, katana */
    SOUND_SFX_HIT_HEAVY = 21,    /* axe, flail; also the deflect of a fist, bag or cloth */
    SOUND_SFX_HIT_UNARMED = 24,  /* fist, bag, cloth */
    SOUND_SFX_SWING_LIGHT = 25,  /* fist, knife, ninja blade, instrument, book, bag, cloth */
    SOUND_SFX_SWING_MEDIUM = 26, /* sword, knight sword, katana, rod, staff */
    SOUND_SFX_SWING_HEAVY = 27,  /* axe, flail, polearm, pole */
    /* Started by the spell-charge and summon-charge secondary effects, which
     * stop them with suzuki_sound_stop_sfx_channels when they finish. */
    SOUND_SFX_SPELL_CHARGE = 31,
    SOUND_SFX_SUMMON_CHARGE = 33,
    SOUND_SFX_TRAP = 34, /* battle_effect_trap_secondary_handler starts */
    /* battle_sound_play_landing_sfx_by_surface_type: water surfaces play
     * 0x23, grassland, thicket and rug 0x28, wooden floor, tree and box 0x3a,
     * every other surface 0x29. A step that ends on another unit also plays
     * 0x28, and mounting a unit named by battle id plays 0x29. */
    SOUND_SFX_LAND_WATER = 35,
    SOUND_SFX_JUMP = 39, /* a jump rise starts; the fall that ends a mount ride starts */
    SOUND_SFX_LAND_SOFT = 40,
    SOUND_SFX_LAND_DEFAULT = 41,
    SOUND_SFX_TEXT_PAGE = 45,            /* dialogue/scroll window advances a page */
    SOUND_SFX_SHIELD_DEFLECT_LIGHT = 46, /* blades, rod, staff, polearm, pole */
    SOUND_SFX_SHIELD_DEFLECT_HEAVY = 47, /* axe, flail */
    /* An error message opens: a memory-card load, save or scan failure, or a
     * refused equip, dismissal, job change or ability.
     * battle_sound_play_weapon_sfx plays it for an attack stopped by an
     * accessory or class evade, a forced failure, a blade grasp or a catch,
     * and as the shield deflect of an instrument or book. */
    SOUND_SFX_ERROR_MESSAGE = 48,
    SOUND_SFX_CAMERA_TILT = 49, /* battle_camera_handle_tilt_request */
    SOUND_SFX_HIT_ROD = 56,     /* rod, staff */
    SOUND_SFX_LAND_WOOD = 58,
    /* Circle on a name-entry character key, or on an occupied tile of the
     * ATTACK deploy screen. */
    SOUND_SFX_SELECT = 60,
    SOUND_SFX_HIT_POLE = 62,         /* polearm, pole */
    SOUND_SFX_EVENT_REVIVE = 65,     /* the event script's revive status change (*_script_inflict_status_thread) */
    SOUND_SFX_WORLD_MAP_ARRIVE = 67, /* a route leg ends at a location with no event or encounter */
    /* Dead status added outside an event
     * (battle_unit_update_animation_for_status_changes), chosen by the unit's
     * male, female or monster flag; the male sound is 0x9a. */
    SOUND_SFX_DEATH_FEMALE = 69,
    SOUND_SFX_DEATH_MONSTER = 70,
    SOUND_SFX_SHIELD_DEFLECT_BULLET = 71, /* gun */
    SOUND_SFX_SHIELD_DEFLECT_ARROW = 73,  /* crossbow, bow */
    /* Proposition result (g_proposition_report.result): 0 plays the
     * 0x4d/0x4e pair through suzuki_sound_play_2_sfx, 1 the 0x4f/0x50 pair,
     * 2 plays 0x51. */
    SOUND_SFX_PROPOSITION_SUCCESS_A = 77,
    SOUND_SFX_PROPOSITION_SUCCESS_B = 78,
    SOUND_SFX_PROPOSITION_PARTIAL_A = 79,
    SOUND_SFX_PROPOSITION_PARTIAL_B = 80,
    SOUND_SFX_PROPOSITION_FAILURE = 81,
    SOUND_SFX_SWING_BOW = 82,
    SOUND_SFX_SWING_CROSSBOW = 86,
    SOUND_SFX_SWING_GUN = 90,
    SOUND_SFX_HIT_GUN = 91,
    SOUND_SFX_TELEPORT_OUT = 106,
    SOUND_SFX_TELEPORT_IN = 107,
    /* Animation step 4 of the world event-transition sprite
     * (event_transition_step_level): 0x6c in mode 0, which a
     * random encounter pushes, 0x6d in the other spinning modes. */
    SOUND_SFX_EVENT_TRANSITION_MODE_0 = 108,
    SOUND_SFX_EVENT_TRANSITION_MODE_1 = 109,
    SOUND_SFX_STATUS_MENU_REQUEST
    = 113,                        /* Triangle queues the status menu on another unit's turn; again when it opens */
    SOUND_SFX_CRITICAL_HIT = 114, /* a weapon hit whose action is critical (battle_sound_play_weapon_sfx) */
    SOUND_SFX_TEXT_GLYPH = 115,   /* typewriter tick after a dialogue glyph */
    SOUND_SFX_HIT_ARROW = 116,    /* crossbow, bow */
    /* World map: 0x76 when a path animation starts, drawing or erasing; 0x77
     * when a hidden location is made visible. */
    SOUND_SFX_WORLD_MAP_PATH = 118,
    SOUND_SFX_WORLD_MAP_LOCATION_APPEAR = 119,
    /* A save or load finished, Start pressed in the opening's new-game
     * transition or to end deployment, a crystal or treasure picked up, or a
     * proposition report showing its reward picture. */
    SOUND_SFX_COMPLETE = 133,
    SOUND_SFX_SHOP_TRANSACTION = 151, /* shop buy/sell confirmed */
    SOUND_SFX_ABILITY_LEARNED = 152,  /* an ability bought in the learn list */
    SOUND_SFX_DEATH_MALE = 154,
    SOUND_SFX_HIT_BOOK = 156, /* instrument, book */
    SOUND_SFX_COUNT = 168,    /* the sound count in the SYSTEM.SED header */
} sound_sfx_e;

/* Sounds of SOUND_RESOURCE_ENV that the code plays by number. ENV.SED
 * holds 27 (0, 0x15, 0x16 and 0x17 are empty); event scripts choose the
 * others through EVENT_OPCODE_BG_SOUND. */
typedef enum sound_env_sfx {
    SOUND_ENV_SFX_RAIN = 2,      /* battle_map_run_weather_command: rain without snow, and the thunder command */
    SOUND_ENV_SFX_THUNDER_A = 3, /* battle_map_draw_mesh_and_weather picks one of the two for a lightning flash */
    SOUND_ENV_SFX_THUNDER_B = 14,
} sound_env_sfx_e;

extern u8 g_sound_system_sed_data[];
extern u8 g_sound_env_sed_data[];
extern s32 g_sound_weather_sfx_id; /* the background sound sound_play_background started, 0 for none */

void sound_open_generic_sfx(void);
void sound_play_sfx(s32 sound_id);
void sound_play_sfx_on_free_channel(s32 sound_id);
/* battle_map_step_init_sequence calls this without loading $a0, but the body reads sound_id. */
void sound_stop_sfx(int sound_id);
void sound_set_sfx_echo(int sound_id, int volume);
void sound_play_background(s32 sound_id);
void sound_replay_background(void);
void sound_stop_weather(void);
void sound_stop_weather_music(void);

#endif
