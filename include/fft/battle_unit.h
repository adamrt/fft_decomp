#ifndef BATTLE_UNIT_H
#define BATTLE_UNIT_H

/* BATTLE units: the runtime unit record, its facing, statuses, mounts and action work, and unit lookups. */

#include "fft/battle_ai.h"
#include "fft/battle_ai_command.h"
#include "fft/battle_unit_path.h"
#include "fft/battle_unit_sprite.h"
#include "fft/data.h"
#include "fft/gfx.h"
#include "fft/map.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* facing */
/* A facing is an angle with 0x1000 to the turn. Units rest on the four
 * cardinals; the diagonals are the boundaries a free angle is snapped at. */
typedef enum battle_unit_facing {
    BATTLE_FACING_SOUTH = 0x000,
    BATTLE_FACING_SOUTH_WEST = 0x200,
    BATTLE_FACING_WEST = 0x400,
    BATTLE_FACING_NORTH_WEST = 0x600,
    BATTLE_FACING_NORTH = 0x800,
    BATTLE_FACING_NORTH_EAST = 0xa00,
    BATTLE_FACING_EAST = 0xc00,
    BATTLE_FACING_SOUTH_EAST = 0xe00,
} battle_unit_facing_e;

enum {
    BATTLE_FACING_QUARTER_TURN = 0x400,
    BATTLE_FACING_MASK = 0x0fff,
};

/* Facing values shifted to position bit 8 for the evasion comparisons. */
enum {
    BATTLE_UNIT_POSITION_FACING_SOUTH = 0x0000,
    BATTLE_UNIT_POSITION_FACING_WEST = 0x0100,
    BATTLE_UNIT_POSITION_FACING_NORTH = 0x0200,
    BATTLE_UNIT_POSITION_FACING_EAST = 0x0300,
};

/* commands */
/* Manual targeting and BATTLE_GAME_STATE_WAIT_DIRECTION reuse the AI command storage.
 * The 32-bit facing value overlaps the action's item/type/target-ID bytes. Cursor
 * coordinate readers use signed 16-bit values over the action's target fields. */
typedef union battle_unit_command_state {
    ai_command_t ai;
    struct {
        u8 _unused_00[12];
        s32 facing_hint;
        SVECTOR target_panel; /* vx = panel x, vy = map level, vz = panel y */
    } cursor;
} battle_unit_command_state_t;
typedef char battle_unit_command_state_size_must_be_24[sizeof(battle_unit_command_state_t) == 24 ? 1 : -1];

/* action results */
/* The experience and JP an action earned and the levels it raised, for the result display. */
typedef struct action_reward_display {
    u8 earned_experience;     /* +0x0 */
    u8 earned_jp;             /* +0x1 */
    u8 level_for_display;     /* +0x2; nonzero selects the "Level UP!" display */
    u8 job_level_for_display; /* +0x3 */
} action_reward_display_t;
typedef char action_reward_display_size_must_be_4[(sizeof(action_reward_display_t) == 4) ? 1 : -1];

/* Packed image IDs in the action-result status list. */
enum {
    ACTION_STATUS_DISPLAY_IMAGE_MASK = 0x7f,
    ACTION_STATUS_DISPLAY_REMOVAL = 0x80,
};

/* Entries of battle_unit_t.status_display_image_list. */
enum { ACTION_STATUS_DISPLAY_CAPACITY = 27 };

/* Block view of the numeric display fields at runtime unit data 0x2bc..0x2cf,
 * taken as one pointer by the action result display builders
 * (battle_gfx_build_next_action_result_display, battle_gfx_build_next_special_action_result_display
 * and battle_gfx_animate_post_action_text). */
