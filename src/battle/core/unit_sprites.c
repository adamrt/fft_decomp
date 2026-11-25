/* The live units' sprites on the battle map: the indicators drawn around them (shadows, status bubbles,
 * afterimages, held items and animated action-result text), spritesheet resolution with palettes,
 * transparency and height, the animation chosen for actions, charging, statuses and target responses,
 * the sprites themselves, which run their unit and weapon SEQ scripts and load SHP, SEQ and SPR
 * assets, the map cursor's tile glow and Wait direction indicator, and unit fades, coordinates and
 * distortion animations. The original compiled the sprite code as one object; its data, defined in
 * address order at the top, is BATTLE's core table block, including tables other files read. */

#include "fft/battle/action.h"
#include "fft/battle/ai.h"
#include "fft/battle/battle_state.h"
#include "fft/battle/camera.h"
#include "fft/battle/command.h"
#include "fft/battle/cursor.h"
#include "fft/battle/deploy.h"
#include "fft/battle/move.h"
#include "fft/battle/projectile.h"
#include "fft/battle/result.h"
#include "fft/battle/weather.h"
#include "fft/effect/effect.h"
#include "fft/effect/effect_script.h"
#include "fft/effect/effect_secondary.h"
#include "fft/game/ability.h"
#include "fft/game/battle_unit.h"
#include "fft/game/item.h"
#include "fft/game/status.h"
#include "fft/gfx/animation.h"
#include "fft/gfx/gfx.h"
#include "fft/gfx/gfx_image.h"
#include "fft/gfx/sprite.h"
#include "fft/gfx/sprite_sheet.h"
#include "fft/map/map.h"
#include "fft/map/map_palette.h"
#include "fft/map/map_unit.h"
#include "fft/menu/option.h"
#include "fft/script/script_variable.h"
#include "fft/sys/file.h"
#include "fft/sys/math.h"
#include "fft/sys/memory.h"
#include "fft/sys/sound.h"
#include "fft/sys/system.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

static void set_real_coords_from_world_coords(map_unit_t* unit);
static s32 distortion_none(map_unit_t* unused_unit);
static s32 distortion_scale_pulse_update(map_unit_t* unit);
static s32 distortion_rise(map_unit_t* unit);
static s32 distortion_settle(map_unit_t* unit);
static s32 distortion_move_to_target(map_unit_t* unit);
static s32 distortion_move_to_own_tile(map_unit_t* unit);
static s32 distortion_move_level_to_target(map_unit_t* unit);
static s32 distortion_move_level_to_own_tile(map_unit_t* unit);
static s32 distortion_jump_to_target(map_unit_t* unit);
static s32 distortion_jump_to_own_tile(map_unit_t* unit);
static s32 distortion_clear(map_unit_t* unit);
static s32 distortion_clear_2(map_unit_t* unit);
static s32 distortion_jump_takeoff(map_unit_t* unit);
static s32 distortion_fall_to_target(map_unit_t* unit);
static s32 distortion_teleport(map_unit_t* unit);
static s32 distortion_back_away(map_unit_t* unit);
static s32 distortion_jump_to_own_tile_faded(map_unit_t* unit);
static s32 distortion_jump_rise(map_unit_t* unit);
static void dispatch_distortion(map_unit_t* unit);

/* WEP.SPR weapon/item graphics: palette rows and texture-row placement. */
typedef struct sprite_item_graphic {
    u8 palettes;    /* 0x00; g_sprite_weapon_clut rows: high nibble for MAP_CLUT_ROW_ITEM_A, low for _B */
    u8 texture_row; /* 0x01; top of the graphic on its texture page, in steps of 8 pixels */
} sprite_item_graphic_t;
typedef char sprite_item_graphic_size_must_be_2[(sizeof(sprite_item_graphic_t) == 2) ? 1 : -1];

enum {
    ANIMATION_DISTORTION_COUNT = 18 /* the handlers of s_distortion_handlers */
};

/* The dispatcher ignores the result and the handlers supply no value. The s32
 * callback shape preserves the pulse and teleport handlers' instruction scheduling. */
typedef s32 (*animation_distortion_handler_t)(map_unit_t* unit);

/* tables */

s32 g_map_deep_dungeon_load_in_progress = 0;

/* Advanced by g_system_vsync_interval once per call and sampled with bit 5 to blink
 * the highlighted arrow. */
static u32 s_unit_facing_wait_arrow_blink_timer = 0;

/* Status flag shown by each status bubble before SOURCE_UNIT. DEAD and DEATH_SENTENCE hold 0 and are
 * tested separately by update_bubble_cycle; bubbles 8 and 11 hold 0 too,
 * so they never show. */
static s32 s_status_bubble_status_masks[STATUS_BUBBLE_SOURCE_UNIT] = { 0, 0x00000040, 0x08000000, 0x02000000,
    0x01000000, 0x00004000, 0x00008000, 0x00010000, 0, 0x00000200, 0x10000000, 0, 0x00000080, 0x00020000, 0x00040000,
    0x00080000, 0x00100000, 0x00200000, 0x00400000, 0x00800000, 0 };

/* Post-action numeric and status displays. */
static u8 s_result_numeric_selectors[ACTION_DISPLAY_VALUE_COUNT]
    = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };

/* Eight per status_changes mask byte, indexed from the byte's high bit. */
static u8 s_result_status_image_ids[STATUS_BYTE_COUNT][8] = { { 0, 0, 0, 0x01, 0, 0, 0, 0 },
    { 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0, 0 }, { 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0, 0, 0 },
    { 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0 }, { 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b } };

/* sound_sfx_e by the weapon's item_type_e (0x20 entries each): the swing, the
 * hit, and the hit a shield or Golem stops (action_play_weapon_sfx). */
static u8 s_action_weapon_swing_sfx_ids[ITEM_TYPE_SHURIKEN] = { 0x19, 0x19, 0x19, 0x1a, 0x1a, 0x1a, 0x1b, 0x1a, 0x1a,
    0x1b, 0x5a, 0x56, 0x52, 0x19, 0x19, 0x1b, 0x1b, 0x19, 0x19, 0, 0, 0x1b, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static u8 s_action_weapon_hit_sfx_ids[ITEM_TYPE_SHURIKEN] = { 0x18, 0x13, 0x13, 0x13, 0x13, 0x13, 0x15, 0x38, 0x38,
    0x15, 0x5b, 0x74, 0x74, 0x9c, 0x9c, 0x3e, 0x3e, 0x18, 0x18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static u8 s_action_shield_deflect_sfx_ids[ITEM_TYPE_SHURIKEN] = { 0x15, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2f, 0x2e, 0x2e,
    0x2f, 0x47, 0x49, 0x49, 0x30, 0x30, 0x2e, 0x2e, 0x15, 0x15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* One handler for each distortion id; id 0 is skipped by the dispatcher. */
static animation_distortion_handler_t s_distortion_handlers[ANIMATION_DISTORTION_COUNT] = {
    distortion_none,
    distortion_scale_pulse_update,
    distortion_rise,
    distortion_settle,
    distortion_move_to_target,
    distortion_move_to_own_tile,
    distortion_jump_to_target,
    distortion_jump_to_own_tile,
    distortion_move_level_to_target,
    distortion_move_level_to_own_tile,
    distortion_clear,
    distortion_clear_2,
    distortion_jump_takeoff,
    distortion_fall_to_target,
    distortion_teleport,
    distortion_back_away,
    distortion_jump_to_own_tile_faded,
    distortion_jump_rise,
};

/* Two animation_e for each set: the one that starts a charge and
 * the one held while the unit charges or performs; 0 for none. */
static u8 s_animation_ability_charge_sets[ANIMATION_CHARGE_SET_COUNT][ANIMATION_CHARGE_VARIANT_COUNT]
    = { { 0x2a, 0x22 }, { 0x2a, 0x2b }, { 0x2a, 0x2b }, { 0x2a, 0x2b }, { 0, 0x22 }, { 0x2a, 0x2b }, { 0x2a, 0x2b },
          { 0, 0x22 }, { 0x2a, 0x22 }, { 0, 0x2b }, { 0x2a, 0x22 }, { 0x2a, 0x2b }, { 0x2a, 0x22 }, { 0, 0x28 },
          { 0, 0x29 }, { 0, 0x75 }, { 0x2a, 0x2b }, { 0, 0x16 }, { 0, 0x22 }, { 0, 0 } };

animation_ability_data_t g_ability_animation_data[ANIMATION_ABILITY_COUNT] = { { 0, 0x2c, 0 }, { 0x01, 0x2c, 0 },
    { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 },
    { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 },
    { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 },
    { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 },
    { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 },
    { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 },
    { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 },
    { 0x01, 0x2c, 0 }, { 0x01, 0x74, 0 }, { 0x01, 0x74, 0 }, { 0x01, 0x74, 0 }, { 0x01, 0x74, 0 }, { 0x01, 0x74, 0 },
    { 0x01, 0x74, 0 }, { 0x01, 0x74, 0 }, { 0x01, 0x74, 0 }, { 0x01, 0x74, 0 }, { 0x01, 0x74, 0 }, { 0x01, 0x74, 0 },
    { 0x01, 0x74, 0 }, { 0x01, 0x74, 0 }, { 0x01, 0x74, 0 }, { 0x01, 0x74, 0 }, { 0x02, 0x2c, 0 }, { 0x02, 0x2c, 0 },
    { 0x02, 0x2c, 0 }, { 0x02, 0x2c, 0 }, { 0x02, 0x2c, 0 }, { 0x02, 0x2c, 0 }, { 0x02, 0x2c, 0 }, { 0x02, 0x2c, 0 },
    { 0x02, 0x2c, 0 }, { 0x02, 0x2c, 0 }, { 0x02, 0x2c, 0 }, { 0x02, 0x2c, 0 }, { 0x02, 0x2c, 0 }, { 0x02, 0x2c, 0 },
    { 0x02, 0x2c, 0 }, { 0x02, 0x2c, 0 }, { 0x03, 0x73, 0 }, { 0x03, 0x73, 0 }, { 0x03, 0x73, 0 }, { 0x03, 0x73, 0 },
    { 0x03, 0x73, 0 }, { 0x03, 0x73, 0 }, { 0x03, 0x73, 0 }, { 0x03, 0x73, 0 }, { 0x03, 0x73, 0 }, { 0x03, 0x73, 0 },
    { 0x0d, 0x28, 0 }, { 0x0d, 0x28, 0 }, { 0x0d, 0x28, 0 }, { 0x0d, 0x28, 0 }, { 0x0d, 0x28, 0 }, { 0x0d, 0x28, 0 },
    { 0x0d, 0x28, 0 }, { 0x0e, 0x6f, 0 }, { 0x0e, 0x6f, 0 }, { 0x0e, 0x6f, 0 }, { 0x0e, 0x6f, 0 }, { 0x0e, 0x6f, 0 },
    { 0x0e, 0x6f, 0 }, { 0x0e, 0x6f, 0 }, { 0x04, 0x67, 0 }, { 0x04, 0x68, 0 }, { 0x04, 0x3e, 0 }, { 0x04, 0x3e, 0 },
    { 0x04, 0x3e, 0 }, { 0x04, 0x69, 0 }, { 0x04, 0x69, 0 }, { 0x04, 0x69, 0 }, { 0x11, 0x71, 0 }, { 0x11, 0x72, 0 },
    { 0x11, 0x71, 0 }, { 0x11, 0x71, 0 }, { 0x11, 0x71, 0 }, { 0x11, 0x71, 0 }, { 0x11, 0x71, 0 }, { 0x11, 0x71, 0 },
    { 0x06, 0x6c, 0x06 }, { 0x06, 0x6c, 0 }, { 0x06, 0x6c, 0 }, { 0x06, 0x6c, 0 }, { 0x06, 0x6c, 0 }, { 0x06, 0x6c, 0 },
    { 0x06, 0x6c, 0 }, { 0x06, 0x6c, 0 }, { 0x06, 0x6c, 0x0e }, { 0x06, 0x6c, 0x0f }, { 0x03, 0x2c, 0 },
    { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 },
    { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x07, 0, 0 },
    { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 },
    { 0x07, 0x16, 0 }, { 0x04, 0x6a, 0 }, { 0x11, 0x4c, 0 }, { 0x03, 0x66, 0x16 }, { 0x03, 0x66, 0 }, { 0x03, 0x66, 0 },
    { 0x03, 0x66, 0 }, { 0x11, 0x22, 0x19 }, { 0x01, 0x2c, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 },
    { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 },
    { 0x07, 0, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x06, 0x65, 0 }, { 0x06, 0x65, 0 },
    { 0x06, 0x65, 0 }, { 0x06, 0x65, 0 }, { 0x06, 0x65, 0 }, { 0x06, 0x65, 0 }, { 0x06, 0x65, 0 }, { 0x06, 0x65, 0 },
    { 0x06, 0x65, 0 }, { 0x06, 0x65, 0 }, { 0x06, 0x65, 0 }, { 0x06, 0x65, 0 }, { 0x05, 0x6d, 0 }, { 0x05, 0x6d, 0 },
    { 0x05, 0x3e, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 },
    { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 }, { 0x03, 0x2c, 0 },
    { 0x03, 0x2c, 0 }, { 0x01, 0, 0 }, { 0x01, 0, 0 }, { 0x01, 0, 0 }, { 0x01, 0, 0 }, { 0x11, 0x16, 0x23 },
    { 0x03, 0x2c, 0x03 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 },
    { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x03, 0x2c, 0 },
    { 0x07, 0, 0x1f }, { 0x07, 0, 0x1e }, { 0x07, 0, 0x27 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 },
    { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 },
    { 0x0a, 0x41, 0 }, { 0x0a, 0x41, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 },
    { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0x2c, 0 }, { 0x01, 0, 0 }, { 0x01, 0, 0 }, { 0x01, 0, 0 },
    { 0x01, 0, 0 }, { 0x01, 0, 0 }, { 0x01, 0, 0 }, { 0x01, 0, 0 }, { 0x01, 0, 0 }, { 0x01, 0, 0 }, { 0x01, 0, 0 },
    { 0x01, 0, 0 }, { 0x01, 0, 0 }, { 0x01, 0, 0 }, { 0x01, 0, 0 }, { 0x08, 0x65, 0 }, { 0x08, 0x65, 0 },
    { 0x08, 0x65, 0 }, { 0x06, 0x66, 0x28 }, { 0x06, 0x66, 0 }, { 0x06, 0x66, 0 }, { 0x06, 0x66, 0 }, { 0x08, 0x65, 0 },
    { 0x01, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x07, 0, 0 },
    { 0x07, 0, 0 }, { 0x07, 0, 0 }, { 0x0a, 0, 0 }, { 0x0a, 0x41, 0 }, { 0x0b, 0x42, 0 }, { 0x0b, 0x44, 0 },
    { 0x0b, 0x44, 0 }, { 0x0a, 0x6a, 0 }, { 0x0a, 0x7d, 0 }, { 0x0a, 0x67, 0 }, { 0x0a, 0, 0 }, { 0x0a, 0x6a, 0 },
    { 0x0a, 0, 0 }, { 0x0a, 0x41, 0 }, { 0x0a, 0x61, 0x24 }, { 0x0a, 0x42, 0 }, { 0x0b, 0x62, 0 }, { 0x0a, 0x43, 0 },
    { 0x0a, 0x63, 0 }, { 0x04, 0x22, 0 }, { 0x0a, 0x45, 0 }, { 0x04, 0x22, 0 }, { 0x0a, 0, 0 }, { 0x0c, 0x65, 0 },
    { 0x0b, 0x29, 0 }, { 0x0b, 0x2c, 0x2a }, { 0x0b, 0x2c, 0 }, { 0x0a, 0x3d, 0 }, { 0x0a, 0x65, 0 }, { 0x0a, 0x65, 0 },
    { 0x0a, 0x65, 0 }, { 0x0a, 0x65, 0 }, { 0x0a, 0x46, 0 }, { 0x0a, 0x47, 0 }, { 0x0a, 0x47, 0 }, { 0x0a, 0x47, 0 },
    { 0x0a, 0x48, 0 }, { 0x0a, 0x49, 0 }, { 0x0b, 0x64, 0 }, { 0x0b, 0x64, 0 }, { 0x0b, 0x64, 0 }, { 0x0b, 0x64, 0 },
    { 0x0a, 0x65, 0 }, { 0x0a, 0, 0 }, { 0x0a, 0x66, 0 }, { 0x0a, 0x67, 0 }, { 0x0a, 0x68, 0 }, { 0x0a, 0x69, 0 },
    { 0x0a, 0x6a, 0 }, { 0x0a, 0x4b, 0 }, { 0x0a, 0x6b, 0 }, { 0x0a, 0x4c, 0 }, { 0x0b, 0x4d, 0 }, { 0x0b, 0x4d, 0 },
    { 0x0b, 0x4d, 0 }, { 0x0b, 0x4d, 0 }, { 0x0b, 0x4d, 0 }, { 0x0a, 0, 0 }, { 0x0a, 0x6c, 0 }, { 0x0a, 0x4f, 0 },
    { 0x0a, 0x50, 0 }, { 0x0c, 0x51, 0 }, { 0x0a, 0, 0 }, { 0x0a, 0x53, 0 }, { 0x0a, 0x6d, 0 }, { 0x0a, 0x6d, 0 },
    { 0x0a, 0x6d, 0x2b }, { 0x0a, 0x6e, 0 }, { 0x0a, 0x6f, 0 }, { 0x0b, 0x54, 0 }, { 0x0b, 0x54, 0 }, { 0x0b, 0x54, 0 },
    { 0x0a, 0x55, 0 }, { 0x0a, 0, 0 }, { 0x0c, 0x57, 0 }, { 0x0c, 0x57, 0 }, { 0x0c, 0x57, 0 }, { 0x0a, 0x58, 0 },
    { 0x0c, 0x59, 0 }, { 0x0b, 0x70, 0 }, { 0x0b, 0x70, 0 }, { 0x0b, 0x70, 0 }, { 0x01, 0, 0 }, { 0x01, 0x2c, 0 },
    { 0x0b, 0x2c, 0 }, { 0x0b, 0, 0 }, { 0x0b, 0x72, 0 }, { 0x0b, 0x72, 0 }, { 0x0a, 0x73, 0 }, { 0x0a, 0x74, 0 },
    { 0x0a, 0x75, 0 }, { 0x0a, 0x76, 0 }, { 0x0a, 0x5c, 0 }, { 0x0a, 0x71, 0 }, { 0, 0x2c, 0 }, { 0, 0x2c, 0 },
    { 0, 0x2c, 0 }, { 0, 0x2c, 0 }, { 0, 0x2c, 0 }, { 0, 0x2c, 0 }, { 0, 0x2c, 0 }, { 0, 0x2c, 0 }, { 0, 0x2c, 0 },
    { 0, 0x2c, 0 }, { 0, 0, 0 }, { 0x09, 0x01, 0 }, { 0x09, 0x01, 0 }, { 0x09, 0x01, 0 }, { 0x09, 0x01, 0 },
    { 0x09, 0x01, 0 }, { 0x09, 0x01, 0 }, { 0x09, 0x01, 0 }, { 0x09, 0x01, 0 }, { 0x09, 0x01, 0 }, { 0x09, 0x01, 0 },
    { 0x09, 0x01, 0 }, { 0x09, 0x01, 0 }, { 0x09, 0x01, 0 }, { 0x09, 0x01, 0 }, { 0x12, 0x4c, 0 }, { 0x12, 0x4c, 0 },
    { 0x12, 0x4c, 0 }, { 0x12, 0x4c, 0 }, { 0x12, 0x4c, 0 }, { 0x12, 0x4c, 0 }, { 0x12, 0x4c, 0 }, { 0x12, 0x4c, 0 },
    { 0x12, 0x4c, 0 }, { 0x12, 0x4c, 0 }, { 0x12, 0x4c, 0 }, { 0x12, 0x4c, 0 }, { 0x0f, 0x76, 0 }, { 0x0f, 0x76, 0 },
    { 0x0f, 0x76, 0 }, { 0x0f, 0x76, 0 }, { 0x0f, 0x76, 0 }, { 0x0f, 0x76, 0 }, { 0x0f, 0x76, 0 }, { 0x0f, 0x76, 0 },
    { 0x0f, 0x76, 0 }, { 0x0f, 0x76, 0 }, { 0x0f, 0x76, 0 }, { 0x0f, 0x76, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
    { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0x2c, 0 }, { 0, 0x2c, 0 }, { 0, 0x2c, 0 },
    { 0, 0x2c, 0 }, { 0, 0x2c, 0 }, { 0, 0x2c, 0 }, { 0, 0x2c, 0 }, { 0, 0x2c, 0 }, { 0x10, 0x69, 0 },
    { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x6d, 0 }, { 0x10, 0x6d, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 },
    { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0, 0 },
    { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x69, 0x2f }, { 0x10, 0x69, 0 },
    { 0x10, 0x69, 0 }, { 0x10, 0, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 },
    { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 }, { 0x10, 0x69, 0 },
    { 0x10, 0x69, 0 } };

/* Three animation_e variants per item_type_e; animation_select_weapon_attack chooses the column. */
static u8 s_animation_weapon_attacks[ITEM_TYPE_SHURIKEN][3] = { { 0x3d, 0x3e, 0x3f }, { 0x40, 0x41, 0x42 },
    { 0x40, 0x41, 0x42 }, { 0x40, 0x41, 0x42 }, { 0x40, 0x41, 0x42 }, { 0x40, 0x41, 0x42 }, { 0x40, 0x41, 0x42 },
    { 0x40, 0x41, 0x42 }, { 0x40, 0x41, 0x42 }, { 0x40, 0x41, 0x42 }, { 0x50, 0x51, 0x52 }, { 0x53, 0x54, 0x55 },
    { 0x53, 0x54, 0x55 }, { 0x57, 0x57, 0x57 }, { 0x56, 0x56, 0x56 }, { 0x4d, 0x4e, 0x4f }, { 0x4d, 0x4e, 0x4f },
    { 0x77, 0x78, 0x79 }, { 0x77, 0x78, 0x79 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
    { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };

/* Projectile secondary animation of each weapon item type: BOW_ARROW for bows, CROSSBOW_BOLT for
 * crossbows, NONE for the rest. */
u8 g_weapon_projectile_animations[32]
    = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static sprite_item_graphic_t s_sprite_item_graphic_data[146] = { { 0, 0 }, { 224, 0 }, { 240, 2 }, { 224, 4 },
    { 48, 6 }, { 208, 0 }, { 48, 2 }, { 81, 4 }, { 64, 6 }, { 64, 2 }, { 130, 4 }, { 224, 8 }, { 224, 10 }, { 240, 8 },
    { 48, 8 }, { 64, 10 }, { 208, 8 }, { 96, 10 }, { 82, 10 }, { 224, 0 }, { 224, 2 }, { 48, 4 }, { 240, 6 }, { 96, 4 },
    { 112, 2 }, { 128, 6 }, { 240, 0 }, { 208, 6 }, { 64, 4 }, { 112, 2 }, { 80, 0 }, { 129, 2 }, { 129, 2 },
    { 128, 12 }, { 240, 12 }, { 128, 14 }, { 224, 14 }, { 80, 12 }, { 208, 16 }, { 208, 18 }, { 208, 20 }, { 80, 16 },
    { 64, 18 }, { 48, 20 }, { 96, 16 }, { 224, 18 }, { 240, 20 }, { 128, 16 }, { 208, 0 }, { 176, 0 }, { 224, 0 },
    { 224, 22 }, { 146, 24 }, { 145, 22 }, { 192, 24 }, { 144, 22 }, { 240, 24 }, { 160, 22 }, { 176, 24 }, { 208, 26 },
    { 112, 26 }, { 64, 26 }, { 224, 26 }, { 208, 28 }, { 64, 28 }, { 80, 28 }, { 112, 28 }, { 240, 3 }, { 144, 3 },
    { 160, 3 }, { 208, 3 }, { 209, 0 }, { 161, 0 }, { 225, 0 }, { 209, 2 }, { 225, 2 }, { 177, 2 }, { 224, 0 },
    { 240, 0 }, { 160, 0 }, { 176, 3 }, { 48, 3 }, { 240, 3 }, { 208, 0 }, { 112, 0 }, { 224, 0 }, { 240, 0 },
    { 48, 0 }, { 224, 3 }, { 192, 3 }, { 176, 3 }, { 240, 3 }, { 128, 0 }, { 96, 0 }, { 240, 0 }, { 208, 0 },
    { 224, 0 }, { 160, 0 }, { 176, 0 }, { 208, 0 }, { 208, 2 }, { 240, 0 }, { 160, 2 }, { 64, 0 }, { 240, 2 },
    { 192, 0 }, { 128, 2 }, { 192, 4 }, { 160, 6 }, { 240, 4 }, { 48, 6 }, { 96, 4 }, { 192, 6 }, { 224, 4 },
    { 176, 6 }, { 226, 0 }, { 162, 0 }, { 210, 0 }, { 210, 0 }, { 242, 0 }, { 162, 0 }, { 146, 0 }, { 224, 0 },
    { 96, 0 }, { 64, 0 }, { 96, 0 }, { 112, 0 }, { 128, 0 }, { 80, 0 }, { 224, 0 }, { 224, 6 }, { 48, 0 }, { 160, 6 },
    { 240, 0 }, { 128, 6 }, { 112, 0 }, { 96, 6 }, { 96, 0 }, { 208, 6 }, { 208, 0 }, { 208, 6 }, { 112, 0 }, { 48, 6 },
    { 128, 0 }, { 128, 6 }, { 0, 0 } };

/* The display rotations that the upper five bits of a frame's first byte select, ONE to the turn. */
static u16 s_sprite_y_rotations[32] = { 0, 182, 262, 301, 304, 307, 341, 489, 512, 626, 683, 797, 1024, 1365, 1366,
    1422, 1456, 1479, 1536, 1593, 1650, 1707, 1741, 1763, 1821, 1877, 0, 0, 0, 0, 0, 0 };

/* The 24 orders in which draw_layers queues a unit's
 * four layers: 0 is the unit's own sprite, n its sprite block n - 1. */
static s32 s_animation_layer_priorities[24][4] = { { 0, 1, 2, 3 }, { 0, 1, 3, 2 }, { 0, 2, 1, 3 }, { 0, 3, 1, 2 },
    { 0, 2, 3, 1 }, { 0, 3, 2, 1 }, { 1, 0, 2, 3 }, { 1, 0, 3, 2 }, { 2, 0, 1, 3 }, { 3, 0, 1, 2 }, { 2, 0, 3, 1 },
    { 3, 0, 2, 1 }, { 1, 2, 0, 3 }, { 1, 3, 0, 2 }, { 2, 1, 0, 3 }, { 3, 1, 0, 2 }, { 2, 3, 0, 1 }, { 3, 2, 0, 1 },
    { 1, 2, 3, 0 }, { 1, 3, 2, 0 }, { 2, 1, 3, 0 }, { 3, 1, 2, 0 }, { 2, 3, 1, 0 }, { 3, 2, 1, 0 } };

/* Sprite part width/height in 8-pixel tiles, indexed by SHP size bits. */
static size32_t s_sprite_part_sizes[16] = { { 1, 1 }, { 2, 1 }, { 2, 2 }, { 2, 3 }, { 3, 1 }, { 3, 2 }, { 3, 3 },
    { 4, 1 }, { 4, 2 }, { 4, 3 }, { 4, 4 }, { 4, 5 }, { 5, 2 }, { 5, 4 }, { 6, 6 }, { 7, 7 } };

sprite_sheet_data_t g_sprite_sheet_data[SPRITE_SHEET_ID_COUNT] = { { 0, 0, 0, 0 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 },
    { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 },
    { 0, 0, 0, 36 }, { 1, 1, 0, 36 }, { 0, 0, 0, 36 }, { 1, 1, 0, 36 }, { 0, 0, 0, 36 }, { 1, 1, 0, 36 },
    { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 },
    { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 0, 0, 0, 36 },
    { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 },
    { 1, 1, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 },
    { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 1, 1, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 1, 1, 0, 36 },
    { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 0, 0, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 0, 0, 0, 36 },
    { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 },
    { 0, 0, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 3, 5, 0, 36 }, { 0, 0, 0, 36 }, { 3, 5, 0, 36 },
    { 0, 0, 0, 36 }, { 3, 5, 0, 36 }, { 6, 6, 0, 60 }, { 1, 1, 0, 36 }, { 3, 5, 0, 36 }, { 1, 1, 0, 36 },
    { 3, 5, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 3, 3, 0, 36 }, { 7, 7, 0, 120 }, { 1, 1, 0, 36 },
    { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 },
    { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 },
    { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 },
    { 3, 3, 0, 36 }, { 0, 0, 0, 36 }, { 1, 1, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 1, 1, 0, 36 },
    { 1, 1, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 },
    { 0, 0, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 },
    { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 1, 1, 0, 36 },
    { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 0, 0, 0, 36 },
    { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 0, 0, 0, 36 }, { 1, 1, 0, 36 },
    { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 1, 1, 0, 36 }, { 2, 2, 0, 36 },
    { 0, 0, 0, 36 }, { 3, 3, 0, 36 }, { 3, 3, 0, 36 }, { 1, 1, 0, 36 }, { 0, 0, 0, 36 }, { 3, 3, 0, 36 },
    { 3, 3, 1, 36 }, { 3, 3, 1, 36 }, { 3, 3, 0, 36 }, { 3, 3, 0, 36 }, { 3, 3, 0, 36 }, { 3, 3, 0, 36 },
    { 3, 3, 0, 36 }, { 3, 3, 0, 36 }, { 3, 3, 1, 36 }, { 3, 3, 0, 36 }, { 3, 5, 0, 36 }, { 3, 3, 0, 36 },
    { 3, 3, 0, 36 }, { 3, 3, 0, 36 }, { 4, 4, 0, 36 }, { 4, 4, 1, 36 }, { 4, 4, 0, 36 }, { 4, 4, 0, 36 } };

static u8 s_animation_targeted_frame_front[9] = { 11, 11, 2, 2, 1, 2, 1, 1, 0 };

/* By sprite_shp_type_e: the frame a unit is left on when its sequence ends or its attack image is taken away, seen
 * from behind and from the front. */
static u8 s_animation_targeted_frame_back[9] = { 16, 16, 4, 4, 6, 4, 10, 10, 0 };

/* Texture coordinates for the status bubble graphics, indexed by status bubble
 * id (0x800949dc/0x800949f4) and by a counter digit (0x80094a0c/0x80094a24).
 * Each table holds 24 bytes; the pairs are read together as one u/v. */
static u8 s_status_bubble_u[24]
    = { 0, 16, 30, 44, 58, 72, 86, 100, 114, 128, 142, 0, 16, 30, 44, 58, 72, 86, 100, 86, 0, 0, 0, 0 };

static u8 s_status_bubble_v[24] = { 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 188, 188, 188, 188, 188, 188,
    188, 188, 200, 176, 0, 0, 0 };

static u8 s_status_bubble_digit_u[24]
    = { 142, 128, 114, 100, 86, 142, 128, 142, 128, 114, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static u8 s_status_bubble_digit_v[24]
    = { 200, 200, 200, 200, 200, 188, 188, 176, 176, 176, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* The four random-battle music tracks (0x04, 0x05, 0x0e, 0x0f); map initialization plays the
 * one at rand() / 0x2000. */
u8 g_map_random_battle_music_tracks[4] = { 4, 5, 0xe, 0xf };

u32 g_status_mask_by_handler_index[STATUS_COUNT + 1] = { 0, 0, 0x00000001, 0x00000004, 0x40000000, 0x00000200,
    0x00000040, 0x00000400, 0x00000100, 0x00000020, 0, 0x08000000, 0x00000080, 0x02000000, 0x00020000, 0x00002000,
    0x00000008, 0x00800000, 0x00000020, 0x10000000, 0x00000010, 0x00004000, 0x00000002, 0x00000004, 0x00000008,
    0x00400000, 0x20000000, 0x00200000, 0x00100000, 0x00001000, 0x00000800, 0x00000010, 0, 0x00008000, 0x00010000,
    0x01000000, 0x00000040, 0x00040000, 0x00080000, 0, 0x04000000 };

/* The tile glow palettes the cursor cycles; nothing loads the third. */
palette_t g_cursor_tile_glow_palettes[3]
    = { { { 0, 0, 0, 0 }, { 6, 9, 24, 1 }, { 7, 10, 22, 1 }, { 8, 11, 20, 1 }, { 9, 12, 19, 1 }, { 10, 13, 17, 1 },
            { 11, 14, 16, 1 }, { 12, 15, 15, 1 }, { 14, 15, 14, 1 }, { 12, 15, 15, 1 }, { 11, 14, 16, 1 },
            { 10, 13, 17, 1 }, { 9, 12, 19, 1 }, { 8, 11, 20, 1 }, { 7, 10, 22, 1 }, { 6, 9, 24, 1 } },
          { { 0, 0, 0, 0 }, { 20, 5, 5, 1 }, { 21, 6, 5, 1 }, { 22, 7, 7, 1 }, { 23, 7, 9, 1 }, { 25, 8, 11, 1 },
              { 26, 10, 15, 1 }, { 27, 12, 20, 1 }, { 29, 14, 25, 1 }, { 27, 11, 19, 1 }, { 26, 8, 13, 1 },
              { 25, 7, 11, 1 }, { 23, 7, 10, 1 }, { 22, 6, 8, 1 }, { 21, 5, 6, 1 }, { 20, 5, 5, 1 } },
          { { 0, 0, 0, 0 }, { 5, 5, 4, 0 }, { 29, 29, 29, 0 }, { 8, 9, 10, 0 }, { 12, 13, 14, 0 }, { 15, 17, 19, 0 },
              { 22, 23, 24, 0 }, { 14, 7, 6, 0 }, { 18, 6, 4, 0 }, { 22, 8, 5, 0 }, { 28, 11, 5, 0 }, { 12, 7, 3, 0 },
              { 20, 11, 4, 0 }, { 23, 15, 6, 0 }, { 26, 19, 8, 0 }, { 28, 25, 14, 0 } } };

palette_t g_unit_facing_wait_arrow_palette = { { 0, 0, 0, 0 }, { 4, 4, 3, 0 }, { 31, 31, 29, 0 }, { 8, 9, 10, 0 },
    { 12, 13, 14, 0 }, { 15, 17, 19, 0 }, { 22, 23, 24, 0 }, { 2, 1, 0, 1 }, { 4, 2, 0, 1 }, { 6, 3, 1, 1 },
    { 8, 4, 2, 1 }, { 12, 7, 3, 1 }, { 19, 11, 5, 0 }, { 23, 15, 6, 0 }, { 26, 19, 8, 0 }, { 28, 25, 14, 0 } };

palette_t g_cursor_map_selection_palette = { { 0, 0, 0, 0 }, { 2, 2, 2, 1 }, { 6, 6, 6, 1 }, { 10, 10, 10, 1 },
    { 15, 15, 15, 1 }, { 19, 19, 19, 1 }, { 26, 26, 26, 1 }, { 9, 10, 12, 0 }, { 14, 15, 17, 0 }, { 20, 21, 23, 0 },
    { 25, 27, 28, 0 }, { 13, 10, 7, 0 }, { 17, 12, 8, 0 }, { 21, 15, 9, 0 }, { 25, 20, 12, 0 }, { 29, 25, 15, 0 } };

palette_t g_weather_particle_palette = { { 0, 0, 0, 0 }, { 28, 28, 28, 1 }, { 25, 25, 25, 1 }, { 23, 23, 23, 1 },
    { 21, 21, 21, 1 }, { 19, 19, 19, 1 }, { 17, 17, 17, 1 }, { 15, 15, 15, 1 }, { 13, 13, 13, 1 }, { 11, 11, 11, 1 },
    { 9, 9, 9, 1 }, { 7, 7, 7, 1 }, { 5, 5, 5, 1 }, { 3, 3, 3, 1 }, { 1, 1, 1, 1 }, { 1, 1, 1, 0 } };

static palette_t s_result_palette = { { 0, 0, 0, 0 }, { 5, 5, 4, 1 }, { 29, 29, 29, 1 }, { 8, 9, 10, 1 },
    { 12, 13, 14, 1 }, { 15, 17, 19, 1 }, { 22, 23, 24, 1 }, { 14, 7, 6, 1 }, { 18, 6, 4, 1 }, { 22, 8, 5, 1 },
    { 28, 11, 5, 1 }, { 12, 7, 3, 1 }, { 20, 11, 4, 1 }, { 23, 15, 6, 1 }, { 26, 19, 8, 1 }, { 28, 25, 14, 1 } };

palette_t g_sprite_shadow_palette = { { 0, 0, 0, 0 }, { 28, 28, 28, 1 }, { 25, 25, 25, 1 }, { 23, 23, 23, 1 },
    { 21, 21, 21, 1 }, { 19, 19, 19, 1 }, { 17, 17, 17, 1 }, { 15, 15, 15, 1 }, { 13, 13, 13, 1 }, { 11, 11, 11, 1 },
    { 9, 9, 9, 1 }, { 7, 7, 7, 1 }, { 5, 5, 5, 1 }, { 3, 3, 3, 1 }, { 1, 1, 1, 1 }, { 0, 0, 0, 0 } };

palette_t g_status_bubble_palette = { { 0, 0, 0, 0 }, { 5, 5, 4, 0 }, { 29, 29, 29, 0 }, { 8, 9, 10, 0 },
    { 12, 13, 14, 0 }, { 15, 17, 19, 0 }, { 22, 23, 24, 0 }, { 14, 7, 6, 0 }, { 18, 6, 4, 0 }, { 22, 8, 5, 0 },
    { 28, 11, 5, 0 }, { 12, 7, 3, 0 }, { 20, 11, 4, 0 }, { 23, 15, 6, 0 }, { 26, 19, 8, 0 }, { 28, 25, 14, 0 } };

static file_extent_t s_seq_files[9] = { { 57037, 8192 }, { 57045, 8192 }, { 57053, 4096 }, { 57055, 6144 },
    { 57058, 4096 }, { 57060, 4096 }, { 57062, 4096 }, { 57064, 4096 }, { 0, 0 } };

static file_extent_t s_shp_files[9] = { { 57017, 8192 }, { 57021, 10240 }, { 57026, 8192 }, { 57030, 4096 },
    { 57032, 4096 }, { 0, 0 }, { 57034, 2048 }, { 57035, 4096 }, { 0, 0 } };

static file_extent_t s_weapon_seq_files[2] = { { 57072, 4096 }, { 57074, 4096 } };

static file_extent_t s_weapon_shp_files[2] = { { 57066, 6144 }, { 57069, 6144 } };

static file_extent_t s_effect_seq_files[2] = { { 57080, 2048 }, { 0, 0 } };

static file_extent_t s_effect_shp_files[2] = { { 57076, 4096 }, { 0, 0 } };

static file_extent_t s_sheet_files[159] = { { 0, 0 }, { 59021, 45056 }, { 59043, 45056 }, { 59065, 45056 },
    { 57586, 45056 }, { 57608, 45056 }, { 57630, 45056 }, { 57229, 45056 }, { 59772, 45056 }, { 57519, 45056 },
    { 58998, 47104 }, { 57913, 45056 }, { 58247, 45056 }, { 58954, 45056 }, { 57805, 45056 }, { 59087, 45056 },
    { 59839, 47104 }, { 57363, 45056 }, { 57827, 43008 }, { 59308, 47104 }, { 57935, 45056 }, { 58932, 45056 },
    { 57848, 45056 }, { 57957, 45056 }, { 57652, 45056 }, { 58976, 45056 }, { 58600, 43008 }, { 57720, 45056 },
    { 57742, 38912 }, { 57319, 45056 }, { 58356, 45056 }, { 57408, 45056 }, { 59685, 45056 }, { 57341, 45056 },
    { 58800, 45056 }, { 59132, 45056 }, { 59619, 45056 }, { 57979, 45056 }, { 58001, 45056 }, { 58023, 45056 },
    { 58045, 45056 }, { 58067, 45056 }, { 58089, 45056 }, { 58111, 45056 }, { 58133, 45056 }, { 58155, 45056 },
    { 57251, 45056 }, { 58177, 45056 }, { 57273, 45056 }, { 57186, 45056 }, { 57473, 43008 }, { 59794, 45056 },
    { 57164, 45056 }, { 58334, 45056 }, { 59353, 49152 }, { 58509, 47104 }, { 58888, 45056 }, { 58621, 45056 },
    { 59286, 45056 }, { 58578, 45056 }, { 59596, 47104 }, { 58465, 45056 }, { 59816, 47104 }, { 59707, 45056 },
    { 58199, 49152 }, { 57295, 49152 }, { 58509, 47104 }, { 58554, 49152 }, { 59532, 45056 }, { 57141, 47104 },
    { 58888, 45056 }, { 59421, 45056 }, { 59109, 47104 }, { 58378, 49152 }, { 59862, 38912 }, { 59881, 38912 },
    { 59900, 38912 }, { 59919, 38912 }, { 59938, 38912 }, { 59957, 38912 }, { 59976, 38912 }, { 59995, 38912 },
    { 60014, 38912 }, { 60033, 38912 }, { 60052, 38912 }, { 60071, 38912 }, { 60090, 47104 }, { 58621, 45056 },
    { 58621, 45056 }, { 58621, 45056 }, { 58621, 45056 }, { 58621, 45056 }, { 58621, 45056 }, { 57541, 45056 },
    { 58402, 40960 }, { 58422, 40960 }, { 58621, 45056 }, { 58643, 45056 }, { 58312, 45056 }, { 58334, 45056 },
    { 58465, 45056 }, { 58487, 45056 }, { 59707, 45056 }, { 59729, 45056 }, { 58712, 45056 }, { 58734, 45056 },
    { 59331, 45056 }, { 59353, 49152 }, { 58509, 47104 }, { 58532, 45056 }, { 59510, 45056 }, { 59532, 45056 },
    { 59398, 47104 }, { 59421, 45056 }, { 59466, 45056 }, { 59488, 45056 }, { 59641, 45056 }, { 59663, 45056 },
    { 58888, 45056 }, { 58910, 45056 }, { 57761, 45056 }, { 57783, 45056 }, { 59154, 45056 }, { 59176, 45056 },
    { 59198, 45056 }, { 59220, 45056 }, { 58822, 45056 }, { 58844, 45056 }, { 59242, 45056 }, { 59264, 45056 },
    { 57870, 45056 }, { 58866, 45056 }, { 58756, 45056 }, { 58778, 45056 }, { 57494, 51200 }, { 57892, 43008 },
    { 57452, 43008 }, { 58269, 45056 }, { 58291, 43008 }, { 59377, 43008 }, { 59751, 43008 }, { 57208, 43008 },
    { 59554, 45056 }, { 59576, 40960 }, { 58442, 47104 }, { 58665, 49152 }, { 58689, 47104 }, { 57385, 47104 },
    { 57674, 47104 }, { 57697, 47104 }, { 57430, 45056 }, { 58223, 49152 }, { 57674, 47104 }, { 57563, 47104 },
    { 59443, 47104 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };

file_extent_t g_sprite_sp2_files[SPRITE_SHEET_ID_COUNT] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 60161, 32768 },
    { 60209, 32768 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 60113, 32768 }, { 60321, 32768 }, { 60337, 32768 }, { 0, 0 },
    { 60289, 32768 }, { 60305, 32768 }, { 60129, 32768 }, { 0, 0 }, { 60193, 32768 }, { 60145, 32768 }, { 0, 0 },
    { 0, 0 }, { 60177, 32768 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };

file_extent_t g_sprite_worker8_sp2_files[4]
    = { { 60273, 32768 }, { 60257, 32768 }, { 60225, 32768 }, { 60241, 32768 } };

/* First 21 OTHER.SPR palettes, copied together and selected by status sheet. */
typedef struct sprite_other_palettes {
    palette_t chicken[5];  /* 0x000 */
    palette_t frog[5];     /* 0x0a0 */
    palette_t crystal[6];  /* 0x140 */
    palette_t treasure[5]; /* 0x200 */
} sprite_other_palettes_t;
typedef char sprite_other_palettes_size_must_be_672[sizeof(sprite_other_palettes_t) == 672 ? 1 : -1];

extern sprite_other_palettes_t g_sprite_other_palettes;

/* Loaded SHP frame and SEQ sequence tables and the WEP/EFF graphics. */
extern sprite_unit_shp_frame_tables_t g_sprite_unit_shp_frame_tables[5];
extern sprite_native_unit_seq_table_t g_sprite_unit_seq_tables[5];
extern sprite_unit_shp_frame_tables_t g_sprite_boss_shp_frame_tables;
extern sprite_native_unit_seq_table_t g_sprite_boss_seq_tables;
extern sprite_wep_eff_graphics_t g_sprite_weapon_graphics[2];
extern sprite_wep_eff_graphics_t g_sprite_effect_graphics[1];

/* indicators */

extern s32 g_cursor_selection_bob_timer;
/* Frame counter and frame index of the selection cursor's bob animation. */
extern s32 g_cursor_selection_bob_frame;

extern u16 g_result_display_values[ACTION_DISPLAY_VALUE_COUNT];
/* Queue a unit's ground shadow quad.
 *
 * The four shadow_quad corners are projected into a semi-transparent POLY_FT4.
 * A unit below the water surface of its tile (world y greater than
 * -(height + depth) * 12, negative y being up) gets the additive water image of
 * the FRAME.BIN page, whose UVs turn a quarter every 16 counts of
 * shadow_counter (advanced by the frame interval); any other unit gets the
 * subtractive shadow image of the same page.
 *
 * A unit at or below the terrain under its event and SEQ adjusted position uses
 * the caller's ordering-table entry. A unit above it (jumping, floating or on
 * water) is sorted at the depth of the ground or water surface below it.
 *
 * The dirty test sets bit 0 of shadow_dirty before testing it, so the quad is
 * recalculated on every call.
 *
 * `unused` reproduces the target's 0x58-byte frame. */
void sprite_draw_shadow(map_unit_t* unit, u32* ot) {
    VECTOR transformed;
    SVECTOR position;
    SVECTOR offset;
    SVECTOR unused;
    s32 flag;
    POLY_FT4* prim;
    map_tile_t* tile;
    SVECTOR* corners;
    u32* packet;
    s32 depth;
    u32 frame;

    if ((unit->shadow_dirty |= 1) != 0) {
        sprite_build_shadow_quad(unit);
        unit->shadow_dirty ^= 1;
    }
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    prim = g_sprite_prim_pool + sprite_take_primitive();
    corners = unit->shadow_quad;
    setPolyFT4(prim);
    RotTrans(&corners[0], &transformed, &flag);
    prim->x0 = transformed.vx;
    prim->y0 = transformed.vy;
    RotTrans(&corners[1], &transformed, &flag);
    prim->x1 = transformed.vx;
    prim->y1 = transformed.vy;
    RotTrans(&corners[2], &transformed, &flag);
    prim->x2 = transformed.vx;
    prim->y2 = transformed.vy;
    RotTrans(&corners[3], &transformed, &flag);
    prim->x3 = transformed.vx;
    prim->y3 = transformed.vy;
    setSemiTrans(prim, 1);
    tile = map_get_tile(unit->world_coords.vx / MAP_TILE_SIZE, unit->world_coords.vz / MAP_TILE_SIZE, unit->map_layer);
    if (tile->depth_half_height.depth
        && -(tile->height + tile->depth_half_height.depth) * MAP_HEIGHT_STEP < unit->world_coords.vy) {
        unit->shadow_counter += g_system_vsync_interval;
        frame = unit->shadow_counter;
        prim->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_ADD;
        prim->clut = unit->clut + 0x40; /* the unit's CLUT of MAP_CLUT_ROW_UNIT_SHADOWS */
        prim->r0 = prim->g0 = prim->b0 = 0x80;
        switch (frame & 0x30) {
        case 0:
            prim->u0 = prim->u2 = 0xa8;
            prim->u1 = prim->u3 = 0xbb;
            prim->v0 = prim->v1 = 0x40;
            prim->v2 = prim->v3 = 0x53;
            break;
        case 0x10:
            prim->u2 = prim->u3 = 0xa8;
            prim->u0 = prim->u1 = 0xbb;
            prim->v0 = prim->v2 = 0x40;
            prim->v1 = prim->v3 = 0x53;
            break;
        case 0x20:
            prim->u1 = prim->u3 = 0xa8;
            prim->u0 = prim->u2 = 0xbb;
            prim->v2 = prim->v3 = 0x40;
            prim->v0 = prim->v1 = 0x53;
            break;
        case 0x30:
            prim->u0 = prim->u1 = 0xa8;
            prim->u2 = prim->u3 = 0xbb;
            prim->v1 = prim->v3 = 0x40;
            prim->v0 = prim->v2 = 0x53;
            break;
        }
    } else {
        prim->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_SUBTRACT;
        prim->clut = unit->clut + 0x40; /* the unit's CLUT of MAP_CLUT_ROW_UNIT_SHADOWS */
        prim->r0 = prim->g0 = prim->b0 = 0x80;
        prim->u0 = prim->u2 = 0x90;
        prim->u1 = prim->u3 = 0xa3;
        prim->v0 = prim->v1 = 0x40;
        prim->v2 = prim->v3 = 0x53;
    }
    vector_set_short(&position, unit->world_coords.vx + unit->event_offset.vx + unit->seq_offset.vx, 0,
        unit->world_coords.vz + unit->event_offset.vz + unit->seq_offset.vz);
    position.vy = map_calculate_surface_height(&position, unit->map_layer);
    if (unit->world_coords.vy >= position.vy) {
        packet = ot;
    } else {
        tile = map_get_tile(position.vx / MAP_TILE_SIZE, position.vz / MAP_TILE_SIZE, unit->map_layer);
        position.vy -= tile->depth_half_height.depth * MAP_HEIGHT_STEP;
        if (unit->step_phase != MOVE_STEP_PHASE_NONE) {
            if (tile->surface.depth_bias) {
                camera_get_biased_step_depth_point(&position, &offset);
            } else {
                camera_get_unbiased_step_depth_point(&position, &offset);
            }
        } else if (tile->surface.depth_bias) {
            camera_get_biased_tile_depth_point(&position, &offset);
        } else {
            camera_get_unbiased_tile_depth_point(&position, &offset);
        }
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        RotTrans(&offset, &transformed, &flag);
        depth = transformed.vz / 4;
        packet = gfx_get_battle_otag() + depth;
    }
    AddPrim(packet, prim);
}

/*
 * Queue the glow primitives for one cursor tile.
 *
 * The four vertices are projected with the battle camera. Shape 0 (see
 * cursor_calculate_tile_polygon) draws a textured quad; shapes 1
 * and 2 split the tile into two textured triangles along the v0-v3 or v1-v2
 * diagonal. Each primitive draws the 13 x 13 glow image of the FRAME.BIN page
 * additively with CLUT palette + 1 of the sprite row, and a copy of it fills
 * the same area from a single texel with CLUT 5 at half blend. Both are queued
 * only when the first three projected vertices face the camera. Every
 * primitive takes a POLY_FT4-sized slot of the pool.
 */
static void build_tile_glow(s32 palette, s32 shape, SVECTOR* v0, SVECTOR* v1, SVECTOR* v2, SVECTOR* v3, u32* ot) {
    VECTOR screen;
    MATRIX unused_matrix; /* unreferenced; reserves the target frame's 0x20-byte gap at sp+0x20 */
    s32 flag;
    POLY_FT4* quad;
    POLY_FT4* quad_glow;
    POLY_FT3* tri_a;
    POLY_FT3* tri_b;
    POLY_FT3* tri_a_glow;
    POLY_FT3* tri_b_glow;

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    switch (shape) {
    case 0:
        quad = g_sprite_prim_pool + sprite_take_primitive();
        quad_glow = g_sprite_prim_pool + sprite_take_primitive();
        setPolyFT4(quad);
        RotTrans(v0, &screen, &flag);
        quad->x0 = screen.vx;
        quad->y0 = screen.vy;
        RotTrans(v1, &screen, &flag);
        quad->x1 = screen.vx;
        quad->y1 = screen.vy;
        RotTrans(v2, &screen, &flag);
        quad->x2 = screen.vx;
        quad->y2 = screen.vy;
        RotTrans(v3, &screen, &flag);
        quad->x3 = screen.vx;
        quad->y3 = screen.vy;
        setSemiTrans(quad, 1);
        quad->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_ADD;
        quad->clut = GetClut(palette * 16 + 16, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
        quad->r0 = quad->g0 = quad->b0 = 0x80;
        quad->u2 = 0;
        quad->u0 = 0;
        quad->u3 = 0xd;
        quad->u1 = 0xd;
        quad->v1 = 0xa0;
        quad->v0 = 0xa0;
        quad->v3 = 0xad;
        quad->v2 = 0xad;
        *quad_glow = *quad;
        quad_glow->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_AVERAGE;
        quad_glow->clut = GetClut(0x50, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
        quad_glow->u2 = 0xc;
        quad_glow->u0 = 0xc;
        quad_glow->u3 = 0xc;
        quad_glow->u1 = 0xc;
        quad_glow->v1 = 0xa0;
        quad_glow->v0 = 0xa0;
        quad_glow->v3 = 0xa0;
        quad_glow->v2 = 0xa0;
        if (NormalClip((quad->y0 << 16) + quad->x0, (quad->y1 << 16) + quad->x1, (quad->y2 << 16) + quad->x2) >= 0) {
            AddPrim(ot, quad);
            AddPrim(ot, quad_glow);
        }
        return;
    case 1:
        tri_a = (POLY_FT3*)(g_sprite_prim_pool + sprite_take_primitive());
        tri_b = (POLY_FT3*)(g_sprite_prim_pool + sprite_take_primitive());
        tri_a_glow = (POLY_FT3*)(g_sprite_prim_pool + sprite_take_primitive());
        tri_b_glow = (POLY_FT3*)(g_sprite_prim_pool + sprite_take_primitive());
        setPolyFT3(tri_a);
        setPolyFT3(tri_b);
        RotTrans(v0, &screen, &flag);
        tri_a->x0 = screen.vx;
        tri_a->y0 = screen.vy;
        RotTrans(v1, &screen, &flag);
        tri_a->x1 = tri_b->x0 = screen.vx;
        tri_a->y1 = tri_b->y0 = screen.vy;
        RotTrans(v2, &screen, &flag);
        tri_a->x2 = tri_b->x2 = screen.vx;
        tri_a->y2 = tri_b->y2 = screen.vy;
        RotTrans(v3, &screen, &flag);
        tri_b->x1 = screen.vx;
        tri_b->y1 = screen.vy;
        setSemiTrans(tri_a, 1);
        setSemiTrans(tri_b, 1);
        tri_a->tpage = tri_b->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_ADD;
        tri_a->clut = tri_b->clut = GetClut(palette * 16 + 16, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
        tri_b->r0 = tri_b->g0 = tri_b->b0 = 0x80;
        tri_a->r0 = tri_a->g0 = tri_a->b0 = 0x80;
        tri_b->u2 = 0;
        tri_a->u2 = 0;
        tri_a->u0 = 0;
        tri_b->u1 = 0xd;
        tri_b->u0 = 0xd;
        tri_a->u1 = 0xd;
        tri_b->v0 = 0xa0;
        tri_a->v1 = 0xa0;
        tri_a->v0 = 0xa0;
        tri_b->v2 = 0xad;
        tri_b->v1 = 0xad;
        tri_a->v2 = 0xad;
        *tri_a_glow = *tri_a;
        *tri_b_glow = *tri_b;
        tri_a_glow->tpage = tri_b_glow->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_AVERAGE;
        tri_a_glow->clut = tri_b_glow->clut = GetClut(0x50, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
        tri_b_glow->u2 = 0xc;
        tri_a_glow->u2 = 0xc;
        tri_a_glow->u0 = 0xc;
        tri_b_glow->u1 = 0xc;
        tri_b_glow->u0 = 0xc;
        tri_a_glow->u1 = 0xc;
        tri_b_glow->v0 = 0xa0;
        tri_a_glow->v1 = 0xa0;
        tri_a_glow->v0 = 0xa0;
        tri_b_glow->v2 = 0xa0;
        tri_b_glow->v1 = 0xa0;
        tri_a_glow->v2 = 0xa0;
        if (NormalClip((tri_a->y0 << 16) + tri_a->x0, (tri_a->y1 << 16) + tri_a->x1, (tri_a->y2 << 16) + tri_a->x2)
            >= 0) {
            AddPrim(ot, tri_a);
            AddPrim(ot, tri_a_glow);
        }
        if (NormalClip((tri_b->y0 << 16) + tri_b->x0, (tri_b->y1 << 16) + tri_b->x1, (tri_b->y2 << 16) + tri_b->x2)
            >= 0) {
            AddPrim(ot, tri_b);
            AddPrim(ot, tri_b_glow);
        }
        break;
    case 2:
        tri_a = (POLY_FT3*)(g_sprite_prim_pool + sprite_take_primitive());
        tri_b = (POLY_FT3*)(g_sprite_prim_pool + sprite_take_primitive());
        tri_a_glow = (POLY_FT3*)(g_sprite_prim_pool + sprite_take_primitive());
        tri_b_glow = (POLY_FT3*)(g_sprite_prim_pool + sprite_take_primitive());
        setPolyFT3(tri_a);
        setPolyFT3(tri_b);
        RotTrans(v0, &screen, &flag);
        tri_a->x0 = tri_b->x0 = screen.vx;
        tri_a->y0 = tri_b->y0 = screen.vy;
        RotTrans(v1, &screen, &flag);
        tri_a->x1 = screen.vx;
        tri_a->y1 = screen.vy;
        RotTrans(v2, &screen, &flag);
        tri_b->x2 = screen.vx;
        tri_b->y2 = screen.vy;
        RotTrans(v3, &screen, &flag);
        tri_a->x2 = tri_b->x1 = screen.vx;
        tri_a->y2 = tri_b->y1 = screen.vy;
        setSemiTrans(tri_a, 1);
        setSemiTrans(tri_b, 1);
        tri_a->tpage = tri_b->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_ADD;
        tri_a->clut = tri_b->clut = GetClut(palette * 16 + 16, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
        tri_b->r0 = tri_b->g0 = tri_b->b0 = 0x80;
        tri_a->r0 = tri_a->g0 = tri_a->b0 = 0x80;
        tri_b->u2 = 0;
        tri_b->u0 = 0;
        tri_a->u0 = 0;
        tri_b->u1 = 0xd;
        tri_a->u2 = 0xd;
        tri_a->u1 = 0xd;
        tri_b->v0 = 0xa0;
        tri_a->v1 = 0xa0;
        tri_a->v0 = 0xa0;
        tri_b->v2 = 0xad;
        tri_b->v1 = 0xad;
        tri_a->v2 = 0xad;
        *tri_a_glow = *tri_a;
        *tri_b_glow = *tri_b;
        tri_a_glow->tpage = tri_b_glow->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_AVERAGE;
        tri_a_glow->clut = tri_b_glow->clut = GetClut(0x50, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
        tri_b_glow->u2 = 0xc;
        tri_a_glow->u2 = 0xc;
        tri_a_glow->u0 = 0xc;
        tri_b_glow->u1 = 0xc;
        tri_b_glow->u0 = 0xc;
        tri_a_glow->u1 = 0xc;
        tri_b_glow->v0 = 0xa0;
        tri_a_glow->v1 = 0xa0;
        tri_a_glow->v0 = 0xa0;
        tri_b_glow->v2 = 0xa0;
        tri_b_glow->v1 = 0xa0;
        tri_a_glow->v2 = 0xa0;
        if (NormalClip((tri_a->y0 << 16) + tri_a->x0, (tri_a->y1 << 16) + tri_a->x1, (tri_a->y2 << 16) + tri_a->x2)
            >= 0) {
            AddPrim(ot, tri_a);
            AddPrim(ot, tri_a_glow);
        }
        if (NormalClip((tri_b->y0 << 16) + tri_b->x0, (tri_b->y1 << 16) + tri_b->x1, (tri_b->y2 << 16) + tri_b->x2)
            >= 0) {
            AddPrim(ot, tri_b);
            AddPrim(ot, tri_b_glow);
        }
        break;
    }
}

/*
 * Queue the map selection cursor over the cursor tile, whose surface point the
 * caller passes in `base` with the camera matrices loaded.
 *
 * The cursor floats 0x3c units above the tile, or 0x18 above the sprite height
 * of the unit the cursor selects there, scaled by the camera zoom. It bobs
 * down through an eight-frame pattern, advanced by the frame interval unless
 * the status screen is open. Its quad is the 16 x 24 cursor image scaled by
 * the camera zoom, sorted at the depth of the tile's surface point.
 *
 * The two initialized locals occupy 64 bytes of .rodata at 0x80067798: the bob
 * offsets and their durations in frames.
 */
static void draw_map_selection_cursor(SVECTOR* base, u32* ot) {
    VECTOR transformed;
    SVECTOR screen;
    s32 bob[8] = { 0, 1, 2, 3, 5, 3, 2, 1 };
    s32 duration[8] = { 16, 8, 2, 2, 6, 4, 4, 10 };
    s32 flag;
    POLY_FT4* prim;
    map_unit_t* unit;
    s32 height;
    s32 half_height;

    prim = g_sprite_prim_pool + sprite_take_primitive();
    setPolyFT4(prim);
    if (g_menu_status_screen_selected != 1) {
        g_cursor_selection_bob_timer += g_system_vsync_interval;
        if (g_cursor_selection_bob_timer >= duration[g_cursor_selection_bob_frame]) {
            g_cursor_selection_bob_frame++;
            g_cursor_selection_bob_timer = 0;
            if (g_cursor_selection_bob_frame >= 8) {
                g_cursor_selection_bob_frame = 0;
            }
        }
    }
    unit = map_unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
    if (unit != 0) {
        height = g_sprite_sheet_data[unit->spritesheet_id].graphic_height * g_camera_zoom.vy / ONE + 0x18;
    } else {
        height = 0x3c;
    }
    base->vy -= height - bob[g_cursor_selection_bob_frame];
    RotTrans(base, &transformed, &flag);

    prim->x0 = transformed.vx - g_camera_zoom.vx / 512;
    prim->y0 = transformed.vy - g_camera_zoom.vy * 12 / ONE;
    prim->x1 = transformed.vx + g_camera_zoom.vx / 512;
    prim->y1 = transformed.vy - g_camera_zoom.vy * 12 / ONE;
    prim->x2 = transformed.vx - g_camera_zoom.vx / 512;
    prim->y2 = transformed.vy + g_camera_zoom.vy * 12 / ONE;
    prim->x3 = transformed.vx + g_camera_zoom.vx / 512;
    half_height = g_camera_zoom.vy * 12 / ONE;
    {
        /* Read as a halfword, as the target does, after the half height. */
        u16 screen_y = transformed.vy;
        prim->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_SUBTRACT;
        prim->y3 = screen_y + half_height;
    }
    setSemiTrans(prim, 1);
    prim->clut = GetClut(0x40, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
    prim->r0 = prim->g0 = prim->b0 = 0x80;
    prim->u2 = 0x40;
    prim->u0 = 0x40;
    prim->u3 = 0x50;
    prim->u1 = 0x50;
    prim->v1 = 0x80;
    prim->v0 = 0x80;
    prim->v3 = 0x98;
    prim->v2 = 0x98;
    camera_get_unbiased_tile_depth_point(base, &screen);
    RotTrans(&screen, &transformed, &flag);
    AddPrim(ot + transformed.vz / 4, prim);
}

/*
 * Queue the four wait-direction arrows around a unit's feet. The arrow of the
 * source unit's facing quadrant blinks between two lit images every 32 timer
 * counts; the others show the unlit image. Called from
 * map_unit_facing_draw_wait_direction with the unit's base position and the
 * ordering-table slot for its depth.
 *
 * The two initialized locals occupy 32 bytes of .rodata at 0x800677d8; the
 * arrow offsets are the four unit steps of length 14 around the tile.
 *
 * Each arm repeats the four V stores because the target hoists 0x9a and 0xa8
 * into saved registers, which only happens with more than one use apiece;
 * cross-jumping merges the repeats back into one tail.
 */
static void draw_wait_arrows(SVECTOR* base, u32* ot) {
    SVECTOR position;
    VECTOR transformed;
    s32 dx[4] = { 0, -14, 0, 14 };
    s32 dz[4] = { -14, 0, 14, 0 };
    s32 flag;
    map_unit_t* unit;
    POLY_FT4* prim;
    /* Signed 16-bit value: the target's division emits the `bgez`/`addiu 0x3ff`
     * correction, so the quadrant bits are not held in an unsigned type. */
    s16 facing_bits;
    s32 i;

    unit = map_unit_get_source();
    s_unit_facing_wait_arrow_blink_timer += g_system_vsync_interval;
    for (i = 0; i < 4; i++) {
        prim = g_sprite_prim_pool + sprite_take_primitive();
        setlen(prim, 9);
        prim->code = 0x2e;
        prim->tpage = SPRITE_TPAGE_FRAME | PSYQ_GPU_TPAGE_ABR_ADD;
        vector_set_short(&position, base->vx + dx[i], base->vy - 48, base->vz + dz[i]);
        RotTrans(&position, &transformed, &flag);
        prim->x0 = prim->x2 = transformed.vx - 7;
        prim->x1 = prim->x3 = transformed.vx + 7;
        prim->y0 = prim->y1 = transformed.vy - 7;
        prim->y2 = prim->y3 = transformed.vy + 7;
        prim->clut = GetClut(0x30, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_SPRITES);
        prim->r0 = prim->g0 = prim->b0 = 0x80;
        facing_bits = unit->facing & 0xc00;
        if (i == facing_bits / 0x400) {
            if (s_unit_facing_wait_arrow_blink_timer & 0x20) {
                prim->u0 = prim->u2 = 0xbc;
                prim->u1 = prim->u3 = 0xca;
                prim->v1 = 0x9a;
                prim->v0 = 0x9a;
                prim->v3 = 0xa8;
                prim->v2 = 0xa8;
            } else {
                prim->u0 = prim->u2 = 0xd6;
                prim->u1 = prim->u3 = 0xe4;
                prim->v1 = 0x9a;
                prim->v0 = 0x9a;
                prim->v3 = 0xa8;
                prim->v2 = 0xa8;
            }
        } else {
            prim->u0 = prim->u2 = 0xca;
            prim->u1 = prim->u3 = 0xd8;
            prim->v1 = 0x9a;
            prim->v0 = 0x9a;
            prim->v3 = 0xa8;
            prim->v2 = 0xa8;
        }
        AddPrim(ot, prim);
    }
}

/* Defining the samples after their reader retains the original memory copy. */
static const s8 s_float_bob_offsets[24];

/* This updater needs a signed 16-bit result view. Changing the public return type
 * instead alters the arithmetic of its callers in the unit motion code. */
typedef s16 (*unit_height_s16_fn_t)(map_unit_t* unit);

/* Raise a floating unit's height offset, or lower it back to the ground.
 *
 * A floating unit rises by the frame interval each update up to 12, then bobs through
 * g_float_bob_offsets (24 entries, two frames each) unless it is
 * a mount's rider. Otherwise the offset decays to 0, except during the
 * post-move event state. The unit's world and real y are then raised to the
 * recalculated ground height when they lie below it. Jumping units whose
 * height is controlled by their jump animation are left untouched. */
static void update_float_height(map_unit_t* unit) {
    s8 wave[24];
    s16 ground_y;
    u32 phase;

    memcpy(wave, s_float_bob_offsets, sizeof(wave));
    if (unit->status_flags_5_6 & (UNIT_STATUS_JUMP | UNIT_STATUS_JUMP_HEIGHT_ACTIVE)) {
        return;
    }
    if (unit->movement.floating) {
        if (unit->depth_height_offset < 12) {
            unit->depth_height_offset += g_system_vsync_interval;
        } else if (unit->mount_state != UNIT_MOUNT_STATE_RIDER) {
            phase = unit->float_bob_phase += g_system_vsync_interval;
            unit->float_bob_offset = wave[phase % 48 / 2];
        } else {
            unit->float_bob_offset = 0;
        }
    } else if (g_battle_state != BATTLE_STATE_POST_MOVE_EVENTS) {
        if (unit->depth_height_offset != 0) {
            unit->depth_height_offset -= g_system_vsync_interval;
            if (unit->depth_height_offset < 0) {
                unit->depth_height_offset = 0;
            }
            ground_y = ((unit_height_s16_fn_t)map_unit_calculate_height_from_world_coords)(unit);
            unit->world_coords.vy = ground_y;
            unit->real.vy = ground_y * ONE;
        } else {
            unit->float_bob_offset = 0;
        }
    }
    ground_y = ((unit_height_s16_fn_t)map_unit_calculate_height_from_world_coords)(unit);
    if (unit->world_coords.vy > ground_y) {
        unit->world_coords.vy = ground_y;
        unit->real.vy = ground_y * ONE;
    }
}

/* Floating height offsets, one sample every two frames. */
static const s8 s_float_bob_offsets[24] = { 4, 4, 4, 4, 3, 3, 3, 2, 2, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 3 };

/* Advances one unit's status bubble by g_system_vsync_interval frames.
 *
 * The bubble switches texture rows every 16 timer ticks (CHARGING every 8).
 * Every 16 ticks the bubble id walks forward to the next of the
 * STATUS_BUBBLE_COUNT bubbles that applies, wrapping to DEAD, and the
 * bubble's x/y offsets are chosen from the unit's SHP type and animation. The
 * bubbles before SOURCE_UNIT follow statuses
 * (s_status_bubble_status_masks); SOURCE_UNIT belongs to the source
 * unit instead. The DEAD and DEATH_SENTENCE bubbles, which show counters, stay
 * while their status lasts. A unit with no bubble status that
 * is not the source unit loses its bubble.
 *
 * The block-scoped bubble_mask keeps GCC from rotating the loop's leading exit
 * tests to the bottom, and indexing s_status_bubble_status_masks inside the loop lets loop.c
 * strength-reduce it into the target's pointer walk. */
static void update_bubble_cycle(map_unit_t* unit) {
    s32 frame;
    s32 bubble;
    u32 status_flags;
    s32 animation;

    for (frame = 0; frame < g_system_vsync_interval; frame++) {
        bubble = unit->status_bubble_id;
        if (bubble == STATUS_BUBBLE_CHARGING) {
            unit->status_bubble_alternate_row = (unit->status_bubble_timer & 8) >> 3;
        } else {
            unit->status_bubble_alternate_row = (unit->status_bubble_timer & 0x10) >> 4;
        }

        if (!(unit->status_bubble_timer & 0xF)) {
            status_flags = unit->status_flags_1_4;
            if (!(status_flags & UNIT_STATUS_BUBBLE_MASK) && unit->map_unit_id != g_unit_source_map_unit_id) {
                unit->status_bubble_active = 0;
                return;
            }

            for (;;) {
                s32 bubble_mask;

                if (bubble == STATUS_BUBBLE_DEAD) {
                    if (status_flags & UNIT_STATUS_DEAD) {
                        break;
                    }
                } else if (bubble == STATUS_BUBBLE_DEATH_SENTENCE) {
                    if (status_flags & UNIT_STATUS_DEATH_SENTENCE) {
                        break;
                    }
                }
                bubble++;
                if (bubble >= STATUS_BUBBLE_COUNT) {
                    bubble = STATUS_BUBBLE_DEAD;
                }
                if (bubble == unit->status_bubble_id) {
                    break;
                }
                if (bubble != STATUS_BUBBLE_SOURCE_UNIT) {
                    bubble_mask = s_status_bubble_status_masks[bubble];
                    if (status_flags & bubble_mask) {
                        break;
                    }
                } else if (unit->map_unit_id == g_unit_source_map_unit_id) {
                    break;
                }
            }
            unit->status_bubble_id = bubble;

            switch (g_sprite_sheet_data[unit->spritesheet_id].shp_id) {
            case SPRITE_SHP_ARUTE:
                unit->status_bubble_x = 0;
                unit->status_bubble_y = -0x46;
                break;
            case SPRITE_SHP_KANZEN:
                unit->status_bubble_x = 0;
                unit->status_bubble_y = -0x78;
                break;
            case SPRITE_SHP_OTHER:
                animation = unit->animation.sequence_id >> 1;
                if (animation == ANIMATION_DEAD || animation == ANIMATION_DEATH) {
                    unit->status_bubble_x = -5;
                    unit->status_bubble_y = -0x19;
                } else {
                    unit->status_bubble_x = 0;
                    unit->status_bubble_y = -0x19;
                }
                break;
            case SPRITE_SHP_TYPE1:
            case SPRITE_SHP_TYPE2:
                if (map_get_tile(unit->map_x, unit->map_y, unit->map_layer)->depth_half_height.depth) {
                    switch (unit->animation.sequence_id >> 1) {
                    case ANIMATION_DEAD:
                    case ANIMATION_DEATH:
                        unit->status_bubble_x = -5;
                        unit->status_bubble_y = -0x23;
                        break;
                    case ANIMATION_SLEEP:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x1E;
                        break;
                    default:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x28;
                        break;
                    }
                } else {
                    switch (unit->animation.sequence_id >> 1) {
                    case ANIMATION_DEAD:
                    case ANIMATION_DEATH:
                        unit->status_bubble_x = -5;
                        unit->status_bubble_y = -0x19;
                        break;
                    case ANIMATION_SLEEP:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x1E;
                        break;
                    default:
                        unit->status_bubble_x = 0;
                        unit->status_bubble_y = -0x28;
                        break;
                    }
                }
                break;
            default:
                animation = unit->animation.sequence_id >> 1;
                if (animation == ANIMATION_DEAD || animation == ANIMATION_DEATH) {
                    unit->status_bubble_x = -5;
                    unit->status_bubble_y = -0x1E;
                } else {
                    unit->status_bubble_x = 0;
                    unit->status_bubble_y = -0x32;
                }
                break;
            }
        }
        unit->status_bubble_timer++;
    }
}

/* Unit scale for status bubbles and post-action text. */
static const SVECTOR s_identity_scale = { ONE, ONE, ONE, 0 };

/*
 * Queue the unit's status bubble into the ordering table.
 *
 * The bubble's texture coordinates are chosen from the status bubble id. The
 * DEAD bubble shows the RERAISE image on a unit with Reraise, else the digit
 * of its death counter, else (counter 0xff) its own image. The DEATH_SENTENCE
 * bubble shows the digit of that status's count. DEAD, CHARGING, RERAISE and
 * SOURCE_UNIT have a second texture row, selected by
 * status_bubble_alternate_row. The bubble is drawn at status_bubble_y (negative
 * is up) from the unit's feet and status_bubble_x across, one pixel higher
 * while bit 4 of the timer is set, and sorted at the depth of a point 12 units
 * below its base.
 */
static void draw_status_bubble_sprite(map_unit_t* unit, const DVECTOR* unused_screen_coords) {
    SVECTOR position;
    SVECTOR zoom = s_identity_scale;
    VECTOR transformed;
    SVECTOR base;
    SVECTOR raised;
    s32 flag;
    sprite_display_t* display;
    s32 bubble_id;
    /* One local for both digit indices: the status-CT arm reuses the dying
     * counter's register, and two separate locals allocate differently. */
    s32 table_index;
    u8 row;
    u8 flag_row;
    u8 default_row;
    s32 depth;
    s16 y;
    u32* otag;

    /* The caller supplies its culling coordinates in a1, but this routine
     * independently projects the bubble and never reads them. */
    (void)unused_screen_coords;

    bubble_id = unit->status_bubble_id;
    display = unit->status_bubble_display;
    if (bubble_id == STATUS_BUBBLE_SOURCE_UNIT) {
        display->parts[0].u = 0x72;
        display->parts[0].v = 0xB0;
        if (unit->status_bubble_alternate_row) {
            display->parts[0].v = 0xBC;
        }
    } else if (bubble_id == STATUS_BUBBLE_DEAD) {
        table_index = unit->battle_unit->death_counter;
        if (unit->status_flags_1_4 & UNIT_STATUS_RERAISE) {
            display->parts[0].u = s_status_bubble_u[STATUS_BUBBLE_RERAISE];
            flag_row = s_status_bubble_v[STATUS_BUBBLE_RERAISE];
            display->parts[0].v = flag_row;
            if (unit->status_bubble_alternate_row) {
                display->parts[0].v = flag_row + 12;
            }
        } else if (table_index != 0xFF) {
            display->parts[0].u = s_status_bubble_digit_u[table_index];
            display->parts[0].v = s_status_bubble_digit_v[table_index];
        } else {
            display->parts[0].u = s_status_bubble_u[STATUS_BUBBLE_DEAD];
            default_row = s_status_bubble_v[STATUS_BUBBLE_DEAD];
            display->parts[0].v = default_row;
            if (unit->status_bubble_alternate_row) {
                display->parts[0].v = default_row + 12;
            }
        }
    } else if (bubble_id == STATUS_BUBBLE_DEATH_SENTENCE) {
        table_index = unit->battle_unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)] & 3;
        display->parts[0].u = s_status_bubble_digit_u[table_index];
        display->parts[0].v = s_status_bubble_digit_v[table_index];
    } else {
        display->parts[0].u = s_status_bubble_u[bubble_id];
        row = s_status_bubble_v[bubble_id];
        display->parts[0].v = row;
        if (bubble_id == STATUS_BUBBLE_CHARGING || bubble_id == STATUS_BUBBLE_RERAISE) {
            if (unit->status_bubble_alternate_row) {
                display->parts[0].v = row + 12;
            }
        }
    }

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    vector_set_short(&base, unit->world_coords.vx + unit->event_offset.vx,
        unit->world_coords.vy + unit->event_offset.vy + unit->status_bubble_y,
        unit->world_coords.vz + unit->event_offset.vz);
    RotTrans(&base, &transformed, &unit->gte_flag);
    position.vx = unit->status_bubble_x + (transformed.vx + unit->screen_offset.vx);
    y = transformed.vy + unit->screen_offset.vy;
    position.vy = y;
    if (unit->status_bubble_timer & 0x10) {
        position.vy = y - 1;
    }
    camera_get_unbiased_tile_depth_point(&base, &raised);
    raised.vy += 12;
    RotTrans(&raised, &transformed, &flag);
    display = unit->status_bubble_display;
    otag = gfx_get_battle_otag();
    depth = transformed.vz / 4;
    sprite_display_draw_parts(display, display->part_count, 0, &position, 0, 0, &zoom, otag + depth);
}

/* Per-frame float and status-bubble update of one unit: adjusts its float height
 * except during the four float-jump step phases, then advances its status bubble
 * when one is active. */
static void update_float_height_and_status_bubble(map_unit_t* unit) {
    u8 phase = unit->step_phase;

    if (phase != MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH && phase != MOVE_STEP_PHASE_FLOAT_JUMP_EAST
        && phase != MOVE_STEP_PHASE_FLOAT_JUMP_WEST && phase != MOVE_STEP_PHASE_FLOAT_JUMP_NORTH) {
        update_float_height(unit);
    }
    if (unit->status_bubble_active) {
        update_bubble_cycle(unit);
    }
}

/* Start the unit's afterimage trail: enable position_copies and seed all six at its world
 * position. Jump launches and SEQ opcode 0xbf start it. */
static void init_afterimage(map_unit_t* unit) {
    s32 i;

    unit->position_copies_active = 1;
    for (i = 0; i < 6; i++) {
        vector_set_short(
            &unit->position_copies[i], unit->world_coords.vx, unit->world_coords.vy, unit->world_coords.vz);
    }
}

/*
 * Draw the unit's current sprite frame once at each of its six position_copies:
 * the afterimage trail that init_afterimage starts.
 *
 * Each copy is projected with the battle camera and queued at its own
 * ordering-table depth using the current camera zoom, semi-transparent with the
 * blend bits cleared (half blend). The camera matrices are rebuilt for every
 * copy.
 */
static void draw_afterimage(map_unit_t* unit) {
    SVECTOR position;
    SVECTOR zoom;
    VECTOR transformed;
    s32 flag;
    sprite_display_t* display;
    s32 depth;
    s32 i;
    s16 flags;

    for (i = 0; i < 6; i++) {
        camera_update_matrices(&g_camera_matrix, &g_camera_render_state, &g_camera_screen_offset, &g_camera_zoom);
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        RotTrans(&unit->position_copies[i], &transformed, &flag);
        depth = transformed.vz / 4;
        position.vx = transformed.vx;
        position.vy = transformed.vy;
        flags = ((unit->horizontal_flip_flag ^ unit->sprite_display_flags) & ~SPRITE_DISPLAY_BLEND_MASK)
            | SPRITE_DISPLAY_SEMI_TRANSPARENT;
        copyVector(&zoom, &g_camera_zoom);
        display = unit->animation.display;
        sprite_display_draw_parts(
            display, display->part_count, 0, &position, 0, flags, &zoom, gfx_get_battle_otag() + depth);
    }
}

/* Advances the unit's position trail: each copy takes the next newer one and copy
 * 0 the unit's current position. */
static void shift_and_fill_afterimage(map_unit_t* unit) {
    s32 index;

    for (index = 5; index > 0; index--) {
        unit->position_copies[index] = unit->position_copies[index - 1];
    }
    map_unit_get_draw_coords(unit->position_copies, unit);
}

/* Make the unit's item display show an item's 16x16 icon, centered on the
 * display's origin. */
void sprite_item_display_set_icon(map_unit_t* unit, u32 item_id) {
    SPRT data;

    item_set_icon_sprite(&data, item_id);
    unit->item_display->clut = data.clut;
    sprite_display_set_part(
        unit->item_display, 0, (16 - data.w) / 2 - 8, (16 - data.h) / 2 - 8, data.u0, data.v0, data.w, data.h, 0);
}

/* Queues the unit's item icon at its item display offset from the unit's screen
 * position, at the unit's ordering-table depth and scale ONE. The x offset is
 * negated unless the sprite is flipped horizontally. */
static void draw_item_display(map_unit_t* unit) {
    SVECTOR position;
    SVECTOR zoom;
    u32* otag;

    if (unit->sprite_display_flags & SPRITE_DISPLAY_HORIZONTAL_FLIP) {
        position.vx = unit->item_display_x_offset;
    } else {
        position.vx = -unit->item_display_x_offset;
    }
    position.vy = unit->item_display_y_offset;
    position.vx += unit->camera_relative_x;
    position.vy += unit->camera_relative_y;
    vector_set_short(&zoom, ONE, ONE, ONE);
    otag = gfx_get_battle_otag();
    otag += unit->otag_depth_index;
    sprite_display_draw_parts(unit->item_display, unit->item_display->part_count, 0, &position, 0, 0, &zoom, otag);
}

/* Load the unit's next pending action result into its numeric display and clear its pending flag:
 * a stat change, then level down or up, stolen, broken, Quick, CT 0, missed, guarded, caught, then
 * the queued status images. Sets the digit sprites (values over 999 show 999) and the color.
 * Returns 0 when nothing is pending. */
static s32 build_next_result_display(map_unit_t* unit) {
    battle_unit_t* battle_unit;
    u32 scan_flags;
    s32 flags;
    action_result_display_t* display;
    sprite_display_t* display_0;
    sprite_display_t* display_1;
    sprite_display_t* display_2;
    const action_display_image_rect_t* image;
    s32 flag;
    s32 index;
    s32 digits[3];
    u8 red;
    u8 green;
    u8 blue;
    s16 y_shift;
    s32 image_id;
    u8 change;
    u8 status;

    flags = unit->action_display_flags;
    battle_unit = unit->battle_unit;
    display = &unit->numeric_display;

    if (flags & ACTION_DISPLAY_NUMERIC_FLAGS_MASK) {
        g_result_display_values[RESULT_DISPLAY_HP_DAMAGE] = battle_unit->action.hp_damage;
        g_result_display_values[RESULT_DISPLAY_HP_HEALING] = battle_unit->action.hp_healing;
        g_result_display_values[RESULT_DISPLAY_MP_DAMAGE] = battle_unit->action.mp_damage;
        g_result_display_values[RESULT_DISPLAY_MP_HEALING] = battle_unit->action.mp_healing;
        /* A stat's gain and damage slots share one magnitude; the pending flag picks which shows. */
        change = battle_unit->action.sp_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_result_display_values[RESULT_DISPLAY_SPEED_GAIN] = change;
        g_result_display_values[RESULT_DISPLAY_SPEED_DAMAGE] = change;
        change = battle_unit->action.ct_change & 0x7f;
        g_result_display_values[RESULT_DISPLAY_CT_GAIN] = change;
        g_result_display_values[RESULT_DISPLAY_CT_DAMAGE] = change;
        change = battle_unit->action.brave_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_result_display_values[RESULT_DISPLAY_BRAVE_GAIN] = change;
        g_result_display_values[RESULT_DISPLAY_BRAVE_DAMAGE] = change;
        change = battle_unit->action.faith_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_result_display_values[RESULT_DISPLAY_FAITH_GAIN] = change;
        g_result_display_values[RESULT_DISPLAY_FAITH_DAMAGE] = change;
        change = battle_unit->action.pa_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_result_display_values[RESULT_DISPLAY_PA_GAIN] = change;
        g_result_display_values[RESULT_DISPLAY_PA_DAMAGE] = change;
        change = battle_unit->action.ma_change & ACTION_STAT_CHANGE_VALUE_MASK;
        g_result_display_values[RESULT_DISPLAY_MA_GAIN] = change;
        g_result_display_values[RESULT_DISPLAY_MA_DAMAGE] = change;

        /* Show the lowest pending numeric result and clear its flag. */
        scan_flags = unit->action_display_flags;
        flag = 1;
        for (index = 0; index < ACTION_DISPLAY_VALUE_COUNT; index++) {
            if (flag & scan_flags) {
                display->value = g_result_display_values[index];
                display->selector = s_result_numeric_selectors[index];
                unit->action_display_flags &= ~flag;
                break;
            }
            flag <<= 1;
        }
    } else if (flags & ACTION_DISPLAY_FLAG_LEVEL_DOWN) {
        unit->numeric_display.selector = RESULT_DISPLAY_LEVEL_DOWN;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_LEVEL_DOWN;
    } else if (flags & ACTION_DISPLAY_FLAG_LEVEL_UP) {
        unit->numeric_display.selector = RESULT_DISPLAY_LEVEL_UP;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_LEVEL_UP;
    } else if (flags & ACTION_DISPLAY_FLAG_STOLEN) {
        unit->numeric_display.selector = RESULT_DISPLAY_STOLEN;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_STOLEN;
    } else if (flags & ACTION_DISPLAY_FLAG_BROKEN) {
        unit->numeric_display.selector = RESULT_DISPLAY_BROKEN;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_BROKEN;
    } else if (flags & ACTION_DISPLAY_FLAG_QUICK) {
        unit->numeric_display.selector = RESULT_DISPLAY_QUICK;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_QUICK;
    } else if (flags & ACTION_DISPLAY_FLAG_CT_ZERO) {
        unit->numeric_display.selector = RESULT_DISPLAY_CT_ZERO;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_CT_ZERO;
    } else if (flags & ACTION_DISPLAY_FLAG_MISSED) {
        unit->numeric_display.selector = RESULT_DISPLAY_MISSED;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_MISSED;
    } else if (flags & ACTION_DISPLAY_FLAG_GUARDED) {
        unit->numeric_display.selector = RESULT_DISPLAY_GUARDED;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_GUARDED;
    } else if (flags & ACTION_DISPLAY_FLAG_CAUGHT) {
        unit->numeric_display.selector = RESULT_DISPLAY_CAUGHT;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_CAUGHT;
    } else if (unit->status_display_image_count != 0) {
        unit->status_display_image_count--;
        status = unit->status_display_image_list[unit->status_display_image_count];
        if (status & ACTION_STATUS_DISPLAY_REMOVAL) {
            display->selector = RESULT_DISPLAY_STATUS_REMOVED;
        } else {
            display->selector = RESULT_DISPLAY_STATUS_INFLICTED;
        }
        status &= ACTION_STATUS_DISPLAY_IMAGE_MASK;
    } else {
        return 0;
    }

    if (unit->battle_unit->action.critical) {
        display->selector |= RESULT_DISPLAY_MODIFIER_CRITICAL;
    } else {
        display->selector &= ~RESULT_DISPLAY_MODIFIER_CRITICAL;
    }

    display->active = 1;
    if (display->progress >= 22) {
        display->progress = 21;
    }

    /* Every message produced above selects a color case.  The target has no
     * default initialization for these three registers. */
    switch (display->selector & RESULT_DISPLAY_SELECTOR_MASK) {
    case RESULT_DISPLAY_HP_DAMAGE:
    case RESULT_DISPLAY_SPEED_DAMAGE:
    case RESULT_DISPLAY_CT_DAMAGE:
    case RESULT_DISPLAY_BRAVE_DAMAGE:
    case RESULT_DISPLAY_FAITH_DAMAGE:
    case RESULT_DISPLAY_PA_DAMAGE:
    case RESULT_DISPLAY_MA_DAMAGE:
    case RESULT_DISPLAY_STATUS_INFLICTED:
    case RESULT_DISPLAY_LEVEL_DOWN:
    case RESULT_DISPLAY_STOLEN:
    case RESULT_DISPLAY_BROKEN:
    case RESULT_DISPLAY_QUICK:
    case RESULT_DISPLAY_CT_ZERO:
        blue = 128;
        green = 128;
        red = 128;
        break;
    case RESULT_DISPLAY_MP_DAMAGE:
        red = 120;
        green = 60;
        blue = 220;
        break;
    case RESULT_DISPLAY_HP_HEALING:
    case RESULT_DISPLAY_SPEED_GAIN:
    case RESULT_DISPLAY_CT_GAIN:
    case RESULT_DISPLAY_BRAVE_GAIN:
    case RESULT_DISPLAY_FAITH_GAIN:
    case RESULT_DISPLAY_PA_GAIN:
    case RESULT_DISPLAY_MA_GAIN:
    case RESULT_DISPLAY_STATUS_REMOVED:
    case RESULT_DISPLAY_LEVEL_UP:
        red = 40;
        green = 140;
        blue = 220;
        break;
    case RESULT_DISPLAY_MP_HEALING:
        red = 78;
        green = 140;
        blue = 30;
        break;
    case RESULT_DISPLAY_MISSED:
    case RESULT_DISPLAY_GUARDED:
    case RESULT_DISPLAY_CAUGHT:
        red = 190;
        green = 40;
        blue = 10;
        break;
    }

    display->displays[0]->color.r = display->displays[1]->color.r = display->displays[2]->color.r = red;
    display->displays[0]->color.g = display->displays[1]->color.g = display->displays[2]->color.g = green;
    display->displays[0]->color.b = display->displays[1]->color.b = display->displays[2]->color.b = blue;

    if (display->value / 1000) {
        digits[2] = 9;
        digits[1] = 9;
        digits[0] = 9;
    } else {
        digits[0] = display->value % 10;
        digits[1] = (display->value / 10) % 10;
        digits[2] = display->value / 100;
    }

    display_0 = display->displays[0];
    display_1 = display->displays[1];
    display_2 = display->displays[2];

    /* Empty displays 0 and 1; each case below fills only the sprites it uses. */
    sprite_display_set_part(display_0, 0, 0, -32, 0, 0, 0, 0, 0);
    sprite_display_set_part(display_1, 0, 0, -32, 0, 0, 0, 0, 0);

    switch (display->selector & RESULT_DISPLAY_SELECTOR_MASK) {
    case RESULT_DISPLAY_HP_DAMAGE:
    case RESULT_DISPLAY_HP_HEALING:
    case RESULT_DISPLAY_MP_DAMAGE:
    case RESULT_DISPLAY_MP_HEALING:
        /* Damage (even selectors) shows at y 16, gains and healing at y -32. */
        y_shift = -32;
        if (!(display->selector & 1)) {
            y_shift = 16;
        }
        sprite_display_set_part(display_1, 0, -14, y_shift, 0, 0, 0, 0, 0);
        sprite_display_set_part(display_2, 0, -21, y_shift, 0, 0, 0, 0, 0);
        if ((digits[2] | digits[1]) == 0) {
            sprite_display_set_part(display_0, 0, -21, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else if (digits[2] == 0) {
            sprite_display_set_part(display_0, 0, -14, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            sprite_display_set_part(display_1, 0, -21, y_shift, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        } else {
            sprite_display_set_part(display_0, 0, -7, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            sprite_display_set_part(display_1, 0, -14, y_shift, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
            sprite_display_set_part(display_2, 0, -21, y_shift, 0xa8 + digits[2] * 8, 0x30, 8, 16, 0);
        }
        break;

    case RESULT_DISPLAY_SPEED_DAMAGE:
    case RESULT_DISPLAY_SPEED_GAIN:
    case RESULT_DISPLAY_CT_DAMAGE:
    case RESULT_DISPLAY_CT_GAIN:
    case RESULT_DISPLAY_BRAVE_DAMAGE:
    case RESULT_DISPLAY_BRAVE_GAIN:
    case RESULT_DISPLAY_FAITH_DAMAGE:
    case RESULT_DISPLAY_FAITH_GAIN:
    case RESULT_DISPLAY_PA_DAMAGE:
    case RESULT_DISPLAY_PA_GAIN:
    case RESULT_DISPLAY_MA_DAMAGE:
    case RESULT_DISPLAY_MA_GAIN:
        y_shift = -32;
        if (!(display->selector & 1)) {
            y_shift = 16;
        }
        if (digits[1] == 0) {
            sprite_display_set_part(display_0, 0, -14, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else {
            sprite_display_set_part(display_0, 0, -14, y_shift, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            sprite_display_set_part(display_1, 0, -21, y_shift, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        }

        /* Masking bit 0 lets each damage case also label the matching gain. */
        switch (display->selector & 0x0e) {
        case RESULT_DISPLAY_SPEED_DAMAGE:
            sprite_display_set_part(display_2, 0, -7, y_shift, 0x9c, 0xc2, 0x18, 0x0a, 0);
            break;
        case RESULT_DISPLAY_CT_DAMAGE:
            sprite_display_set_part(display_2, 0, -7, y_shift, 0xc2, 0xc2, 0x0e, 0x0a, 0);
            break;
        case RESULT_DISPLAY_BRAVE_DAMAGE:
            sprite_display_set_part(display_2, 0, -7, y_shift, 0xa8, 0xb8, 0x0b, 0x0a, 0);
            break;
        case RESULT_DISPLAY_FAITH_DAMAGE:
            sprite_display_set_part(display_2, 0, -7, y_shift, 0x9c, 0xb8, 0x0c, 0x0a, 0);
            break;
        case RESULT_DISPLAY_PA_DAMAGE:
            sprite_display_set_part(display_2, 0, -7, y_shift, 0x40, 0, 0x0c, 0x0d, 0);
            break;
        case RESULT_DISPLAY_MA_DAMAGE:
            sprite_display_set_part(display_2, 0, -7, y_shift, 0x40, 0x0d, 0x0b, 0x0a, 0);
            break;
        }
        break;

    case RESULT_DISPLAY_MISSED:
        sprite_display_set_part(display_2, 0, -21, -32, 0xbe, 0x48, 0x1b, 8, 0);
        break;
    case RESULT_DISPLAY_GUARDED:
        sprite_display_set_part(display_2, 0, -21, -32, 0xb8, 0x60, 0x1e, 8, 0);
        break;
    case RESULT_DISPLAY_CAUGHT:
        sprite_display_set_part(display_2, 0, -21, -32, 0x30, 0xe6, 0x1a, 0x0a, 0);
        break;
    case RESULT_DISPLAY_STATUS_INFLICTED:
    case RESULT_DISPLAY_STATUS_REMOVED:
        image_id = status;
        sprite_display_set_part(display_2, 0, -21, -32, g_result_image_rects[image_id].u,
            g_result_image_rects[image_id].v, g_result_image_rects[image_id].width,
            g_result_image_rects[image_id].height, 0);
        break;
    case RESULT_DISPLAY_LEVEL_DOWN:
        /* The fall-through overwrites both sprites, so level down places them like level up. */
        sprite_display_set_part(display_1, 0, -7, 16, 0xb0, 0x30, 8, 16, 0);
        sprite_display_set_part(display_2, 0, 0, 16, 0xb4, 0xc1, 0x0e, 0x0a, 0);
        /* fall through */
    case RESULT_DISPLAY_LEVEL_UP:
        sprite_display_set_part(display_1, 0, -7, -32, 0xb0, 0x30, 8, 16, 0);
        sprite_display_set_part(display_2, 0, 0, -32, 0xb4, 0xc1, 0x0e, 0x0a, 0);
        break;
    case RESULT_DISPLAY_STOLEN:
        image = &g_result_image_rects[ACTION_DISPLAY_IMAGE_STOLEN];
        sprite_display_set_part(display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case RESULT_DISPLAY_BROKEN:
        image = &g_result_image_rects[ACTION_DISPLAY_IMAGE_BROKEN];
        sprite_display_set_part(display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case RESULT_DISPLAY_QUICK:
        image = &g_result_image_rects[ACTION_DISPLAY_IMAGE_QUICK];
        sprite_display_set_part(display_2, 0, -21, -32, image->u, image->v, image->width, image->height, 0);
        break;
    case RESULT_DISPLAY_CT_ZERO:
        /* y_shift is never set on this path; its stale value picks the row (QUIRKS.md). */
        sprite_display_set_part(display_1, 0, -16, y_shift, 0xc2, 0xc2, 0x0e, 8, 0);
        sprite_display_set_part(display_2, 0, 0, y_shift, 0xa8, 0x34, 8, 8, 0);
        break;
    }

    map_palette_load_data(&s_result_palette, MAP_CLUT_ROW_ACTION_RESULTS, unit->map_unit_id, 0);
    return 1;
}

/* Queues the post-action displays for a unit's resolved action.
 *
 * A hit sets one action_display_flags bit per HP/MP, stat, level, steal and
 * break result and lists the inflicted/removed status images (removals
 * marked 0x80); a miss queues MISSED, GUARDED or CAUGHT by miss type. The
 * first display is built immediately unless a numeric display is active. */
void result_prepare_display(map_unit_t* unit) {
    battle_unit_t* battle_unit;
    action_data_t* action;
    s32 i;
    s32 j;
    s32 bit;
    u8 count;
    u8 image;
    u8 miss;

    unit->numeric_display.displays[0]->tpage = unit->numeric_display.displays[1]->tpage
        = unit->numeric_display.displays[2]->tpage = SPRITE_TPAGE_FRAME;
    battle_unit = unit->battle_unit;
    action = &battle_unit->action;
    if (action->hit) {
        if (action->attack_type & ACTION_TYPE_HP_DAMAGE) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_HP_DAMAGE;
        }
        if (action->attack_type & ACTION_TYPE_HP_HEALING) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_HP_HEALING;
        }
        if (action->attack_type & ACTION_TYPE_MP_DAMAGE) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_MP_DAMAGE;
        }
        if (action->attack_type & ACTION_TYPE_MP_HEALING) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_MP_HEALING;
        }
        if (action->attack_type & ACTION_TYPE_PSEUDO_STATUS) {
            if (action->sp_change != 0) {
                if (action->sp_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_SPEED_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_SPEED_DAMAGE;
                }
            }
            if (action->ct_change != 0) {
                if (action->ct_change == ACTION_CT_CHANGE_QUICK) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_QUICK;
                } else if (action->ct_change == ACTION_CT_CHANGE_ZERO) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_CT_ZERO;
                } else if (action->ct_change & 0x80) { /* the top bit marks a gain, as for the stat changes */
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_CT_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_CT_DAMAGE;
                }
            }
            if (action->pa_change != 0) {
                if (action->pa_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_PA_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_PA_DAMAGE;
                }
            }
            if (action->ma_change != 0) {
                if (action->ma_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_MA_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_MA_DAMAGE;
                }
            }
            if (action->brave_change != 0) {
                if (action->brave_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_BRAVE_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_BRAVE_DAMAGE;
                }
            }
            if (action->faith_change != 0) {
                if (action->faith_change & ACTION_STAT_CHANGE_INCREASE) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_FAITH_GAIN;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_FAITH_DAMAGE;
                }
            }
        }
        if (action->special_effect & ACTION_SPECIAL_EFFECT_LEVEL_UP) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_LEVEL_UP;
        }
        if (action->special_effect & ACTION_SPECIAL_EFFECT_LEVEL_DOWN) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_LEVEL_DOWN;
        }
        if (action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_STOLEN;
        }
        if (action->special_effect
            & (ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT | ACTION_SPECIAL_EFFECT_DRAW_OUT_KATANA_BROKEN)) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_BROKEN;
        }
        if (action->attack_type & ACTION_TYPE_STATUS_CHANGE) {
            unit->status_display_image_count = 0;
            for (i = 0; i < STATUS_BYTE_COUNT; i++) {
                for (j = 0; j < 8; j++) {
                    count = unit->status_display_image_count;
                    bit = 0x80 >> j;
                    if (count < ACTION_STATUS_DISPLAY_CAPACITY) {
                        if (action->status_changes.infliction[i] & bit) {
                            image = s_result_status_image_ids[i][j];
                            unit->status_display_image_list[count] = image;
                            if (image != 0) {
                                unit->status_display_image_count++;
                            }
                        } else if (action->status_changes.removal[i] & bit) {
                            image = s_result_status_image_ids[i][j];
                            unit->status_display_image_list[count] = image;
                            if (image != 0) {
                                unit->status_display_image_list[unit->status_display_image_count]
                                    |= ACTION_STATUS_DISPLAY_REMOVAL;
                                unit->status_display_image_count++;
                            }
                        }
                    }
                }
            }
        }
    } else {
        /* An evade (accessory, either hand, Blade Grasp or Finger Guard) or a forced
         * failure reads MISSED for an ability that targets allies only
         * (ACTION_MISS_DISPLAY_ALLIES_ONLY) and GUARDED otherwise; a catch reads
         * CAUGHT, a canceled or reflected action nothing, any other miss MISSED. */
        miss = action->miss_type;
        if (miss != ACTION_MISS_TYPE_HIT) {
            /* Types 1 and 2, accessory and right-hand evade, as the original range test. */
            if ((u32)(action->miss_type - 1) < 2 || miss == ACTION_MISS_TYPE_LEFT_HAND_EVADE
                || miss == ACTION_MISS_TYPE_FORCED_FAILURE || miss == ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD) {
                if ((action->miss_display_flags & ACTION_MISS_DISPLAY_TEAM_MASK) == ACTION_MISS_DISPLAY_ALLIES_ONLY) {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_MISSED;
                } else {
                    unit->action_display_flags |= ACTION_DISPLAY_FLAG_GUARDED;
                }
            } else if (miss == ACTION_MISS_TYPE_CATCH) {
                unit->action_display_flags |= ACTION_DISPLAY_FLAG_CAUGHT;
            } else if (miss != ACTION_MISS_TYPE_CANCELED && miss != ACTION_MISS_TYPE_HIT
                && miss != ACTION_MISS_TYPE_REFLECTED) {
                unit->action_display_flags |= ACTION_DISPLAY_FLAG_MISSED;
            }
        }
    }
    if (!unit->numeric_display.active) {
        unit->numeric_display.progress = 0;
        build_next_result_display(unit);
    }
}

/* Builds the next EXP/JP or failure-message display: gained Exp, then gained JP,
 * then no target, silenced or not enough MP, clearing that flag. A number shows
 * its last two digits (a value over 999 shows 99).
 *
 * The companion of build_next_result_display for flags
 * 0x02000000 and above. Returns 0 when no flag is pending. Reading the
 * flags into a block-local before clearing keeps the target's and operand
 * order. */
static s32 build_next_exp_jp_or_failure_display(map_unit_t* unit) {
    action_result_display_t* display;
    sprite_display_t* display_0;
    sprite_display_t* display_1;
    sprite_display_t* display_2;
    s32 digits[3];
    u32 flags;
    u32 clear;
    u16 selector;

    flags = unit->action_display_flags;
    display = &unit->numeric_display;
    if (flags & ACTION_DISPLAY_FLAG_GAINED_EXP) {
        unit->numeric_display.selector = RESULT_DISPLAY_GAINED_EXP;
        unit->numeric_display.active = 1;
        unit->numeric_display.value = unit->strike.action_rewards.earned_experience;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_GAINED_EXP;
        if (unit->numeric_display.progress >= 22) {
            unit->numeric_display.progress = 21;
        }
    } else if (flags & ACTION_DISPLAY_FLAG_GAINED_JP) {
        unit->numeric_display.selector = RESULT_DISPLAY_GAINED_JP;
        unit->numeric_display.active = 1;
        unit->numeric_display.value = unit->strike.action_rewards.earned_jp;
        unit->action_display_flags &= ~ACTION_DISPLAY_FLAG_GAINED_JP;
        if (unit->numeric_display.progress >= 22) {
            unit->numeric_display.progress = 21;
        }
    } else {
        if (flags & ACTION_DISPLAY_FLAG_NO_TARGET) {
            clear = ~ACTION_DISPLAY_FLAG_NO_TARGET;
            selector = RESULT_DISPLAY_NO_TARGET;
        } else if (flags & ACTION_DISPLAY_FLAG_SILENCED) {
            clear = ~ACTION_DISPLAY_FLAG_SILENCED;
            selector = RESULT_DISPLAY_SILENCED;
        } else if (flags & ACTION_DISPLAY_FLAG_NO_MP) {
            clear = ~ACTION_DISPLAY_FLAG_NO_MP;
            selector = RESULT_DISPLAY_NO_MP;
        } else {
            return 0;
        }
        unit->numeric_display.selector = selector;
        unit->numeric_display.active = 1;
        {
            u32 current;
            current = unit->action_display_flags;
            unit->action_display_flags = current & clear;
        }
        if (unit->numeric_display.progress >= 22) {
            unit->numeric_display.progress = 21;
        }
    }

    display->displays[0]->color.r = display->displays[1]->color.r = display->displays[2]->color.r = 150;
    display->displays[0]->color.g = display->displays[1]->color.g = display->displays[2]->color.g = 140;
    display->displays[0]->color.b = display->displays[1]->color.b = display->displays[2]->color.b = 20;

    if ((display->value / 1000) & 0xffff) {
        digits[2] = 9;
        digits[1] = 9;
        digits[0] = 9;
    } else {
        digits[0] = (display->value % 10) & 0xffff;
        digits[1] = (((display->value / 10) & 0xffff) % 10) & 0xffff;
        digits[2] = (display->value / 100) & 0xffff;
    }

    display_0 = display->displays[0];
    display_1 = display->displays[1];
    display_2 = display->displays[2];

    /* Empty displays 1 and 2; each case below fills only the sprites it uses. */
    sprite_display_set_part(display_2, 0, -21, 16, 0, 0, 0, 0, 0);
    sprite_display_set_part(display_1, 0, -21, 16, 0, 0, 0, 0, 0);

    switch (display->selector & RESULT_DISPLAY_SELECTOR_MASK) {
    case RESULT_DISPLAY_GAINED_EXP:
        if (digits[1] == 0) {
            sprite_display_set_part(display_0, 0, -7, -24, 0xc0, 0xb8, 0x12, 9, 0);
            sprite_display_set_part(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else {
            sprite_display_set_part(display_0, 0, -7, -24, 0xc0, 0xb8, 0x12, 9, 0);
            sprite_display_set_part(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            sprite_display_set_part(display_2, 0, -21, -32, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        }
        break;
    case RESULT_DISPLAY_GAINED_JP:
        if (digits[1] == 0) {
            sprite_display_set_part(display_0, 0, -7, -24, 0xb4, 0xb8, 0x0c, 9, 0);
            sprite_display_set_part(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
        } else {
            sprite_display_set_part(display_0, 0, -7, -24, 0xb4, 0xb8, 0x0c, 9, 0);
            sprite_display_set_part(display_1, 0, -14, -32, 0xa8 + digits[0] * 8, 0x30, 8, 16, 0);
            sprite_display_set_part(display_2, 0, -21, -32, 0xa8 + digits[1] * 8, 0x30, 8, 16, 0);
        }
        break;
    case RESULT_DISPLAY_NO_TARGET:
        display->displays[0]->color.r = display->displays[0]->color.g = display->displays[0]->color.b = 128;
        sprite_display_set_part(display_0, 0, -14, -24, 0x1a, 0xdb, 0x25, 10, 0);
        break;
    case RESULT_DISPLAY_SILENCED:
        display->displays[0]->color.r = display->displays[0]->color.g = display->displays[0]->color.b = 128;
        sprite_display_set_part(display_0, 0, -14, -24, 0xd8, 0xe9, 0x1e, 8, 0);
        break;
    case RESULT_DISPLAY_NO_MP:
        display->displays[0]->color.r = display->displays[0]->color.g = display->displays[0]->color.b = 128;
        sprite_display_set_part(display_0, 0, -14, -24, 0x64, 0x20, 0x1a, 9, 0);
        break;
    }

    map_palette_load_data(&s_result_palette, MAP_CLUT_ROW_ACTION_RESULTS, unit->map_unit_id, 0);
    return 1;
}

/* Queues the gained Exp and JP displays after an action, when the Display Gained
 * Exp/JP option is on and the amount is not 0. The first display is built at once
 * unless one is already running. */
void result_queue_exp_jp_display(map_unit_t* unit) {
    sprite_display_t* display_0;
    sprite_display_t* display_1;
    sprite_display_t* display_2;

    if (g_game_options.fields.display_gained_exp_jp == GAME_OPTION_ON) {
        display_0 = unit->numeric_display.displays[0];
        display_1 = unit->numeric_display.displays[1];
        display_2 = unit->numeric_display.displays[2];
        display_2->tpage = SPRITE_TPAGE_FRAME;
        display_1->tpage = SPRITE_TPAGE_FRAME;
        display_0->tpage = SPRITE_TPAGE_FRAME;
        if (unit->strike.action_rewards.earned_experience != 0) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_GAINED_EXP;
        }
        if (unit->strike.action_rewards.earned_jp != 0) {
            unit->action_display_flags |= ACTION_DISPLAY_FLAG_GAINED_JP;
        }
        if (!unit->numeric_display.active) {
            unit->numeric_display.progress = 0;
            build_next_exp_jp_or_failure_display(unit);
        }
    }
}

/* Queues the failure message for a charged action that cannot start: mode is the
 * action_rejection_e of action_init_resolution, and only no target,
 * silenced and not enough MP queue one.
 *
 * The three numeric displays are pointed at the FRAME.BIN page, and the first
 * display is built at once unless one is already running. */
void result_queue_charge_failure(map_unit_t* unit, s32 reason) {
    sprite_display_t* display_0 = unit->numeric_display.displays[0];
    sprite_display_t* display_1 = unit->numeric_display.displays[1];
    sprite_display_t* display_2 = unit->numeric_display.displays[2];

    display_2->tpage = SPRITE_TPAGE_FRAME;
    display_1->tpage = SPRITE_TPAGE_FRAME;
    display_0->tpage = SPRITE_TPAGE_FRAME;
    switch (reason) {
    case ACTION_REJECTION_NO_TARGET:
        unit->action_display_flags |= ACTION_DISPLAY_FLAG_NO_TARGET;
        break;
    case ACTION_REJECTION_SILENCED:
        unit->action_display_flags |= ACTION_DISPLAY_FLAG_SILENCED;
        break;
    case ACTION_REJECTION_NOT_ENOUGH_MP:
        unit->action_display_flags |= ACTION_DISPLAY_FLAG_NO_MP;
        break;
    }
    if (!unit->numeric_display.active) {
        unit->numeric_display.progress = 0;
        build_next_exp_jp_or_failure_display(unit);
    }
}

/* Animate and queue the three numeric post-action display sprites.
 *
 * Over the first 21 frames each digit sprite scales by its own zoom curve (the
 * critical curves for a critical hit); the word displays (Missed, Guarded, EXP,
 * JP) scale all three parts by the middle digit's curve. At frame 50 the next
 * queued result starts, or else the display begins fading: its palette darkens
 * and its parts turn semi-transparent. After frame 61 it retires unless another
 * result follows. Damage draws 0x30 pixels above the unit's screen position,
 * healing and gains 0x10, the words 8. */
static void animate_result_text(map_unit_t* unit, const SVECTOR* screen_coords) {
    SVECTOR position;
    SVECTOR zoom_0 = s_identity_scale;
    SVECTOR zoom_1 = s_identity_scale;
    SVECTOR zoom_2 = s_identity_scale;
    s16 scale_0[21] = { 0, 1060, 2048, 2896, 3574, 3956, ONE, 3584, 3584, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE, ONE,
        ONE, ONE, ONE };
    s16 scale_1[21]
        = { 0, 0, 0, 0, 0, 0, 0, 1060, 2048, 2896, 3574, 3956, ONE, 3584, 3584, ONE, ONE, ONE, ONE, ONE, ONE };
    s16 scale_2[21] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1060, 2048, 2896, 3574, 3956, ONE, 3584, 3584 };
    s16 critical_scale_0[21] = { 0, 1228, 2457, 3685, 4913, 6144, 4913, 3685, 3900, ONE, ONE, ONE, ONE, ONE, ONE, ONE,
        ONE, ONE, ONE, ONE, ONE };
    s16 critical_scale_1[21]
        = { 0, 0, 0, 0, 0, 0, 0, 1228, 2457, 3685, 4913, 6144, 4913, 3685, 3900, ONE, ONE, ONE, ONE, ONE, ONE };
    s16 critical_scale_2[21]
        = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1228, 2457, 3685, 4913, 6144, 4913, 3685, 3900, ONE };
    sprite_display_t* display_0;
    sprite_display_t* display_1;
    sprite_display_t* display_2;
    action_result_display_t* display;
    u16 progress;

    display_0 = unit->numeric_display.displays[0];
    display_1 = unit->numeric_display.displays[1];
    display_2 = unit->numeric_display.displays[2];
    progress = unit->numeric_display.progress + g_system_vsync_interval;
    display = &unit->numeric_display;
    if (progress < 21) {
        if (unit->numeric_display.selector & RESULT_DISPLAY_MODIFIER_CRITICAL) {
            vector_set_short(
                &zoom_0, critical_scale_0[progress], critical_scale_0[progress], critical_scale_0[progress]);
            vector_set_short(
                &zoom_1, critical_scale_1[progress], critical_scale_1[progress], critical_scale_1[progress]);
            vector_set_short(
                &zoom_2, critical_scale_2[progress], critical_scale_2[progress], critical_scale_2[progress]);
        } else {
            switch (unit->numeric_display.selector & RESULT_DISPLAY_SELECTOR_MASK) {
            case RESULT_DISPLAY_HP_DAMAGE ... RESULT_DISPLAY_MA_GAIN:
                vector_set_short(&zoom_0, scale_0[progress], scale_0[progress], scale_0[progress]);
                vector_set_short(&zoom_1, scale_1[progress], scale_1[progress], scale_1[progress]);
                vector_set_short(&zoom_2, scale_2[progress], scale_2[progress], scale_2[progress]);
                break;
            case RESULT_DISPLAY_MISSED:
            case RESULT_DISPLAY_GUARDED:
            case RESULT_DISPLAY_GAINED_EXP:
            case RESULT_DISPLAY_GAINED_JP:
                vector_set_short(&zoom_0, scale_1[progress], scale_1[progress], scale_1[progress]);
                vector_set_short(&zoom_1, scale_1[progress], scale_1[progress], scale_1[progress]);
                vector_set_short(&zoom_2, scale_1[progress], scale_1[progress], scale_1[progress]);
                break;
            }
        }
    } else if (progress >= 50 && unit->numeric_display.progress < 50) {
        if (build_next_result_display(unit)) {
            return;
        }
        if (build_next_exp_jp_or_failure_display(unit)) {
            return;
        }
        unit->numeric_display.displays[0]->tpage = unit->numeric_display.displays[1]->tpage
            = unit->numeric_display.displays[2]->tpage = SPRITE_TPAGE_FRAME | SPRITE_DISPLAY_BLEND_ADD;
        map_palette_modify(MAP_COLOR_MODE_BASE, 2, MAP_CLUT_ROW_ACTION_RESULTS, unit->map_unit_id, 0, -31, -31, -31);
        sprite_display_set_part_semi_transparent(display_0, 0, 1);
        sprite_display_set_part_semi_transparent(display_1, 0, 1);
        sprite_display_set_part_semi_transparent(display_2, 0, 1);
    } else if (progress >= 61) {
        if (build_next_result_display(unit)) {
            return;
        }
        if (build_next_exp_jp_or_failure_display(unit)) {
            return;
        }
        unit->numeric_display.active = 0;
    }
    position.vx = screen_coords->vx + 0x15;
    position.vy = screen_coords->vy - 8;
    switch (display->selector & RESULT_DISPLAY_SELECTOR_MASK) {
    case RESULT_DISPLAY_HP_DAMAGE:
    case RESULT_DISPLAY_MP_DAMAGE:
    case RESULT_DISPLAY_SPEED_DAMAGE:
    case RESULT_DISPLAY_CT_DAMAGE:
    case RESULT_DISPLAY_BRAVE_DAMAGE:
    case RESULT_DISPLAY_FAITH_DAMAGE:
    case RESULT_DISPLAY_PA_DAMAGE:
    case RESULT_DISPLAY_MA_DAMAGE:
        position.vy = screen_coords->vy - 0x30;
        break;
    case RESULT_DISPLAY_HP_HEALING:
    case RESULT_DISPLAY_MP_HEALING:
    case RESULT_DISPLAY_SPEED_GAIN:
    case RESULT_DISPLAY_CT_GAIN:
    case RESULT_DISPLAY_BRAVE_GAIN:
    case RESULT_DISPLAY_FAITH_GAIN:
    case RESULT_DISPLAY_PA_GAIN:
    case RESULT_DISPLAY_MA_GAIN:
        position.vy = screen_coords->vy - 0x10;
        break;
    case RESULT_DISPLAY_MISSED:
    case RESULT_DISPLAY_GUARDED:
    case RESULT_DISPLAY_GAINED_EXP:
    case RESULT_DISPLAY_GAINED_JP:
        break;
    }
    sprite_display_draw_parts(display_0, display_0->part_count, 0, &position, 0, 0, &zoom_0, gfx_get_battle_otag() + 1);
    sprite_display_draw_parts(display_1, display_1->part_count, 0, &position, 0, 0, &zoom_1, gfx_get_battle_otag() + 1);
    sprite_display_draw_parts(display_2, display_2->part_count, 0, &position, 0, 0, &zoom_2, gfx_get_battle_otag() + 1);
    display->progress += g_system_vsync_interval;
}

/* sprite state */

/* Request `animation` with facing `facing` (0x400 per quarter turn) for the next animation update;
 * the request is dropped when that sequence is already playing in the same view. The request
 * is stored as the animation plus one, so 0 means none. */
void animation_request(u32 animation, s32 facing, map_unit_t* unit) {
    animation++;
    unit->requested_animation = animation;
    unit->facing = facing;
}

/* animation_request with the force flag: the animation
 * starts on the next update, from its beginning even when it is playing. */
void animation_request_forced(s32 animation, s32 facing, map_unit_t* unit) {
    animation++;
    unit->requested_animation = animation;
    unit->facing = facing;
    unit->movement.force_animation_update = 1;
}

/* The SHP frame tables of a sheet: one of the five resident ones (TYPE1 through OTHER), or the
 * battle's boss SHP for the higher SHP types. */
static sprite_unit_shp_frame_tables_t* get_unit_shp(u32 spritesheet_id) {
    s32 value = g_sprite_sheet_data[spritesheet_id & 0xffff].shp_id;

    if (value < 5) {
        return &g_sprite_unit_shp_frame_tables[value];
    }
    return &g_sprite_boss_shp_frame_tables;
}

/* The SEQ table of a sheet: one of the five resident ones, or the table of
 * the battle's RUKA, ARUTE or KANZEN file. */
static sprite_unit_seq_table_t* get_unit_seq(u32 spritesheet_id) {
    s32 value = g_sprite_sheet_data[spritesheet_id & 0xffff].seq_id;

    if (value < SPRITE_SEQ_RUKA) {
        return &g_sprite_unit_seq_tables[value].header;
    }
    return &g_sprite_boss_seq_tables.header;
}

/* The weapon SHP of a sheet. The weapon tables are 0xc40-byte records, a
 * sprite_weapon_shp_t with 0x200 frames and then its 0x100 sequence
 * pointers: WEP1 for SHP type TYPE1 and WEP2 for TYPE2. The other SHP types
 * index past the two records; none of their sequences starts a weapon block. */
static sprite_weapon_shp_t* get_weapon_shp(u32 spritesheet_id) {
    return &g_sprite_weapon_graphics[g_sprite_sheet_data[spritesheet_id & 0xffff].shp_id].shp;
}

/* The weapon SEQ table of a sheet: the second part of the record that
 * get_weapon_shp describes. */
static u8** get_weapon_seq(u32 spritesheet_id) {
    return g_sprite_weapon_graphics[g_sprite_sheet_data[spritesheet_id & 0xffff].shp_id].seq;
}

/* Every sheet shares the one effect SHP, EFF1. */
static sprite_weapon_shp_t* get_effect_shp(u32 unused_spritesheet_id) {
    return &g_sprite_effect_graphics[0].shp;
}

/* Every sheet shares the one effect SEQ table, EFF1. */
static u8** get_effect_seq(u32 unused_spritesheet_id) {
    return g_sprite_effect_graphics[0].seq;
}

/* Nonzero for a sheet that idles with the flying animations. */
static u32 get_flying_flag(u32 spritesheet_id) {
    return g_sprite_sheet_data[spritesheet_id & 0xffff].flying_flag;
}

/* Center the unit on its map tile: its move destination becomes its tile, its world position the
 * tile center at its standing height, and its real position follows. */
void map_unit_set_move_and_world_coords(map_unit_t* unit) {
    unit->movement.destination_x = unit->map_x;
    unit->movement.destination_y = unit->map_y;
    unit->movement.destination_layer = unit->map_layer;
    unit->world_coords.vx = unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit->world_coords.vz = unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit->world_coords.vy = map_unit_calculate_height_from_world_coords(unit);
    set_real_coords_from_world_coords(unit);
}

/* Moves a rider killed on its mount (pending_attack_result set) off it: the pair
 * is unlinked, the unit takes the relocation tile in dismount as its map position
 * and screen position, its battle stats follow, and the pending result is
 * cleared. */
void map_unit_dismount_dead_rider(map_unit_t* unit) {
    /* pending_attack_result is -1 when the action killed a rider on its mount. */
    if (unit->pending_attack_result != 0) {
        map_unit_rider_dismount_and_update_display(unit);
        unit->map_x = unit->dismount.x;
        unit->map_y = unit->dismount.y;
        unit->map_layer = unit->dismount.layer;
        map_unit_set_move_and_world_coords(unit);
        battle_unit_set_tile_position(unit->battle_unit->battle_unit_id, unit->map_x, unit->map_y, unit->map_layer,
            (unit->facing / UNIT_FACING_QUARTER_TURN));
        unit->pending_attack_result = 0;
    }
}

/* Return 1 when one of the nine VRAM spritesheet slots already holds `spritesheet_id`. */
static s32 is_sheet_loaded(s16 spritesheet_id) {
    s32 slot;

    for (slot = 0; slot < SPRITE_SHEET_SLOT_COUNT; slot++) {
        if (g_sprite_sheet_slots[slot].spritesheet_id == spritesheet_id) {
            return 1;
        }
    }
    return 0;
}

/* Claim a VRAM spritesheet slot for `spritesheet_id`: reuse the slot that
 * already holds it, otherwise take the first free one. Returns the slot, or
 * reports SYSTEM_ERROR_NO_SPRITESHEET_SLOT and returns 0xFFFF when all
 * nine are taken. A slot's in_use holds the id of the sheet claiming it. */
static s32 claim_sheet_slot(s16 spritesheet_id) {
    s32 slot;
    u16 found = 0xFF;

    for (slot = 0; slot < SPRITE_SHEET_SLOT_COUNT; slot++) {
        if (g_sprite_sheet_slots[slot].spritesheet_id == spritesheet_id) {
            g_sprite_sheet_slots[slot].in_use = spritesheet_id;
            found = slot;
            break;
        }
    }
    if (found == 0xFF) {
        for (slot = 0; slot < SPRITE_SHEET_SLOT_COUNT; slot++) {
            if (g_sprite_sheet_slots[slot].in_use == 0) {
                g_sprite_sheet_slots[slot].in_use = spritesheet_id;
                g_sprite_sheet_slots[slot].spritesheet_id = spritesheet_id;
                found = slot;
                break;
            }
        }
    }
    if (slot >= SPRITE_SHEET_SLOT_COUNT) {
        system_handle_error(SYSTEM_ERROR_NO_SPRITESHEET_SLOT);
        return 0xFFFF;
    }
    return found;
}

/* Reload a unit's palette tinted for its tile's shading, the weather and the
 * time of day.
 *
 * The second CLUT row is installed first from the slot's upper eight
 * palettes. The RGB offset starts at -4, -8 or -12 for the three tile shading
 * levels. Unless the map ignores weather, rain and storms cool it (less red,
 * more blue), night cools it further, and a unit more than 0x30 above the
 * ground at its position (in the air, as during Jump) stays untinted. Crystal,
 * chicken, frog and treasure sheets tint fixed RAM palettes instead of the
 * slot's.
 *
 * Each branch makes its own palette call, as the original evidently did:
 * cross-jumping merges them only after register allocation, so the extra
 * references still rank clut_index above unit in global allocation. The two snow
 * (weather bit 0) branches are identical. */
void sprite_palette_tint_unit(map_unit_t* unit, s32 level, s32 update_current_colors) {
    s32 clut_index;
    s32 stored_palette;
    s32 slot = unit->spritesheet_vram_slot;
    s32 weather;
    s32 blue;
    s32 green;
    s32 red;
    s16 standing_world_y;

    stored_palette = unit->stored_palette;
    clut_index = unit->map_unit_id;

    /* The second CLUT row comes from the upper eight palettes: the unit's own for job and
     * monster sheets, the first one for the rest. */
    if (g_sprite_sheet_slots[slot].spritesheet_id >= SPRITE_SHEET_ID_JOB_FIRST
        && g_sprite_sheet_slots[slot].spritesheet_id <= SPRITE_SHEET_ID_STEEL_GIANT) {
        map_palette_load_data(
            &g_sprite_sheet_slots[slot].clut[stored_palette + 8], MAP_CLUT_ROW_UNITS_SECOND, clut_index, 0);
    } else {
        map_palette_load_data(&g_sprite_sheet_slots[slot].clut[8], MAP_CLUT_ROW_UNITS_SECOND, clut_index, 0);
    }

    blue = 0;
    green = 0;
    red = 0;
    switch (level) {
    case MAP_TILE_SHADING_DARK:
        blue = -4;
        green = -4;
        red = -4;
        break;
    case MAP_TILE_SHADING_DARKER:
        blue = -8;
        green = -8;
        red = -8;
        break;
    case MAP_TILE_SHADING_DARKEST:
        blue = -0xc;
        green = -0xc;
        red = -0xc;
        break;
    }

    {
        const s32* weather_flags = &g_weather_flags;

        if (!(*weather_flags & WEATHER_FLAG_IGNORE_WEATHER)) {
            weather = script_variable_get(SCRIPT_VAR_WEATHER);
            /* Weather cools the tint (less red, more blue); at night the shift is stronger and
             * applies in clear weather too. The snow branches match the plain ones. */
            if (script_variable_get(SCRIPT_VAR_TIME_OF_DAY) == TIME_OF_DAY_DAY) {
                if (*weather_flags & WEATHER_FLAG_SNOW) {
                    switch (weather) {
                    case WEATHER_NONE:
                    case WEATHER_NONE_ALT:
                        break;
                    case WEATHER_RAIN:
                        red -= 1;
                        blue += 1;
                        break;
                    case WEATHER_STORM:
                    case WEATHER_STRONG_STORM:
                    default:
                        red -= 2;
                        green -= 1;
                        blue += 2;
                        break;
                    }
                } else {
                    switch (weather) {
                    case WEATHER_NONE:
                    case WEATHER_NONE_ALT:
                        break;
                    case WEATHER_RAIN:
                        red -= 1;
                        blue += 1;
                        break;
                    case WEATHER_STORM:
                    case WEATHER_STRONG_STORM:
                    default:
                        red -= 2;
                        green -= 1;
                        blue += 2;
                        break;
                    }
                }
            } else {
                if (*weather_flags & WEATHER_FLAG_SNOW) {
                    switch (weather) {
                    case WEATHER_NONE:
                    case WEATHER_NONE_ALT:
                        red -= 3;
                        green -= 1;
                        blue += 2;
                        break;
                    case WEATHER_RAIN:
                        red -= 4;
                        green -= 1;
                        blue += 3;
                        break;
                    case WEATHER_STORM:
                    case WEATHER_STRONG_STORM:
                    default:
                        red -= 4;
                        green -= 2;
                        blue += 4;
                        break;
                    }
                } else {
                    switch (weather) {
                    case WEATHER_NONE:
                    case WEATHER_NONE_ALT:
                        red -= 3;
                        green -= 1;
                        blue += 2;
                        break;
                    case WEATHER_RAIN:
                        red -= 4;
                        green -= 1;
                        blue += 3;
                        break;
                    case WEATHER_STORM:
                    case WEATHER_STRONG_STORM:
                    default:
                        red -= 4;
                        green -= 2;
                        blue += 4;
                        break;
                    }
                }
            }
        }
    }

    /* y grows downward: a unit more than 0x30 above the ground at its position stays untinted. */
    standing_world_y = map_unit_calculate_height_from_world_coords(unit);
    if (!(g_weather_flags & WEATHER_FLAG_IGNORE_WEATHER) && standing_world_y - 0x30 > unit->world_coords.vy) {
        blue = 0;
        green = 0;
        red = 0;
    }

    /* Crystal, chicken, frog and treasure sheets tint fixed RAM palettes instead of the slot's. */
    switch (unit->spritesheet_id) {
    case SPRITE_SHEET_ID_CRYSTAL:
        map_palette_set_from_source(g_sprite_other_palettes.crystal[stored_palette], MAP_CLUT_ROW_UNITS, clut_index, 0,
            red, green, blue, update_current_colors);
        break;
    case SPRITE_SHEET_ID_CHICKEN:
        map_palette_set_from_source(g_sprite_other_palettes.chicken[stored_palette], MAP_CLUT_ROW_UNITS, clut_index, 0,
            red, green, blue, update_current_colors);
        break;
    case SPRITE_SHEET_ID_FROG:
        map_palette_set_from_source(g_sprite_other_palettes.frog[stored_palette], MAP_CLUT_ROW_UNITS, clut_index, 0,
            red, green, blue, update_current_colors);
        break;
    case SPRITE_SHEET_ID_TREASURE:
        map_palette_set_from_source(g_sprite_other_palettes.treasure[stored_palette], MAP_CLUT_ROW_UNITS, clut_index, 0,
            red, green, blue, update_current_colors);
        break;
    default:
        map_palette_set_from_source(g_sprite_sheet_slots[slot].clut[stored_palette], MAP_CLUT_ROW_UNITS, clut_index, 0,
            red, green, blue, update_current_colors);
        break;
    }
}

/* Apply palette modulation to a renderer-side unit's current CLUT.
 *
 * Crystal, Chicken, Frog, and Treasure use their dedicated RAM palette
 * tables; every other sheet uses its VRAM-slot palette. The color parameters
 * must remain s16 to preserve the target's register allocation and tail
 * sharing.
 */
static void modify_unit_palette(map_unit_t* unit, s32 mode, s32 clut_row, s32 clut_index, s32 all_colors, s16 red,
    s16 green, s16 blue, s32 update_current_colors) {
    s32 slot;
    s32 sheet;
    s32 palette;

    slot = unit->spritesheet_vram_slot;
    sheet = unit->spritesheet_id;
    palette = unit->stored_palette;
    switch (sheet) {
    case SPRITE_SHEET_ID_CRYSTAL:
        map_palette_modify_from_source(mode, g_sprite_other_palettes.crystal[palette], clut_row, clut_index, all_colors,
            red, green, blue, update_current_colors);
        break;
    case SPRITE_SHEET_ID_CHICKEN:
        map_palette_modify_from_source(mode, g_sprite_other_palettes.chicken[palette], clut_row, clut_index, all_colors,
            red, green, blue, update_current_colors);
        break;
    case SPRITE_SHEET_ID_FROG:
        map_palette_modify_from_source(mode, g_sprite_other_palettes.frog[palette], clut_row, clut_index, all_colors,
            red, green, blue, update_current_colors);
        break;
    case SPRITE_SHEET_ID_TREASURE:
        map_palette_modify_from_source(mode, g_sprite_other_palettes.treasure[palette], clut_row, clut_index,
            all_colors, red, green, blue, update_current_colors);
        break;
    default:
        map_palette_modify_from_source(mode, g_sprite_sheet_slots[slot].clut[palette], clut_row, clut_index, all_colors,
            red, green, blue, update_current_colors);
        break;
    }
}

/* Tint a unit's palette for its first palette-affecting status.
 *
 * Unless the unit already has a palette modifier, tints its CLUT for the
 * first matching renderer status (mode/RGB pairs below; Undead skips Skeleton
 * and Ghoul spritesheets); otherwise, or with no tinting status, it falls back
 * to sprite_palette_tint_unit. */
static void update_unit_status_palette(map_unit_t* unit, s32 tile_effect_level, s32 update_current_colors) {
    u32 status;
    s32 unit_id;

    if (unit->fade_out_phase != UNIT_FADE_OUT_NONE) {
        return;
    }
    status = unit->status_flags_1_4;
    unit_id = unit->map_unit_id;
    if (status & UNIT_STATUS_PALETTE_MOD_MASK) {
        if (status & UNIT_STATUS_PETRIFY) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_GRAY, MAP_CLUT_ROW_UNITS, unit_id, 0, 0, 0, 0, update_current_colors);
        } else if (status & UNIT_STATUS_BLOOD_SUCK) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_GRAY, MAP_CLUT_ROW_UNITS, unit_id, 0, 3, -1, 8, update_current_colors);
        } else if (status & UNIT_STATUS_BERSERK) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE, MAP_CLUT_ROW_UNITS, unit_id, 0, 8, 0, 0, update_current_colors);
        } else if (status & UNIT_STATUS_POISON) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_HALF, MAP_CLUT_ROW_UNITS, unit_id, 0, 0, 8, 0, update_current_colors);
        } else if (status & UNIT_STATUS_REGEN) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE, MAP_CLUT_ROW_UNITS, unit_id, 0, 0, 0, 8, update_current_colors);
        } else if (status & UNIT_STATUS_OIL) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_HALF, MAP_CLUT_ROW_UNITS, unit_id, 0, -4, -4, -4, update_current_colors);
        } else if (status & UNIT_STATUS_UNNAMED_0E) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_HALF, MAP_CLUT_ROW_UNITS, unit_id, 0, -8, -8, -8, update_current_colors);
        } else if ((status & UNIT_STATUS_UNDEAD) && unit->spritesheet_id != SPRITE_SHEET_ID_SKELETON
            && unit->spritesheet_id != SPRITE_SHEET_ID_GHOUL) {
            modify_unit_palette(
                unit, MAP_COLOR_MODE_BASE_HALF, MAP_CLUT_ROW_UNITS, unit_id, 0, 4, 0, 5, update_current_colors);
        } else {
            sprite_palette_tint_unit(unit, tile_effect_level, update_current_colors);
        }
        return;
    }
    sprite_palette_tint_unit(unit, tile_effect_level, update_current_colors);
}

/* Refresh the status palette of a unit and of its mount partner, unless suppressed. */
void sprite_palette_update(map_unit_t* unit, s32 tile_effect_level, s32 update_current_colors) {
    map_unit_t* partner;

    if (!(unit->movement.suppress_palette_update)) {
        update_unit_status_palette(unit, tile_effect_level, update_current_colors);
    }
    if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
        partner = map_unit_get(unit->mount_partner_map_unit_id);
        if (partner != 0 && !(partner->movement.suppress_palette_update)) {
            update_unit_status_palette(partner, tile_effect_level, update_current_colors);
        }
    }
}

/* Enables the status bubble, restarting its timer, for a unit with a status that
 * shows a bubble (UNIT_STATUS_BUBBLE_MASK) and for the acting unit;
 * disables it for any other unit. */
void sprite_status_bubble_update_active(map_unit_t* unit) {
    if ((unit->status_flags_1_4 & UNIT_STATUS_BUBBLE_MASK) != 0 || unit->map_unit_id == g_unit_source_map_unit_id) {
        unit->status_bubble_active = 1;
        unit->status_bubble_timer = 0;
    } else {
        unit->status_bubble_active = 0;
    }
}

/* Draws a unit with the Transparent status semi-transparent with averaging blend
 * (the blend bits cleared), and any other unit opaque. */
void sprite_update_transparency(map_unit_t* unit) {
    if (unit->status_flags_5_6 & UNIT_STATUS_TRANSPARENT) {
        unit->sprite_display_flags
            = (unit->sprite_display_flags & ~SPRITE_DISPLAY_BLEND_MASK) | SPRITE_DISPLAY_SEMI_TRANSPARENT;
    } else {
        unit->sprite_display_flags = unit->sprite_display_flags & ~SPRITE_DISPLAY_SEMI_TRANSPARENT;
    }
}

/* Mirror the Float status into movement.floating and, unless the unit is in the air for Jump
 * (Jump, its height flag, or a Jump being removed), set it to its standing height and refresh its
 * real position. */
void map_unit_update_float_height(map_unit_t* unit) {
    if (unit->status_flags_5_6 & UNIT_STATUS_FLOAT) {
        unit->movement.floating = 1;
    } else {
        unit->movement.floating = 0;
    }
    if ((unit->status_flags_5_6 & (UNIT_STATUS_JUMP | UNIT_STATUS_JUMP_HEIGHT_ACTIVE)) == 0
        && (unit->statuses_to_remove_5_6 & UNIT_STATUS_JUMP) == 0) {
        unit->world_coords.vy = map_unit_calculate_height_from_world_coords(unit);
        set_real_coords_from_world_coords(unit);
    }
}

/* animation policy */

extern clut_t g_sprite_weapon_clut;

/* Play the weapon swing sound (`hit` 0) or the sound of each strike target's
 * outcome (`hit` 1).
 *
 * The sound tables cover the 0x20 weapon item types, so shuriken and the later
 * item types use the knife's sounds. A target's miss type picks its sound: a
 * hand or Golem guard plays the weapon's deflect sound; an accessory or class
 * evasion, a forced failure, Blade Grasp or a catch the error sound; a
 * nullified, missed or canceled action none; anything else (a hit or a
 * reflection) the weapon's hit sound or, when critical, SOUND_SFX_CRITICAL_HIT.
 * A target without battle stats is skipped. */
void action_play_weapon_sfx(map_unit_t* unit, s32 hit) {
    map_unit_t* target;
    s32 weapon_type = item_get_data(unit->strike.used_weapon_id)->type;
    s32 i;

    if (weapon_type >= ITEM_TYPE_SHURIKEN) {
        weapon_type = ITEM_TYPE_KNIFE;
    }
    switch (hit) {
    case 0:
        map_unit_play_sfx(unit, s_action_weapon_swing_sfx_ids[weapon_type]);
        break;
    case 1:
        for (i = 0; i < unit->strike.target_count; i++) {
            target = map_unit_get_by_battle_unit_id(unit->strike.target_list[i]);
            if (target == 0 || target->battle_unit == 0) {
                continue;
            }
            switch (target->battle_unit->action.miss_type) {
            case ACTION_MISS_TYPE_RIGHT_HAND_EVADE:
            case ACTION_MISS_TYPE_LEFT_HAND_EVADE:
            case ACTION_MISS_TYPE_GOLEM_GUARD:
                map_unit_play_sfx(unit, s_action_shield_deflect_sfx_ids[weapon_type]);
                break;
            case ACTION_MISS_TYPE_ACCESSORY_EVADE:
            case ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD:
            case ACTION_MISS_TYPE_FORCED_FAILURE:
            case ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD:
            case ACTION_MISS_TYPE_CATCH:
                map_unit_play_sfx(unit, SOUND_SFX_ERROR_MESSAGE);
                break;
            case ACTION_MISS_TYPE_NULLIFIED:
            case ACTION_MISS_TYPE_ACCURACY_MISS:
            case ACTION_MISS_TYPE_CANCELED:
                break;
            default:
                if (target->battle_unit->action.critical != 0) {
                    map_unit_play_sfx(unit, SOUND_SFX_CRITICAL_HIT);
                } else {
                    map_unit_play_sfx(unit, s_action_weapon_hit_sfx_ids[weapon_type]);
                }
                break;
            }
        }
        break;
    }
}

/* The depth of water the unit stands in, which picks its animations: 0 on the surface (ON_WATER
 * movement, Float, a crystal), 1 for a chicken, frog or treasure and for IN_WATER movement,
 * otherwise the depth of its tile. */
static u8 get_water_depth(map_unit_t* unit) {
    map_tile_t* tile;
    s32 movement;
    u32 statuses;

    tile = map_get_tile(unit->world_coords.vx / MAP_TILE_SIZE, unit->world_coords.vz / MAP_TILE_SIZE, unit->map_layer);
    if (unit->battle_unit != 0) {
        movement = move_get_effective_flags(unit->battle_unit);
        if (movement & MOVE_EFFECTIVE_ON_WATER) {
            return 0;
        }
        statuses = unit->status_flags_5_6;
        if (statuses & UNIT_STATUS_FLOAT) {
            return 0;
        }
        if (statuses & (UNIT_STATUS_CHICKEN | UNIT_STATUS_FROG | UNIT_STATUS_TREASURE)) {
            return 1;
        }
        if (movement & MOVE_EFFECTIVE_IN_WATER) {
            return 1;
        }
        if (statuses & UNIT_STATUS_CRYSTAL) {
            return 0;
        }
    }
    return tile->depth_half_height.depth;
}

/* Choose the attacker's weapon attack animation.
 *
 * Each item type has three animation variants. Bows select their table
 * column from the last trajectory test. Other weapons compare the vertical
 * centers of the two sprites: a target 12 or more above the attacker takes
 * the first animation and one 12 or more below the third. A Golem or Blade
 * Grasp guard always takes the first, and the sheets from the crystal up
 * always play the unarmed level attack. */
void animation_select_weapon_attack(map_unit_t* attacker, map_unit_t* target) {
    s32 miss_type;
    s16 attacker_center;
    s16 target_center;
    s32 difference;

    if (attacker == 0) {
        return;
    }
    if (target != 0) {
        if (attacker->spritesheet_id >= SPRITE_SHEET_ID_CRYSTAL) {
            animation_request_forced(s_animation_weapon_attacks[0][1], attacker->facing, attacker);
            return;
        }
        if (attacker->equipped_weapon_type == ITEM_TYPE_BOW) {
            animation_request_forced(s_animation_weapon_attacks[ITEM_TYPE_BOW][projectile_get_bow_animation_column()],
                attacker->facing, attacker);
            return;
        }
        miss_type = target->battle_unit->action.miss_type;
        if (miss_type != ACTION_MISS_TYPE_HIT) {
            if (miss_type >= ACTION_MISS_TYPE_REFLECTED) {
                if (miss_type < ACTION_MISS_TYPE_BUGGED_REFLECTION) {
                    if (miss_type >= ACTION_MISS_TYPE_GOLEM_GUARD) {
                        animation_request_forced(
                            s_animation_weapon_attacks[attacker->equipped_weapon_type][0], attacker->facing, attacker);
                        return;
                    }
                }
            }
        }
        /* The target loads the attacker's world_coords.vy zero-extended (lhu). */
        attacker_center = sprite_sheet_map_unit_get_height(attacker->map_unit_id) / 2 - attacker->world_coords.vy;
        target_center = sprite_sheet_map_unit_get_height(target->map_unit_id) / 2 - target->world_coords.vy;
        difference = attacker_center - target_center;
        if (difference < -11) {
            animation_request_forced(
                s_animation_weapon_attacks[attacker->equipped_weapon_type][0], attacker->facing, attacker);
            return;
        }
        if (difference >= 12) {
            animation_request_forced(
                s_animation_weapon_attacks[attacker->equipped_weapon_type][2], attacker->facing, attacker);
            return;
        }
    }
    animation_request_forced(s_animation_weapon_attacks[attacker->equipped_weapon_type][1], attacker->facing, attacker);
}

/* Start the animation of an item ability: the item held up for a target on
 * the unit's own tile or beside it, thrown for one further away. */
static void set_item_ability_animation(map_unit_t* unit, map_unit_t* target) {
    s32 dx;
    s32 dy;

    sprite_item_display_set_icon(unit, unit->used_item_or_weapon_id);
    if (target != 0) {
        dx = target->map_x - unit->map_x;
        dy = target->map_y - unit->map_y;
    } else {
        dx = 0;
        dy = 0;
    }
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    /* facing is read signed (lh). */
    if ((dx <= 0 && dy <= 0) || (dx < 2 && dy == 0) || (dx == 0 && dy < 2)) {
        animation_request(ANIMATION_HOLD_ITEM, unit->facing, unit);
    } else {
        unit->item_display_active = 0;
        animation_request_forced(ANIMATION_THROW_ITEM, unit->facing, unit);
    }
}

/* Start the first animation of the used ability's charge set, if it has one. */
void animation_start_ability_charge(map_unit_t* unit) {
    u8 set_id = g_ability_animation_data[unit->used_ability_id].charge_animation_set_id;
    u8 animation = s_animation_ability_charge_sets[set_id][0];

    if (animation != 0) {
        animation_request(animation, unit->facing, unit);
    }
}

/* Start the second animation of the used ability's charge set, the one held
 * while charging or performing; the submerged idle in water two or more deep. */
void animation_start_ability_charge_hold(map_unit_t* unit) {
    u8 set_id = g_ability_animation_data[unit->used_ability_id].charge_animation_set_id;
    u8 animation = s_animation_ability_charge_sets[set_id][1];
    u8 water_depth;
    s32 selected_animation;

    if (animation != 0) {
        water_depth = get_water_depth(unit);
        selected_animation = ANIMATION_SUBMERGED_IDLE;
        if (water_depth < 2) {
            selected_animation = animation;
        }
        animation_request(selected_animation, unit->facing, unit);
    }
}

/* Start the attacker's animation for the ability it uses: the cast for the
 * RUKA, ARUTE and KANZEN sequences, else the ability table's attack
 * animation, which may ask for the weapon's attack or the item animation. */
void animation_select_ability_attack(map_unit_t* attacker, map_unit_t* target) {
    s32 animation;
    s32 seq_type = g_sprite_sheet_data[attacker->spritesheet_id].seq_id;

    if (seq_type <= SPRITE_SEQ_KANZEN) {
        if (seq_type >= SPRITE_SEQ_RUKA) {
            animation_request(ANIMATION_CAST, attacker->facing, attacker);
            return;
        }
    }

    animation = g_ability_animation_data[attacker->used_ability_id].attack_animation;
    if (animation == ANIMATION_ATTACK_USE_WEAPON) {
        animation_select_weapon_attack(attacker, target);
        return;
    }
    if (animation == ANIMATION_ATTACK_USE_ITEM) {
        set_item_ability_animation(attacker, target);
        return;
    }
    animation_request(animation, attacker->facing, attacker);
}

/* Start a unit's level-up animation: the jump on dry land, the hops for a
 * rider and in water one deep, the submerged idle in water two deep. */
void animation_set_level_up(map_unit_t* unit) {
    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        animation_request(ANIMATION_LEVEL_UP_HOP, unit->facing, unit);
        return;
    }
    switch (get_water_depth(unit)) {
    case 0:
        animation_request(ANIMATION_LEVEL_UP, unit->facing, unit);
        break;
    case 1:
        animation_request(ANIMATION_LEVEL_UP_HOP, unit->facing, unit);
        break;
    case 2:
        animation_request(ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
        break;
    }
}

/* Give a rider the riding animation and any other unit the standing one, or
 * the submerged idle in water two or more deep; a mount on land also puts
 * its rider back in the riding animation. The depth is a byte, so its
 * negative test never holds. */
void animation_set_stand(map_unit_t* unit) {
    s32 anim = ANIMATION_RIDING;
    s32 selected;
    s32 water_depth;
    map_unit_t* other;

    if (unit->mount_state != UNIT_MOUNT_STATE_RIDER) {
        water_depth = get_water_depth(unit);
        if (water_depth < 0) {
            selected = ANIMATION_SUBMERGED_IDLE;
        } else if (water_depth < 2) {
            selected = ANIMATION_STAND;
            if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
                other = map_unit_get(unit->mount_partner_map_unit_id);
                if (other != 0) {
                    animation_request(ANIMATION_RIDING, unit->facing, other);
                }
            }
        } else {
            selected = ANIMATION_SUBMERGED_IDLE;
        }
        if (selected == 0) {
            return;
        }
        anim = selected;
    }
    animation_request(anim, unit->facing, unit);
}

/*
 * Select the animation of a unit that is walking: riding for a rider, else
 * the walk of its walk speed (fast from 0x3000, slow up to 0x1400), or the
 * submerged idle of that speed in water two or more deep. A mount on land
 * also puts its rider in the riding animation.
 */
void animation_set_walk(map_unit_t* unit) {
    s32 anim;
    s32 walk_speed;
    map_unit_t* rider;

    if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
        animation_request(ANIMATION_RIDING, unit->facing, unit);
        return;
    }
    switch (get_water_depth(unit)) {
    case 0:
    case 1:
        walk_speed = unit->walk_speed;
        if (walk_speed >= 0x3000) {
            anim = ANIMATION_WALK_FAST;
        } else {
            anim = ANIMATION_WALK;
            if (walk_speed < 0x1401) {
                anim = ANIMATION_WALK_SLOW;
            }
        }
        if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
            rider = map_unit_get(unit->mount_partner_map_unit_id);
            if (rider != 0) {
                animation_request(ANIMATION_RIDING, unit->facing, rider);
            }
        }
        break;
    case 2:
    default:
        walk_speed = unit->walk_speed;
        if (walk_speed >= 0x3000) {
            anim = ANIMATION_SUBMERGED_IDLE_HASTE;
        } else {
            anim = ANIMATION_SUBMERGED_IDLE;
            if (walk_speed < 0x1401) {
                anim = ANIMATION_SUBMERGED_IDLE_SLOW;
            }
        }
        break;
    }
    if (anim != 0) {
        animation_request(anim, unit->facing, unit);
    }
}

/*
 * Select a unit's resting animation from its renderer-side status mirror.
 *
 * In water less than two deep a crystal or a treasure shows its own
 * animation. Any other unit takes the animation of the first of: dead,
 * riding, critical (the kneel of sleep; not for a mount), stop, sleep,
 * petrify, confusion, performing or charging (the hold animation of the
 * ability's charge set), defending, slow, haste and cursed; with none of
 * them it idles. The RUKA, ARUTE and KANZEN sequences have no
 * riding or critical animation, and a sheet with the flying flag idles with
 * the three flying animations. In deeper water the choice is the dead, the
 * riding or one of the submerged idle animations.
 *
 * The two animation 0x21 branches test bit 0 of status_flags_1_4, which normal
 * status updates never set. Crystal's bit 0 belongs to status_flags_5_6.
 */
void animation_set_status(map_unit_t* unit) {
    s32 animation;
    u8 water_depth = get_water_depth(unit);
    u8 spritesheet_id;

    animation = 0;
    switch (water_depth) {
    case 0:
    case 1:
        if (unit->status_flags_5_6 & UNIT_STATUS_CRYSTAL) {
            animation = ANIMATION_SUBMERGED_IDLE;
        } else if (unit->status_flags_5_6 & UNIT_STATUS_TREASURE) {
            animation = ANIMATION_TREASURE;
        } else if (!(unit->status_flags_1_4 & UNIT_STATUS_ANIMATION_SELECTION_MASK)) {
            animation = ANIMATION_IDLE_FLYING;
            if (get_flying_flag(unit->spritesheet_id) == 0) {
                animation = ANIMATION_IDLE;
            }
        } else {
            spritesheet_id = unit->spritesheet_id;
            if (g_sprite_sheet_data[spritesheet_id].seq_id >= SPRITE_SEQ_RUKA) {
                if (unit->status_flags_1_4 & UNIT_STATUS_DEAD) {
                    animation = ANIMATION_DEAD;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_STOP) {
                    animation = ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_SLEEP) {
                    animation = ANIMATION_SLEEP;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_PETRIFY) {
                    animation = ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_CONFUSION) {
                    animation = ANIMATION_CONFUSION;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_PERFORMING) {
                    unit->used_ability_id = unit->battle_unit->action_command.ability_id;
                    animation_start_ability_charge_hold(unit);
                } else if (unit->status_flags_1_4 & UNIT_STATUS_CHARGING) {
                    unit->used_ability_id = unit->battle_unit->action_command.ability_id;
                    animation_start_ability_charge_hold(unit);
                } else if (unit->status_flags_1_4 & UNIT_STATUS_DEFENDING) {
                    animation = ANIMATION_DEFEND;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_SLOW) {
                    animation = ANIMATION_IDLE_FLYING_SLOW;
                    if (get_flying_flag(unit->spritesheet_id) == 0) {
                        animation = ANIMATION_IDLE_SLOW;
                    }
                } else if (unit->status_flags_1_4 & UNIT_STATUS_HASTE) {
                    animation = ANIMATION_IDLE_FLYING_HASTE;
                    if (get_flying_flag(unit->spritesheet_id) == 0) {
                        animation = ANIMATION_IDLE_HASTE;
                    }
                } else if (unit->status_flags_1_4 & UNIT_STATUS_UNNAMED_0E) {
                    animation = ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & 1) {
                    animation = 0x21;
                } else {
                    animation = ANIMATION_IDLE_FLYING;
                    if (get_flying_flag(unit->spritesheet_id) == 0) {
                        animation = ANIMATION_IDLE;
                    }
                }
            } else {
                if (unit->status_flags_1_4 & UNIT_STATUS_DEAD) {
                    animation = ANIMATION_DEAD;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_MOUNTED) {
                    animation = ANIMATION_RIDING;
                } else if ((unit->status_flags_1_4 & UNIT_STATUS_CRITICAL)
                    && unit->mount_state != UNIT_MOUNT_STATE_MOUNT) {
                    animation = ANIMATION_SLEEP;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_STOP) {
                    animation = ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_SLEEP) {
                    animation = ANIMATION_SLEEP;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_PETRIFY) {
                    animation = ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_CONFUSION) {
                    animation = ANIMATION_CONFUSION;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_PERFORMING) {
                    unit->used_ability_id = unit->battle_unit->action_command.ability_id;
                    animation_start_ability_charge_hold(unit);
                } else if (unit->status_flags_1_4 & UNIT_STATUS_CHARGING) {
                    unit->used_ability_id = unit->battle_unit->action_command.ability_id;
                    animation_start_ability_charge_hold(unit);
                } else if (unit->status_flags_1_4 & UNIT_STATUS_DEFENDING) {
                    animation = ANIMATION_DEFEND;
                } else if (unit->status_flags_1_4 & UNIT_STATUS_SLOW) {
                    animation = ANIMATION_IDLE_FLYING_SLOW;
                    if (get_flying_flag(unit->spritesheet_id) == 0) {
                        animation = ANIMATION_IDLE_SLOW;
                    }
                } else if (unit->status_flags_1_4 & UNIT_STATUS_HASTE) {
                    animation = ANIMATION_IDLE_FLYING_HASTE;
                    if (get_flying_flag(unit->spritesheet_id) == 0) {
                        animation = ANIMATION_IDLE_HASTE;
                    }
                } else if (unit->status_flags_1_4 & UNIT_STATUS_UNNAMED_0E) {
                    animation = ANIMATION_STAND;
                } else if (unit->status_flags_1_4 & 1) {
                    animation = 0x21;
                } else {
                    animation = ANIMATION_IDLE_FLYING;
                    if (get_flying_flag(unit->spritesheet_id) == 0) {
                        animation = ANIMATION_IDLE;
                    }
                }
            }
        }
        break;
    case 2:
    default:
        if (unit->status_flags_5_6 & UNIT_STATUS_TRANSFORMATION_MASK) {
            if (unit->status_flags_5_6 & UNIT_STATUS_CRYSTAL) {
                animation = ANIMATION_SUBMERGED_IDLE;
            } else if (unit->status_flags_5_6 & UNIT_STATUS_TREASURE) {
                animation = ANIMATION_TREASURE;
            }
        } else if (unit->status_flags_1_4 & UNIT_STATUS_ANIMATION_SELECTION_MASK) {
            if (unit->status_flags_1_4 & UNIT_STATUS_DEAD) {
                animation = ANIMATION_DEAD;
            } else if (unit->status_flags_1_4 & UNIT_STATUS_MOUNTED) {
                animation = ANIMATION_RIDING;
            } else if (unit->status_flags_1_4 & UNIT_STATUS_HASTE) {
                animation = ANIMATION_SUBMERGED_IDLE_HASTE;
            } else {
                animation = ANIMATION_SUBMERGED_IDLE;
                if (unit->status_flags_1_4 & UNIT_STATUS_SLOW) {
                    animation = ANIMATION_SUBMERGED_IDLE_SLOW;
                }
            }
        } else {
            animation = ANIMATION_SUBMERGED_IDLE;
        }
        break;
    }
    if (animation != 0) {
        animation_request(animation, unit->facing, unit);
    }
}

/*
 * Apply this frame's pending status changes to a unit's animation.
 *
 * A unit that becomes a chicken or a frog starts its idle animation, and one
 * that stops being either the animation its statuses select. A death raises
 * the death smoke for the chocobo and monster sequences and, outside events,
 * the cry of the unit's sex or kind; a new crystal or treasure gets its
 * puff. In an event, gaining and losing the Jump status play the jump's
 * departure and landing. Outside events a death or a revival on land plays
 * ANIMATION_DEATH or _REVIVE; in water two or more deep, in
 * events too, the unit is put straight into its dead or its submerged idle
 * sequence, with the wait cleared so that the sequence starts at once.
 */
void animation_update_status_changes(map_unit_t* unit) {
    battle_unit_t* battle_unit;

    if (unit->statuses_to_add_5_6 & UNIT_STATUS_CHICKEN) {
        animation_request_forced(ANIMATION_IDLE, unit->facing, unit);
    } else if (unit->statuses_to_remove_5_6 & UNIT_STATUS_CHICKEN) {
        animation_set_status(unit);
    }
    if (unit->statuses_to_add_5_6 & UNIT_STATUS_FROG) {
        animation_request_forced(ANIMATION_IDLE, unit->facing, unit);
    } else if (unit->statuses_to_remove_5_6 & UNIT_STATUS_FROG) {
        animation_set_status(unit);
    }
    if (unit->statuses_to_add_1_4 & UNIT_STATUS_DEAD) {
        if (g_sprite_sheet_data[unit->spritesheet_id].seq_id == SPRITE_SEQ_CYOKO
            || g_sprite_sheet_data[unit->spritesheet_id].seq_id == SPRITE_SEQ_MON) {
            effect_secondary_start_death_smoke(unit);
        }
        if (g_battle_state != BATTLE_STATE_EVENT) {
            battle_unit = unit->battle_unit;
            if (battle_unit != 0) {
                if (battle_unit->unit_flags & UNIT_FLAG_MALE) {
                    map_unit_play_sfx(unit, SOUND_SFX_DEATH_MALE);
                } else if (battle_unit->unit_flags & UNIT_FLAG_FEMALE) {
                    map_unit_play_sfx(unit, SOUND_SFX_DEATH_FEMALE);
                } else if (battle_unit->unit_flags & UNIT_FLAG_MONSTER) {
                    map_unit_play_sfx(unit, SOUND_SFX_DEATH_MONSTER);
                }
            }
        }
    }
    if (unit->statuses_to_add_5_6 & (UNIT_STATUS_CRYSTAL | UNIT_STATUS_TREASURE)) {
        effect_secondary_start_zodiac_poof(unit);
    }
    if (g_battle_state == BATTLE_STATE_EVENT) {
        if (unit->statuses_to_add_5_6 & UNIT_STATUS_JUMP) {
            animation_request_forced(ANIMATION_JUMP_DEPART, unit->facing, unit);
        } else if (unit->statuses_to_remove_5_6 & UNIT_STATUS_JUMP) {
            map_unit_show(unit->map_unit_id);
            animation_request_forced(ANIMATION_JUMP_LAND, unit->facing, unit);
        }
        if (get_water_depth(unit) >= 2) {
            if (unit->statuses_to_add_1_4 & UNIT_STATUS_DEAD) {
                animation_request_forced(ANIMATION_DEAD, unit->facing, unit);
                unit->animation.sequence_id = ANIMATION_DEAD << 1;
                unit->animation.wait = 0;
            } else if (unit->statuses_to_remove_1_4 & UNIT_STATUS_DEAD) {
                animation_request_forced(ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
                unit->animation.sequence_id = ANIMATION_SUBMERGED_IDLE << 1;
                unit->animation.wait = 0;
            }
        }
    } else if (get_water_depth(unit) < 2) {
        if (unit->statuses_to_add_1_4 & UNIT_STATUS_DEAD) {
            animation_request(ANIMATION_DEATH, unit->facing, unit);
        } else if (unit->statuses_to_remove_1_4 & UNIT_STATUS_DEAD) {
            animation_request(ANIMATION_REVIVE, unit->facing, unit);
        }
    } else if (unit->statuses_to_add_1_4 & UNIT_STATUS_DEAD) {
        animation_request_forced(ANIMATION_DEAD, unit->facing, unit);
        unit->animation.sequence_id = ANIMATION_DEAD << 1;
        unit->animation.wait = 0;
    } else if (unit->statuses_to_remove_1_4 & UNIT_STATUS_DEAD) {
        animation_request_forced(ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
        unit->animation.sequence_id = ANIMATION_SUBMERGED_IDLE << 1;
        unit->animation.wait = 0;
    }
}

/* Carries out a pending Poached or Morbol display flag (status_flags_5_6) and
 * clears it; Poached is tested first and only one is handled per call.
 *
 * Poached configures the unit's palette modulation
 * (map_unit_start_red_fade_out). Morbol swaps the sprite
 * for the Morbol spritesheet (VRAM slot, SHP, SEQ, texture page), refreshes the
 * tile palette and the idle animation, then rewrites the battle stats as a Morbol
 * and reinitializes the unit's AI ability data. */
void map_unit_apply_poach_or_morbol(map_unit_t* unit) {
    u32 flags = unit->status_flags_5_6;
    map_tile_t* tile;

    if (!(flags & (UNIT_STATUS_POACHED | UNIT_STATUS_MORBOL))) {
        return;
    }
    if (flags & UNIT_STATUS_POACHED) {
        unit->status_flags_5_6 = flags & ~UNIT_STATUS_POACHED;
        map_unit_start_red_fade_out(unit->map_unit_id);
        return;
    }
    if (!(flags & UNIT_STATUS_MORBOL)) {
        return;
    }
    unit->status_flags_5_6 = flags & ~UNIT_STATUS_MORBOL;
    unit->special_graphic_y_offset = 0;
    unit->spritesheet_id = SPRITE_SHEET_ID_MORBOL;
    unit->spritesheet_vram_slot = claim_sheet_slot(SPRITE_SHEET_ID_MORBOL);
    unit->animation.shp = get_unit_shp(SPRITE_SHEET_ID_MORBOL);
    unit->animation.seq = get_unit_seq(SPRITE_SHEET_ID_MORBOL);
    unit->stored_palette = 0;
    unit->tpage = unit->spritesheet_vram_slot + SPRITE_TPAGE_SPRITESHEET_SLOT_0;
    tile = map_get_tile(unit->map_x, unit->map_y, unit->map_layer);
    sprite_palette_update(unit, tile->flags.shading, 1);
    animation_request_forced(ANIMATION_IDLE, unit->facing, unit);
    battle_unit_transform_into_morbol(unit->battle_unit);
    ai_init_unit(unit->battle_unit->battle_unit_id);
}

/* Swap the unit's sheet for its first transformation status (Crystal, Treasure, Chicken, Frog),
 * saving its own SHP and SEQ unless it already shows one; Crystal and Treasure also clear every
 * other status. Without such a status, a swapped unit gets back the sheet of its VRAM slot. */
void sprite_sheet_apply_status_change(map_unit_t* unit, s32 tile_effect_level) {
    u32 flags = unit->status_flags_5_6;
    u16 spritesheet_id;
    u8 original_id;

    if (flags & UNIT_STATUS_TRANSFORMATION_MASK) {
        if (flags & UNIT_STATUS_CRYSTAL) {
            spritesheet_id = SPRITE_SHEET_ID_CRYSTAL;
            unit->special_graphic_y_offset = 0x60;
            unit->status_flags_5_6 = UNIT_STATUS_CRYSTAL;
            unit->status_flags_1_4 = 0;
            animation_request(ANIMATION_SUBMERGED_IDLE, unit->facing, unit);
        } else if (flags & UNIT_STATUS_TREASURE) {
            spritesheet_id = SPRITE_SHEET_ID_TREASURE;
            unit->special_graphic_y_offset = 0;
            unit->status_flags_5_6 = UNIT_STATUS_TREASURE;
            unit->status_flags_1_4 = 0;
            animation_request(ANIMATION_TREASURE, unit->facing, unit);
        } else if (flags & UNIT_STATUS_CHICKEN) {
            spritesheet_id = SPRITE_SHEET_ID_CHICKEN;
            unit->special_graphic_y_offset = 0;
        } else if (flags & UNIT_STATUS_FROG) {
            spritesheet_id = SPRITE_SHEET_ID_FROG;
            unit->special_graphic_y_offset = 0x30;
        }
        if (unit->spritesheet_id != spritesheet_id) {
            if (unit->spritesheet_id < SPRITE_SHEET_ID_CRYSTAL) {
                unit->animation.saved_shp = unit->animation.shp;
                unit->animation.saved_seq = unit->animation.seq;
            }
            unit->animation.shp = get_unit_shp(spritesheet_id);
            unit->animation.seq = get_unit_seq(spritesheet_id);
            unit->spritesheet_id = spritesheet_id;
            sprite_palette_tint_unit(unit, tile_effect_level, 1);
            animation_set_status(unit);
            sprite_load_unit_frame(unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
        }
    } else if (unit->spritesheet_id != g_sprite_sheet_slots[unit->spritesheet_vram_slot].spritesheet_id) {
        unit->special_graphic_y_offset = 0;
        original_id = g_sprite_sheet_slots[unit->spritesheet_vram_slot].spritesheet_id;
        unit->animation.shp = unit->animation.saved_shp;
        unit->animation.seq = unit->animation.saved_seq;
        unit->spritesheet_id = original_id;
        sprite_palette_update(unit, tile_effect_level, 1);
        animation_set_status(unit);
        sprite_load_unit_frame(unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
    }
}

/* Make item `item_id` the one a unit's weapon and effect blocks draw.
 *
 * The item's two palettes of s_sprite_item_graphic_data go to the unit's
 * CLUTs of the ITEM_A and ITEM_B rows, and its type selects the frame set of
 * the weapon SHP; a thrown weapon, any Throw ability and Throw Stone use
 * set 0x15. */
void sprite_set_weapon_graphic(s32 item_id, map_unit_t* unit) {
    const item_data_t* item;
    u32 weapon_type;
    s32 clut_index;
    /* The last test re-reads the ability id rather than reusing the value the
     * range test loaded; assigning it through its own local stops cse folding
     * the two reads into one. */
    u16 ability_id;

    item = item_get_data(item_id);
    weapon_type = item->type;
    if ((weapon_type >= ITEM_TYPE_SHURIKEN)
        || ((u32)(unit->used_ability_id - ABILITY_ID_THROW_FIRST) < ABILITY_ID_JUMP_FIRST - ABILITY_ID_THROW_FIRST)
        || (ability_id = unit->used_ability_id) == ABILITY_ID_BASIC_SKILL_THROW_STONE) {
        weapon_type = 0x15;
    }
    clut_index = unit->map_unit_id;
    map_palette_load_data(
        &g_sprite_weapon_clut[s_sprite_item_graphic_data[item_id].palettes >> 4], MAP_CLUT_ROW_ITEM_A, clut_index, 0);
    map_palette_load_data(
        &g_sprite_weapon_clut[s_sprite_item_graphic_data[item_id].palettes & 0xF], MAP_CLUT_ROW_ITEM_B, clut_index, 0);
    unit->used_item_or_weapon_id = item_id;
    unit->equipped_weapon_type = weapon_type;
}

/* A plain call through to animation_set_status. Provisional: named for its
 * two callers, the battle states that resume after an action. */
void animation_set_status_after_action(map_unit_t* unit) {
    animation_set_status(unit);
}

/* When the target blocked with a hand or a golem took the blow, replace the
 * attacker's unarmed, swing or thrust animation with its recoil at the same
 * height. Only the TYPE1 and TYPE2 sheets have the recoils. */
static void set_shield_block_attacker(map_unit_t* attacker, map_unit_t* target) {
    battle_unit_t* battle_unit;
    s32 miss_type;
    s32 anim;

    battle_unit = target->battle_unit;
    if (battle_unit == 0) {
        return;
    }
    if (g_sprite_sheet_data[attacker->spritesheet_id].shp_id >= SPRITE_SHP_CYOKO) {
        return;
    }
    miss_type = battle_unit->action.miss_type;
    anim = 0;
    if (miss_type >= ACTION_MISS_TYPE_RIGHT_HAND_EVADE
        && (miss_type < ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD || miss_type == ACTION_MISS_TYPE_GOLEM_GUARD)) {
        switch (attacker->animation.sequence_id >> 1) {
        case ANIMATION_UNARMED_ATTACK_HIGH:
            anim = ANIMATION_UNARMED_ATTACK_BLOCKED_HIGH;
            break;
        case ANIMATION_UNARMED_ATTACK_LEVEL:
            anim = ANIMATION_UNARMED_ATTACK_BLOCKED_LEVEL;
            break;
        case ANIMATION_UNARMED_ATTACK_LOW:
            anim = ANIMATION_UNARMED_ATTACK_BLOCKED_LOW;
            break;
        case ANIMATION_SWING_HIGH:
            anim = ANIMATION_SWING_BLOCKED_HIGH;
            break;
        case ANIMATION_SWING_LEVEL:
            anim = ANIMATION_SWING_BLOCKED_LEVEL;
            break;
        case ANIMATION_SWING_LOW:
            anim = ANIMATION_SWING_BLOCKED_LOW;
            break;
        case ANIMATION_THRUST_HIGH:
            anim = ANIMATION_THRUST_BLOCKED_HIGH;
            break;
        case ANIMATION_THRUST_LEVEL:
            anim = ANIMATION_THRUST_BLOCKED_LEVEL;
            break;
        case ANIMATION_THRUST_LOW:
            anim = ANIMATION_THRUST_BLOCKED_LOW;
            break;
        }
    }
    if (anim != 0) {
        animation_request_forced(anim, attacker->facing, attacker);
    }
}

/*
 * Pick and start the reaction animation a target plays after an action
 * resolves, then apply the action's special-effect side effects.
 *
 * A target already dead or dying is left alone. In water two or more deep it
 * takes the submerged idle, and a target without battle stats the hurt
 * animation. Otherwise the miss type decides: a hand guard turns to the
 * attacker and parries at the attacker's relative height (the same 12-unit
 * thresholds as animation_select_weapon_attack), an evasion with
 * the dodge display flag dodges, Blade Grasp and catches raise the hands, and
 * a hit hurts on damage or raises the hands on healing. The sheets from the
 * crystal up play only the hurt animation. Knockback, Poach and Morbol start
 * here, and a broken or stolen item rebuilds the target's AI ability data.
 */
void action_start_target_response(map_unit_t* attacker, map_unit_t* target) {
    battle_unit_t* battle_unit;
    action_data_t* action;
    s32 animation;
    s32 difference;
    s16 attacker_center;
    s16 target_center;
    u32 water_depth;

    if (attacker == target) {
        return;
    }
    if ((target->animation.sequence_id >> 1) == ANIMATION_DEAD) {
        return;
    }
    if ((target->animation.sequence_id >> 1) == ANIMATION_DEATH) {
        return;
    }
    water_depth = get_water_depth(target);
    animation = ANIMATION_SUBMERGED_IDLE;
    if (water_depth < 2) {
        battle_unit = target->battle_unit;
        if (battle_unit != 0) {
            action = &battle_unit->action;
            animation = ANIMATION_STAND;
            switch (action->miss_type) {
            case ACTION_MISS_TYPE_RIGHT_HAND_EVADE:
            case ACTION_MISS_TYPE_LEFT_HAND_EVADE:
                map_unit_facing_turn_to_target(target, attacker);
                sprite_set_weapon_graphic(action->item_lost, target);
                attacker_center
                    = sprite_sheet_map_unit_get_height(attacker->map_unit_id) / 2 - attacker->world_coords.vy;
                target_center = sprite_sheet_map_unit_get_height(target->map_unit_id) / 2 - target->world_coords.vy;
                difference = attacker_center - target_center;
                if (difference < -11) {
                    animation = ANIMATION_WEAPON_GUARD_LOW;
                } else {
                    animation = ANIMATION_WEAPON_GUARD_LEVEL;
                    if (difference >= 12) {
                        animation = ANIMATION_WEAPON_GUARD_HIGH;
                    }
                }
                break;
            case ACTION_MISS_TYPE_ACCESSORY_EVADE:
            case ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD:
            case ACTION_MISS_TYPE_NULLIFIED:
            case ACTION_MISS_TYPE_ACCURACY_MISS:
            case ACTION_MISS_TYPE_FORCED_FAILURE:
                if (action->miss_display_flags & ACTION_MISS_DISPLAY_DODGE_ANIMATION) {
                    map_unit_facing_turn_to_target(target, attacker);
                    animation = ANIMATION_DODGE;
                }
                break;
            case ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD:
            case ACTION_MISS_TYPE_CATCH:
                map_unit_facing_turn_to_target(target, attacker);
                animation = ANIMATION_RAISE_HANDS;
                break;
            case ACTION_MISS_TYPE_CANCELED:
            case ACTION_MISS_TYPE_REFLECTED:
            case ACTION_MISS_TYPE_GOLEM_GUARD:
                break;
            default:
                if (action->hit != 0) {
                    if ((action->hp_damage != 0) || (action->mp_damage != 0)) {
                        animation = ANIMATION_HURT;
                    } else if ((action->hp_healing != 0) || (action->mp_healing != 0)) {
                        animation = ANIMATION_RAISE_HANDS;
                    }
                }
                break;
            }

            if (animation != 0) {
                if (target->spritesheet_id < SPRITE_SHEET_ID_CRYSTAL) {
                    animation_request_forced(animation, target->facing, target);
                } else if (animation == ANIMATION_HURT) {
                    animation_request_forced(ANIMATION_HURT, target->facing, target);
                }
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_KNOCKBACK) {
                move_start_knockback(attacker, target);
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_POACH) {
                target->status_flags_5_6 |= UNIT_STATUS_POACHED;
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_MORBOL) {
                target->status_flags_5_6 |= UNIT_STATUS_MORBOL;
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT) {
                ai_init_unit(target->battle_unit->battle_unit_id);
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
                ai_init_unit(target->battle_unit->battle_unit_id);
            }
            return;
        }
        animation = ANIMATION_HURT;
    }
    animation_request(animation, target->facing, target);
}

/* Refresh the display of every unit in the strike's target list. */
void map_unit_update_target_displays(map_unit_t* unit) {
    s32 i;
    map_unit_t* target;

    if (unit->strike.target_count != 0) {
        for (i = 0; i < unit->strike.target_count; i++) {
            target = map_unit_get_by_battle_unit_id(unit->strike.target_list[i]);
            if (target != 0) {
                map_unit_update_display(target->map_unit_id);
            }
        }
    }
}

/* unit sprites */

/* sheet files */
/* Unit SPR file: 16 palettes, two uncompressed 4bpp image blocks, then
 * a variable-length compressed attack-image stream. */
typedef struct unit_spr_file {
    clut_t clut;                          /* 0x0000; 16 palettes of 16 colors */
    u8 pixels[256][128];                  /* 0x0200; 256 x 256 pixels, two per byte */
    u8 portrait_and_body_pixels[32][128]; /* 0x8200; 256 x 32 pixels, two per byte */
    u8 compressed_attack_image[0];        /* 0x9200; variable byte tail */
} unit_spr_file_t;
typedef char unit_spr_file_size_must_be_37376[sizeof(unit_spr_file_t) == 37376 ? 1 : -1];

/* Packed unit SEQ file. Values and offsets are little-endian byte
 * sequences, so this view needs no native 16-bit or 32-bit alignment. */
typedef struct unit_seq_file {
    u8 first_attack_sequence[2];   /* 0x000; unsigned little-endian 16-bit value */
    u8 first_sp2_sequence[2];      /* 0x002; unsigned little-endian 16-bit value */
    u8 sequence_offsets[0x100][4]; /* 0x004; signed little-endian 32-bit values, -1 means offset zero */
    u8 sequence_data_size[2];      /* 0x404; unsigned little-endian 16-bit value */
    u8 sequence_data[0];           /* 0x406; variable byte tail */
} unit_seq_file_t;
typedef char unit_seq_file_size_must_be_1030[sizeof(unit_seq_file_t) == 1030 ? 1 : -1];

/* file layouts */
/* Serialized SHP/SEQ values are little-endian byte sequences. These file
 * views have byte alignment; their decoded RAM tables remain separate. */

/* A unit SHP frame set: primary at file +8; an optional submerged set is
 * addressed by the file header. Only the first 0xd0 offsets are installed. */
typedef struct unit_shp_frame_set {
    u8 frame_offsets[0x100][4]; /* 0x000; signed 32-bit values, -1 resolves to offset zero */
    u8 frame_data_size[2];      /* 0x400; unsigned 16-bit value */
    u8 frame_data[0];           /* 0x402; variable tail */
} unit_shp_frame_set_t;
typedef char unit_shp_frame_set_size_must_be_1026[sizeof(unit_shp_frame_set_t) == 1026 ? 1 : -1];

typedef struct unit_shp_file {
    u8 submerged_frame_set_offset[4];       /* 0x000; 8 means both native tables share the primary set */
    u8 first_attack_frame[2];               /* 0x004; unsigned 16-bit value */
    u8 first_sp2_frame[2];                  /* 0x006; unsigned 16-bit value */
    unit_shp_frame_set_t primary_frame_set; /* 0x008 */
} unit_shp_file_t;
typedef char unit_shp_file_size_must_be_1034[sizeof(unit_shp_file_t) == 1034 ? 1 : -1];

/* WEP/EFF SHP has a different header and twice as many frame offsets. */
typedef struct weapon_shp_file {
    u8 _unused_000[4];          /* 0x000; loader skips this 32-bit value */
    u8 first_frame[0x20][2];    /* 0x004; unsigned zero-frame indices by weapon type */
    u8 frame_offsets[0x200][4]; /* 0x044; signed 32-bit values, -1 resolves to offset zero */
    u8 frame_data_size[2];      /* 0x844; unsigned 16-bit value */
    u8 frame_data[0];           /* 0x846; variable tail */
} weapon_shp_file_t;
typedef char weapon_shp_file_size_must_be_2118[sizeof(weapon_shp_file_t) == 2118 ? 1 : -1];

/* WEP/EFF SEQ stores 256 offsets followed by a variable sequence payload. */
typedef struct packed_seq_file {
    u8 _unused_000[4];  /* 0x000; WEP/EFF loader skips the 32-bit header value */
    u8 entry[0x100][4]; /* 0x004; signed sequence offsets, -1 resolves to zero */
    u8 payload_size[2]; /* 0x404; unsigned 16-bit value */
    u8 payload[0];      /* 0x406; variable sequence data */
} packed_seq_file_t;
typedef char packed_seq_file_size_must_be_1030[sizeof(packed_seq_file_t) == 1030 ? 1 : -1];

/* OTHER.SPR: the palette helper copies only the first 21 of 32 palettes. */
typedef struct other_spr_file {
    palette_t palettes[32]; /* 0x0000 */
    u8 pixels[256 * 128];   /* 0x0400; uncompressed 256x256, 4bpp */
} other_spr_file_t;
typedef char other_spr_file_size_must_be_33792[sizeof(other_spr_file_t) == 33792 ? 1 : -1];

/* WEP.SPR contains three CLUT/image pairs: weapons, trails and particles. */
typedef struct weapon_spr_file {
    clut_t weapon_clut;           /* 0x00000 */
    u8 weapon_pixels[256][128];   /* 0x00200; 4bpp */
    clut_t trail_clut;            /* 0x08200 */
    u8 trail_pixels[256][128];    /* 0x08400; 4bpp */
    clut_t particle_clut;         /* 0x10400 */
    u8 particle_pixels[144][128]; /* 0x10600; 4bpp */
} weapon_spr_file_t;

/* Loaded and fallback SHP/SEQ data. */
extern u8 g_sprite_shp_frame_data_buffer[];
extern u8 g_sprite_seq_data_buffer[];
extern u8 g_sprite_fallback_shp_frame_data[];
extern u8 g_sprite_fallback_seq_data[];

/* The SHP/SEQ loaders advance these allocation cursors by byte counts. */
extern u8* g_sprite_shp_frame_data_cursor;
extern u8* g_sprite_seq_data_cursor;
extern s32 g_status_bubble_delay;

extern clut_t g_sprite_weapon_clut;

/* Trail CLUT copied from WEP.SPR at offset 0x8200. */
extern clut_t g_sprite_trail_clut;

/* Set before allocation and read acceptance: a failed request can be skipped
 * on the next step because its ID already matches. */
static s32 s_sprite_last_requested_seq_id;
static s32 s_sprite_last_requested_shp_id;

static map_unit_t* claim_map_unit_slot(s16 requested_map_unit_id);

static void run_unit_seq(map_unit_t* unit, animation_state_t* state, s32 advance);

static void run_block_seq(map_unit_t* unit, sprite_block_t* sprite, s32 index, s32 advance);

/*
 * Claim a map unit record slot and stamp its map unit ID.
 *
 * Ids below 16 take their own slot, failing if another in-use slot already
 * carries that id or the slot is taken. Larger ids take the first free slot,
 * whose index becomes the stored map unit ID; exhausting all 16 raises
 * SYSTEM_ERROR_NO_MAP_UNIT_SLOT. Returns 0 on failure.
 */
static map_unit_t* claim_map_unit_slot(s16 requested_map_unit_id) {
    s32 i;

    if (requested_map_unit_id < MAP_UNIT_SLOT_COUNT) {
        for (i = 0; i < MAP_UNIT_SLOT_COUNT; i++) {
            if (g_map_unit_slots[i].in_use != 0 && g_map_unit_slots[i].unit.map_unit_id == requested_map_unit_id) {
                break;
            }
        }
        if (i < MAP_UNIT_SLOT_COUNT) {
            return 0;
        }
        if (g_map_unit_slots[requested_map_unit_id].in_use != 0) {
            return 0;
        }
        g_map_unit_slots[requested_map_unit_id].in_use = 1;
        g_map_unit_slots[requested_map_unit_id].unit.map_unit_id = requested_map_unit_id;
        return &g_map_unit_slots[requested_map_unit_id].unit;
    }
    for (i = 0; i < MAP_UNIT_SLOT_COUNT; i++) {
        if (g_map_unit_slots[i].in_use == 0) {
            g_map_unit_slots[i].in_use = 1;
            g_map_unit_slots[i].unit.map_unit_id = i;
            return &g_map_unit_slots[i].unit;
        }
    }
    system_handle_error(SYSTEM_ERROR_NO_MAP_UNIT_SLOT);
    return 0;
}

/* Fill a sprite display from SHP frame `src`: each part takes its shift, a texture position in
 * 8-pixel tiles, its size from s_sprite_part_sizes and its flip bits; the frame's blend bits
 * are merged into texture page `tpage`. */
static void load_frame_parts(map_unit_t* unit, sprite_display_t* display, const sprite_frame_t* src, s32 tpage) {
    const sprite_frame_part_t* part;
    u32 i;
    u32 count;
    u8 flags;
    u16 attr;
    u16 size;
    u32 tile;
    s32 u;
    s32 v;
    s32 width;
    s32 height;

    flags = src->flags;
    count = display->part_count;
    display->tpage = (flags & SPRITE_DISPLAY_BLEND_MASK) | tpage;
    display->clut = unit->clut;
    for (i = 0; i < count; i++) {
        part = &src->parts[i];
        attr = part->attributes;
        size = (attr & SPRITE_SHP_PART_SIZE_MASK) >> 10;
        width = s_sprite_part_sizes[size].width;
        height = s_sprite_part_sizes[size].height;
        tile = attr & SPRITE_SHP_PART_TILE_MASK;
        u = (tile & 0x1f) * 8;
        v = (tile >> 5) * 8;
        sprite_display_set_part(display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8,
            (flags & (SPRITE_DISPLAY_SEMI_TRANSPARENT | SPRITE_DISPLAY_BLEND_MASK))
                | ((attr & SPRITE_SHP_PART_FLIP_MASK) >> 13));
    }
}

/* Return the frame tables that a unit frame id indexes and the frame itself:
 * a sprite_frame_t pointer.
 *
 * A frame below SPRITE_SHP_FRAME_EVTCHR_FIRST is one of the unit's own SHP.
 * Its submerged table replaces the plain one for a rider that is not
 * jumping, and for a unit on a water tile whose surface is 12 or more above
 * the unit's feet; from 36 above, the unit shows submerged frame 0 whatever
 * the frame id. A unit that floats or moves on water, a crystal and a
 * treasure keep the plain table, and a chicken or a frog changes at 4.
 * Higher frame ids index the SHP of an EVTCHR slot: slot 0 for the sequences
 * below 600 and slot 1 for the rest, the submerged table on a water tile. */
static void select_unit_shp_frame(map_unit_t* unit, animation_state_t* state, u16 frame, u16 sequence_id,
    const sprite_unit_shp_frame_tables_t** out_table, const sprite_frame_t** out_frame) {
    map_tile_t* tile;
    const sprite_unit_shp_frame_tables_t* table;
    const sprite_frame_t* selected;
    s32 flags;
    s32 water_height;
    s32 unit_height;

    tile = map_get_tile(unit->world_coords.vx / MAP_TILE_SIZE, unit->world_coords.vz / MAP_TILE_SIZE, unit->map_layer);
    if (frame < SPRITE_SHP_FRAME_EVTCHR_FIRST) {
        table = state->shp;
        if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
            if (unit->status_flags_5_6 & (UNIT_STATUS_JUMP | UNIT_STATUS_JUMP_HEIGHT_ACTIVE)) {
                selected = table->frames[frame];
            } else {
                selected = table->submerged_frames[frame];
            }
        } else if (tile->depth_half_height.depth) {
            if (unit->battle_unit != 0) {
                flags = move_get_effective_flags(unit->battle_unit);
                /* The combined test keeps the table pointer's reference count
                 * low enough for the target's s0/s1 allocation. */
                if (unit->status_flags_5_6 & UNIT_STATUS_FLOAT) {
                    selected = table->frames[frame];
                } else if ((flags & MOVE_EFFECTIVE_ON_WATER) || (flags & MOVE_EFFECTIVE_FLOAT)) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == SPRITE_SHEET_ID_CRYSTAL) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == SPRITE_SHEET_ID_TREASURE) {
                    selected = table->frames[frame];
                } else if (unit->spritesheet_id == SPRITE_SHEET_ID_CHICKEN
                    || unit->spritesheet_id == SPRITE_SHEET_ID_FROG) {
                    if ((tile->height + tile->depth_half_height.depth) * 12 >= -unit->world_coords.vy + 4) {
                        selected = table->submerged_frames[frame];
                    } else {
                        selected = table->frames[frame];
                    }
                } else {
                    water_height = (tile->height + tile->depth_half_height.depth) * 12;
                    unit_height = -unit->world_coords.vy;
                    if (water_height >= unit_height + 0x24) {
                        selected = table->submerged_frames[0];
                    } else if (water_height >= unit_height + 0xc) {
                        selected = table->submerged_frames[frame];
                    } else {
                        selected = table->frames[frame];
                    }
                }
            } else {
                water_height = (tile->height + tile->depth_half_height.depth) * 12;
                unit_height = -unit->world_coords.vy;
                if (water_height >= unit_height + 0x24) {
                    selected = table->submerged_frames[0];
                } else if (water_height >= unit_height + 0xc) {
                    selected = table->submerged_frames[frame];
                } else {
                    selected = table->frames[frame];
                }
            }
        } else {
            selected = table->frames[frame];
        }
    } else {
        table = &g_sprite_evtchr_slots[1].shp.tables;
        if (sequence_id < ANIMATION_SEQUENCE_EVTCHR_SLOT_1) {
            table = &g_sprite_evtchr_slots[0].shp.tables;
        }
        if (tile->depth_half_height.depth) {
            selected = table->submerged_frames[frame - SPRITE_SHP_FRAME_EVTCHR_FIRST];
        } else {
            selected = table->frames[frame - SPRITE_SHP_FRAME_EVTCHR_FIRST];
        }
    }
    *out_table = table;
    *out_frame = selected;
}

/* Expand one SHP frame into the parts of a unit's sprite display.
 *
 * Where the frame's pictures are decides the texture page: the OTHER page
 * for the crystal, chicken, frog and treasure sheets; the unit's spritesheet
 * slot for frames below the SHP's first attack frame; above that, the EVTCHR
 * slot that holds the unit's attack image (no slot, no parts). Frames from
 * SPRITE_SHP_FRAME_EVTCHR_FIRST up belong to an EVTCHR slot's own SHP and
 * the sequence id says which slot.
 *
 * A 48x48 part from a spritesheet slot ends the frame: it is stored as two
 * 48x24 halves, the upper one flagged to draw nearer the camera. The two
 * Altima sheets keep theirs whole. */
void sprite_load_unit_frame(map_unit_t* unit, animation_state_t* state, u16 frame, u16 sequence_id) {
    const sprite_unit_shp_frame_tables_t* frame_table;
    const sprite_frame_t* frame_data;
    sprite_display_t* display;
    const sprite_frame_part_t* part;
    u32 y_offset;
    u32 count;
    u16 i;
    u8 flags;
    u16 attr;
    u16 size;
    s32 u;
    u32 tile_index;
    s16 part_flags;
    u32 row;

    y_offset = unit->special_graphic_y_offset;
    /* The unpackers never store the -1 tested here, and three bits
     * of part count cannot pass 8. */
    select_unit_shp_frame(unit, state, frame, sequence_id, &frame_table, &frame_data);
    if (frame_data == (const sprite_frame_t*)-1) {
        system_handle_error(SYSTEM_ERROR_SHP_FRAME);
    }
    display = state->display;
    count = display->part_count = (frame_data->part_count_and_rotation & 7) + 1;
    display->rotation = s_sprite_y_rotations[frame_data->part_count_and_rotation >> 3];
    if (count > 8) {
        count = 8;
        system_handle_error(SYSTEM_ERROR_FRAME_PART_COUNT);
    }
    flags = frame_data->flags;
    if (unit->spritesheet_id >= SPRITE_SHEET_ID_CRYSTAL) {
        display->tpage = (flags & SPRITE_DISPLAY_BLEND_MASK) | SPRITE_TPAGE_OTHER;
        display->clut = unit->clut;
        for (i = 0; i < count; i++) {
            part = &frame_data->parts[i];
            attr = part->attributes;
            size = (attr & SPRITE_SHP_PART_SIZE_MASK) >> 10;
            {
                u32 tile;
                s32 part_u;
                s16 part_v;
                s32 width;
                s32 height;
                width = s_sprite_part_sizes[size].width;
                height = s_sprite_part_sizes[size].height;
                tile = attr & SPRITE_SHP_PART_TILE_MASK;
                part_u = (tile & 0x1f) * 8;
                part_v = y_offset + (tile >> 5) * 8;
                sprite_display_set_part(display, i, part->x_shift, part->y_shift, part_u, part_v, width * 8, height * 8,
                    (flags & (SPRITE_DISPLAY_SEMI_TRANSPARENT | SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & SPRITE_SHP_PART_FLIP_MASK) >> 13));
            }
        }
    } else if (frame < state->shp->first_attack_frame) {
        display->tpage = unit->tpage | (flags & SPRITE_DISPLAY_BLEND_MASK);
        display->clut = unit->clut;
        i = 0;
        if (count != 0) {
            do {
                part = &frame_data->parts[i];
                attr = part->attributes;
                size = (attr & SPRITE_SHP_PART_SIZE_MASK) >> 10;
                if (size == SPRITE_SHP_PART_SIZE_48X48) {
                    if (unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_SECOND_FORM
                        || unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_FIRST_FORM) {
                        u32 tile;
                        s32 part_u;
                        s16 part_v;
                        s32 width;
                        s32 height;
                        tile = attr & SPRITE_SHP_PART_TILE_MASK;
                        part_u = (tile & 0x1f) * 8;
                        part_v = y_offset + (tile >> 5) * 8;
                        width = s_sprite_part_sizes[size].width;
                        height = s_sprite_part_sizes[size].height;
                        sprite_display_set_part(display, i, part->x_shift, part->y_shift, part_u, part_v, width * 8,
                            height * 8,
                            (flags & (SPRITE_DISPLAY_SEMI_TRANSPARENT | SPRITE_DISPLAY_BLEND_MASK))
                                | ((attr & SPRITE_SHP_PART_FLIP_MASK) >> 13));
                    } else {
                        tile_index = attr & SPRITE_SHP_PART_TILE_MASK;
                        u = (tile_index & 0x1f) * 8;
                        row = tile_index >> 5;
                        part_flags = (flags & (SPRITE_DISPLAY_SEMI_TRANSPARENT | SPRITE_DISPLAY_BLEND_MASK))
                            | ((attr & SPRITE_SHP_PART_FLIP_MASK) >> 13);
                        sprite_display_set_part(
                            display, i, part->x_shift, part->y_shift + 0x18, u, row * 8 + 0x18, 0x30, 0x18, part_flags);
                        sprite_display_set_part(display, i + 1, part->x_shift, part->y_shift, u, row * 8, 0x30, 0x18,
                            part_flags | SPRITE_DISPLAY_NEARER);
                        display->part_count++;
                        return;
                    }
                } else {
                    u32 tile;
                    s32 part_u;
                    s16 part_v;
                    s32 width;
                    s32 height;
                    width = s_sprite_part_sizes[size].width;
                    height = s_sprite_part_sizes[size].height;
                    tile = attr & SPRITE_SHP_PART_TILE_MASK;
                    part_u = (tile & 0x1f) * 8;
                    part_v = y_offset + (tile >> 5) * 8;
                    sprite_display_set_part(display, i, part->x_shift, part->y_shift, part_u, part_v, width * 8,
                        height * 8,
                        (flags & (SPRITE_DISPLAY_SEMI_TRANSPARENT | SPRITE_DISPLAY_BLEND_MASK))
                            | ((attr & SPRITE_SHP_PART_FLIP_MASK) >> 13));
                }
            } while (++i < count);
        }
    } else if (frame < SPRITE_SHP_FRAME_EVTCHR_FIRST) {
        if ((g_sprite_evtchr_slots[0].owner & SPRITE_EVTCHR_SLOT_OWNER_ID_MASK) == unit->map_unit_id) {
            load_frame_parts(unit, display, frame_data, SPRITE_TPAGE_EVTCHR_SLOT_0);
        } else if ((g_sprite_evtchr_slots[1].owner & SPRITE_EVTCHR_SLOT_OWNER_ID_MASK) == unit->map_unit_id) {
            load_frame_parts(unit, display, frame_data, SPRITE_TPAGE_EVTCHR_SLOT_1);
        }
    } else if (sequence_id < ANIMATION_SEQUENCE_EVTCHR_SLOT_1) {
        load_frame_parts(unit, display, frame_data, SPRITE_TPAGE_EVTCHR_SLOT_0);
    } else {
        load_frame_parts(unit, display, frame_data, SPRITE_TPAGE_EVTCHR_SLOT_1);
    }
}

/* SEQ script step: moves the sprite offset by delta across the unit's facing: +x
 * facing north, -x facing south, +z facing west, -z facing east. A diagonal
 * facing matches no case. */
static void shift_seq_lateral(map_unit_t* unit, s8 delta) {
    switch (unit->facing & (UNIT_FACING_EAST | UNIT_FACING_NORTH)) {
    case UNIT_FACING_NORTH:
        unit->seq_offset.vx += delta;
        return;
    case UNIT_FACING_SOUTH:
        unit->seq_offset.vx -= delta;
        return;
    case UNIT_FACING_WEST:
        unit->seq_offset.vz += delta;
        return;
    case UNIT_FACING_EAST:
        unit->seq_offset.vz -= delta;
        return;
    }
}

/* SEQ script step: moves the sprite offset by delta along the height axis;
 * positive is down. */
static void shift_seq_height(map_unit_t* unit, s8 delta) {
    unit->seq_offset.vy += delta;
}

/* SEQ script step: moves the sprite offset by delta along the unit's facing
 * (north is +z, east is +x). A diagonal facing matches no case. */
static void shift_seq_forward(map_unit_t* unit, s8 delta) {
    switch (unit->facing & (UNIT_FACING_EAST | UNIT_FACING_NORTH)) {
    case UNIT_FACING_NORTH:
        unit->seq_offset.vz += delta;
        break;
    case UNIT_FACING_SOUTH:
        unit->seq_offset.vz -= delta;
        break;
    case UNIT_FACING_WEST:
        unit->seq_offset.vx -= delta;
        break;
    case UNIT_FACING_EAST:
        unit->seq_offset.vx += delta;
        break;
    }
}

/* Run a unit's SEQ sequence up to its next frame instruction, or to its end.
 *
 * `state->sequence_id` selects the sequence: one of the unit's own SEQ table,
 * or sequence n of an EVTCHR slot for 500 + n and 600 + n. The commands in
 * front of the frame instruction run at once (animation_seq_opcode_e); then the
 * frame's parts are expanded into the unit's display and the state keeps the
 * frame, its wait and the position after it. With `advance` clear the
 * position first steps back to the frame instruction read last, which shows
 * that frame again without restarting its wait.
 *
 * A sequence that ends leaves a unit on its SHP type's
 * s_animation_targeted_frame_front or _back frame with a wait of 0. The
 * unpackers resolve a missing sequence to the start of the sequence data,
 * never to -1, so the SYSTEM_ERROR_SEQ_ANIMATION test cannot fire. */
static void run_unit_seq(map_unit_t* unit, animation_state_t* state, s32 advance) {
    sprite_unit_seq_table_t* seq;
    sprite_block_t* block;
    map_unit_t* target;
    u16 sequence_id;
    u16 index;
    u16 frame;
    u16 i;
    s16 pos;
    s16 attack_id;
    u16 value;
    s16 offset;
    u8 block_id;
    u8 block_sequence;
    u8 timer;

    sequence_id = state->sequence_id;
    pos = state->script_pos;
    index = sequence_id;
    if (sequence_id < ANIMATION_SEQUENCE_EVTCHR_SLOT_0) {
        seq = state->seq;
    } else if (sequence_id < ANIMATION_SEQUENCE_EVTCHR_SLOT_1) {
        index = sequence_id - ANIMATION_SEQUENCE_EVTCHR_SLOT_0;
        seq = &g_sprite_evtchr_slots[0].seq.header;
    } else {
        seq = &g_sprite_evtchr_slots[1].seq.header;
        index = sequence_id - ANIMATION_SEQUENCE_EVTCHR_SLOT_1;
    }

    /* A frame instruction is two bytes, so this backs up to the frame read last. */
    if (!advance) {
        pos -= 2;
    }
    if (seq->sequences[index] == (u8*)-1) {
        system_handle_error(SYSTEM_ERROR_SEQ_ANIMATION);
    }

    for (;;) {
        u8 op;

        if (seq->sequences[index][pos] == ANIMATION_SEQ_OPCODE_PREFIX) {
            pos++;
            op = seq->sequences[index][pos++];
            switch (op) {
            case ANIMATION_SEQ_OPCODE_FLIP_HORIZONTAL:
                state->flags ^= SPRITE_DISPLAY_HORIZONTAL_FLIP;
                break;
            case ANIMATION_SEQ_OPCODE_FLIP_VERTICAL:
                state->flags ^= SPRITE_DISPLAY_VERTICAL_FLIP;
                break;
            case ANIMATION_SEQ_OPCODE_SHOW_SHADOW:
                unit->shadow_graphic_trigger = 1;
                break;
            case ANIMATION_SEQ_OPCODE_HIDE_SHADOW:
                unit->shadow_graphic_trigger = 0;
                break;
            case ANIMATION_SEQ_OPCODE_END_ANIMATION:
            case ANIMATION_SEQ_OPCODE_PAUSE_ANIMATION:
                if (sequence_id < ANIMATION_SEQUENCE_EVTCHR_SLOT_0) {
                    /* Odd sequence ids are the views from behind. */
                    if (sequence_id % 2) {
                        frame = s_animation_targeted_frame_back[g_sprite_sheet_data[unit->spritesheet_id].shp_id];
                    } else {
                        frame = s_animation_targeted_frame_front[g_sprite_sheet_data[unit->spritesheet_id].shp_id];
                    }
                    unit->seq_offset.vz = 0;
                    unit->seq_offset.vy = 0;
                    unit->seq_offset.vx = 0;
                    sprite_load_unit_frame(unit, state, frame, sequence_id);
                }
                state->wait = 0;
                return;
            case ANIMATION_SEQ_OPCODE_RESTART:
                pos = 0;
                state->restart_count++;
                break;
            case ANIMATION_SEQ_OPCODE_CLEAR_ROTATION:
                state->display->rotation = 0;
                break;
            case ANIMATION_SEQ_OPCODE_STRIKE_TARGETS:
                if (g_battle_state == BATTLE_STATE_EVENT) {
                    break;
                }
                /* The abilities whose hit is the weapon swing itself: a plain
                 * attack (0), the frog's, the Battle Skill breaks, the Snipe
                 * shots, Jump and Charge. */
                attack_id = unit->strike.last_attack_id;
                if (attack_id == 0 || attack_id == ABILITY_ID_FROG_ATTACK
                    || (attack_id >= ABILITY_ID_BATTLE_SKILL_HEAD_BREAK
                        && attack_id <= ABILITY_ID_BATTLE_SKILL_MIND_BREAK)
                    || (attack_id >= ABILITY_ID_SNIPE_LEG_AIM && attack_id <= ABILITY_ID_SNIPE_SEAL_EVIL)
                    || (attack_id >= ABILITY_ID_JUMP_FIRST && attack_id < ABILITY_ID_MATH_FIRST)) {
                    if (unit->strike.target_count != 0) {
                        for (i = 0; i < unit->strike.target_count; i++) {
                            target = map_unit_get_by_battle_unit_id(unit->strike.target_list[i]);
                            if (target != 0) {
                                action_start_target_response(unit, target);
                                set_shield_block_attacker(unit, target);
                                result_map_unit_prepare_display(target->map_unit_id);
                                effect_secondary_start_weapon_hit(unit, 0);
                            }
                        }
                    }
                }
                break;
            case ANIMATION_SEQ_OPCODE_LAUNCH_PROJECTILE:
                effect_secondary_start_projectile(unit);
                /* fallthrough: the command's two operand bytes are skipped like the unknowns' */
            case ANIMATION_SEQ_OPCODE_UNKNOWN_C7:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_C8:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_C9:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_E6:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_E7:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_E8:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_EA:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_F1:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_F8:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_FB:
                pos += 2;
                break;
            case ANIMATION_SEQ_OPCODE_RETURN:
                sequence_id = state->saved_sequence_id;
                pos = state->saved_script_pos;
                index = sequence_id;
                if (sequence_id >= ANIMATION_SEQUENCE_EVTCHR_SLOT_0) {
                    if (sequence_id < ANIMATION_SEQUENCE_EVTCHR_SLOT_1) {
                        index = sequence_id - ANIMATION_SEQUENCE_EVTCHR_SLOT_0;
                    } else {
                        index = sequence_id - ANIMATION_SEQUENCE_EVTCHR_SLOT_1;
                    }
                }
                break;
            case ANIMATION_SEQ_OPCODE_WAIT_FOR_EFFECT:
                if (g_battle_animation_wait_state != 0) {
                    /* Jump offsets count from the prefix byte, two bytes behind pos. */
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case ANIMATION_SEQ_OPCODE_PLAY_SOUND:
                value = seq->sequences[index][pos++];
                map_unit_play_sfx(unit, value);
                break;
            case ANIMATION_SEQ_OPCODE_MOVE_UNIT_RL:
                shift_seq_lateral(unit, seq->sequences[index][pos++]);
                break;
            case ANIMATION_SEQ_OPCODE_MOVE_UNIT_DU:
                shift_seq_height(unit, seq->sequences[index][pos++]);
                break;
            case ANIMATION_SEQ_OPCODE_MOVE_UNIT_FB:
                shift_seq_forward(unit, seq->sequences[index][pos++]);
                break;
            case ANIMATION_SEQ_OPCODE_SET_WAIT_BIAS:
                state->wait_bias = (s8)seq->sequences[index][pos++];
                break;
            case ANIMATION_SEQ_OPCODE_SET_FRAME_BIAS:
                state->frame_bias = (s8)seq->sequences[index][pos++];
                break;
            case ANIMATION_SEQ_OPCODE_CALL_SEQUENCE:
                state->saved_sequence_id = sequence_id;
                sequence_id = seq->sequences[index][pos++];
                index = sequence_id;
                if (sequence_id >= ANIMATION_SEQUENCE_EVTCHR_SLOT_0) {
                    if (sequence_id < ANIMATION_SEQUENCE_EVTCHR_SLOT_1) {
                        index = sequence_id - ANIMATION_SEQUENCE_EVTCHR_SLOT_0;
                    } else {
                        index = sequence_id - ANIMATION_SEQUENCE_EVTCHR_SLOT_1;
                    }
                }
                state->saved_script_pos = pos;
                pos = 0;
                break;
            case ANIMATION_SEQ_OPCODE_SET_LAYER_PRIORITY:
                unit->layer_priority = seq->sequences[index][pos++];
                break;
            case ANIMATION_SEQ_OPCODE_START_BLOCK_SEQUENCE:
                block_id = seq->sequences[index][pos++];
                block_sequence = seq->sequences[index][pos++];
                if (block_id == 0) {
                    system_handle_error(SYSTEM_ERROR_SEQ_OPCODE);
                }
                block = &unit->sprite_blocks[block_id - 1];
                block->wait = 1;
                block->sequence_id = block_sequence;
                block->script_pos = 0;
                block->wait_bias = 0;
                block->frame_bias = 0;
                block->repeat_count = 0;
                block->trigger = 1;
                break;
            case ANIMATION_SEQ_OPCODE_REPEAT:
                offset = (s8)seq->sequences[index][pos++];
                value = seq->sequences[index][pos++];
                /* pos is four bytes past the prefix the offset counts from. */
                if (state->repeat_count != 0) {
                    if (--state->repeat_count != 0) {
                        pos += offset - 4;
                    }
                } else {
                    pos += offset - 4;
                    state->repeat_count = value;
                }
                break;
            case ANIMATION_SEQ_OPCODE_MOVE_SCREEN_OFFSET:
                unit->screen_offset.vx += (s8)seq->sequences[index][pos++];
                unit->screen_offset.vy += (s8)seq->sequences[index][pos++];
                break;
            case ANIMATION_SEQ_OPCODE_SET_ROTATION:
                value = seq->sequences[index][pos++];
                value |= seq->sequences[index][pos++] << 8;
                state->display->rotation = value;
                break;
            case ANIMATION_SEQ_OPCODE_MOVE_UNIT:
                shift_seq_lateral(unit, seq->sequences[index][pos++]);
                shift_seq_height(unit, seq->sequences[index][pos++]);
                shift_seq_forward(unit, seq->sequences[index][pos++]);
                break;
            case ANIMATION_SEQ_OPCODE_UNKNOWN_F7:
                pos += 3;
                break;
            case ANIMATION_SEQ_OPCODE_MOVE_FORWARD_1:
                shift_seq_forward(unit, 1);
                break;
            case ANIMATION_SEQ_OPCODE_MOVE_FORWARD_2:
                shift_seq_forward(unit, 2);
                break;
            case ANIMATION_SEQ_OPCODE_MOVE_DOWN_1:
                shift_seq_height(unit, 1);
                break;
            case ANIMATION_SEQ_OPCODE_MOVE_DOWN_2:
                shift_seq_height(unit, 2);
                break;
            case ANIMATION_SEQ_OPCODE_MOVE_BACKWARD_1:
                shift_seq_forward(unit, -1);
                break;
            case ANIMATION_SEQ_OPCODE_MOVE_BACKWARD_2:
                shift_seq_forward(unit, -2);
                break;
            case ANIMATION_SEQ_OPCODE_MOVE_UP_1:
                shift_seq_height(unit, -1);
                break;
            case ANIMATION_SEQ_OPCODE_MOVE_UP_2:
                shift_seq_height(unit, -2);
                break;
            case ANIMATION_SEQ_OPCODE_SHOW_ITEM:
                unit->item_display_active = 1;
                break;
            case ANIMATION_SEQ_OPCODE_HIDE_ITEM:
                unit->item_display_active = 0;
                break;
            case ANIMATION_SEQ_OPCODE_WAIT_FOR_INPUT:
                if (g_battle_window_closed == 0) {
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case ANIMATION_SEQ_OPCODE_PLAY_WEAPON_SOUND:
                action_play_weapon_sfx(unit, seq->sequences[index][pos++]);
                break;
            case ANIMATION_SEQ_OPCODE_WAIT_FOR_EFFECT_LOAD:
                if (g_battle_animation_wait_state == 1) {
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case ANIMATION_SEQ_OPCODE_SET_ITEM_POSITION:
                unit->item_display_x_offset = seq->sequences[index][pos++];
                unit->item_display_y_offset = seq->sequences[index][pos++];
                break;
            case ANIMATION_SEQ_OPCODE_START_DISTORTION:
                /* The operand stores the id less 2, so a script cannot start NONE or the scale pulse. */
                unit->distortion_animation_id = seq->sequences[index][pos++] + 2;
                timer = seq->sequences[index][pos++];
                unit->distortion_phase = 0;
                unit->distortion_timer = timer;
                break;
            case ANIMATION_SEQ_OPCODE_WAIT_FOR_DISTORTION:
                if (unit->distortion_animation_id != ANIMATION_DISTORTION_NONE) {
                    offset = (s8)seq->sequences[index][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case ANIMATION_SEQ_OPCODE_JUMP:
                offset = (s8)seq->sequences[index][pos];
                pos += offset - 2;
                break;
            case ANIMATION_SEQ_OPCODE_START_POSITION_COPIES:
                init_afterimage(unit);
                break;
            case ANIMATION_SEQ_OPCODE_STOP_POSITION_COPIES:
                unit->position_copies_active = 0;
                break;
            case ANIMATION_SEQ_OPCODE_NOP:
                break;
            case ANIMATION_SEQ_OPCODE_UNKNOWN_CA:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_D7:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_E3:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_E4:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_E9:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_ED:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_F3:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_F4:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_F5:
                pos++;
                break;
            default:
                system_handle_error(SYSTEM_ERROR_SEQ_OPCODE);
                break;
            }
        } else {
            /* A frame instruction: the frame, then its wait, which a replay skips. */
            frame = state->frame_bias + seq->sequences[index][pos++];
            if (advance) {
                state->wait = seq->sequences[index][pos++] + state->wait_bias;
                /* A bias below the wait wraps the 16-bit value: wait one frame. */
                if (state->wait > 0x100) {
                    state->wait = 1;
                }
            } else {
                pos++;
            }
            sprite_load_unit_frame(unit, state, frame, sequence_id);
            state->script_pos = pos;
            state->frame = frame;
            state->sequence_id = sequence_id;
            return;
        }
    }
}

/* Rebuilds the casting unit's sprite parts for its current animation and frame. */
static void load_casting_unit_frame(void) {
    map_unit_t* unit = map_unit_get_casting();

    if (unit != 0) {
        sprite_load_unit_frame(unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
    }
}

/* Run a weapon or effect sprite block's sequence up to its next frame
 * instruction, or to its end, as run_unit_seq does for the
 * unit's own sprite. The unit-only commands are errors here, the ones that
 * move a unit only skip their operands, and ANIMATION_SEQ_OPCODE_STRIKE_TARGETS and
 * the two shadow commands do nothing.
 *
 * A frame id counts from the first frame of the unit's weapon type
 * (sprite_weapon_shp_t.first_frame). The frame's parts, three at most,
 * are expanded into the block's display: a weapon block draws them from the
 * weapon sheet, at the row of the used item's graphic, and an effect block
 * from the effect sheet. `index` is the block's number; it selects the CLUT
 * row.
 *
 * Matching constraints: the size table is indexed by element (`>> 10`); the
 * shift that combine later folds into `>> 7` keeps the script loop just large
 * enough that loop.c does not hoist the constant 1. `part` and `attr` are
 * function-scope (global allocation) while the other part temporaries are
 * block-local and `mode` is single-set, which gives the target's register
 * ties and schedule. */
static void run_block_seq(map_unit_t* unit, sprite_block_t* sprite, s32 index, s32 advance) {
    const sprite_weapon_shp_t* shp;
    u8** seq;
    u8 op;
    u8* data;
    u16 sequence_id;
    s16 pos;
    u16 frame;
    u16 graphic_v;
    const sprite_frame_t* src;
    sprite_display_t* display;
    sprite_block_t* block;
    const sprite_frame_part_t* part;
    s16 offset;
    u16 value;
    u8 block_id;
    u8 block_sequence;
    u32 parts;
    u8 flags;
    u16 i;
    u16 attr;

    sequence_id = sprite->sequence_id;
    graphic_v = s_sprite_item_graphic_data[unit->used_item_or_weapon_id].texture_row * 8;
    pos = sprite->script_pos;
    shp = sprite->shp;
    seq = sprite->seq;
    if (advance == 0) {
        pos -= 2;
    }
    if (sequence_id > 0x100) {
        system_handle_error(SYSTEM_ERROR_SEQ_ANIMATION);
    }
    if (seq[sequence_id] == (u8*)-1) {
        system_handle_error(SYSTEM_ERROR_SEQ_ANIMATION);
    }
    for (;;) {
        data = seq[sequence_id];
        if (data[pos] == ANIMATION_SEQ_OPCODE_PREFIX) {
            pos++;
            op = seq[sequence_id][pos++];
            switch (op) {
            case ANIMATION_SEQ_OPCODE_FLIP_HORIZONTAL:
                sprite->flags ^= SPRITE_DISPLAY_HORIZONTAL_FLIP;
                break;
            case ANIMATION_SEQ_OPCODE_FLIP_VERTICAL:
                sprite->flags ^= SPRITE_DISPLAY_VERTICAL_FLIP;
                break;
            case ANIMATION_SEQ_OPCODE_END_ANIMATION:
                /* Ending also switches the block off; both stop the script. */
                sprite->trigger = 0;
            case ANIMATION_SEQ_OPCODE_PAUSE_ANIMATION:
                sprite->wait = 0;
                return;
            case ANIMATION_SEQ_OPCODE_RESTART:
                sprite->restart_count++;
                pos = 0;
                break;
            case ANIMATION_SEQ_OPCODE_CLEAR_ROTATION:
                sprite->display->rotation = 0;
                break;
            case ANIMATION_SEQ_OPCODE_LAUNCH_PROJECTILE:
                effect_secondary_start_projectile(unit);
                /* fallthrough: the command's two operand bytes are skipped like the unknowns' */
            case ANIMATION_SEQ_OPCODE_UNKNOWN_E6:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_E7:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_E8:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_EA:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_F1:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_F8:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_FB:
                pos += 2;
                break;
            case ANIMATION_SEQ_OPCODE_RETURN:
                pos = sprite->saved_script_pos;
                sequence_id = sprite->saved_sequence_id;
                break;
            case ANIMATION_SEQ_OPCODE_SET_LAYER_PRIORITY:
                unit->layer_priority = seq[sequence_id][pos++];
                break;
            case ANIMATION_SEQ_OPCODE_WAIT_FOR_EFFECT:
                if (g_battle_animation_wait_state != 0) {
                    offset = (s8)seq[sequence_id][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case ANIMATION_SEQ_OPCODE_PLAY_SOUND:
                value = seq[sequence_id][pos++];
                map_unit_play_sfx(unit, value);
                break;
            case ANIMATION_SEQ_OPCODE_SET_WAIT_BIAS:
                sprite->wait_bias = (s8)seq[sequence_id][pos++];
                break;
            case ANIMATION_SEQ_OPCODE_SET_FRAME_BIAS:
                sprite->frame_bias = (s8)seq[sequence_id][pos++];
                break;
            case ANIMATION_SEQ_OPCODE_CALL_SEQUENCE:
                sprite->saved_sequence_id = sequence_id;
                sequence_id = seq[sequence_id][pos++];
                sprite->saved_script_pos = pos;
                pos = 0;
                break;
            case ANIMATION_SEQ_OPCODE_MOVE_SCREEN_OFFSET:
                unit->screen_offset.vx += (s8)seq[sequence_id][pos++];
                unit->screen_offset.vy += (s8)seq[sequence_id][pos++];
                break;
            case ANIMATION_SEQ_OPCODE_REPEAT:
                offset = (s8)seq[sequence_id][pos++];
                value = seq[sequence_id][pos++];
                if (sprite->repeat_count != 0) {
                    if (--sprite->repeat_count != 0) {
                        pos += offset - 4;
                    }
                } else {
                    pos += offset - 4;
                    sprite->repeat_count = value;
                }
                break;
            case ANIMATION_SEQ_OPCODE_START_BLOCK_SEQUENCE:
                block_id = seq[sequence_id][pos++];
                block_sequence = seq[sequence_id][pos++];
                if (block_id == 0) {
                    system_handle_error(SYSTEM_ERROR_SEQ_OPCODE);
                }
                block = &unit->sprite_blocks[block_id - 1];
                block->sequence_id = block_sequence;
                block->script_pos = 0;
                block->wait = 1;
                block->wait_bias = 0;
                block->frame_bias = 0;
                block->repeat_count = 0;
                block->trigger = 1;
                break;
            case ANIMATION_SEQ_OPCODE_SET_ROTATION:
                value = seq[sequence_id][pos++];
                value |= seq[sequence_id][pos++] << 8;
                sprite->display->rotation = value;
                break;
            case ANIMATION_SEQ_OPCODE_UNKNOWN_F7:
            case ANIMATION_SEQ_OPCODE_MOVE_UNIT:
                pos += 3;
                break;
            case ANIMATION_SEQ_OPCODE_WAIT_FOR_EFFECT_LOAD:
                if (g_battle_animation_wait_state == 1) {
                    offset = (s8)seq[sequence_id][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case ANIMATION_SEQ_OPCODE_JUMP:
                offset = (s8)seq[sequence_id][pos];
                pos += offset - 2;
                break;
            case ANIMATION_SEQ_OPCODE_UNKNOWN_D7:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_E3:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_E4:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_E9:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_ED:
            case ANIMATION_SEQ_OPCODE_MOVE_UNIT_FB:
            case ANIMATION_SEQ_OPCODE_MOVE_UNIT_DU:
            case ANIMATION_SEQ_OPCODE_MOVE_UNIT_RL:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_F3:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_F4:
            case ANIMATION_SEQ_OPCODE_UNKNOWN_F5:
                pos++;
                break;
            case ANIMATION_SEQ_OPCODE_STRIKE_TARGETS:
            case ANIMATION_SEQ_OPCODE_HIDE_SHADOW:
            case ANIMATION_SEQ_OPCODE_SHOW_SHADOW:
                break;
            default:
                system_handle_error(SYSTEM_ERROR_SEQ_OPCODE);
                break;
            }
        } else {
            frame = sprite->frame_bias + (data[pos++] + shp->first_frame[unit->equipped_weapon_type]);
            if (advance != 0) {
                sprite->wait = data[pos++] + sprite->wait_bias;
            } else {
                pos++;
            }
            src = shp->frames[frame];
            if (frame > 0x200) {
                system_handle_error(SYSTEM_ERROR_SHP_FRAME);
            }
            if (src == (const sprite_frame_t*)-1) {
                system_handle_error(SYSTEM_ERROR_SHP_FRAME);
            }
            display = sprite->display;
            parts = display->part_count = (src->part_count_and_rotation & 7) + 1;
            display->rotation = s_sprite_y_rotations[src->part_count_and_rotation >> 3];
            if (unit->sprite_display_flags & SPRITE_DISPLAY_HORIZONTAL_FLIP) {
                display->rotation = -display->rotation;
            }
            if (parts > 3) {
                parts = 3;
            }
            flags = src->flags;
            switch (sprite->graphic_type) {
            case SPRITE_GRAPHIC_WEAPON:
                display->tpage = (flags & SPRITE_DISPLAY_BLEND_MASK) | SPRITE_TPAGE_WEAPON;
                display->clut = unit->clut + index * 64 + 0x80; /* MAP_CLUT_ROW_ITEM_A or _B */
                for (i = 0; i < parts; i++) {
                    u32 size;
                    u32 tile;
                    s32 width;
                    s32 height;
                    s32 u;
                    s32 v;
                    u32 mode;

                    part = &src->parts[i];
                    attr = part->attributes;
                    size = (attr & SPRITE_SHP_PART_SIZE_MASK) >> 10;
                    width = s_sprite_part_sizes[size].width;
                    height = s_sprite_part_sizes[size].height;
                    tile = attr & SPRITE_SHP_PART_TILE_MASK;
                    u = (tile & 0x1f) * 8;
                    v = (tile >> 5) * 8 + graphic_v;
                    mode = (flags & (SPRITE_DISPLAY_SEMI_TRANSPARENT | SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & SPRITE_SHP_PART_FLIP_MASK) >> 13);
                    sprite_display_set_part(
                        display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8, mode);
                }
                break;
            case SPRITE_GRAPHIC_EFFECT:
                display->tpage = (flags & SPRITE_DISPLAY_BLEND_MASK) | SPRITE_TPAGE_EFFECT;
                display->clut = unit->clut + index * 64 + 0x80; /* MAP_CLUT_ROW_ITEM_A or _B */
                for (i = 0; i < parts; i++) {
                    u32 size;
                    u32 tile;
                    s32 width;
                    s32 height;
                    s32 u;
                    s32 v;
                    u32 mode;

                    part = &src->parts[i];
                    attr = part->attributes;
                    size = (attr & SPRITE_SHP_PART_SIZE_MASK) >> 10;
                    width = s_sprite_part_sizes[size].width;
                    height = s_sprite_part_sizes[size].height;
                    tile = attr & SPRITE_SHP_PART_TILE_MASK;
                    u = (tile & 0x1f) * 8;
                    v = (tile >> 5) * 8;
                    mode = (flags & (SPRITE_DISPLAY_SEMI_TRANSPARENT | SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & SPRITE_SHP_PART_FLIP_MASK) >> 13);
                    sprite_display_set_part(
                        display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8, mode);
                }
                break;
            }
            sprite->script_pos = pos;
            sprite->frame = frame;
            sprite->sequence_id = sequence_id;
            return;
        }
    }
}

/* Capture a unit's graphics into an EVTCHR VRAM cache slot.
 *
 * The unit's compressed spritesheet slot is expanded into `image_slot`'s
 * image buffer and uploaded to `slot`'s rectangle; the unit's 64x32 VRAM
 * block (column spritesheet slot / 8, row spritesheet slot % 8 of the
 * SPRITE_SHEET_STRIP_VRAM_X/Y area) is copied 200 lines below it. `slot` is then owned by the unit's attack
 * image (map unit id | SPRITE_EVTCHR_SLOT_OWNER_ATTACK_IMAGE), and the image
 * buffer stays busy for SPRITE_EVTCHR_SLOT_UPLOAD_FRAMES frames while the
 * upload completes. */
static void load_attack_image(map_unit_t* unit, s32 slot, s32 image_slot) {
    RECT rect;

    sprite_decompress_attack_image(g_sprite_sheet_slots[unit->spritesheet_vram_slot].compressed_attack_image,
        g_sprite_evtchr_slots[image_slot].image_data);
    LoadImage(&g_sprite_evtchr_slots[slot].image_rect, (u32*)g_sprite_evtchr_slots[image_slot].image_data);
    rect.x = (unit->spritesheet_vram_slot >> 3) * SPRITE_SHEET_STRIP_WIDTH + SPRITE_SHEET_STRIP_VRAM_X;
    rect.y = (unit->spritesheet_vram_slot & 7) * SPRITE_SHEET_STRIP_HEIGHT + SPRITE_SHEET_STRIP_VRAM_Y;
    rect.w = SPRITE_SHEET_STRIP_WIDTH;
    rect.h = SPRITE_SHEET_STRIP_HEIGHT;
    MoveImage(&rect, g_sprite_evtchr_slots[slot].image_rect.x, g_sprite_evtchr_slots[slot].image_rect.y + 200);
    g_sprite_evtchr_slots[slot].owner = unit->map_unit_id | SPRITE_EVTCHR_SLOT_OWNER_ATTACK_IMAGE;
    g_system_frame_pacing_suppressed = 1;
    g_sprite_evtchr_slots[image_slot].image_buffer_state = SPRITE_EVTCHR_SLOT_UPLOAD_FRAMES;
    g_sprite_evtchr_slots[slot].evtchr_load_state = 1;
}

/* Free the EVTCHR slots whose page holds the unit's attack image. One that
 * holds its SP2 image stays. */
static void release_attack_image(map_unit_t* unit) {
    s32 i;
    s32 slot_id;

    for (i = 0; i < SPRITE_EVTCHR_SLOT_COUNT; i++) {
        slot_id = g_sprite_evtchr_slots[i].owner;
        if (slot_id != SPRITE_EVTCHR_SLOT_OWNER_EVTCHR && slot_id != SPRITE_EVTCHR_SLOT_OWNER_NONE
            && (slot_id & SPRITE_EVTCHR_SLOT_OWNER_ID_MASK) == unit->map_unit_id
            && (slot_id & SPRITE_EVTCHR_SLOT_OWNER_SP2) == 0) {
            g_sprite_evtchr_slots[i].owner = SPRITE_EVTCHR_SLOT_OWNER_NONE;
            g_sprite_evtchr_slots[i].evtchr_load_state = 0;
        }
    }
}

/* Update one unit's sprite animation for the frame.
 *
 * The unit's facing relative to the camera yaw gives the quadrant and the
 * sixteenth of a turn it is seen from. A request for
 * ANIMATION_STAND shows the standing sequence of that sixteenth.
 * Any other request becomes sequence 2 * animation, + 1 seen from behind
 * (a request from 500 up is the sequence id itself). Animations below 11
 * start only on the unit's frame in four unless forced, and a request for
 * the sequence already playing is dropped. A sequence of the SEQ file's attack
 * set needs the unit's attack image in an EVTCHR slot first: a free slot and
 * a free image buffer or, failing that, slot 1 taken from the unit that
 * holds it, which is left on its rest frame. Without a request, a change of
 * sixteenth turns the standing pose or swaps the running sequence for its
 * other side; otherwise the sequence and the three weapon/effect blocks
 * count their waits down by g_system_vsync_interval.
 *
 * unit is initialized in its declaration so its copy precedes the table
 * copies, and the unread unused_10 sizes the target's frame. */
static void update_animation(map_unit_t* unit_data) {
    map_unit_t* unit = unit_data;
    s16 unused_10[4]; /* Unread; occupies sp+0x10 like the target. */
    /* The flip tables hold SPRITE_DISPLAY_HORIZONTAL_FLIP for the views drawn mirrored. */
    s16 flip_by_quadrant[4] = { 0, 0, 2, 2 };
    s16 back_by_quadrant[4] = { 0, 1, 1, 0 };
    s16 flip_by_direction[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 0 };
    s16 stand_by_direction[16] = { 1, 2, 2, 3, 3, 4, 4, 5, 5, 4, 4, 3, 3, 2, 2, 1 };
    map_unit_t* other;
    sprite_unit_seq_table_t* seq;
    s32 direction;
    s16 quadrant;
    s16 sixteenth;
    s32 animation;
    s32 index;
    s32 ok;
    u16 sequence_id;
    s32 i;
    s32 j;

    if (unit->draw_enabled == 0) {
        return;
    }

    if (unit->position_copies_active != 0) {
        shift_and_fill_afterimage(unit);
    }

    direction = (g_camera_render_state.vy + unit->facing) & 0xfff;
    quadrant = direction / 1024;
    sixteenth = direction / 256;

    /* requested_animation holds the animation plus one; 0 means no request. */
    if (unit->requested_animation != 0) {
        if (unit->requested_animation - 1 == ANIMATION_STAND) {
            for (i = 0; i < 3; i++) {
                unit->sprite_blocks[i].trigger = 0;
            }

            unit->animation.sequence_id = stand_by_direction[sixteenth];
            unit->sprite_display_flags
                = (unit->sprite_display_flags & ~(SPRITE_DISPLAY_HORIZONTAL_FLIP | SPRITE_DISPLAY_VERTICAL_FLIP))
                | flip_by_direction[sixteenth];
            unit->camera_facing_quadrant = quadrant;
            unit->camera_facing_sixteenth = sixteenth;

            unit->animation.script_pos = 0;
            unit->animation.wait = 0;
            unit->animation.restart_count = 0;
            unit->seq_offset.vz = 0;
            unit->seq_offset.vy = 0;
            unit->seq_offset.vx = 0;
            unit->screen_offset.vz = 0;
            unit->screen_offset.vy = 0;
            unit->screen_offset.vx = 0;
            unit->animation.wait_bias = 0;
            unit->animation.frame_bias = 0;
            unit->animation.repeat_count = 0;
            unit->animation.flags = 0;

            run_unit_seq(unit, &unit->animation, 1);
            unit->requested_animation = 0;
            release_attack_image(unit);
        } else if (unit->requested_animation >= 12 || (unit->map_unit_id & 3) == g_frame_counter % 4
            || (unit->movement.force_animation_update)) {
            /* An animation below 11 waits for the unit's frame in four. */
            ok = 1;
            if ((unit->requested_animation - 1) * 2 + back_by_quadrant[quadrant] != unit->animation.sequence_id
                || unit->animation.wait == 0 || (unit->movement.force_animation_update)) {
                animation = unit->requested_animation - 1;
                if (animation < ANIMATION_SEQUENCE_EVTCHR_SLOT_0) {
                    seq = unit->animation.seq;
                    index = animation * 2 + back_by_quadrant[quadrant];
                    if (index < seq->first_sp2_sequence) {
                        if (index >= seq->first_attack_sequence) {
                            /* Keep a slot the unit already owns; otherwise load into a free slot and a free
                             * image buffer. */
                            for (i = 0; i < SPRITE_EVTCHR_SLOT_COUNT; i++) {
                                if ((g_sprite_evtchr_slots[i].owner & SPRITE_EVTCHR_SLOT_OWNER_ID_MASK)
                                    == unit->map_unit_id) {
                                    break;
                                }
                            }
                            if (i == SPRITE_EVTCHR_SLOT_COUNT) {
                                for (i = 0; i < SPRITE_EVTCHR_SLOT_COUNT; i++) {
                                    if (g_sprite_evtchr_slots[i].owner == SPRITE_EVTCHR_SLOT_OWNER_NONE) {
                                        for (j = 0; j < SPRITE_EVTCHR_SLOT_COUNT; j++) {
                                            if (g_sprite_evtchr_slots[j].image_buffer_state
                                                == SPRITE_EVTCHR_SLOT_BUFFER_FREE) {
                                                load_attack_image(unit, i, j);
                                                break;
                                            }
                                        }
                                        break;
                                    }
                                }
                                if (i == SPRITE_EVTCHR_SLOT_COUNT) {
                                    ok = 0;
                                }
                                /* With no free slot, j was never set (QUIRKS.md). */
                                if (j == SPRITE_EVTCHR_SLOT_COUNT) {
                                    ok = 0;
                                }

                                /* None free: take slot 1 unless it holds an EVTCHR image or nothing, and leave
                                 * its owner on its rest frame. */
                                if (ok == 0) {
                                    if (g_sprite_evtchr_slots[1].owner - SPRITE_EVTCHR_SLOT_OWNER_EVTCHR >= 2) {
                                        other = map_unit_get(
                                            g_sprite_evtchr_slots[1].owner & SPRITE_EVTCHR_SLOT_OWNER_ID_MASK);
                                        if (other != 0) {
                                            if (other->animation.sequence_id & 1) {
                                                other->animation.frame = s_animation_targeted_frame_back
                                                    [g_sprite_sheet_data[other->spritesheet_id].shp_id];
                                            } else {
                                                other->animation.frame = s_animation_targeted_frame_front
                                                    [g_sprite_sheet_data[other->spritesheet_id].shp_id];
                                            }
                                            other->seq_offset.vz = 0;
                                            other->seq_offset.vy = 0;
                                            other->seq_offset.vx = 0;
                                            sprite_load_unit_frame(other, &other->animation, other->animation.frame,
                                                other->animation.sequence_id);
                                            ok = 1;
                                            unit->animation.wait = 0;
                                            load_attack_image(unit, 1, 1);
                                        }
                                    }
                                }
                            }
                        } else {
                            release_attack_image(unit);
                        }
                    }
                }

                if (ok != 0) {
                    for (i = 0; i < 3; i++) {
                        unit->sprite_blocks[i].trigger = 0;
                    }

                    animation = unit->requested_animation - 1;
                    if (animation >= ANIMATION_SEQUENCE_EVTCHR_SLOT_0) {
                        unit->animation.sequence_id = unit->requested_animation - 1;
                    } else {
                        unit->animation.sequence_id = animation * 2 + back_by_quadrant[quadrant];
                    }

                    unit->sprite_display_flags = (unit->sprite_display_flags
                                                     & ~(SPRITE_DISPLAY_HORIZONTAL_FLIP | SPRITE_DISPLAY_VERTICAL_FLIP))
                        | flip_by_quadrant[quadrant];
                    unit->camera_facing_quadrant = quadrant;
                    unit->camera_facing_sixteenth = sixteenth;

                    unit->animation.script_pos = 0;
                    unit->animation.wait = 0;
                    unit->animation.restart_count = 0;
                    unit->seq_offset.vz = 0;
                    unit->seq_offset.vy = 0;
                    unit->seq_offset.vx = 0;
                    unit->screen_offset.vz = 0;
                    unit->screen_offset.vy = 0;
                    unit->screen_offset.vx = 0;
                    unit->animation.wait_bias = 0;
                    unit->animation.frame_bias = 0;
                    unit->animation.repeat_count = 0;
                    unit->animation.flags = 0;

                    run_unit_seq(unit, &unit->animation, 1);
                    unit->movement.force_animation_update = 0;
                    unit->requested_animation = 0;

                    /* A mount restarts its rider's sequence along with its own. */
                    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
                        other = map_unit_get(unit->mount_partner_map_unit_id);
                        if (other != 0) {
                            other->animation.script_pos = 0;
                            other->animation.wait = 0;
                            run_unit_seq(other, &other->animation, 1);
                        }
                    }
                } else if (unit->animation.wait != 0) {
                    /* No slot could be had: the request stays pending while the current sequence runs on. */
                    for (i = 0; i < g_system_vsync_interval; i++) {
                        if (unit->animation.wait != 0) {
                            unit->animation.wait--;
                            if (unit->animation.wait == 0) {
                                run_unit_seq(unit, &unit->animation, 1);
                            }
                        }
                    }
                }
            } else {
                /* The requested sequence is already playing: drop the request. */
                unit->requested_animation = 0;
            }
        }
    } else if (!(unit->movement.suppress_facing_update)) {
        /* No request: a new view direction re-aims the running sequence without restarting
         * its wait. */
        if (sixteenth != unit->camera_facing_sixteenth) {
            sequence_id = unit->animation.sequence_id;
            /* Sequences 0-5 are the standing poses, chosen per sixteenth; the others keep their
             * front/back pair and switch side. */
            if (sequence_id < (ANIMATION_IDLE << 1)) {
                unit->animation.sequence_id = stand_by_direction[sixteenth];
                unit->sprite_display_flags
                    = (unit->sprite_display_flags & ~(SPRITE_DISPLAY_HORIZONTAL_FLIP | SPRITE_DISPLAY_VERTICAL_FLIP))
                    | flip_by_direction[sixteenth];
            } else {
                unit->animation.sequence_id = (sequence_id & 0xfffe) + back_by_quadrant[quadrant];
                unit->sprite_display_flags
                    = (unit->sprite_display_flags & ~(SPRITE_DISPLAY_HORIZONTAL_FLIP | SPRITE_DISPLAY_VERTICAL_FLIP))
                    | flip_by_quadrant[quadrant];
            }
            unit->camera_facing_quadrant = quadrant;
            unit->camera_facing_sixteenth = sixteenth;
            run_unit_seq(unit, &unit->animation, 0);
        } else if (unit->animation.wait != 0) {
            for (i = 0; i < g_system_vsync_interval; i++) {
                if (unit->animation.wait != 0) {
                    unit->animation.wait--;
                    if (unit->animation.wait == 0) {
                        run_unit_seq(unit, &unit->animation, 1);
                    }
                }
                dispatch_distortion(unit);
            }
        }
    } else if (unit->animation.wait != 0) {
        /* Facing updates are suppressed: only count the wait down. */
        for (i = 0; i < g_system_vsync_interval; i++) {
            if (unit->animation.wait != 0) {
                unit->animation.wait--;
                if (unit->animation.wait == 0) {
                    run_unit_seq(unit, &unit->animation, 1);
                }
            }
            dispatch_distortion(unit);
        }
    }

    /* Count down the three weapon/effect sprite blocks. */
    for (i = 0; i < 3; i++) {
        if (unit->sprite_blocks[i].trigger != 0 && unit->sprite_blocks[i].wait != 0) {
            for (j = 0; j < g_system_vsync_interval; j++) {
                if (unit->sprite_blocks[i].wait != 0) {
                    unit->sprite_blocks[i].wait--;
                    if (unit->sprite_blocks[i].wait == 0) {
                        run_block_seq(unit, &unit->sprite_blocks[i], i, 1);
                    }
                }
            }
        }
    }
}

/* Draws the unit's status bubble when the unit is on screen and has one.
 *
 * Nothing is drawn unless the camera-relative position lies within x 0x61..0x19f
 * and y -0x1f..0x10f, the ordering-table depth within 1..0x17f,
 * animation.trigger is set, the unit is not a mount carrying its rider and
 * status_bubble_active is set. */
static void draw_status_bubble(map_unit_t* unit) {
    DVECTOR screen_coords;

    /* The camera-relative position is read unsigned (lhu) here. */
    screen_coords.vx = unit->camera_relative_x;
    screen_coords.vy = unit->camera_relative_y;
    if (screen_coords.vx < 0x61 || screen_coords.vx > 0x19f) {
        return;
    }
    if (screen_coords.vy < -0x1f || screen_coords.vy > 0x10f) {
        return;
    }
    if ((unit->otag_depth_index - 1) >= 0x17f) {
        return;
    }
    if (unit->animation.trigger == 0) {
        return;
    }
    if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
        return;
    }
    if (unit->status_bubble_active == 0) {
        return;
    }
    draw_status_bubble_sprite(unit, &screen_coords);
}

/* Queue a unit's sprite layers into the ordering table at its depth.
 *
 * s_animation_layer_priorities[layer_priority] lists four layers in draw order: 0 is the unit's own
 * sprite display, n > 0 is weapon/effect sprite block n - 1 (skipped while
 * its trigger is clear). A rider (mount state 1) is skipped here; a mount
 * (state 2) draws its first three body parts, then its rider's layers, then
 * the rest of its body. Unmounted units mirror their body sprite by the
 * animation state's flags. The shadow and the position copies follow.
 *
 * The rider loop issues one call per arm; cross-jumping merges them after
 * reload, but the duplicated RTL makes the mount loop large enough that
 * loop.c leaves `row * 16 + s_animation_layer_priorities` inside it, which is why the target
 * recomputes the table address each iteration and spills `row`. */
static void draw_layers(map_unit_t* unit) {
    SVECTOR position;
    SVECTOR rider_position;
    SVECTOR zoom = { ONE, ONE, ONE };
    map_unit_t* rider;
    sprite_display_t* display;
    sprite_display_t* rider_display;
    s32 row;
    s32 rider_row;
    s32 i;
    s32 j;
    s32 entry;
    s16 angle;
    s16 flags;
    s16 rider_flags;

    position.vx = unit->camera_relative_x;
    position.vy = unit->camera_relative_y;
    SetGeomScreen(0x200);
    SetGeomOffset(0, 0);
    angle = 0;
    if (unit->otag_depth_index - 1 >= 0x17f) {
        return;
    }
    if (unit->numeric_display.active) {
        animate_result_text(unit, &position);
    }
    if (!((position.vx >= 0x61 && position.vx <= 0x19f) && (position.vy >= -0x1f && position.vy <= 0x10f))
        && unit->spritesheet_id != SPRITE_SHEET_ID_ALTIMA_SECOND_FORM) {
        return;
    }
    copyVector(&zoom, &g_camera_zoom);
    flags = unit->horizontal_flip_flag ^ unit->sprite_display_flags;
    row = unit->layer_priority;
    if (unit->animation.trigger == 0) {
        return;
    }
    if (unit->item_display_active) {
        draw_item_display(unit);
    }
    if (unit->mount_state != UNIT_MOUNT_STATE_NONE) {
        if (unit->mount_state == UNIT_MOUNT_STATE_RIDER) {
            /* Its mount draws it. */
        } else if (unit->mount_state == UNIT_MOUNT_STATE_MOUNT) {
            for (i = 0; i < 4; i++) {
                entry = s_animation_layer_priorities[row][i];
                if (entry != 0) {
                    if (unit->sprite_blocks[entry - 1].trigger == 0) {
                        continue;
                    }
                    display = unit->sprite_blocks[entry - 1].display;
                    sprite_display_draw_parts(display, display->part_count, 0, &position, angle, flags, &zoom,
                        gfx_get_battle_otag() + unit->otag_depth_index);
                } else {
                    display = unit->animation.display;
                    sprite_display_draw_parts(
                        display, 3, 0, &position, angle, flags, &zoom, gfx_get_battle_otag() + unit->otag_depth_index);
                    rider = map_unit_get(unit->mount_partner_map_unit_id);
                    if (rider != 0 && rider->draw_enabled != 0) {
                        rider_position.vx = rider->camera_relative_x;
                        rider_position.vy = rider->camera_relative_y;
                        rider_flags = rider->sprite_display_flags;
                        rider_row = rider->layer_priority;
                        for (j = 0; j < 4; j++) {
                            entry = s_animation_layer_priorities[rider_row][j];
                            if (entry != 0) {
                                if (rider->sprite_blocks[entry - 1].trigger == 0) {
                                    continue;
                                }
                                rider_display = rider->sprite_blocks[entry - 1].display;
                                sprite_display_draw_parts(rider_display, rider_display->part_count, 0, &rider_position,
                                    angle, rider_flags, &zoom, gfx_get_battle_otag() + unit->otag_depth_index);
                            } else {
                                rider_display = rider->animation.display;
                                sprite_display_draw_parts(rider_display, rider_display->part_count, 0, &rider_position,
                                    angle, rider_flags, &zoom, gfx_get_battle_otag() + unit->otag_depth_index);
                            }
                        }
                    }
                    sprite_display_draw_parts(display, display->part_count, 3, &position, angle, flags, &zoom,
                        gfx_get_battle_otag() + unit->otag_depth_index);
                }
            }
            if (unit->shadow_graphic_trigger) {
                sprite_draw_shadow(unit, gfx_get_battle_otag() + unit->otag_depth_index);
            }
        }
    } else {
        for (i = 0; i < 4; i++) {
            entry = s_animation_layer_priorities[row][i];
            if (entry != 0) {
                if (unit->sprite_blocks[entry - 1].trigger == 0) {
                    continue;
                }
                display = unit->sprite_blocks[entry - 1].display;
                sprite_display_draw_parts(display, display->part_count, 0, &position, angle, flags, &zoom,
                    gfx_get_battle_otag() + unit->otag_depth_index);
            } else {
                display = unit->animation.display;
                sprite_display_draw_parts(display, display->part_count, 0, &position, angle,
                    flags ^ unit->animation.flags, &zoom, gfx_get_battle_otag() + unit->otag_depth_index);
            }
        }
        if (unit->shadow_graphic_trigger) {
            sprite_draw_shadow(unit, gfx_get_battle_otag() + unit->otag_depth_index);
        }
    }
    if (unit->position_copies_active) {
        draw_afterimage(unit);
    }
}

/* Project every unit record into camera space for drawing.
 *
 * camera_relative_x and _y take the projected draw position plus the unit's
 * screen offset, subtracted along an axis whose sprite is mirrored. The
 * ordering-table depth comes from a second projection of the unit's depth
 * point (the stepping variant while the unit is mid-step, offset 5 instead
 * of 1 on a tile with a depth bias). A unit standing in water takes that
 * depth at the water surface, 12 units per depth level above its feet; In
 * Water moves it up one level only on a water tile, and Float and Walk on
 * Water not at all. */
void sprite_project_units(void) {
    VECTOR transformed;
    SVECTOR screen;
    SVECTOR adjusted;
    s32 gte_flag;
    map_unit_t* unit;
    map_tile_t* tile;
    move_effective_flags_e movement_flags;

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);

    unit = g_map_unit_list_head;
    while (unit != 0) {
        map_unit_get_draw_coords(&screen, unit);
        RotTrans(&screen, &transformed, &unit->gte_flag);

        if (unit->sprite_display_flags & SPRITE_DISPLAY_HORIZONTAL_FLIP) {
            unit->camera_relative_x = transformed.vx - unit->screen_offset.vx;
        } else {
            unit->camera_relative_x = transformed.vx + unit->screen_offset.vx;
        }
        if (unit->sprite_display_flags & SPRITE_DISPLAY_VERTICAL_FLIP) {
            unit->camera_relative_y = transformed.vy - unit->screen_offset.vy;
        } else {
            unit->camera_relative_y = transformed.vy + unit->screen_offset.vy;
        }

        tile = map_get_tile(
            unit->world_coords.vx / MAP_TILE_SIZE, unit->world_coords.vz / MAP_TILE_SIZE, unit->map_layer);
        if (unit->step_phase != MOVE_STEP_PHASE_NONE) {
            if ((tile->surface.depth_bias) != 0) {
                camera_get_biased_step_depth_point(&screen, &adjusted);
            } else {
                camera_get_unbiased_step_depth_point(&screen, &adjusted);
            }
        } else if ((tile->surface.depth_bias) != 0) {
            camera_get_biased_tile_depth_point(&screen, &adjusted);
        } else {
            camera_get_unbiased_tile_depth_point(&screen, &adjusted);
        }

        if (unit->battle_unit != 0) {
            movement_flags = move_get_effective_flags(unit->battle_unit);
            if (movement_flags & MOVE_EFFECTIVE_IN_WATER) {
                if (tile->depth_half_height.depth != 0) {
                    adjusted.vy -= 12;
                }
            } else if (!(movement_flags & MOVE_EFFECTIVE_FLOAT)) {
                if (!(movement_flags & MOVE_EFFECTIVE_ON_WATER)) {
                    adjusted.vy -= tile->depth_half_height.depth * 12;
                }
            }
        }

        RotTrans(&adjusted, &transformed, &gte_flag);
        unit->otag_depth_index = transformed.vz / 4;
        unit = unit->previous;
    }
}

/*
 * Per-frame status-bubble and unit-graphics update.
 *
 * In the cursor, menu, move and targeting states listed below, status bubbles
 * stay hidden while the camera zooms, tilts, rotates or pans and for 0x3c
 * vertical blanks (one second) after it stops; then every unit record updates
 * its bubble. Every state then reloads the casting unit's frame parts and
 * draws every unit record's sprite layers.
 */
void sprite_draw_units(void) {
    map_unit_t* unit;

    switch (g_battle_state) {
    case BATTLE_STATE_FREE_CURSOR:
    case BATTLE_STATE_HIGHLIGHT_UNITS:
    case BATTLE_STATE_OPEN_ACTION_MENUS:
    case BATTLE_STATE_IDLING_ACTION_MENUS:
    case BATTLE_STATE_AI_COMMAND:
    case BATTLE_STATE_DISPLAY_MOVE_AREA:
    case BATTLE_STATE_UNIT_MOVE:
    case BATTLE_STATE_SELECT_MOVE_TILE:
    case BATTLE_STATE_TARGETING_PROMPT:
    case BATTLE_STATE_TARGETING_RANGE:
    case BATTLE_STATE_ABILITY_PREVIEW_HANDLING:
    case BATTLE_STATE_CONFIRM_ACTION:
        if (g_camera_zoom_action == CAMERA_STEP_NONE && g_camera_tilt_action == CAMERA_STEP_NONE
            && g_camera_rotation_action == CAMERA_ROTATION_NONE
            && !(g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz)) {
            if (g_status_bubble_delay != 0) {
                g_status_bubble_delay -= g_system_vsync_interval;
                if (g_status_bubble_delay <= 0) {
                    g_status_bubble_delay = 0;
                }
            } else {
                for (unit = g_map_unit_list_head; unit != 0; unit = unit->previous) {
                    draw_status_bubble(unit);
                }
            }
        } else {
            g_status_bubble_delay = 0x3c;
        }
        break;
    }
    load_casting_unit_frame();
    for (unit = g_map_unit_list_head; unit != 0; unit = unit->previous) {
        draw_layers(unit);
    }
}

/* Unlink a unit record from the unit list and free its map unit slot; its spritesheet VRAM slot is
 * freed too unless another live unit still uses it. */
void map_unit_remove(map_unit_t* unit) {
    map_unit_t* it;
    s32 shared = 0;

    sprite_display_release_unit(unit->animation.display);
    g_map_unit_slots[unit->map_unit_id].in_use = 0;
    /* The 32-bit list-head pointer doubles as a sentinel record (previous is at 0x000);
     * reading the first link through that view keeps the target's addressing. */
    for (it = ((map_unit_t*)&g_map_unit_list_head)->previous; it != 0; it = it->previous) {
        if (it->spritesheet_vram_slot == unit->spritesheet_vram_slot && g_map_unit_slots[it->map_unit_id].in_use != 0) {
            shared = 1;
        }
    }
    if (shared == 0) {
        g_sprite_sheet_slots[unit->spritesheet_vram_slot].in_use = 0;
    }
    for (it = (map_unit_t*)&g_map_unit_list_head; it != 0; it = it->previous) {
        if (it->previous == unit) {
            it->previous = unit->previous;
            return;
        }
    }
}

/* Remove every unit record. The walk reads each record's link after removing it, which works
 * because map_unit_remove leaves that link in place. */
void map_unit_clear_all(void) {
    map_unit_t* unit = g_map_unit_list_head;

    while (unit != 0) {
        map_unit_remove(unit);
        unit = unit->previous;
    }
}

/* Steps a unit's fade-out removal; each phase waits for the unit's palette
 * animation to end.
 *
 * FLASH starts the fade to black and moves to DARKEN. DARKEN then disables the
 * battle unit slot, removes the map unit record and returns 1; for the source unit it
 * only hides the unit. Every other path returns 0. */
static s32 update_fade_out_removal(map_unit_t* unit) {
    u8 phase = unit->fade_out_phase;

    if (phase == UNIT_FADE_OUT_NONE) {
        return 0;
    }
    if (g_map_palette_state.clut_rows[MAP_CLUT_ROW_UNITS].animations[unit->map_unit_id].active != 0) {
        return 0;
    }
    switch (phase) {
    case UNIT_FADE_OUT_FLASH:
        unit->fade_out_phase = UNIT_FADE_OUT_DARKEN;
        sprite_palette_apply_modulation_to_units(MAP_COLOR_MODE_BASE, 4, unit->map_unit_id, -0x1f, -0x1f, -0x1f);
        return 0;
    case UNIT_FADE_OUT_DARKEN:
        if (unit->map_unit_id != g_unit_source_map_unit_id) {
            unit->fade_out_phase = UNIT_FADE_OUT_REMOVED;
            if (unit->battle_unit != 0) {
                battle_unit_disable(unit->battle_unit->battle_unit_id);
            }
            map_unit_remove(unit);
            return 1;
        }
        map_unit_hide(unit->map_unit_id);
        return 0;
    }
    return 0;
}

/* Per-frame update of every unit record: its animation with the weapon and effect
 * sprites, its status bubble and its fade-out removal. */
void map_unit_update_animations(void) {
    map_unit_t* unit = g_map_unit_list_head;

    while (unit != 0) {
        update_animation(unit);
        update_float_height_and_status_bubble(unit);
        update_fade_out_removal(unit);
        unit = unit->previous;
    }
}

/* Unpack a unit SEQ file: copy its first attack and SP2 sequence numbers, turn its 256 sequence
 * offsets into pointers and append the sequence data at g_sprite_seq_data_cursor. An offset of
 * -1 resolves to the start of that data. */
static void unpack_unit_seq_data(sprite_native_unit_seq_table_t* destination, const unit_seq_file_t* source) {
    const unit_seq_file_t* header;
    const u8(*entry)[4];
    u32 cursor;
    u32 index;
    u32 payload_size;
    s32 offset;

    header = source;
    cursor = (u32)g_sprite_seq_data_cursor;
    destination->header.first_attack_sequence
        = source->first_attack_sequence[0] + (source->first_attack_sequence[1] << 8);
    destination->header.first_sp2_sequence = source->first_sp2_sequence[0] + (source->first_sp2_sequence[1] << 8);
    entry = source->sequence_offsets;
    for (index = 0; index < 0x100; index++) {
        offset = entry[index][0] + (entry[index][1] << 8) + (entry[index][2] << 16) + (entry[index][3] << 24);
        if (offset == -1) {
            offset = 0;
        }
        destination->sequences[index] = (u8*)(offset + cursor);
    }

    payload_size = header->sequence_data_size[0] + (header->sequence_data_size[1] << 8);
    for (index = 0; index < payload_size; index++) {
        const u8* payload = source->sequence_data;

        g_sprite_seq_data_cursor[index] = payload[index];
    }
    g_sprite_seq_data_cursor += index;
}

/* Unpack a weapon or effect SEQ file, which has no header fields: its 256
 * sequence offsets become pointers into the sequence data, which is appended
 * at g_sprite_seq_data_cursor.
 *
 * An offset of -1 resolves to the start of that data instead of to a real
 * sequence.
 *
 * `header` and `unused_frame_pad` are matching constraints. The target keeps
 * two live copies of the record pointer (`move t2,a1` at +0x4, with `a1` still
 * used for the entry base and the payload address), which only a second
 * variable reproduces; and it reserves an 8-byte frame that no instruction
 * touches, which only a declared local aggregate reproduces. */
static void unpack_weapon_seq_data(u8** entries, const packed_seq_file_t* record) {
    const u8(*entry)[4];
    u32 index;
    u32 payload_size;
    const u8* payload;
    s32 offset;
    u8 unused_frame_pad[8];
    const packed_seq_file_t* header;

    header = record;
    entry = record->entry;
    for (index = 0; index < 0x100; index++) {
        offset = entry[index][0] + (entry[index][1] << 8) + (entry[index][2] << 16) + (entry[index][3] << 24);
        if (offset == -1) {
            offset = 0;
        }
        entries[index] = g_sprite_seq_data_cursor + offset;
    }

    payload_size = header->payload_size[0] + (header->payload_size[1] << 8);
    index = 0;
    if (payload_size != 0) {
        payload = record->payload;
        do {
            g_sprite_seq_data_cursor[index] = *payload;
            index += 1;
            payload += 1;
        } while (index < payload_size);
    }
    g_sprite_seq_data_cursor += index;
}

/* Unpack a unit SHP file into its two frame tables.
 *
 * The file's first 32-bit value is the offset of its second frame set, the submerged
 * frames; 8 means it has only the set that starts there and both tables get
 * its frames. A set is 0x100 frame offsets, of which the first 0xd0 become
 * pointers into the frame data (-1 maps to its start), then the frame data:
 * a 16-bit count and that many bytes, appended at
 * g_sprite_shp_frame_data_cursor.
 */
static void unpack_unit_shp_data(sprite_unit_shp_frame_tables_t* dst, const unit_shp_file_t* src) {
    u32 i;
    u32 j;
    s32 offset;
    s32 value;
    u32 count;
    const u8* data;
    s32 header;

    /* Named table indexing removes the retail cursor and skip-loop instructions. */
    data = (const u8*)src;
    header = src->submerged_frame_set_offset[0] + (src->submerged_frame_set_offset[1] << 8)
        + (src->submerged_frame_set_offset[2] << 16) + (src->submerged_frame_set_offset[3] << 24);
    dst->first_attack_frame = src->first_attack_frame[0] + (src->first_attack_frame[1] << 8);
    dst->first_sp2_frame = src->first_sp2_frame[0] + (src->first_sp2_frame[1] << 8);
    offset = offsetof(unit_shp_file_t, primary_frame_set);
    for (i = 0; i < 0xd0; i++) {
        value = data[offset] + (data[offset + 1] << 8) + (data[offset + 2] << 16) + (data[offset + 3] << 24);
        if (value == -1) {
            value = 0;
        }
        dst->frames[i] = (sprite_frame_t*)(g_sprite_shp_frame_data_cursor + value);
        if (header == 8) {
            dst->submerged_frames[i] = (sprite_frame_t*)(g_sprite_shp_frame_data_cursor + value);
        }
        offset += 4;
    }
    for (i = 0xd0; i < 0x100; i++) {
        offset += 4;
    }
    count = data[offset] + (data[offset + 1] << 8);
    offset += 2;
    for (j = 0; j < count; j++) {
        g_sprite_shp_frame_data_cursor[j] = ((const u8*)src)[offset++];
    }
    g_sprite_shp_frame_data_cursor += j;
    if (header != 8) {
        offset = header;
        for (i = 0; i < 0xd0; i++) {
            value = data[offset] + (data[offset + 1] << 8) + (data[offset + 2] << 16) + (data[offset + 3] << 24);
            if (value == -1) {
                value = 0;
            }
            dst->submerged_frames[i] = (sprite_frame_t*)(g_sprite_shp_frame_data_cursor + value);
            offset += 4;
        }
        for (i = 0xd0; i < 0x100; i++) {
            offset += 4;
        }
        count = data[offset] + (data[offset + 1] << 8);
        offset += 2;
        for (j = 0; j < count; j++) {
            g_sprite_shp_frame_data_cursor[j] = ((const u8*)src)[offset++];
        }
        g_sprite_shp_frame_data_cursor += j;
    }
}

/*
 * Unpack a weapon or effect SHP file into dst: after four unread bytes, the
 * first frame of each of 32 weapon types, 0x200 frame offsets that become
 * pointers into the frame data (-1 maps to its start), and the frame data
 * itself, a 16-bit count and that many bytes, which is appended at
 * g_sprite_shp_frame_data_cursor.
 *
 * The header loops read through `data` while the blob copy indexes `src`;
 * using one name for both loses the target's separate copy of the argument.
 */
static void unpack_weapon_shp_data(sprite_weapon_shp_t* dst, const weapon_shp_file_t* src) {
    u32 i;
    u32 j;
    s32 offset;
    s32 value;
    u32 count;
    const u8* word;
    const u8* data;

    /* Named field indexing removes the retail running-offset instructions. */
    data = (const u8*)src;
    offset = offsetof(weapon_shp_file_t, first_frame);
    for (i = 0; i < 32; i++) {
        word = data + offset;
        dst->first_frame[i] = word[0] | (word[1] << 8);
        offset += 2;
    }
    for (i = 0; i < 0x200; i++) {
        word = data + offset;
        value = word[0] + (word[1] << 8) + (word[2] << 16) + (word[3] << 24);
        if (value == -1) {
            value = 0;
        }
        dst->frames[i] = (sprite_frame_t*)(g_sprite_shp_frame_data_cursor + value);
        offset += 4;
    }
    word = data + offset;
    offset += 2;
    count = word[0] + (word[1] << 8);
    for (j = 0; j < count; j++) {
        g_sprite_shp_frame_data_cursor[j] = ((const u8*)src)[offset++];
    }
    g_sprite_shp_frame_data_cursor += j;
}

/* Load a unit's SPR file into spritesheet VRAM slot `index`.
 *
 * The file's 16 palettes go to the slot's CLUT copy with the STP bit set on
 * colors 1..15 (color 0 stays transparent). The 256x256 4-bit sprite image
 * goes to VRAM x 0x100 + 64 * index, y 0x100, and the 256x32 portrait and
 * body strip to the SPRITE_SHEET_STRIP_VRAM_X/Y area (column
 * index / 8, row index % 8). The
 * compressed attack image is copied into the slot for
 * load_attack_image to expand later; its fixed length
 * overruns most files (QUIRKS.md). A null file loads nothing, and `unused`
 * and `unused_palette` are not read.
 *
 * Indexed byte copies preserve the original loop scheduling. Three RECT
 * locals preserve the 0x50-byte frame and its otherwise unused x/y stores. */
static void load_vram_sheet(
    const unit_spr_file_t* source, u32 index, s32 unused, map_unit_t* unit, s16 unused_palette) {
    const u8* data;
    u32 column;
    u8* clut_bytes;
    u8* compressed_attack_image;
    s32 byte_index;
    RECT unused_rect;
    RECT rect;
    RECT unused_rect2;

    /* Byte stores preserve the palette loop and shared image cursor. */
    data = (const u8*)source;
    column = index;
    clut_bytes = (u8*)g_sprite_sheet_slots[unit->spritesheet_vram_slot].clut[0];
    compressed_attack_image = g_sprite_sheet_slots[unit->spritesheet_vram_slot].compressed_attack_image;
    if (data != 0) {
        for (byte_index = 0; byte_index < 0x200; byte_index++) {
            if (byte_index % 2 != 0 && byte_index % 32 != 1) {
                clut_bytes[byte_index] = data[byte_index] | 0x80;
            } else {
                clut_bytes[byte_index] = data[byte_index];
            }
        }
        data += sizeof(source->clut);
        rect.x = column * 64 + 0x100;
        rect.y = 256;
        LoadTPage((u32*)data, 0, 0, rect.x, 0x100, 0x100, 0x100);
        rect.x = (column / 8) * SPRITE_SHEET_STRIP_WIDTH + SPRITE_SHEET_STRIP_VRAM_X;
        rect.y = (column % 8) * SPRITE_SHEET_STRIP_HEIGHT + SPRITE_SHEET_STRIP_VRAM_Y;
        LoadTPage((u32*)(data + sizeof(source->pixels)), 0, 0, rect.x, rect.y, 0x100, 0x20);
        for (byte_index = 0; byte_index < 0x30d4; byte_index++) {
            compressed_attack_image[byte_index]
                = data[byte_index + sizeof(source->pixels) + sizeof(source->portrait_and_body_pixels)];
        }
    }
}

/* Upload WEP.SPR's weapon, trail and particle images. Keep weapon and trail
 * CLUTs in their CPU tables; the particle CLUTs use the map palette manager. */
static void load_wep_spr_data(weapon_spr_file_t* archive) {
    s32 i;
    u8* cursor = (u8*)archive->weapon_clut;

    for (i = 0; i < (s32)sizeof(archive->weapon_clut); i++) {
        ((u8*)g_sprite_weapon_clut)[i] = *cursor++;
    }
    LoadTPage((u32*)archive->weapon_pixels, 0, 0, 0x240, 0, 0x100, 0x100);
    for (i = 0, cursor = (u8*)archive->trail_clut; i < (s32)sizeof(archive->trail_clut); i++) {
        ((u8*)g_sprite_trail_clut)[i] = *cursor++;
    }
    LoadTPage((u32*)archive->trail_pixels, 0, 0, 0x200, 0, 0x100, 0x100);
    map_palette_load_data(archive->particle_clut, MAP_CLUT_ROW_WEAPONS, 0, 1);
    LoadTPage((u32*)archive->particle_pixels, 0, 0, 0x200, 0x70, 0x100, 0x90);
}

/* Copy the first 21 OTHER.SPR palettes, setting STP on all but color zero. */
static void copy_other_spr_palettes(const other_spr_file_t* archive) {
    s32 i = 0;
    s32 one = 1; /* held in a local: a literal changes the setup schedule */
    u8* dst = (u8*)&g_sprite_other_palettes;
    /* Byte transfers preserve the palette loop and STP store sequence. */
    const u8* src = (const u8*)archive->palettes;
    do {
        u8 color_byte;
        if ((i & 1) && i % 32 != one) {
            color_byte = *src++ | 0x80;
        } else {
            color_byte = *src++;
        }
        *dst++ = color_byte;
        i++;
    } while (i < (s32)sizeof(g_sprite_other_palettes));
}

/* Create a unit record on a map tile: claim a map unit slot and a spritesheet VRAM slot (0 when
 * either is exhausted), load its sheet and palette, reset its motion, display and animation state,
 * set up its weapon and effect blocks and link it at the head of the unit list. A job sheet with
 * stats uses the stats' palette; palettes past 5 become 0, and nonzero `flags` starts the unit
 * hidden. */
static map_unit_t* init_map_unit(s32 map_x, s32 map_y, s32 map_level, s16 facing, s32 spritesheet_id, s16 palette,
    s16 requested_map_unit_id, battle_unit_t* battle_unit, u32 flags, const unit_spr_file_t* data) {
    map_unit_t* unit;
    union {
        animation_state_t* animation;
        sprite_block_t* block;
    } cursor;
    map_tile_t* tile;
    s32 map_unit_id;
    s32 slot;
    u16 id;
    s32 i;
    map_unit_t** head;
    u8 tile_x;
    u8 tile_y;
    u8 tile_layer;

    id = spritesheet_id;
    unit = claim_map_unit_slot(requested_map_unit_id);
    if (unit == 0) {
        return 0;
    }
    map_unit_id = unit->map_unit_id;
    unit->spritesheet_id = id;
    /* slot reads the claim back through the u8 field, so a failed claim
     * (0xffff) arrives as 0xff and this test never holds (QUIRKS.md). */
    slot = unit->spritesheet_vram_slot = claim_sheet_slot(spritesheet_id);
    if (slot == 0xffff) {
        g_map_unit_slots[unit->map_unit_id].in_use = 0;
        return 0;
    }

    if (id >= SPRITE_SHEET_ID_JOB_FIRST && id < SPRITE_SHEET_ID_CHOCOBO && battle_unit != 0) {
        palette = battle_unit->sprite_palette;
    }
    if (palette >= 6) {
        palette = 0;
    }
    unit->stored_palette = palette;

    unit->statuses_to_remove_5_6 = 0;
    unit->statuses_to_remove_1_4 = 0;
    unit->statuses_to_add_5_6 = 0;
    unit->statuses_to_add_1_4 = 0;
    unit->status_flags_5_6 = 0;
    unit->status_flags_1_4 = 0;

    load_vram_sheet(data, slot, map_unit_id, unit, palette);
    tile = map_get_tile(map_x, map_y, map_level);
    /* The tint runs before mount_state is reset below (QUIRKS.md). */
    sprite_palette_update(unit, tile->flags.shading, 1);
    map_palette_load_data(&g_sprite_shadow_palette, MAP_CLUT_ROW_UNIT_SHADOWS, map_unit_id, 0);
    unit->clut = GetClut(map_unit_id << 4, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_UNITS);
    unit->tpage = slot + SPRITE_TPAGE_SPRITESHEET_SLOT_0;

    if (flags == 0) {
        unit->draw_enabled = 1;
        unit->animation.trigger = 1;
    } else {
        unit->draw_enabled = 0;
        unit->animation.trigger = 0;
    }

    unit->standing_on_battle_unit_id_plus_one = 0;
    unit->battle_unit = battle_unit;
    unit->requested_animation = 0;
    unit->facing = facing * UNIT_FACING_QUARTER_TURN;
    unit->camera_facing_sixteenth = unit->camera_facing_quadrant = unit->attack_facing = -1;
    unit->mount_state = UNIT_MOUNT_STATE_NONE;
    unit->horizontal_flip_flag = 0;
    unit->depth_height_offset = 0;
    unit->float_bob_offset = 0;
    unit->float_bob_phase = 0;
    unit->step_phase = MOVE_STEP_PHASE_NONE;
    unit->path.count = 0;
    unit->sprite_display_flags = 0;
    unit->layer_priority = 0;
    unit->turn_action_flags = 0;
    unit->special_graphic_y_offset = 0;
    unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
    unit->distortion_phase = 0;

    tile_x = map_x;
    unit->map_x = tile_x;
    unit->movement.suppress_facing_update = 0;
    tile_y = map_y;
    unit->map_y = tile_y;
    unit->movement.suppress_palette_update = 0;
    tile_layer = map_level;
    unit->map_layer = tile_layer;
    unit->movement.alternate_short_hop = 0;
    unit->movement.floating = 0;
    unit->movement.force_animation_update = 0;
    unit->movement.destination_x = tile_x;
    unit->movement.destination_y = tile_y;
    unit->movement.destination_layer = tile_layer;
    map_unit_set_move_and_world_coords(unit);

    cursor.animation = &unit->animation;
    unit->velocity.vz = 0;
    unit->velocity.vy = 0;
    unit->velocity.vx = 0;
    unit->walk_speed = 0x2000;
    unit->seq_offset.vz = 0;
    unit->seq_offset.vy = 0;
    unit->seq_offset.vx = 0;
    unit->screen_offset.vz = 0;
    unit->screen_offset.vy = 0;
    unit->screen_offset.vx = 0;
    unit->event_offset.vz = 0;
    unit->event_offset.vy = 0;
    unit->event_offset.vx = 0;
    unit->fade_out_phase = UNIT_FADE_OUT_NONE;
    unit->shadow_graphic_trigger = 1;
    unit->shadow_dirty = 1;
    unit->shadow_counter = 0;

    unit->numeric_display.active = 0;
    unit->numeric_display.selector = RESULT_DISPLAY_HP_DAMAGE;
    unit->numeric_display.progress = 0;
    unit->numeric_display.displays[0] = sprite_display_init_numeric(map_unit_id, 0);
    unit->numeric_display.displays[1] = sprite_display_init_numeric(map_unit_id, 1);
    unit->numeric_display.displays[2] = sprite_display_init_numeric(map_unit_id, 2);
    unit->numeric_display.displays[0]->scale_x = unit->numeric_display.displays[0]->scale_y
        = unit->numeric_display.displays[1]->scale_x = unit->numeric_display.displays[1]->scale_y
        = unit->numeric_display.displays[2]->scale_x = unit->numeric_display.displays[2]->scale_y = ONE;
    unit->numeric_display.displays[0]->tpage = unit->numeric_display.displays[1]->tpage
        = unit->numeric_display.displays[2]->tpage = SPRITE_TPAGE_FRAME;
    unit->numeric_display.displays[0]->clut = unit->numeric_display.displays[1]->clut
        = unit->numeric_display.displays[2]->clut
        = unit->clut + 0x100; /* the unit's CLUT of MAP_CLUT_ROW_ACTION_RESULTS */
    unit->numeric_display.displays[0]->part_count = unit->numeric_display.displays[1]->part_count
        = unit->numeric_display.displays[2]->part_count = 1;

    unit->status_bubble_active = 0;
    unit->status_bubble_id = 0;
    unit->status_bubble_timer = 0;
    unit->status_bubble_display = sprite_display_init_status_bubble(map_unit_id);
    unit->item_display_active = 0;
    unit->item_display = sprite_display_init_item(map_unit_id);
    unit->position_copies_active = 0;

    unit->animation.display = sprite_display_init_unit(map_unit_id);
    cursor.animation->graphic_type = SPRITE_GRAPHIC_UNIT;
    unit->animation.shp = unit->animation.saved_shp = get_unit_shp(id);
    unit->animation.seq = unit->animation.saved_seq = get_unit_seq(id);
    cursor.animation->wait = 1;
    cursor.animation->restart_count = 0;
    cursor.animation->script_pos = 0;
    cursor.animation->sequence_id = ANIMATION_IDLE << 1;
    cursor.animation->frame = 0;
    cursor.animation->flags = 0;
    cursor.animation->wait_bias = 0;
    cursor.animation->frame_bias = 0;
    cursor.animation->repeat_count = 0;
    unit->animation.display->scale_x = ONE;
    unit->animation.display->scale_y = ONE;
    unit->animation.display->rotation = 0;
    animation_set_status(unit);
    run_unit_seq(unit, cursor.animation, 1);

    /* Block 0 draws the weapon sheet, blocks 1 and 2 the effect sheet. */
    for (i = 0; i < 3; i++) {
        cursor.block = &unit->sprite_blocks[i];
        cursor.block->display = sprite_display_init_weapon(map_unit_id, i);
        if (i == 0) {
            cursor.block->graphic_type = SPRITE_GRAPHIC_WEAPON;
            cursor.block->shp = get_weapon_shp(id);
            cursor.block->seq = get_weapon_seq(id);
        } else {
            cursor.block->graphic_type = SPRITE_GRAPHIC_EFFECT;
            cursor.block->shp = get_effect_shp(id);
            cursor.block->seq = get_effect_seq(id);
        }
        cursor.block->trigger = 0;
        cursor.block->wait = 1;
        cursor.block->script_pos = 0;
        cursor.block->sequence_id = 0;
        cursor.block->frame = 0;
        cursor.block->flags = 0;
        cursor.block->wait_bias = 0;
        cursor.block->frame_bias = 0;
        cursor.block->repeat_count = 0;
        cursor.block->display->scale_x = ONE;
        cursor.block->display->scale_y = ONE;
        cursor.block->display->rotation = 0;
        /* No id is both below VELIUS and from BOY up, so this never runs and the
         * blocks wait for their first sequence command. */
        if ((s16)id < SPRITE_SHEET_ID_CHOCOBO && (s16)id < SPRITE_SHEET_ID_VELIUS && (s16)id >= SPRITE_SHEET_ID_BOY
            && (s16)id != SPRITE_SHEET_ID_DAMI) {
            run_block_seq(unit, &unit->sprite_blocks[i], i, 1);
        }
    }

    head = &g_map_unit_list_head;
    unit->previous = *head;
    *head = unit;
    sprite_set_weapon_graphic(unit->battle_unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON], unit);
    status_init_display_flags(unit->battle_unit->battle_unit_id);
    /* Settle the new unit's display and pose without landing sounds or effects. */
    unit->movement.suppress_sfx_and_landing_effects = 1;
    map_unit_update_display(unit->map_unit_id);
    animation_set_status(unit);
    unit->movement.suppress_sfx_and_landing_effects = 0;
    return unit;
}

/* Reset the unit graphics for a new battle: empty the unit list, free the
 * map unit slots, the nine spritesheet slots and both EVTCHR slots (whose
 * image rectangles are 64x200 at 0x100,0 and 0x140,0), rewind the SEQ and SHP
 * data cursors and restart the shared-file loader.
 *
 * The two clearing loops walk their arrays by byte offset. A struct-subscript
 * walk leaves GCC with both the index and the scaled address as induction
 * variables, adding an `addiu`/`li` pair to each loop; the byte offset is what
 * reproduces the target's single `bgez` on the address offset. */
void sprite_reset_unit_graphics(void) {
    s32 i;
    s32 offset;

    g_map_unit_list_head = 0;
    for (offset = (MAP_UNIT_SLOT_COUNT - 1) * sizeof(map_unit_slot_t); offset >= 0; offset -= sizeof(map_unit_slot_t)) {
        ((map_unit_slot_t*)((u8*)g_map_unit_slots + offset))->in_use = 0;
    }
    for (offset = (SPRITE_SHEET_SLOT_COUNT - 1) * sizeof(sprite_sheet_slot_t); offset >= 0;
        offset -= sizeof(sprite_sheet_slot_t)) {
        ((sprite_sheet_slot_t*)((u8*)g_sprite_sheet_slots + offset))->in_use = 0;
    }
    for (i = 0; i < SPRITE_EVTCHR_SLOT_COUNT; i++) {
        g_sprite_evtchr_slots[i].owner = SPRITE_EVTCHR_SLOT_OWNER_NONE;
        g_sprite_evtchr_slots[i].image_buffer_state = SPRITE_EVTCHR_SLOT_BUFFER_FREE;
        g_sprite_evtchr_slots[i].evtchr_load_state = 0;
    }
    setRECT(&g_sprite_evtchr_slots[0].image_rect, 256, 0, 64, 200);
    setRECT(&g_sprite_evtchr_slots[1].image_rect, 320, 0, 64, 200);
    g_sprite_seq_data_cursor = g_sprite_seq_data_buffer;
    g_sprite_load_state.file_kind = SPRITE_LOAD_KIND_FIRST;
    g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
    g_sprite_load_state.index = 0;
    g_sprite_load_state.buffer = 0;
    g_sprite_shp_frame_data_cursor = g_sprite_shp_frame_data_buffer;
}

/* Advances the unit SHP/SEQ loader by one step and returns the file kind index.
 *
 * g_sprite_load_state.file_kind selects the file kind (sprite SHP/SEQ, WEP SHP/SEQ, EFF SHP/SEQ,
 * WEP.SPR, OTHER.SPR), g_sprite_load_state.phase the phase (0 allocate and start the read,
 * 1 wait, 2 copy into the RAM tables, 3 free and advance) and g_sprite_load_state.index the
 * file index within the kind.
 *
 * The target keeps several of these globals' addresses in registers; only a
 * single-assignment pointer local reproduces that (a plain `g++` folds %lo into
 * each access), and a block-scoped one per case also lets the scheduler hoist
 * the g_sprite_load_state.file_kind load above the counter store, as the target does. */
s32 sprite_update_shared_load(void) {
    s32* phase;
    s32* index;
    void** slot;
    void* buffer;

    phase = &g_sprite_load_state.phase;
    switch (*phase) {
    case SPRITE_LOAD_PHASE_START_READ:
        switch (g_sprite_load_state.file_kind) {
        case SPRITE_LOAD_SHARED_SPRITE_SHP:
            index = &g_sprite_load_state.index;
            /* A failed allocation or read leaves the phase unchanged, so the next step retries. */
            buffer = heap_alloc(s_shp_files[*index].size);
            g_sprite_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (file_request_read_bytes(s_shp_files[*index].sector, s_shp_files[*index].size, buffer) == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_release_block(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_SHARED_SPRITE_SEQ:
            index = &g_sprite_load_state.index;
            buffer = heap_alloc(s_seq_files[*index].size);
            g_sprite_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (file_request_read_bytes(s_seq_files[*index].sector, s_seq_files[*index].size, buffer) == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_release_block(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_SHARED_WEAPON_SHP:
            index = &g_sprite_load_state.index;
            buffer = heap_alloc(s_weapon_shp_files[*index].size);
            g_sprite_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (file_request_read_bytes(s_weapon_shp_files[*index].sector, s_weapon_shp_files[*index].size, buffer)
                == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_release_block(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_SHARED_WEAPON_SEQ:
            index = &g_sprite_load_state.index;
            buffer = heap_alloc(s_weapon_seq_files[*index].size);
            g_sprite_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (file_request_read_bytes(s_weapon_seq_files[*index].sector, s_weapon_seq_files[*index].size, buffer)
                == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_release_block(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_SHARED_EFFECT_SHP:
            index = &g_sprite_load_state.index;
            buffer = heap_alloc(s_effect_shp_files[*index].size);
            g_sprite_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (file_request_read_bytes(s_effect_shp_files[*index].sector, s_effect_shp_files[*index].size, buffer)
                == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_release_block(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_SHARED_EFFECT_SEQ:
            index = &g_sprite_load_state.index;
            buffer = heap_alloc(s_effect_seq_files[*index].size);
            g_sprite_load_state.buffer = buffer;
            if (buffer == 0) {
                break;
            }
            if (file_request_read_bytes(s_effect_seq_files[*index].sector, s_effect_seq_files[*index].size, buffer)
                == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_release_block(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_SHARED_WEAPON_SPR:
            *(slot = &g_sprite_load_state.buffer) = heap_alloc(0x15000);
            if (*slot == 0) {
                break;
            }
            if (file_request_read_bytes(DISC_SECTOR_BATTLE_WEP_SPR, 0x15000, *slot) == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_release_block(*slot);
            }
            break;
        case SPRITE_LOAD_SHARED_OTHER_SPR:
            *(slot = &g_sprite_load_state.buffer) = heap_alloc(0x8800);
            if (*slot == 0) {
                break;
            }
            if (file_request_read_bytes(DISC_SECTOR_BATTLE_OTHER_SPR, 0x8800, *slot) == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_release_block(*slot);
            }
            break;
        }
        break;
    case SPRITE_LOAD_PHASE_WAIT:
        if (file_get_load_state() != 0) {
            break;
        }
        (*phase)++;
        break;
    case SPRITE_LOAD_PHASE_INSTALL:
        switch (g_sprite_load_state.file_kind) {
        case SPRITE_LOAD_SHARED_SPRITE_SHP:
            unpack_unit_shp_data(
                &g_sprite_unit_shp_frame_tables[g_sprite_load_state.index], g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_SHARED_SPRITE_SEQ:
            unpack_unit_seq_data(&g_sprite_unit_seq_tables[g_sprite_load_state.index], g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_SHARED_WEAPON_SHP:
            unpack_weapon_shp_data(
                &g_sprite_weapon_graphics[g_sprite_load_state.index].shp, g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_SHARED_WEAPON_SEQ:
            unpack_weapon_seq_data(g_sprite_weapon_graphics[g_sprite_load_state.index].seq, g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_SHARED_EFFECT_SHP:
            unpack_weapon_shp_data(
                &g_sprite_effect_graphics[g_sprite_load_state.index].shp, g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_SHARED_EFFECT_SEQ:
            unpack_weapon_seq_data(g_sprite_effect_graphics[g_sprite_load_state.index].seq, g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_SHARED_WEAPON_SPR:
            load_wep_spr_data(g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_SHARED_OTHER_SPR:
            copy_other_spr_palettes(g_sprite_load_state.buffer);
            break;
        }
        {
            s32* next_phase = &g_sprite_load_state.phase;

            (*next_phase)++;
        }
        break;
    case SPRITE_LOAD_PHASE_FINISH:
        heap_release_block(g_sprite_load_state.buffer);
        *phase = SPRITE_LOAD_PHASE_START_READ;
        /* Five sprite SHP and SEQ files, two weapon, one effect, then each SPR file once. */
        switch (g_sprite_load_state.file_kind) {
        case SPRITE_LOAD_SHARED_SPRITE_SHP: {
            s32* counter = &g_sprite_load_state.index;

            if (++*counter >= 5) {
                *counter = 0;
                g_sprite_load_state.file_kind++;
            }
            break;
        }
        case SPRITE_LOAD_SHARED_SPRITE_SEQ: {
            s32* counter = &g_sprite_load_state.index;

            if (++*counter >= 5) {
                *counter = 0;
                g_sprite_load_state.file_kind++;
            }
            break;
        }
        case SPRITE_LOAD_SHARED_WEAPON_SHP:
        case SPRITE_LOAD_SHARED_WEAPON_SEQ: {
            s32* counter = &g_sprite_load_state.index;

            if (++*counter >= 2) {
                *counter = 0;
                g_sprite_load_state.file_kind++;
            }
            break;
        }
        case SPRITE_LOAD_SHARED_EFFECT_SHP:
        case SPRITE_LOAD_SHARED_EFFECT_SEQ: {
            s32* counter = &g_sprite_load_state.index;

            if (++*counter >= 1) {
                *counter = 0;
                g_sprite_load_state.file_kind++;
            }
            break;
        }
        case SPRITE_LOAD_SHARED_WEAPON_SPR:
        case SPRITE_LOAD_SHARED_OTHER_SPR: {
            s32* kind = &g_sprite_load_state.file_kind;

            g_sprite_load_state.index = 0;
            (*kind)++;
            break;
        }
        }
        break;
    }

    return g_sprite_load_state.file_kind;
}

/* Run the per-frame unit update three times; the queued graphics loader calls it right after it
 * creates a unit. */
static void run_initial_frames(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        map_unit_update_animations();
    }
}

/* Rewinds the unit graphics loader (g_sprite_load_state) to its
 * first file kind, the start-read phase, index 0 and no buffer. The argument is
 * not read. */
void sprite_clear_load_state(s32 unused_map_id) {
    g_sprite_load_state.file_kind = SPRITE_LOAD_KIND_FIRST;
    g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
    g_sprite_load_state.index = 0;
    g_sprite_load_state.buffer = 0;
}

/* Builds the deployed units' data when a map starts and rewinds the unit graphics
 * loader.
 *
 * A battle whose deployment screen is skipped places the units from
 * g_deploy_deployed_unit_map_coordinates; otherwise the coordinate slots of the
 * deployment screen are used, and in the debug battle style the red team is set
 * up from them first. */
void deploy_init_placed_units(void) {
    g_unit_graphics_load_queue.count = 0;
    if (script_variable_get(SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) == 0) {
        if (g_deploy_skipped != 0) {
            deploy_init_units(g_deploy_deployed_unit_map_coordinates, 0);
            g_sprite_load_state.file_kind = SPRITE_LOAD_KIND_FIRST;
            g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
            g_sprite_load_state.index = 0;
            g_sprite_load_state.buffer = 0;
            return;
        }
    } else {
        deploy_init_debug_red_team_units(deploy_get_coordinate_slots(), 0);
    }
    deploy_init_units(deploy_get_coordinate_slots(), 0);
    g_sprite_load_state.file_kind = SPRITE_LOAD_KIND_FIRST;
    g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
    g_sprite_load_state.index = 0;
    g_sprite_load_state.buffer = 0;
}

/* Steps the per-unit graphics loader for the queued load descriptors.
 *
 * g_sprite_load_state holds the file kind (0 SHP, 1 SEQ, 2 SPR), the
 * phase (0 read, 1 wait, 2 install, 3 finish), the descriptor index and the
 * heap buffer. Shared SHP/SEQ ids or the last requested id skip the read, as
 * does an already loaded spritesheet. Returns 0 once every
 * descriptor is done, otherwise 1 inside a step and 2 after a step boundary. */
s32 sprite_update_queued_load(void) {
    s32 result = 1;
    s32 id;

    switch (g_sprite_load_state.phase) {
    case SPRITE_LOAD_PHASE_START_READ:
        switch (g_sprite_load_state.file_kind) {
        case SPRITE_LOAD_QUEUED_SHP:
            id = g_sprite_sheet_data[g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id]
                     .shp_id;
            /* SHP ids up to OTHER are the shared files the startup loader keeps resident. */
            if (id <= SPRITE_SHP_OTHER || id == s_sprite_last_requested_shp_id) {
                result = 2;
                g_sprite_load_state.buffer = 0;
                g_sprite_load_state.phase = SPRITE_LOAD_PHASE_FINISH;
                break;
            }

            s_sprite_last_requested_shp_id = id;
            g_sprite_load_state.buffer = heap_alloc(s_shp_files[id].size);
            if (g_sprite_load_state.buffer == 0) {
                break;
            }
            if (file_request_read_bytes(s_shp_files[id].sector, s_shp_files[id].size, g_sprite_load_state.buffer)
                == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_release_block(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_QUEUED_SEQ:
            id = g_sprite_sheet_data[g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id]
                     .seq_id;
            /* SEQ ids up to OTHER are likewise resident. */
            if (id < SPRITE_SEQ_RUKA || id == s_sprite_last_requested_seq_id) {
                result = 2;
                g_sprite_load_state.buffer = 0;
                g_sprite_load_state.phase = SPRITE_LOAD_PHASE_FINISH;
                break;
            }

            s_sprite_last_requested_seq_id = id;
            g_sprite_load_state.buffer = heap_alloc(s_seq_files[id].size);
            if (g_sprite_load_state.buffer == 0) {
                break;
            }
            if (file_request_read_bytes(s_seq_files[id].sector, s_seq_files[id].size, g_sprite_load_state.buffer)
                == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_release_block(g_sprite_load_state.buffer);
            }
            break;
        case SPRITE_LOAD_QUEUED_SPR:
            /* Ids from 0xa0 up load Ramza's chapter 1 sheet instead; id 0x9f also lies past the
             * 159-entry table but slips through (QUIRKS.md). */
            if (g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id >= 0xa0) {
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id
                    = SPRITE_SHEET_ID_RAMZA_CHAPTER_1;
            }

            /* A sheet already in VRAM skips the read but still goes to install, which creates the unit. */
            if (is_sheet_loaded(g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id)
                != 0) {
                g_sprite_load_state.buffer = 0;
                g_sprite_load_state.phase = SPRITE_LOAD_PHASE_INSTALL;
                result = 2;
                break;
            }

            g_sprite_load_state.buffer = heap_alloc(
                s_sheet_files[g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id].size);
            if (g_sprite_load_state.buffer == 0) {
                break;
            }
            if (file_request_read_bytes(
                    s_sheet_files[g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id]
                        .sector,
                    s_sheet_files[g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id]
                        .size,
                    g_sprite_load_state.buffer)
                == 0) {
                g_sprite_load_state.phase++;
            } else {
                heap_release_block(g_sprite_load_state.buffer);
            }
            break;
        }
        break;
    case SPRITE_LOAD_PHASE_WAIT:
        if (!file_get_load_state()) {
            g_sprite_load_state.phase++;
        }
        break;
    case SPRITE_LOAD_PHASE_INSTALL:
        switch (g_sprite_load_state.file_kind) {
        case SPRITE_LOAD_QUEUED_SHP:
            /* Queued SHP and SEQ data share one boss table each, so each load replaces the last. */
            g_sprite_shp_frame_data_cursor = g_sprite_fallback_shp_frame_data;
            unpack_unit_shp_data(&g_sprite_boss_shp_frame_tables, g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_QUEUED_SEQ:
            g_sprite_seq_data_cursor = g_sprite_fallback_seq_data;
            unpack_unit_seq_data(&g_sprite_boss_seq_tables, g_sprite_load_state.buffer);
            break;
        case SPRITE_LOAD_QUEUED_SPR:
            init_map_unit(g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].map_x,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].map_y,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].map_level,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].facing,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].spritesheet_id,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].palette_id,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].map_unit_id,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].battle_unit,
                g_unit_graphics_load_queue.descriptors[g_sprite_load_state.index].flags, g_sprite_load_state.buffer);
            run_initial_frames();
            break;
        }
        g_sprite_load_state.phase++;
        break;
    case SPRITE_LOAD_PHASE_FINISH:
        if (g_sprite_load_state.buffer != 0) {
            heap_release_block(g_sprite_load_state.buffer);
        }
        g_sprite_load_state.phase = SPRITE_LOAD_PHASE_START_READ;
        switch (g_sprite_load_state.file_kind) {
        case SPRITE_LOAD_QUEUED_SHP:
        case SPRITE_LOAD_QUEUED_SEQ:
            g_sprite_load_state.file_kind++;
            break;
        case SPRITE_LOAD_QUEUED_SPR:
            g_sprite_load_state.file_kind = SPRITE_LOAD_QUEUED_SHP;
            g_sprite_load_state.index++;
            break;
        }
        result = 2;
        break;
    }

    return (g_sprite_load_state.index < g_unit_graphics_load_queue.count) ? result : 0;
}

/* cursor */

/* Palette cycling for the cursor tile glow. */
extern palette_t g_cursor_tile_glow_cycled_palettes[2];
extern s32 g_cursor_tile_glow_cycle_offset;
extern s32 g_cursor_tile_glow_cycle_timer;

/* Cycles the colors of the two cursor tile glow palettes (sprite CLUTs 1 and 2).
 *
 * Every fourth animation tick the offset advances through 0..14 and the palettes
 * are uploaded again with colors 1..15 rotated down by that many places; color
 * 0 stays in place, and offset 0 uploads the palettes as stored. */
static void update_tile_glow_palettes(void) {
    s32 dst;
    s32 src;
    palette_t* palettes;
    RECT rect; /* never used; retail reserves its 8 bytes */

    g_cursor_tile_glow_cycle_timer += g_system_vsync_interval;
    if (g_cursor_tile_glow_cycle_timer >= 4) {
        g_cursor_tile_glow_cycle_timer = 0;
        g_cursor_tile_glow_cycle_offset++;
        if (g_cursor_tile_glow_cycle_offset >= 15) {
            g_cursor_tile_glow_cycle_offset = 0;
        }
        if (g_cursor_tile_glow_cycle_offset != 0) {
            /* Bytewise rotation retains the native stores and first-color 16-bit load. */
            /* dst before src: the reverse order changes the copy loop registers. */
            dst = 2;
            src = g_cursor_tile_glow_cycle_offset * 2 + 2;
            ((u8*)g_cursor_tile_glow_cycled_palettes[0])[0] = ((u8*)g_cursor_tile_glow_palettes)[0];
            ((u8*)g_cursor_tile_glow_cycled_palettes[0])[1] = ((u8*)g_cursor_tile_glow_palettes)[1];
            ((u8*)g_cursor_tile_glow_cycled_palettes[1])[0] = *(u16*)&g_cursor_tile_glow_palettes[1][0];
            ((u8*)g_cursor_tile_glow_cycled_palettes[1])[1] = ((u8*)g_cursor_tile_glow_palettes[1])[1];
            /* Colors offset + 1..15 move down to 1.., and colors 1..offset wrap to the top. */
            for (; src < 32; src++, dst++) {
                ((u8*)g_cursor_tile_glow_cycled_palettes[0])[dst] = ((u8*)g_cursor_tile_glow_palettes)[src];
                ((u8*)g_cursor_tile_glow_cycled_palettes[1])[dst] = ((u8*)g_cursor_tile_glow_palettes[1])[src];
            }
            for (src = 2, dst = 32 - g_cursor_tile_glow_cycle_offset * 2; src < g_cursor_tile_glow_cycle_offset * 2 + 2;
                src++, dst++) {
                ((u8*)g_cursor_tile_glow_cycled_palettes[0])[dst] = ((u8*)g_cursor_tile_glow_palettes)[src];
                ((u8*)g_cursor_tile_glow_cycled_palettes[1])[dst] = ((u8*)g_cursor_tile_glow_palettes[1])[src];
            }
            palettes = g_cursor_tile_glow_cycled_palettes;
        } else {
            palettes = g_cursor_tile_glow_palettes;
        }
        map_palette_load_data(palettes, MAP_CLUT_ROW_SPRITES, 1, 0);
        map_palette_load_data(palettes + 1, MAP_CLUT_ROW_SPRITES, 2, 0);
    }
}

/* Project the center of the cursor tile, at its surface height less any water depth, to the screen
 * and store the cursor's screen x and y. `position` receives the tile point and `raised` its depth
 * point. It first cycles the tile glow palettes. */
void cursor_project_tile(VECTOR* projected, SVECTOR* position, SVECTOR* raised) {
    map_tile_t* tile;
    s32 flag;

    update_tile_glow_palettes();
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    vector_set_short(position, g_cursor_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2), 0,
        g_cursor_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
    position->vy = map_calculate_surface_height(position, (u8)g_cursor_layer);
    tile = map_get_tile(position->vx / MAP_TILE_SIZE, position->vz / MAP_TILE_SIZE, g_cursor_layer);
    position->vy -= tile->depth_half_height.depth * 12;
    if (tile->surface.depth_bias) {
        camera_get_biased_tile_depth_point(position, raised);
    } else {
        camera_get_unbiased_tile_depth_point(position, raised);
    }
    RotTrans(raised, projected, &flag);
    g_cursor_screen_x = projected->vx;
    g_cursor_screen_y = projected->vy;
}

/* Project the cursor tile and draw the map cursor over it with the glow quad on the tile surface;
 * update_cursor calls it while g_cursor_visible is set. The tile's
 * untargetable flag picks the glow's palette. */
void cursor_draw(void) {
    SVECTOR position;
    SVECTOR raised;
    SVECTOR corner0;
    SVECTOR corner1;
    SVECTOR corner2;
    SVECTOR corner3;
    VECTOR projected;
    s32 depth;
    s32 shape;
    map_tile_t* tile;

    cursor_project_tile(&projected, &position, &raised);
    /* The projected z is the ordering-table depth for the cursor glow. */
    depth = projected.vz / 4;
    draw_map_selection_cursor(&position, gfx_get_battle_otag());
    /* The polygon routine reads the same three 16-bit values in renderer x, z, y order. */
    shape = cursor_calculate_tile_polygon(&position, g_cursor_layer, &corner0);
    tile = map_get_tile(position.vx / MAP_TILE_SIZE, position.vz / MAP_TILE_SIZE, g_cursor_layer);
    build_tile_glow(
        tile->flags.untargetable, shape, &corner0, &corner1, &corner2, &corner3, gfx_get_battle_otag() + depth);
}

/*
 * Queue the acting unit's wait-direction indicator into the ordering table at
 * the depth of a point 48 screen units above its feet.
 */
void map_unit_facing_draw_wait_direction(void) {
    SVECTOR base;
    SVECTOR raised;
    VECTOR transformed;
    s32 flag;
    map_unit_t* unit;
    s32 depth;
    u32* ot;

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    unit = map_unit_get_source();
    vector_set_short(&base, unit->world_coords.vx, unit->world_coords.vy, unit->world_coords.vz);
    /* The second RotTrans overwrites this result unread. */
    RotTrans(&base, &transformed, &flag);
    camera_get_unbiased_tile_depth_point(&unit->world_coords, &raised);
    raised.vy -= 48;
    RotTrans(&raised, &transformed, &flag);
    depth = transformed.vz / 4;
    ot = gfx_get_battle_otag();
    draw_wait_arrows(&base, &ot[depth]);
}

/* unit motion */

/* Phases of the teleport distortion; other distortions reuse the phase field separately. */
typedef enum teleport_phase {
    ANIMATION_TELEPORT_PHASE_START_DEPARTURE = 0,
    ANIMATION_TELEPORT_PHASE_WAIT_DEPARTURE_LOAD = 1,
    ANIMATION_TELEPORT_PHASE_HOLD_DEPARTURE_FLASH = 2,
    ANIMATION_TELEPORT_PHASE_MOVE_TO_DESTINATION = 3,
    ANIMATION_TELEPORT_PHASE_WAIT_ARRIVAL_LOAD = 4,
    ANIMATION_TELEPORT_PHASE_HOLD_ARRIVAL_DARK = 5,
    ANIMATION_TELEPORT_PHASE_WAIT_ARRIVAL_FADE = 6,
    ANIMATION_TELEPORT_PHASE_WAIT_ARRIVAL_END = 7
} teleport_phase_e;

/* Starts a unit's fade-out removal with a color flash.
 *
 * The unit moves to the head of the map unit list, enters UNIT_FADE_OUT_FLASH,
 * loses its shadow and its float and depth offsets, and is drawn with additive
 * blending while its palette fades by (red, green, blue) from the last applied
 * color. `fade_step` is the amount map_palette_modify steps the fade by.
 * update_fade_out_removal continues from there. */
void map_unit_start_fade_out(s32 map_unit_id, s32 fade_step, s16 red, s16 green, s16 blue) {
    map_unit_t* unit = map_unit_get(map_unit_id);

    if (unit != 0) {
        map_unit_move_to_list_head(map_unit_id);
        unit->fade_out_phase = UNIT_FADE_OUT_FLASH;
        unit->shadow_graphic_trigger = 0;
        unit->sprite_display_flags = (unit->sprite_display_flags & ~SPRITE_DISPLAY_BLEND_MASK)
            | (SPRITE_DISPLAY_BLEND_ADD | SPRITE_DISPLAY_SEMI_TRANSPARENT);
        sprite_palette_apply_modulation_to_units(MAP_COLOR_MODE_BASE, fade_step, map_unit_id, red, green, blue);
        unit->float_bob_offset = 0;
        unit->depth_height_offset = 0;
    }
}

/* Start a unit's fade-out removal with a red flash: fade step 4 and a red offset
 * of 0x1f. */
void map_unit_start_red_fade_out(s32 map_unit_id) {
    map_unit_start_fade_out(map_unit_id, 4, 0x1F, 0, 0);
}

/* Sets the 20.12 fixed-point position from the integer world position. */
static void set_real_coords_from_world_coords(map_unit_t* unit) {
    unit->real.vx = unit->world_coords.vx * ONE;
    unit->real.vy = unit->world_coords.vy * ONE;
    unit->real.vz = unit->world_coords.vz * ONE;
}

/* Sets the integer world position from the 20.12 fixed-point position. */
void map_unit_set_world_coords_from_real_coords(map_unit_t* unit) {
    unit->world_coords.vx = unit->real.vx / ONE;
    unit->world_coords.vy = unit->real.vy / ONE;
    unit->world_coords.vz = unit->real.vz / ONE;
}

/* Convert map-tile coordinates to tile-centered Q12 world coordinates.
 *
 * Vertical position is derived separately from the unit's terrain and mount
 * state. The stack-backed SVECTOR preserves the target's narrow stores. */
static void set_real_coords_from_map_coords(map_unit_t* unit) {
    SVECTOR tile_center;

    tile_center.vx = unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit->real.vx = tile_center.vx * ONE;
    tile_center.vz = unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit->real.vz = tile_center.vz * ONE;
    unit->real.vy = map_unit_calculate_height_from_tile(unit) * ONE;
}

/* Entry 0 of s_distortion_handlers. Never called: id 0 means no distortion
 * animation and dispatch_distortion skips it. */
static s32 distortion_none(map_unit_t* unused_unit) {
}

/* Distortion animation 1 (ANIMATION_DISTORTION_SCALE_PULSE). Over 40 frames the sprite's width follows sin(timer) as
 * the timer runs from 0x300 to 0x800, shrinking it to nothing, while its height swells to 2.5 times and back (1 + 1.5
 * sin(angle) for an angle stepping 51 per frame); then both scales return to 1.
 *
 * An ignored s32 result keeps $v0 live at the exit and the dispatch branch's delay
 * slot empty. This callback supplies no return value. */
static s32 distortion_scale_pulse_update(map_unit_t* unit) {
    s32 angle;
    s32 stretch;

    switch (unit->distortion_phase) {
    case 0:
        unit->distortion_phase = 1;
        unit->distortion_timer = 0x300;
        unit->distortion_target = 0;
    case 1:
        angle = unit->distortion_target;
        unit->animation.display->scale_x = rsin(unit->distortion_timer);
        stretch = rsin(angle);
        unit->animation.display->scale_y = stretch + stretch / 2 + ONE;
        if ((unit->distortion_timer & 0xfff) != 0x800) {
            unit->distortion_timer += 32;
            unit->distortion_target += 51;
        } else {
            unit->distortion_phase++;
        }
        break;
    case 2:
        unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        unit->animation.display->scale_x = ONE;
        unit->animation.display->scale_y = ONE;
        break;
    }
}

/* Distortion animation 0x02: raise the unit by 0x24000 (36 world units) in equal
 * steps over distortion_timer frames.
 *
 * Phase 0 replaces the frame count in distortion_timer with the per-frame step
 * and stores the destination height in distortion_target; phase 1 steps until the
 * unit reaches it, then snaps and ends the animation. */
static s32 distortion_rise(map_unit_t* unit) {
    s32 step = unit->distortion_timer;
    s32 target = unit->distortion_target;

    switch (unit->distortion_phase) {
    case 0:
        step = -0x24000 / step;
        target = unit->real.vy - 0x24000;
        unit->distortion_phase = 1;
    case 1:
        if (target < unit->real.vy) {
            unit->real.vy += step;
        } else {
            unit->real.vy = target;
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        unit->world_coords.vy = unit->real.vy / ONE;
        break;
    }
    unit->distortion_timer = step;
    unit->distortion_target = target;
}

/* Distortion animation 0x03: lower the unit to the ground height under it in
 * equal steps over distortion_timer frames.
 *
 * Phase 0 stores the ground height (Q12) in distortion_target and replaces the
 * frame count in distortion_timer with the per-frame step; phase 1 steps while
 * the unit is above the ground, then snaps and ends the animation. A unit that
 * starts at or below the ground snaps on the first frame. */
static s32 distortion_settle(map_unit_t* unit) {
    s32 step = unit->distortion_timer;
    s32 ground = unit->distortion_target;

    switch (unit->distortion_phase) {
    case 0:
        ground = map_unit_calculate_height_from_world_coords(unit) * ONE;
        step = (ground - (unit->world_coords.vy * ONE)) / step;
        unit->distortion_phase++;
    case 1:
        if (unit->real.vy < ground) {
            unit->real.vy += step;
        } else {
            unit->real.vy = ground;
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        unit->world_coords.vy = unit->real.vy / ONE;
        break;
    }
    unit->distortion_timer = step;
    unit->distortion_target = ground;
}

/* Moves a unit toward its action target over distortion_timer frames.
 *
 * Phase 0 picks the destination and splits the remaining distance into equal
 * Q12 steps: during an event the unit's own map tile, else the first entry of
 * the target list, else the recorded action target panel.  Phase 1 applies the
 * steps (height only when allow_height_change is set). */
static void step_toward_target(map_unit_t* unit, s32 allow_height_change) {
    SVECTOR tile_center;
    VECTOR velocity;
    s32 timer = unit->distortion_timer;
    s32 saved_target = unit->distortion_target;
    map_unit_t* target;

    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->battle_unit != 0) {
            if (g_battle_state == BATTLE_STATE_EVENT) {
                velocity.vx = ((unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2)) * ONE) - unit->real.vx;
                velocity.vz = ((unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2)) * ONE) - unit->real.vz;
                velocity.vy = 0;
            } else if (unit->strike.target_count != 0) {
                target = map_unit_get_by_battle_unit_id(unit->strike.target_list[0]);
                setVector(&velocity, target->real.vx - unit->real.vx, target->real.vy - unit->real.vy,
                    target->real.vz - unit->real.vz);
            } else {
                velocity.vx = (unit->battle_unit->action_command.target_position.vx * MAP_TILE_SIZE
                                  + (MAP_TILE_SIZE / 2) - unit->world_coords.vx)
                    * ONE;
                velocity.vz = (unit->battle_unit->action_command.target_position.vz * MAP_TILE_SIZE
                                  + (MAP_TILE_SIZE / 2) - unit->world_coords.vz)
                    * ONE;
                tile_center.vx
                    = unit->battle_unit->action_command.target_position.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                tile_center.vz
                    = unit->battle_unit->action_command.target_position.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                velocity.vy
                    = (map_calculate_surface_height(&tile_center, unit->battle_unit->action_command.target_position.vy)
                          - unit->world_coords.vy)
                    * ONE;
            }
            velocity.vx /= timer;
            velocity.vy /= timer;
            velocity.vz /= timer;
            unit->distortion_phase++;
        } else {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
            break;
        }
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            if (allow_height_change) {
                unit->real.vy += velocity.vy;
            }
            unit->real.vz += velocity.vz;
        } else {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    map_unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
    unit->distortion_target = saved_target;
}

/* Moves a unit to the center of its map tile over distortion_timer frames.
 *
 * Phase 0 splits the distance from its world position to the tile center into equal Q12 steps;
 * phase 1 applies them (height only when allow_height_change is set) and
 * snaps to the tile once the timer runs out. */
static void step_to_own_tile(map_unit_t* unit, s32 allow_height_change) {
    SVECTOR tile_center;
    VECTOR velocity;
    s32 timer = unit->distortion_timer;
    s32 saved_target = unit->distortion_target;

    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        velocity.vx = (unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2) - unit->world_coords.vx) * ONE;
        velocity.vz = (unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2) - unit->world_coords.vz) * ONE;
        tile_center.vx = unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        tile_center.vz = unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        velocity.vy = (map_calculate_surface_height(&tile_center, unit->map_layer) - unit->world_coords.vy) * ONE;
        velocity.vx /= timer;
        velocity.vy /= timer;
        velocity.vz /= timer;
        unit->distortion_phase++;
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            if (allow_height_change) {
                unit->real.vy += velocity.vy;
            }
            unit->real.vz += velocity.vz;
        } else {
            set_real_coords_from_map_coords(unit);
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    map_unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
    unit->distortion_target = saved_target;
}

/* Distortion animation 0x04: move toward the action target in equal steps,
 * height included (step_toward_target). */
static s32 distortion_move_to_target(map_unit_t* unit) {
    step_toward_target(unit, 1);
}

/* Distortion animation 0x05: move to the center of the unit's map tile in equal
 * steps, height included (step_to_own_tile). */
static s32 distortion_move_to_own_tile(map_unit_t* unit) {
    step_to_own_tile(unit, 1);
}

/* Distortion animation 0x08: move toward the action target in equal steps without
 * changing height (step_toward_target). */
static s32 distortion_move_level_to_target(map_unit_t* unit) {
    step_toward_target(unit, 0);
}

/* Distortion animation 0x09: move to the center of the unit's map tile without
 * changing height. */
static s32 distortion_move_level_to_own_tile(map_unit_t* unit) {
    step_to_own_tile(unit, 0);
}

/* Velocity and duration of a jump arc from `from` to `to`.
 *
 * The arc has two legs under g_move_jump_gravity, each taking
 * sqrt(2 * gravity * height) / gravity frames: one over the height difference
 * plus 24, one over 24. `out->vy` is the negated speed of the first leg; x and
 * y (stored in vz) cover their distances evenly over the returned frame count. */
static s32 calculate_jump_arc_velocity(const SVECTOR* from, const SVECTOR* to, VECTOR* out) {
    s32 to_height = to->vy;
    s32 from_height = from->vy;
    s32 height = from_height - to_height + 24;
    s32 speed;
    s32 frames;

    height = height * g_move_jump_gravity;
    speed = SquareRoot12(height * 2);
    frames = speed / g_move_jump_gravity;
    out->vy = -speed;
    /* The second leg reloads height through 24 first; folding the constant compiles shorter. */
    height = 24;
    height = g_move_jump_gravity * height;
    speed = SquareRoot12(height * 2);
    frames += speed / g_move_jump_gravity;
    out->vx = ((to->vx - from->vx) * ONE) / frames;
    out->vz = ((to->vz - from->vz) * ONE) / frames;
    return frames;
}

/* Distortion animation 0x06: jump along an arc toward the first target's sprite
 * center (its world position raised by half its spritesheet height), or to the
 * action target tile when the unit has no targets.
 *
 * Phase 0 builds the target point and asks
 * calculate_jump_arc_velocity for the velocity and frame count; a
 * unit without battle data ends the animation instead. Phase 1 applies the
 * velocity to all three real coordinates for that many frames, the height speed
 * gaining g_move_jump_gravity each frame, and ends the animation where the
 * arc leaves the unit. */
static s32 distortion_jump_to_target(map_unit_t* unit) {
    SVECTOR target;
    /* The retail frame reserves eight bytes beyond the target vector. */
    u8 unused_target_tail[8];
    VECTOR velocity;
    map_unit_t* other;
    s32 timer;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->battle_unit != 0) {
            if (unit->strike.target_count != 0) {
                other = map_unit_get_by_battle_unit_id(unit->strike.target_list[0]);
                setVector(&target, other->world_coords.vx,
                    other->world_coords.vy - sprite_sheet_map_unit_get_height(other->map_unit_id) / 2,
                    other->world_coords.vz);
            } else {
                target.vx = unit->battle_unit->action_command.target_position.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                target.vz = unit->battle_unit->action_command.target_position.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                target.vy = map_calculate_surface_height(&target, unit->battle_unit->action_command.target_position.vy);
            }
            timer = calculate_jump_arc_velocity(&unit->world_coords, &target, &velocity);
            unit->distortion_phase++;
        } else {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
            break;
        }
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            unit->real.vy += velocity.vy;
            unit->real.vz += velocity.vz;
            velocity.vy += g_move_jump_gravity;
        } else {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    map_unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}

/* Distortion animation 0x07: jump along an arc to the center of the unit's own
 * map tile.
 *
 * Phase 0 asks calculate_jump_arc_velocity for the velocity and frame
 * count from the current world position to the tile center at ground height.
 * Phase 1 applies the velocity for that many frames, the height speed gaining
 * g_move_jump_gravity each frame, then stops the horizontal motion. Phase
 * 2 keeps falling until the unit reaches the ground under it, snaps it to the
 * tile and ends the animation. */
static s32 distortion_jump_to_own_tile(map_unit_t* unit) {
    /* Unreferenced 8-byte frame slot below the target point, like the one in
     * distortion_jump_takeoff; it puts `target` at sp+0x18. */
    SVECTOR unused_10;
    SVECTOR target;
    VECTOR velocity;
    s32 timer;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        target.vx = unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        target.vz = unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        target.vy = map_unit_calculate_height_from_tile(unit);
        timer = calculate_jump_arc_velocity(&unit->world_coords, &target, &velocity);
        unit->distortion_phase++;
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            unit->real.vy += velocity.vy;
            unit->real.vz += velocity.vz;
            velocity.vy += g_move_jump_gravity;
        } else {
            velocity.vz = 0;
            velocity.vx = 0;
            unit->distortion_phase++;
        }
        timer--;
        break;
    case 2:
        vector_set_short(&target, unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2), 0,
            unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
        target.vy = map_unit_calculate_height_from_world_coords(unit);
        if (unit->world_coords.vy < target.vy) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_move_jump_gravity;
        } else {
            set_real_coords_from_map_coords(unit);
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        break;
    }
    unit->velocity = velocity;
    map_unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}

/* Distortion animation 10 of s_distortion_handlers: ends at once. Animation 11
 * is a second copy of this routine. */
static s32 distortion_clear(map_unit_t* unit) {
    unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
}

/* Distortion animation 11: ends at once. The same routine as animation 10, compiled twice. */
static s32 distortion_clear_2(map_unit_t* unit) {
    unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
}

/*
 * Distortion animation 0x0c: launch a unit straight up at the start of a jump.
 *
 * Phase 0 picks an initial rise speed of sqrt(2 * gravity * (0x120 - ground y))
 * and the matching frame count, and starts the trailing position copies; phase 1
 * then applies the velocity to the height each frame, decelerating by
 * g_move_jump_gravity, and ends the animation when the count runs out.
 * World y grows downward, so 0x120 - ground y is the altitude 0x120 above the
 * ground under the unit, and the rise covers that distance from wherever the
 * unit starts. `unused_10` reproduces an unreferenced 16-byte frame slot below
 * the velocity copy.
 */
static s32 distortion_jump_takeoff(map_unit_t* unit) {
    VECTOR unused_10;
    VECTOR velocity;
    s32 timer = unit->distortion_timer;
    s32 speed;

    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        speed = SquareRoot12((-map_unit_calculate_height_from_world_coords(unit) + 0x120) * g_move_jump_gravity * 2);
        timer = speed / g_move_jump_gravity;
        velocity.vy = -speed;
        init_afterimage(unit);
        unit->status_flags_5_6 |= UNIT_STATUS_JUMP_HEIGHT_ACTIVE;
        unit->distortion_phase++;
    case 1:
        if (timer > 0) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_move_jump_gravity;
        } else {
            unit->position_copies_active = 0;
            unit->shadow_graphic_trigger = 0;
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
            animation_request_forced(ANIMATION_STAND, unit->facing, unit);
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    map_unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}

/* This retail call passes an unused coordinate-buffer argument in $a1.
 * Preserve the call shape even though the callee reads only the unit. */
#define world_height_from_world_coords_passing_coords                                                                  \
    ((s32 (*)(map_unit_t*, const SVECTOR*))map_unit_calculate_height_from_world_coords)

/* Distortion animation 0x0d: drop the unit straight down until it reaches the
 * ground.
 *
 * Phase 0 sets the fall speed to g_move_jump_gravity, starts the six
 * position copies that draw the sprite's trail and shows the shadow. Phase 1
 * takes the landing height each frame: during an event the ground under the
 * unit's own tile, otherwise the ground under the first target's tile, or under
 * the action target tile when the unit has no target. Above that height the unit
 * falls and gains g_move_jump_gravity per frame; on reaching it the unit
 * snaps to it, the position copies stop and the animation ends.
 *
 * During an event the unit is also centered on its tile, but retail writes the
 * depth coordinate into real.vy instead of real.vz. The height then sits below
 * the ground on the next frame, so an event fall ends on its second frame. */
static s32 distortion_fall_to_target(map_unit_t* unit) {
    SVECTOR coords;
    /* Unreferenced 8-byte frame slot above `coords`; it puts `velocity` at
     * sp+0x20, matching the sibling distortion handlers. */
    SVECTOR unused_18;
    VECTOR velocity;
    map_unit_t* other;
    battle_unit_t* battle_unit;
    s32 timer;
    s32 x;
    s32 y;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        velocity.vy = g_move_jump_gravity;
        init_afterimage(unit);
        unit->shadow_graphic_trigger = 1;
        unit->distortion_phase++;
    case 1:
        if (g_battle_state == BATTLE_STATE_EVENT) {
            unit->strike.target_count = 0;
            x = unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
            unit->world_coords.vx = x;
            unit->real.vx = x * ONE;
            y = unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
            unit->world_coords.vz = y;
            /* Retail stores the tile-center depth into real.vy (the height),
             * not real.vz. The first frame adds the fall speed to it and the
             * next one snaps the height to the ground. */
            unit->real.vy = y * ONE;
            vector_set_short(&coords, unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2), unit->map_layer,
                unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
            coords.vy = world_height_from_world_coords_passing_coords(unit, &coords);
        } else {
            if (unit->strike.target_count != 0) {
                other = map_unit_get_by_battle_unit_id(unit->strike.target_list[0]);
                vector_set_short(&coords, other->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2), other->map_layer,
                    other->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
            } else {
                battle_unit = unit->battle_unit;
                vector_set_short(&coords,
                    battle_unit->action_command.target_position.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2),
                    battle_unit->action_command.target_position.vy,
                    battle_unit->action_command.target_position.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
            }
            coords.vy = map_unit_calculate_height_with_caller_coords(unit, &coords);
        }
        if (unit->world_coords.vy < coords.vy) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_move_jump_gravity;
        } else {
            unit->real.vy = coords.vy * ONE;
            unit->position_copies_active = 0;
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        break;
    }
    unit->velocity = velocity;
    map_unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}

/* Fill the init record of a teleport effect: the unit is the caster and the only
 * target, and there are no spectators. Both teleport starters use it for every
 * unit, not only for Altima. */
static void init_teleport_effect_data(map_unit_t* unit, effect_context_t* teleport_data) {
    teleport_data->targets[EFFECT_CASTER_ENTRY].target_type = EFFECT_TARGET_TYPE_UNIT;
    teleport_data->targets[EFFECT_CASTER_ENTRY].data.unit.map_unit_id = unit->map_unit_id;
    teleport_data->target_count = 1;
    teleport_data->targets[0].target_type = EFFECT_TARGET_TYPE_UNIT;
    teleport_data->targets[0].data.unit.map_unit_id = unit->map_unit_id;
    teleport_data->spectator_count = 0;
}

/* Start the departure effect of a teleport. Altima's two forms request their own
 * ability effect and the result is 1; any other unit gets the teleport secondary
 * effect and the teleport-out sound, and the result is 0. */
static s32 start_teleport_departure(map_unit_t* unit) {
    effect_context_t teleport_data;

    init_teleport_effect_data(unit, &teleport_data);
    switch (unit->spritesheet_id) {
    case SPRITE_SHEET_ID_ALTIMA_FIRST_FORM:
        effect_playback_request(0, ABILITY_ID_ALTIMA_HOLY_ANGEL_TELEPORT_DEPARTURE, &teleport_data);
        return 1;
    case SPRITE_SHEET_ID_ALTIMA_SECOND_FORM:
        effect_playback_request(0, ABILITY_ID_ALTIMA_ARCH_ANGEL_TELEPORT_DEPARTURE, &teleport_data);
        return 1;
    }
    effect_secondary_start_teleport(unit);
    sound_play_sfx_on_free_channel(SOUND_SFX_TELEPORT_OUT);
    return 0;
}

/* Start the arrival effect of a teleport. Altima's two forms request their own
 * ability effect and the result is 1; any other unit gets the teleport secondary
 * effect and the teleport-in sound, and the result is 0. */
static s32 start_teleport_arrival(map_unit_t* unit) {
    effect_context_t teleport_data;

    init_teleport_effect_data(unit, &teleport_data);
    switch (unit->spritesheet_id) {
    case SPRITE_SHEET_ID_ALTIMA_FIRST_FORM:
        effect_playback_request(0, ABILITY_ID_ALTIMA_HOLY_ANGEL_TELEPORT_ARRIVAL, &teleport_data);
        return 1;
    case SPRITE_SHEET_ID_ALTIMA_SECOND_FORM:
        effect_playback_request(0, ABILITY_ID_ALTIMA_ARCH_ANGEL_TELEPORT_ARRIVAL, &teleport_data);
        return 1;
    }
    effect_secondary_start_teleport(unit);
    sound_play_sfx_on_free_channel(SOUND_SFX_TELEPORT_IN);
    return 0;
}

/* Teleport distortion: begin the departure flash. Altima's forms release the
 * ability effect that start_teleport_departure requested;
 * the sprite switches to additive semi-transparency and a palette modulation of
 * +31 on each color component starts. The distortion moves to its next phase
 * with the timer at 0. */
static void start_teleport_white_flash(map_unit_t* unit) {
    if (unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_FIRST_FORM
        || unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_SECOND_FORM) {
        effect_playback_start();
    }
    unit->sprite_display_flags = (unit->sprite_display_flags & ~SPRITE_DISPLAY_BLEND_MASK)
        | (SPRITE_DISPLAY_BLEND_ADD | SPRITE_DISPLAY_SEMI_TRANSPARENT);
    sprite_palette_apply_modulation_to_units(MAP_COLOR_MODE_BASE, 2, unit->map_unit_id, 0x1f, 0x1f, 0x1f);
    unit->distortion_phase++;
    unit->distortion_timer = 0;
}

/* Teleport distortion: begin the arrival. Altima's forms release the ability
 * effect that start_teleport_arrival requested; the unit's
 * palette is refreshed for the shadow mode of the tile it stands on and a palette
 * modulation of -31 on each color component starts. The distortion moves to its
 * next phase with the timer at 0. */
static void start_teleport_arrival_dark(map_unit_t* unit) {
    map_tile_t* tile;

    if (unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_FIRST_FORM
        || unit->spritesheet_id == SPRITE_SHEET_ID_ALTIMA_SECOND_FORM) {
        effect_playback_start();
    }
    tile = map_get_tile(unit->map_x, unit->map_y, unit->map_layer);
    sprite_palette_update(unit, tile->flags.shading, 1);
    sprite_palette_apply_modulation_to_units(MAP_COLOR_MODE_BASE, 0, unit->map_unit_id, -31, -31, -31);
    unit->distortion_phase++;
    unit->distortion_timer = 0;
}

/* Distortion animation 14 (TELEPORT), for every unit: advance one frame of the eight-step
 * warp-out / warp-in sequence.
 *
 * Jump table: .rodata 0x80068208, 8 32-bit addresses.
 *
 * An ignored s32 result keeps $v0 live at the epilogue and the switch range
 * check's delay slot empty. This callback supplies no return value. */
static s32 distortion_teleport(map_unit_t* unit) {
    unit->distortion_timer++;
    switch (unit->distortion_phase) {
    case ANIMATION_TELEPORT_PHASE_START_DEPARTURE:
        if (start_teleport_departure(unit) == 0) {
            start_teleport_white_flash(unit);
        }
        unit->distortion_phase++;
        break;
    case ANIMATION_TELEPORT_PHASE_WAIT_DEPARTURE_LOAD:
        /* The target passes no argument here. */
        if (effect_update_file_load() == 0 && camera_is_active() == 0) {
            start_teleport_white_flash(unit);
        }
        break;
    case ANIMATION_TELEPORT_PHASE_HOLD_DEPARTURE_FLASH:
        if (unit->distortion_timer >= 0x11) {
            sprite_palette_apply_modulation_to_units(MAP_COLOR_MODE_BASE, 4, unit->map_unit_id, -31, -31, -31);
            unit->distortion_timer = 0;
            unit->distortion_phase++;
        }
        break;
    case ANIMATION_TELEPORT_PHASE_MOVE_TO_DESTINATION:
        if (g_battle_animation_wait_state == 0) {
            unit->map_x = unit->movement.destination_x;
            unit->map_y = unit->movement.destination_y;
            unit->map_layer = unit->movement.destination_layer;
            set_real_coords_from_map_coords(unit);
            map_unit_set_world_coords_from_real_coords(unit);
            if (start_teleport_arrival(unit) == 0) {
                start_teleport_arrival_dark(unit);
            }
            unit->distortion_phase++;
        }
        break;
    case ANIMATION_TELEPORT_PHASE_WAIT_ARRIVAL_LOAD:
        if (effect_update_file_load() == 0 && camera_is_active() == 0) {
            start_teleport_arrival_dark(unit);
        }
        break;
    case ANIMATION_TELEPORT_PHASE_HOLD_ARRIVAL_DARK:
        if (unit->distortion_timer >= 0x11) {
            sprite_palette_apply_modulation_to_units(MAP_COLOR_MODE_BASE, 4, unit->map_unit_id, 31, 31, 31);
            unit->distortion_phase++;
        }
        break;
    case ANIMATION_TELEPORT_PHASE_WAIT_ARRIVAL_FADE:
        if (unit->distortion_timer >= 0x21) {
            sprite_palette_apply_modulation_to_units(MAP_COLOR_MODE_RESTORE, 2, unit->map_unit_id, 0, 0, 0);
            unit->distortion_timer = 0;
            unit->distortion_phase++;
        }
        break;
    case ANIMATION_TELEPORT_PHASE_WAIT_ARRIVAL_END:
        if (g_battle_animation_wait_state == 0) {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
            unit->sprite_display_flags &= ~SPRITE_DISPLAY_SEMI_TRANSPARENT;
        }
        break;
    }
}

/* Distortion animation 0x0f: slide a unit away from its target over
 * distortion_timer frames (Rush winds up with it).
 *
 * Phase 0 takes the horizontal offset from the first unit in the target list,
 * or from the center of the action target tile, to the unit, reduces it by an
 * eighth, keeps the remainder of it modulo 0xe000 (half a tile, 14 world units,
 * in Q12), and divides that into per-frame velocity. Phase 1 applies that
 * velocity until the timer runs out. Clearing
 * velocity.vy in each branch keeps its division, which GCC folds away when the
 * zero is stored once after the branches. `unused_10` reproduces an
 * unreferenced 8-byte frame slot below the velocity copy. */
static s32 distortion_back_away(map_unit_t* unit) {
    s32 unused_10[2];
    VECTOR velocity;
    map_unit_t* target;
    s32 timer;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->battle_unit != 0) {
            if (unit->strike.target_count != 0) {
                target = map_unit_get_by_battle_unit_id(unit->strike.target_list[0]);
                velocity.vx = unit->real.vx - target->real.vx;
                velocity.vx = (velocity.vx - velocity.vx / 8) % 0xe000;
                velocity.vz = unit->real.vz - target->real.vz;
                velocity.vz = (velocity.vz - velocity.vz / 8) % 0xe000;
                velocity.vy = 0;
            } else {
                velocity.vx = (unit->world_coords.vx
                                  - (unit->battle_unit->action_command.target_position.vx * MAP_TILE_SIZE
                                      + (MAP_TILE_SIZE / 2)))
                    * ONE;
                velocity.vx = (velocity.vx - velocity.vx / 8) % 0xe000;
                velocity.vz = (unit->world_coords.vz
                                  - (unit->battle_unit->action_command.target_position.vz * MAP_TILE_SIZE
                                      + (MAP_TILE_SIZE / 2)))
                    * ONE;
                velocity.vz = (velocity.vz - velocity.vz / 8) % 0xe000;
                velocity.vy = 0;
            }
            velocity.vx /= timer;
            velocity.vy /= timer;
            velocity.vz /= timer;
            unit->distortion_phase++;
        } else {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
            break;
        }
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            unit->real.vz += velocity.vz;
        } else {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    map_unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}

/* Distortion animation 0x10: jump along an arc to the center of the unit's own
 * map tile, darkened for the first part of the flight.
 *
 * Phase 0 computes the arc as distortion_jump_to_own_tile does,
 * hides the shadow, selects quarter-add blending and starts
 * a palette modulation of -31 on each color component. Phase 1 flies until 4
 * frames remain (the threshold kept in distortion_target), then restores the
 * palette. Phase 2 flies the remaining frames and stops the horizontal motion.
 * Phase 3 falls to the ground, snaps the unit to the tile, shows the shadow,
 * clears UNIT_STATUS_JUMP_HEIGHT_ACTIVE and the semi-transparency bit, and
 * ends the animation. */
static s32 distortion_jump_to_own_tile_faded(map_unit_t* unit) {
    /* Unreferenced 8-byte frame slot below the target point, like the one in
     * distortion_jump_to_own_tile; it puts `target` at sp+0x20. */
    SVECTOR unused_18;
    SVECTOR target;
    VECTOR velocity;
    s32 timer;
    s32 threshold;

    timer = unit->distortion_timer;
    threshold = unit->distortion_target;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        target.vx = unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        target.vz = unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        target.vy = map_unit_calculate_height_from_tile(unit);
        timer = calculate_jump_arc_velocity(&unit->world_coords, &target, &velocity);
        unit->shadow_graphic_trigger = 0;
        unit->sprite_display_flags |= SPRITE_DISPLAY_BLEND_ADD_QUARTER | SPRITE_DISPLAY_SEMI_TRANSPARENT;
        sprite_palette_apply_modulation_to_units(MAP_COLOR_MODE_BASE, 1, unit->map_unit_id, -31, -31, -31);
        threshold = 4;
        unit->distortion_phase++;
    case 1:
        if (timer > threshold) {
            unit->real.vx += velocity.vx;
            unit->real.vy += velocity.vy;
            unit->real.vz += velocity.vz;
            velocity.vy += g_move_jump_gravity;
        } else {
            sprite_palette_apply_modulation_to_units(MAP_COLOR_MODE_RESTORE, 1, unit->map_unit_id, 0, 0, 0);
            unit->distortion_phase++;
        }
        timer--;
        break;
    case 2:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            unit->real.vy += velocity.vy;
            unit->real.vz += velocity.vz;
            velocity.vy += g_move_jump_gravity;
        } else {
            velocity.vz = 0;
            velocity.vx = 0;
            unit->distortion_phase++;
        }
        timer--;
        break;
    case 3:
        vector_set_short(&target, unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2), 0,
            unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
        target.vy = map_unit_calculate_height_from_world_coords(unit);
        if (unit->world_coords.vy < target.vy) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_move_jump_gravity;
        } else {
            set_real_coords_from_map_coords(unit);
            unit->shadow_graphic_trigger = 1;
            unit->status_flags_5_6 &= ~UNIT_STATUS_JUMP_HEIGHT_ACTIVE;
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
            unit->sprite_display_flags &= ~SPRITE_DISPLAY_SEMI_TRANSPARENT;
        }
        break;
    }
    unit->velocity = velocity;
    map_unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
    unit->distortion_target = threshold;
}

/*
 * Distortion animation 0x11: launch a unit upward during a jump landing.
 *
 * Like distortion_jump_takeoff, phase 0 picks a rise speed of
 * sqrt(2 * gravity * (0x120 - ground y)) and a frame count, and phase 1
 * applies it to the height, decelerating by g_move_jump_gravity. Here phase 0 also
 * plays the jump sound, and ends the animation at once when the unit is already
 * at or above 0x120 over the ground; the end of phase 1 only clears the animation id.
 */
static s32 distortion_jump_rise(map_unit_t* unit) {
    VECTOR unused_10;
    VECTOR velocity;
    s32 timer = unit->distortion_timer;
    s32 height;
    s32 speed;

    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        height = -map_unit_calculate_height_from_world_coords(unit) + 0x120;
        unit->status_flags_5_6 |= UNIT_STATUS_JUMP_HEIGHT_ACTIVE;
        if (-unit->world_coords.vy >= height) {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
            break;
        }
        speed = SquareRoot12(height * g_move_jump_gravity * 2);
        timer = speed / g_move_jump_gravity;
        velocity.vy = -speed;
        init_afterimage(unit);
        map_unit_play_sfx(unit, SOUND_SFX_JUMP);
        unit->distortion_phase++;
    case 1:
        if (timer > 0) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_move_jump_gravity;
        } else {
            unit->distortion_animation_id = ANIMATION_DISTORTION_NONE;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    map_unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}

/* Run one frame of the unit's distortion animation from
 * s_distortion_handlers. An id past the table reports
 * SYSTEM_ERROR_DISTORTION_ANIMATION. */
static void dispatch_distortion(map_unit_t* unit) {
    u32 distortion_id = unit->distortion_animation_id;

    if (distortion_id != ANIMATION_DISTORTION_NONE) {
        if (distortion_id < ANIMATION_DISTORTION_COUNT) {
            s_distortion_handlers[distortion_id](unit);
        } else {
            system_handle_error(SYSTEM_ERROR_DISTORTION_ANIMATION);
        }
    }
}
