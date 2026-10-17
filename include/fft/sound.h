#ifndef SOUND_H
#define SOUND_H

/* Sound: the event scripts' music requests and the scenario's music tracks, the sound-effect ids and MAIN's effect and
 * background-sound wrappers, the event library's queued menu effects and background sound threads, OPEN's CD audio fade
 * and WLDCORE's audio queue. */

#include "psx/types.h"

/* music requests */
/* Left by the event script's music instructions for script_process_pending_requests, which
 * carries them out once per event frame; -1 means no request. The switch request is a trigger,
 * not a track: it toggles g_sound_current_music_track_index and starts the track in that slot
 * (g_sound_music_track_1_id or _2_id) with the volume and fade of
 * g_event_music_switch_volume and g_event_music_switch_fade (script.h). */
extern s16 g_sound_music_set_track_request;
extern s16 g_sound_music_switch_request;
extern s16 g_sound_music_unload_slot_request;

/* The target volume in the low 16 bits; the fade time is (packed >> 14) & 0x3ffc, not a linear
 * field. */
extern s32 g_sound_music_volume_transition_request;
extern u16 g_sound_current_music_track_index; /* zero-based music slot */

/* The scenario's music tracks in main music slots 1 and 2; 0 is none. */
extern u16 g_sound_music_track_1_id;
extern u16 g_sound_music_track_2_id;

/* sound effects */
/* Sound resources. A sound id is SUZUKI_SOUND_ID(resource, sound): the id of a
 * loaded suzuki_sound_resource_t in the high 16 bits and the index of a
 * sound in it in the low. Each battle effect file brings a resource of its
 * own (effect_file_load_sound_section). */
typedef enum sound_resource {
    SOUND_RESOURCE_SYSTEM = 0,       /* SOUND/SYSTEM.SED: sound_sfx_e */
    SOUND_RESOURCE_ENV = 1,          /* SOUND/ENV.SED: sound_env_sfx_e */
    SOUND_RESOURCE_SOUND_NOVEL = 225 /* g_sound_novel_sound_resource, 81 sounds */
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
    SOUND_SFX_RED_ORBS = 8,    /* effect_secondary_red_orbs_handler starts */
    SOUND_SFX_DISMISS = 9,     /* dismiss unit */
    SOUND_SFX_EQUIP = 10,      /* equip, set ability, best fit */
    SOUND_SFX_CAMERA_ROTATE_LEFT = 11,
    SOUND_SFX_CAMERA_ROTATE_RIGHT = 12,
    SOUND_SFX_CAMERA_ROTATE_STOP = 13, /* the rotation reached its target */
    /* Paired zoom sounds: camera_zoom_toggle and camera_zoom_start
     * play 0x11 with zoom action 4 and 0xe with action 2; the in/out reading
     * follows the zoom-target comment in camera_zoom_toggle. The
     * ATTACK deploy screen also plays 0x11 when a placed unit is moved. */
    SOUND_SFX_CAMERA_ZOOM_IN = 14,
    /* L1/R1 on the world map (world_map_step_cursor_level): 0x10 when
     * the toggle sets the zoomed-out bit of g_world_map_zoom, 0xf
     * when it clears it. */
    SOUND_SFX_WORLD_MAP_ZOOM_IN = 15,
    SOUND_SFX_WORLD_MAP_ZOOM_OUT = 16,
    SOUND_SFX_CAMERA_ZOOM_OUT = 17,
    SOUND_SFX_WINDOW_OPEN = 18, /* help, message or text window opens */
    /* The weapon sounds of action_play_weapon_sfx, by the weapon's
     * item_type_e: g_action_weapon_swing_sfx_ids (the swing, played
     * before the hit is known), g_action_weapon_hit_sfx_ids and
     * g_action_shield_deflect_sfx_ids. */
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
    SOUND_SFX_TRAP = 34, /* effect_secondary_trap_handler starts */
    /* move_step_play_landing_sfx_by_surface_type: water surfaces play
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
     * action_play_weapon_sfx plays it for an attack stopped by an
     * accessory or class evade, a forced failure, a blade grasp or a catch,
     * and as the shield deflect of an instrument or book. */
    SOUND_SFX_ERROR_MESSAGE = 48,
    SOUND_SFX_CAMERA_TILT = 49, /* camera_tilt_start */
    SOUND_SFX_HIT_ROD = 56,     /* rod, staff */
    SOUND_SFX_LAND_WOOD = 58,
    /* Circle on a name-entry character key, or on an occupied tile of the
     * ATTACK deploy screen. */
    SOUND_SFX_SELECT = 60,
    SOUND_SFX_HIT_POLE = 62,         /* polearm, pole */
    SOUND_SFX_EVENT_REVIVE = 65,     /* the event script's revive status change (*_script_inflict_status_thread) */
    SOUND_SFX_WORLD_MAP_ARRIVE = 67, /* a route leg ends at a location with no event or encounter */
    /* Dead status added outside an event
     * (animation_update_for_status_changes), chosen by the unit's
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
    SOUND_SFX_CRITICAL_HIT = 114, /* a weapon hit whose action is critical (action_play_weapon_sfx) */
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
    SOUND_SFX_COUNT = 168     /* the sound count in the SYSTEM.SED header */
} sound_sfx_e;