typedef struct action_result_display {
    u8 active;                                     /* 0x00 */
    u8 _padding_01;                                /* 0x01; aligns selector */
    u16 selector;                                  /* 0x02; battle_numeric_display_selector_e */
    u16 value;                                     /* 0x04 */
    u16 progress;                                  /* 0x06 */
    battle_gfx_sprite_display_data_t* displays[3]; /* 0x08 */
} action_result_display_t;
typedef char action_result_display_size_must_be_20[(sizeof(action_result_display_t) == 20) ? 1 : -1];

/* The strike work record, embedded in battle_unit_t.strike (+0x18c)
 * and used as a standalone AI work record. Reaction targeting writes its
 * redirected tile at +0x1c..+0x1e before the strike is processed. */
typedef struct battle_strike_work {
    u8 actor_id;
    u8 target_count;
    u8 target_list[BATTLE_UNIT_RUNTIME_SLOT_COUNT];
    u8 skip_empty_tile_effect;
    u8 math_skill_flag;
    s16 last_attack_id; /* 0x14 */
    u8 post_action_message;
    u8 reaction_occurred;
    u8 continue_attack;
    u8 current_hit_number;
    s16 reaction_id;
    u8 target_new_x;
    u8 target_new_y;
    u8 target_new_map_level;
    u8 used_weapon_id;
    s16 reaction_ability_id; /* 0x20; runtime 0x1ac */
    /* 0x22; runtime 0x1ae. g_battle_action_current_ability.knockback_flags & 0x7f, stored with
     * the knockback destination. */
    u8 knockback_flags;
    u8 can_earn_experience;
    action_reward_display_t action_rewards; /* 0x24; runtime 0x1b0..0x1b3 */
} battle_strike_work_t;
typedef char battle_strike_work_must_be_40[sizeof(battle_strike_work_t) == 40 ? 1 : -1];

/* unit record */
/* Live on-map units own movement, animation, commands and displays.
 * runtime_id indexes g_battle_units; stats optionally links combat stats. */
