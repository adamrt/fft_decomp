#ifndef BATTLE_STATUS_H
#define BATTLE_STATUS_H

/* BATTLE status effects: infliction and removal, turn flags, per-turn upkeep and the graphics
 * changes a status queues. Shared status IDs and stored status sets are defined in unit.h. */

#include "fft/data.h"
#include "fft/unit.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* turn flags */
s32 battle_status_calculate_turn_flags(const unit_stats_t* unit);
void battle_status_enable_disable_acting(unit_stats_t* unit);

/* infliction and removal */
/* Set when a unit that started on the blue team turns Crystal or Dead; it voids the battle's
 * level bonus money. */
extern u8 g_battle_player_unit_fallen;

s32 battle_status_inflict_defending_to_battle_id(s32 unit_id);
s32 battle_status_init_special_flag_enabling(s32 unit_id);
s32 battle_status_modify_inflictions(s32 removal_only);
s32 battle_status_prepare_terrain_poison(unit_stats_t* unit);
void battle_status_clear_all(unit_stats_t* unit);
s32 battle_status_remove_charging_ability_ct(unit_stats_t* unit, s32 do_disable);
s32 battle_status_remove_transparent(unit_stats_t* unit);
void battle_status_remove_transparent_if_jump_used(unit_stats_t* unit);
s32 battle_status_resolve_unit_changes(s32 unit_id, s32 removal_only);

/* upkeep */
s32 battle_status_apply_poison_and_regen(unit_stats_t* unit);
void battle_status_apply_unit_action_removal(unit_stats_t* unit);
void battle_status_update_expiring(s32 unit_id);

/* display */
extern SVECTOR g_battle_status_bubble_zoom;
extern const u8 g_battle_status_display_image_ids[STATUS_COUNT];

void battle_status_queue_graphics_change_if_executing(s32 status_id, s32 enabled, s32 battle_id);
s32 battle_status_queue_runtime_graphics_flag_change(s32 status, s32 enabled, s32 battle_id);

#endif
