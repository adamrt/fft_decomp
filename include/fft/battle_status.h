#ifndef BATTLE_STATUS_H
#define BATTLE_STATUS_H

/* BATTLE status infliction, upkeep and display state. Shared status IDs and
 * stored status sets are defined in unit.h. */

#include "fft/data.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* status indices */
/* Event infliction helpers number bits LSB-first, unlike stored status IDs. */
#define STATUS_LSB_INDEX(id) (((id) & ~7) | (7 - ((id) & 7)))
#define STATUS_LSB_MASK(id)  ((u8)(1U << ((id) & 7)))
/* The special-status callback table reserves index 0; timed slots begin at Poison. */
#define STATUS_HANDLER_INDEX(id) ((id) + 1)
#define STATUS_CT_INDEX(id)      ((id) - STATUS_ID_POISON)
#define STATUS_PACKED_MASK(id)   ((u32)STATUS_BYTE_MASK(id) << (STATUS_BYTE_INDEX(id) * 8))

/* turn flags */
extern u8 g_battle_player_unit_fallen;

s32 battle_status_calculate_turn_flags(const unit_stats_t* unit);
void battle_status_enable_disable_acting(unit_stats_t* unit);

/* infliction and removal */
void battle_status_enable_special_flags(s32 status_id, s32 enabled, s32 battle_id);
s32 battle_status_inflict_defending_to_battle_id(s32 unit_id);
s32 battle_status_init_special_flag_enabling(s32 unit_id);
s32 battle_status_modify_inflictions(s32 removal_only);
s32 battle_status_prepare_terrain_poison(unit_stats_t* unit);
void battle_status_reapply_active_flags(unit_stats_t* unit);
s32 battle_status_remove_charging_ability_ct(unit_stats_t* unit, s32 do_disable);
s32 battle_status_remove_transparent(unit_stats_t* unit);
void battle_status_remove_transparent_if_jump_used(unit_stats_t* unit);
s32 battle_status_resolve_unit_changes(s32 unit_id, s32 removal_only);

/* upkeep */
s32 battle_status_apply_poison_and_regen(unit_stats_t* unit);
void battle_status_apply_unit_action_removal(unit_stats_t* unit);
void battle_status_update_expiring(s32 unit_id);

/* graphics */
extern u8 g_status_bubble_digit_u[24];
extern u8 g_status_bubble_digit_v[24];
/* Texture coordinates for the status bubble graphics, indexed by status bubble
 * id (0x800949dc/0x800949f4) and by a counter digit (0x80094a0c/0x80094a24).
 * Each table holds 24 bytes; the pairs are read together as one u/v. */
extern u8 g_status_bubble_u[24];
extern u8 g_status_bubble_v[24];
extern SVECTOR g_status_bubble_zoom;
extern const u8 g_status_display_image_ids[STATUS_COUNT];
extern s32 g_status_menu_open;

s32 battle_status_queue_runtime_graphics_flag_change(s32 status, s32 enabled, s32 battle_id);

#endif