typedef struct battle_unit {
    struct battle_unit* previous; /* 0x000; previous unit in the runtime list */
    u8 runtime_id;                /* 0x004; runtime slot ID */
    u8 spritesheet_vram_slot;     /* 0x005 */
    u8 spritesheet_id;            /* 0x006 */
    u8 stored_palette;            /* 0x007; the palette of its sheet that the unit wears, 0 to 5 */
    u16 state_frame_counter;      /* 0x008; incremented while renderer action states wait */
    u16 draw_enabled;             /* 0x00a; set by event Draw, cleared by Erase; a 16-bit value */
    u16 requested_animation;      /* 0x00c; battle_unit_animation_e + 1 to start at the next update; 0 none */
    u16 tpage;                    /* 0x00e; VRAM slot + 0x14: the sheet's texture page */
    u16 clut;                     /* 0x010 */
    /* battle_sprite_display_flags_e and battle_sprite_display_blend_mode_e.
     * Read-modify-written as a 16-bit value over
     * 0x012..0x013 (battle_gfx_update_sprite_transparency_flag). */
    u16 sprite_display_flags; /* 0x012 */
    /* 0x014; BATTLE_SEQ_OP_SET_LAYER_PRIORITY: the row of g_battle_gfx_animation_layer_priorities. */
    u16 layer_priority;
    u8 _padding_016[2]; /* aligns real */
    VECTOR real;        /* 0x018; world coordinates in 20.12 */
    /* Per-frame real-coordinate velocity; the jump-start distortion animation
     * (battle_move_animate_jump_start) copies all four 32-bit values as one block. */
    VECTOR velocity; /* 0x028 */
    /* Movement interpolation speed, stored and passed as a signed 32-bit value. */
    s32 walk_speed;       /* 0x038 */
    s32 step_speed;       /* 0x03c; interpolation speed of the current step; 0x2000 when a move/climb starts, raised by
                             jump gravity */
    SVECTOR world_coords; /* 0x040; integer world coordinates: x, height, map depth */
    u8 _unused_048[8];
    /* The sprite is drawn at world_coords + event_offset + seq_offset
     * (battle_unit_copy_draw_coords); both offsets are in the units
     * of world_coords (vx = x, vy = height, vz = map depth). */
    SVECTOR seq_offset; /* 0x050; the shift of the running sequence's move commands; cleared when one starts */
    /* Added to the projected sprite position, in pixels, by
     * BATTLE_SEQ_OP_MOVE_SCREEN_OFFSET; cleared when a sequence starts and,
     * with the other two offsets, by battle_unit_place_in_new_location. vz
     * is only ever cleared. */
    SVECTOR screen_offset; /* 0x058 */
    SVECTOR event_offset;  /* 0x060; accumulated by the event SpriteMove instructions */
    u8 _unused_068[4];
    /* Camera-relative facing, ((camera yaw + facing) & 0xfff) / 1024 and / 256.
     * Initialized to -1 and copied from mount to rider. */
    s16 camera_facing_quadrant;  /* 0x06c */
    s16 camera_facing_sixteenth; /* 0x06e */
    s16 facing;                  /* 0x070 */
    s16 attack_facing;           /* 0x072 */
    s16 depth_height_offset;     /* 0x074 */
    u16 float_bob_offset;        /* 0x076; from the float bob table */
    /* 0x078; battle_unit_increment_or_decrement_height_mod advances it by
     * g_system_vsync_interval. */
    u16 float_bob_phase;
    u16 special_graphic_y_offset;      /* 0x07a; added to the v of each part: a status sheet's rows in OTHER.SPR */
    u8 map_x;                          /* 0x07c */
    u8 map_y;                          /* 0x07d */
    u8 map_z;                          /* 0x07e */
    u8 step_phase;                     /* 0x07f; move_step_phase_e */
    move_destination_t movement;       /* 0x080 */
    u8 previous_map_x;                 /* 0x084 */
    u8 previous_map_y;                 /* 0x085 */
    u8 previous_map_z;                 /* 0x086 */
    u8 distortion_animation_id;        /* 0x087; battle_distortion_animation_e */
    s32 distortion_phase;              /* 0x088; the handler's own step, 0 when the distortion animation starts */
    s32 distortion_timer;              /* 0x08c; frames left: the second operand of BATTLE_SEQ_OP_START_DISTORTION */
    s32 distortion_target;             /* 0x090; target real.vy or frame threshold of the distortion step */
    u16 previous_facing;               /* 0x094 */
    u8 destination_edge_height;        /* 0x096; destination slope corner, as g_battle_move_destination_edge_height */
    u8 current_edge_height;            /* 0x097; current slope corner, as g_battle_move_current_edge_height */
    u32 movement_path_offset;          /* 0x098; index of the next packed path step */
    battle_walk_path_t path;           /* 0x09c..0x11b */
    move_step_t current_path_step;     /* 0x11c; path step byte being executed */
    u8 last_path_count;                /* 0x11d; prior path count (battle_move_finalize_path_after_animation) */
    u8 standing_on_battle_id_plus_one; /* 0x11e; unit stood on, 0 = none */
    u8 previous_standing_on_battle_id_plus_one; /* 0x11f; restored when a move is undone */
    s16 camera_relative_x;                      /* 0x120; camera-space x from RotTrans */
    s16 camera_relative_y;                      /* 0x122 */
    u8 _unused_124[4];
    u32 otag_depth_index;                 /* 0x128 */
    s32 gte_flag;                         /* 0x12c; RotTrans flag output of the unit and status bubble transforms */
    u8 mount_state;                       /* 0x130; battle_unit_mount_state_e */
    u8 mount_partner_runtime_id;          /* 0x131; linked rider or mount runtime ID */
    u8 previous_mount_state;              /* 0x132; battle_unit_mount_state_e */
    u8 previous_mount_partner_runtime_id; /* 0x133 */
    unit_stats_t* stats;                  /* 0x134; unit_stats_t backlink */
    u16 used_ability_id;                  /* 0x138 */
    u8 used_item_or_weapon_id;            /* 0x13a; the item the weapon and effect blocks draw */
    u8 equipped_weapon_type;              /* 0x13b; its item type, or 0x15 thrown: the frame set of the weapon SHP */
    u8 turn_action_flags;                 /* 0x13c; |= 1 moved, |= 2 acted this turn */
    u8 team_flags;                        /* 0x13d */
    u8 fade_out_phase;                    /* 0x13e; 0 none, 1 flash, 2 darken, 3 removed */
    u8 horizontal_flip_flag; /* 0x13f; BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP or 0: the event MirrorSprite */
    u32 status_flags_1_4;    /* 0x140; battle_unit_status_flags_1_4_e */
    u32 status_flags_5_6;    /* 0x144; battle_unit_status_flags_5_6_e */
    /* 0x148..0x157: statuses to add/remove;
     * battle_unit_update_float_and_jump_height tests the pending removal of Jump. */
    u32 statuses_to_add_1_4;                   /* 0x148 */
    u32 statuses_to_add_5_6;                   /* 0x14c */
    u32 statuses_to_remove_1_4;                /* 0x150 */
    u32 statuses_to_remove_5_6;                /* 0x154 */
    battle_unit_command_state_t command_state; /* 0x158..0x16f */
    s32 sp2_ability_id;                        /* 0x170: ability whose SP2 file battle_gfx_open_sp2_file loads */
    s32 ability_preview_phase;                 /* 0x174: target_panels_e */
    /* 0x178; battle_target_calculate_for_menu_types result stored by
     * battle_target_select_tile: 0/1 preview, 2 stop, -1 back to the menu. */
    s32 target_select_result;
    s32 attack_phase_state;    /* 0x17c: battle_attack_preparation_e */
    s32 pending_attack_result; /* 0x180: attack result code; -1 signals death */
    /* Death-by-dismount destination (unit runtime record 0x184/0x186/
     * 0x188); battle_unit_dismount_dead_rider copies them to
     * map_x/map_z/map_y.  battle_unit_apply_attack_result passes &dismount. */
    map_tile_coordinates_t dismount; /* 0x184 */
    u8 _unused_18a[2];
    battle_strike_work_t strike; /* 0x18c..0x1b3; current action targets, result and rewards */
    /* 0x1b4; battle_state_handle_ai_command_state sets it once the AI command is built; it
     * gates command dispatch. */
    u8 command_ready;
    /* 0x1b5; strikes run after the first while continue_attack is set; zeroed
     * for each new action. */
    u8 continue_attack_count;
    /* 0x1b6; the auto-battle command kind stored before target select
     * (AI_COMMAND_AUTOBATTLE_FIGHT_FOR_LIFE_SELECT requires an enemy, _PROTECT_ALLIES_SELECT an ally) or
     * target display (the two DISPLAY kinds). */
    u8 target_select_command;
    u8 status_display_image_count; /* 0x1b7; capped at 0x1b */
    u32 action_display_flags;      /* 0x1b8..0x1bb */
    /* Action-display image IDs, not canonical status IDs.  Bit 0x80 marks a
     * removal; the dispatcher consumes entries from the end. */
    u8 status_display_image_list[ACTION_STATUS_DISPLAY_CAPACITY]; /* 0x1bc..0x1d6 */
    u8 _padding_1d7;                                              /* aligns animation */
    battle_unit_anim_state_t animation;                           /* 0x1d8..0x207 */
    battle_unit_sprite_block_t sprite_blocks[3];                  /* 0x208..0x297 */
    u8 shadow_graphic_trigger;                                    /* 0x298; nonzero: draw the unit's shadow */
    u8 shadow_dirty;                         /* 0x299; bit 0 set when a step lands, cleared on shadow redraw */
    u8 shadow_counter;                       /* 0x29a; shadow animation counter (battle_gfx_draw_unit_shadow) */
    u8 _padding_29b;                         /* aligns shadow_quad */
    SVECTOR shadow_quad[4];                  /* 0x29c; shadow corners written by battle_gfx_build_unit_shadow_quad */
    action_result_display_t numeric_display; /* 0x2bc..0x2cf */
    u8 item_display_active;                  /* 0x2d0; BATTLE_SEQ_OP_SHOW_ITEM: draw item_display */
    u8 _unused_2d1;                          /* 0x2d1 */
    s8 item_display_x_offset;                /* 0x2d2; BATTLE_SEQ_OP_SET_ITEM_POSITION, in pixels */
    s8 item_display_y_offset;                /* 0x2d3 */
    u8 _unused_2d4[4];
    battle_gfx_sprite_display_data_t* item_display; /* 0x2d8; item/Equip display pointer */
    /* Status bubble (0x2dc..0x2e7);
     * battle_gfx_update_status_bubble_active sets the flag and clears
     * the timer with a 16-bit store, so the timer is declared u16. */
    u8 status_bubble_active;                                 /* 0x2dc */
    u8 status_bubble_id;                                     /* 0x2dd; battle_status_bubble_e */
    s8 status_bubble_x;                                      /* 0x2de */
    s8 status_bubble_y;                                      /* 0x2df */
    u16 status_bubble_timer;                                 /* 0x2e0 */
    u16 status_bubble_alternate_row;                         /* 0x2e2; non-zero selects the second texture row */
    battle_gfx_sprite_display_data_t* status_bubble_display; /* 0x2e4 */
    /* 0x2e8; enables the six-copy position trail seeded by
     * battle_gfx_init_position_vector_copies. */
    u32 position_copies_active;
    /* Six past draw positions of the afterimage trail, newest first (0x2ec);
     * battle_gfx_init_position_vector_copies seeds all six from world_coords. */
    SVECTOR position_copies[6]; /* 0x2ec..0x31b */
    /* Fixed sprite-display storage; API pointers address each header. */
    battle_gfx_unit_sprite_display_t unit_sprite_display;                 /* 0x31c; 8 parts */
    battle_gfx_weapon_sprite_display_t weapon_sprite_displays[3];         /* 0x362; 3 parts each */
    battle_gfx_single_part_sprite_display_t numeric_sprite_displays[3];   /* 0x3ce */
    battle_gfx_single_part_sprite_display_t status_bubble_sprite_display; /* 0x410 */
    battle_gfx_single_part_sprite_display_t item_sprite_display;          /* 0x426 */
    u8 _unused_43c[4]; /* 0x43c; the next slot's in-use 32-bit value (g_battle_unit_slot_states) */
} battle_unit_t;
typedef char battle_unit_size_must_be_1088[(sizeof(battle_unit_t) == 1088) ? 1 : -1];

