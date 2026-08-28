#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "psx/types.h"

/* Filters unit-occupied targeting panels by team eligibility.
 *
 * The low byte of `raw_unit_id` selects the acting unit, which is treated as
 * same-team even if its initial and live team fields differ. `allow_allies`
 * and `allow_enemies` are raw nonzero flag values. Unless the ability AoE is
 * `0xff`, every nonempty panel left after filtering is marked selected. */
void battle_target_apply_unit_team_eligibility(s32 raw_unit_id, u8 allow_allies, u8 allow_enemies, u8 aoe_is_0xff) {
    /* Retain the team comparison value in $v0 until the actor override. */
    register s32 relation __asm__("$2");
    register s32 relation_out __asm__("$4");
    s32 actor_id;
    s32 team_flags;
    s32 allowed;
    s32 panel_index;
    battle_target_panel_t* panel;
    s32 i;
    /* The index and panel masks occupy the caller registers before each loop. */
    register s32 actor_index __asm__("$4");
    /* Keep the selected-panel marker live through the first pass. */
    register s32 mark __asm__("$18");
    register s32 mask __asm__("$2");
    register s32 mark_all __asm__("$4");

    actor_index = raw_unit_id & 0xff;
    team_flags = g_battle_unit_stats[actor_index].team_flags;
    actor_id = actor_index;
    for (i = 0, mark = 1; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (battle_target_is_unit_untargetable_and_store_tile(i, &panel_index) == 0) {
            relation = team_flags ^ g_battle_unit_stats[i].initial_team_flags;
            /* Keep the xor ahead of the acting-unit branch. */
            __asm__("" : "=r"(relation) : "0"(relation));
            relation_out = relation;
            if (actor_id == i) {
                relation_out = 0;
            }
            panel = &g_battle_target_panels[panel_index];
            if (panel->remaining_range != 0) {
                mask = relation_out & BATTLE_TEAM_MASK;
                if (mask) {
                    allowed = allow_enemies;
                } else {
                    allowed = allow_allies;
                }
                if (allowed != 0) {
                    panel->mark = mark;
                } else {
                    panel->remaining_range = 0;
                }
            }
        }
    }
    if (!aoe_is_0xff) {
        for (i = 0, mark_all = 1, panel = g_battle_target_panels; i < 0x200; i++) {
            if (panel->remaining_range != 0) {
                panel->mark = mark_all;
            }
            panel++;
        }
    }
}