/* Sounds of SOUND_RESOURCE_ENV that the code plays by number. ENV.SED
 * holds 27 (0, 0x15, 0x16 and 0x17 are empty); event scripts choose the
 * others through EVENT_OPCODE_BG_SOUND. */
typedef enum sound_env_sfx {
    SOUND_ENV_SFX_RAIN = 2,      /* weather_run_command: rain without snow, and the thunder command */
    SOUND_ENV_SFX_THUNDER_A = 3, /* battle_map_draw_mesh_and_weather picks one of the two for a lightning flash */
    SOUND_ENV_SFX_THUNDER_B = 14
} sound_env_sfx_e;

void sound_open_generic_sfx(void);
void sound_play_sfx(s32 sound_id);
void sound_play_sfx_on_free_channel(s32 sound_id);
void sound_set_sfx_volume(int sound_id, int volume);
void sound_play_background(s32 sound_id);
void sound_replay_background(void);
void sound_stop_background(void);
void sound_stop_all_sfx(void);

/* battle_map_step_init_sequence calls this without loading $a0, but the body reads sound_id. */
void sound_stop_sfx(int sound_id);

/* queued effects */
extern s32 g_sound_pending_sfx_id; /* sound_sfx_e; -1 means no request */
extern s32 g_sound_suppressed;
extern u8 g_sound_queued_effect_id; /* sound_sfx_e */

void sound_queue_sfx_cancel(void);
void sound_queue_sfx_confirm(void);
void sound_queue_sfx_cursor_move(void);
void sound_queue_sfx_invalid(void);

/* background sound threads */
void sound_background_fade_thread(void);
void sound_background_thread(void);

/* CD audio */
/* Fades the CD audio in at the given mix level (suzuki_sound_put_type), or out when it is 0. */
void sound_set_cd_mix_and_volume(s32 cd_mix_level, s32 duration);

/* audio queue */
/* Flags of audio_music_slots_t. */
typedef enum audio_music_flag {
    AUDIO_MUSIC_PLAYING = 0x01 /* the current slot's music is audible: a new start fades it out first */
} audio_music_flag_e;

/* Playback flags followed by the loaded music IDs for slots 1 and 2. */
typedef struct audio_music_slots {
    s32 flags; /* audio_music_flag_e */
    s32 tracks[2];
} audio_music_slots_t;

/* Value of AUDIO_COMMAND_PLAY_MUSIC and of
 * audio_queue_start_music: a music slot (1 or 2) and the
 * music_id_e id to load into it. */
#define AUDIO_MUSIC(slot, music_id) (((slot) << 8) | (music_id))

/* Commands of the audio queue (audio_queue_add), and the
 * wait states audio_queue_process moves the current command
 * through. */
typedef enum audio_command {
    AUDIO_COMMAND_NONE = 0,       /* idle: takes the next queued command */
    AUDIO_COMMAND_PLAY_MUSIC = 1, /* value: AUDIO_MUSIC(slot, music id) */
    AUDIO_COMMAND_FADE_OUT = 2,   /* value << 2: fade time */
    AUDIO_COMMAND_FADE_IN = 3,    /* the current slot, to the stored volume over value << 2 */
    AUDIO_COMMAND_UNLOAD = 4,     /* value: slot, 0 for the current one */
    AUDIO_COMMAND_WAIT_LOAD = 16,
    AUDIO_COMMAND_WAIT_FADE_OUT = 17,
    AUDIO_COMMAND_WAIT_FADE_IN = 18
} audio_command_e;

enum { AUDIO_QUEUE_CAPACITY = 16 };

/* The audio command FIFO, filled by audio_queue_add and serviced each frame by
 * audio_queue_process, followed by the music state that consumer owns. current_command and count are
 * the busy fields audio_queue_wait_until_empty and the level steps test. */
typedef struct audio_queue {
    audio_command_e commands[AUDIO_QUEUE_CAPACITY];
    s32 values[AUDIO_QUEUE_CAPACITY];
    audio_command_e current_command; /* 0x80 */
    s32 current_value;               /* 0x84 */
    s32 count;                       /* 0x88 */
    audio_music_slots_t music;       /* 0x8c */
    s32 current_slot;                /* 0x98 */
    s32 volume;                      /* 0x9c */
} audio_queue_t;
typedef char audio_queue_size_must_be_160[sizeof(audio_queue_t) == 160 ? 1 : -1];

extern audio_queue_t g_audio_queue;

void audio_queue_add(audio_command_e command, s32 value);
void audio_queue_start_music(s32 value);
void audio_queue_process(void);
void audio_queue_wait_until_empty(void);
void sound_play_effect(s32 sound_id);

#endif