/* battle_unit_t.fade_out_phase: the fade-out removal's progress. */
typedef enum battle_unit_fade_out_phase {
    BATTLE_UNIT_FADE_OUT_NONE = 0,
    BATTLE_UNIT_FADE_OUT_FLASH = 1,
    BATTLE_UNIT_FADE_OUT_DARKEN = 2,
    BATTLE_UNIT_FADE_OUT_REMOVED = 3,
} battle_unit_fade_out_phase_e;

/* battle_unit_t.turn_action_flags. */
typedef enum battle_unit_turn_action_flags {
    BATTLE_UNIT_TURN_ACTION_MOVED = 0x01,
    BATTLE_UNIT_TURN_ACTION_ACTED = 0x02,
} battle_unit_turn_action_flags_e;

/* unit runtime record 0x130 identifies which role 0x131 links. */
typedef enum battle_unit_mount_state {
    BATTLE_UNIT_MOUNT_STATE_NONE = 0,
    BATTLE_UNIT_MOUNT_STATE_RIDER = 1,
    BATTLE_UNIT_MOUNT_STATE_MOUNT = 2,
} battle_unit_mount_state_e;

/* Renderer-side runtime unit status mirror. These packed masks do not use the
 * canonical status_id_e byte order. */
typedef enum battle_unit_status_flags_1_4 {
    /* runtime +0x140. Bit 0x00000001 is tested for animation 0x21 and never set. */
    BATTLE_UNIT_STATUS_CONFUSION = 0x00000080,
    BATTLE_UNIT_STATUS_SLEEP = 0x00000040,
    BATTLE_UNIT_STATUS_PETRIFY = 0x00000020,
    BATTLE_UNIT_STATUS_STOP = 0x00000010,
    BATTLE_UNIT_STATUS_CRITICAL = 0x00000008,
    BATTLE_UNIT_STATUS_DEAD = 0x00000004,
    BATTLE_UNIT_STATUS_MOUNTED = 0x00000002,

    /* runtime +0x141. */
    BATTLE_UNIT_STATUS_FAITH = 0x00008000,
    BATTLE_UNIT_STATUS_BERSERK = 0x00004000,
    BATTLE_UNIT_STATUS_UNNAMED_0E = 0x00002000,
    BATTLE_UNIT_STATUS_HASTE = 0x00001000,
    BATTLE_UNIT_STATUS_SLOW = 0x00000800,
    BATTLE_UNIT_STATUS_DEFENDING = 0x00000400,
    BATTLE_UNIT_STATUS_CHARGING = 0x00000200,
    BATTLE_UNIT_STATUS_PERFORMING = 0x00000100,

    /* runtime +0x142. */
    BATTLE_UNIT_STATUS_OIL = 0x00800000,
    BATTLE_UNIT_STATUS_POISON = 0x00400000,
    BATTLE_UNIT_STATUS_PROTECT = 0x00200000,
    BATTLE_UNIT_STATUS_SHELL = 0x00100000,
    BATTLE_UNIT_STATUS_DONT_ACT = 0x00080000,
    BATTLE_UNIT_STATUS_DONT_MOVE = 0x00040000,
    BATTLE_UNIT_STATUS_BLOOD_SUCK = 0x00020000,
    BATTLE_UNIT_STATUS_INNOCENT = 0x00010000,

    /* runtime +0x143. Bit 0x80000000 remains unknown. */
    BATTLE_UNIT_STATUS_UNDEAD = 0x40000000,
    BATTLE_UNIT_STATUS_REGEN = 0x20000000,
    BATTLE_UNIT_STATUS_RERAISE = 0x10000000,
    BATTLE_UNIT_STATUS_DARKNESS = 0x08000000,
    BATTLE_UNIT_STATUS_DEATH_SENTENCE = 0x04000000,
    BATTLE_UNIT_STATUS_SILENCE = 0x02000000,
    BATTLE_UNIT_STATUS_CHARM = 0x01000000,

    BATTLE_UNIT_STATUS_PALETTE_MOD_MASK = 0x60c26020,
    BATTLE_UNIT_STATUS_BUBBLE_MASK = 0x1fffc2c4,
    BATTLE_UNIT_STATUS_PREVENT_FACING_MASK = 0x00080074,
    BATTLE_UNIT_STATUS_ANIMATION_SELECTION_MASK = 0x00003fff,
} battle_unit_status_flags_1_4_e;

typedef enum battle_unit_status_flags_5_6 {
    /* runtime +0x144. */
    BATTLE_UNIT_STATUS_CRYSTAL = 0x00000001,
    BATTLE_UNIT_STATUS_CHICKEN = 0x00000002,
    BATTLE_UNIT_STATUS_FROG = 0x00000004,
    BATTLE_UNIT_STATUS_TREASURE = 0x00000008,
    BATTLE_UNIT_STATUS_TRANSPARENT = 0x00000010,
    BATTLE_UNIT_STATUS_FLOAT = 0x00000020,
    BATTLE_UNIT_STATUS_JUMP = 0x00000040,
    BATTLE_UNIT_STATUS_POACHED = 0x00000080,

    /* runtime +0x145. Other bits remain unknown. */
    BATTLE_UNIT_STATUS_MORBOL = 0x00000100,
    BATTLE_UNIT_STATUS_JUMP_HEIGHT_ACTIVE = 0x00000200,

    BATTLE_UNIT_STATUS_TRANSFORMATION_MASK = 0x0000000f,
} battle_unit_status_flags_5_6_e;

/* unit slots and lookups */
/* The in-use 32-bit values of g_battle_unit_slot_states, stride 0x440: each sits four bytes ahead
 * of its g_battle_units record, in the previous record's last four bytes. */
typedef struct battle_unit_slot_state {
    s32 in_use;
    u8 _unused_004[0x43c];
} battle_unit_slot_state_t;
typedef char battle_unit_slot_state_size_must_be_1088[(sizeof(battle_unit_slot_state_t) == 1088) ? 1 : -1];

/* Results of battle_unit_find_slot_by_character_identity that are not a stats slot. */
typedef enum battle_character_slot {
    BATTLE_CHARACTER_SLOT_SELECTOR = -1, /* the identity is a selector value, not a character */
    BATTLE_CHARACTER_SLOT_OFF_FIELD = -2,
    BATTLE_CHARACTER_SLOT_UNAVAILABLE = -3, /* a crystal or a treasure, or a disabled unit */
    BATTLE_CHARACTER_SLOT_IN_PARTY = -4,    /* only in the party roster */
    BATTLE_CHARACTER_SLOT_ABSENT = -5,
} battle_character_slot_e;

/* The battle's unit stats, indexed by battle id (unit_stats_t.battle_id). */
extern unit_stats_t g_battle_unit_stats[BATTLE_UNIT_SLOT_COUNT];
extern battle_unit_slot_state_t g_battle_unit_slot_states[];
extern s32 g_battle_unit_graphics_load_complete;
/* The live units, chained through battle_unit_t.previous. */
extern battle_unit_t* g_battle_unit_list_head;
/* Runtime id of the acting unit, which battle_unit_get_source finds; 0xff for none. */
extern s32 g_battle_unit_source_runtime_id;

s32 battle_unit_draw_and_activate_by_runtime_id(u32 runtime_id);
s32 battle_unit_disable_remove(s32 battle_id);
s32 battle_unit_get_battle_id_by_runtime_id(u32 runtime_id);
battle_unit_t* battle_unit_get_casting(void);
battle_unit_t* battle_unit_get_by_battle_id(u16 battle_id);
battle_unit_t* battle_unit_get_by_runtime_id(u16 runtime_id);
s32 battle_unit_get_runtime_id_by_battle_id(u32 battle_id);
battle_unit_t* battle_unit_get_source(void);
s32 battle_unit_has_runtime_id(u32 runtime_id);
s32 battle_unit_move_to_head(u32 runtime_id);
s32 battle_unit_remove_by_runtime_id(u32 runtime_id);
s32 battle_unit_set_off_field(s32 battle_id);
unit_stats_t* battle_unit_find_active_data_pointer(void);
action_command_t* battle_unit_get_action_command_by_battle_id(s32 battle_id);
action_command_t* battle_unit_get_runtime_action_command_by_battle_id(u32 battle_id);
void battle_unit_remove(battle_unit_t* unit);
void battle_unit_clear_all(void);
s32 battle_unit_find_slot_by_character_identity(u32 character_identity);
unit_stats_t* battle_unit_get_existing_pointer(u32 battle_id);
unit_stats_t* battle_unit_get_stats_from_battle_id(u32 battle_id);

/* tile queries */
typedef enum battle_unit_height_flags {
    BATTLE_UNIT_HEIGHT_NON_CHOCOBO = 0x20,
    BATTLE_UNIT_HEIGHT_SHARED_TILE = 0x40,
    BATTLE_UNIT_HEIGHT_UNAVAILABLE = 0x80,
} battle_unit_height_flags_e;

/* Selection policy for battle_unit_find_at_tile.  Status bits are inclusive
 * alternatives; the Chocobo bit is an additional requirement. */
typedef enum battle_unit_tile_filter {
    BATTLE_UNIT_TILE_FILTER_TREASURE = 0x01,
    BATTLE_UNIT_TILE_FILTER_DRY_LOW_SLOPE = 0x02,
    BATTLE_UNIT_TILE_FILTER_TRANSPARENT = 0x10,
    BATTLE_UNIT_TILE_FILTER_CRYSTAL = 0x40,
    BATTLE_UNIT_TILE_FILTER_MOUNTABLE_CHOCOBO = 0x80,
    BATTLE_UNIT_TILE_STATUS_FILTERS
    = BATTLE_UNIT_TILE_FILTER_TREASURE | BATTLE_UNIT_TILE_FILTER_TRANSPARENT | BATTLE_UNIT_TILE_FILTER_CRYSTAL,
} battle_unit_tile_filter_e;

/* Seven result bytes; assigning elevation preserves the unassigned high bit. */
typedef struct battle_unit_height_data {
    u8 x;
    u8 y;
    u8 elevation : 7;
    u8 _unused_02 : 1; /* not read or assigned */
    u8 unit_flags;
    u8 unit_height;
    u8 walking_height;
    u8 total_height;
} battle_unit_height_data_t;
typedef char battle_unit_height_data_size_must_be_7[sizeof(battle_unit_height_data_t) == 7 ? 1 : -1];

s32 battle_unit_find_at_tile(s32 x, s32 y, u32 level, s32 filters);
battle_unit_t* battle_unit_find_overlapping_at_tile(battle_unit_t* self, s32 x, s32 y, s32 z);

/* mounts and statuses */
/* Result of battle_unit_get_chocobo_mount_state. */
typedef enum battle_chocobo_mount_state {
    BATTLE_CHOCOBO_MOUNT_NONE = 0, /* not a chocobo, or one in an unmountable status */
    BATTLE_CHOCOBO_MOUNT_FREE = 1,
    BATTLE_CHOCOBO_MOUNT_PAIRED = 2, /* already carries a rider */
} battle_chocobo_mount_state_e;

/* Result of battle_unit_get_action_restriction. */
typedef enum battle_unit_action_restriction {
    BATTLE_UNIT_ACTION_RESTRICTION_NONE = 0,
    /* Don't Act status or UNIT_MOUNT_INFO_FLAG_MOUNT. */
    BATTLE_UNIT_ACTION_RESTRICTION_CANNOT_ACT = 1,
    /* Tile depth 2 or more, unless the unit has Float, Chicken or Frog status,
     * a water movement ability or UNIT_MOUNT_INFO_FLAG_RIDER. */
    BATTLE_UNIT_ACTION_RESTRICTION_DEEP_WATER = 2,
} battle_unit_action_restriction_e;

extern const u32 g_battle_unit_status_mask_by_handler_index[STATUS_COUNT + 1];

void battle_unit_dismount_rider(battle_unit_t* unit);
void battle_unit_mount_rider_onto_unit(battle_unit_t* rider, battle_unit_t* mount);
void battle_unit_dismount_dead_rider(battle_unit_t* unit);
battle_chocobo_mount_state_e battle_unit_get_chocobo_mount_state(const unit_stats_t* unit);
void battle_unit_mount_onto_pending_mount(battle_unit_t* unit);
battle_unit_action_restriction_e battle_unit_get_action_restriction(const unit_stats_t* unit);

/* event library */
extern image_load_parameters_t g_menu_unit_summary_portrait_image_params;

#endif
