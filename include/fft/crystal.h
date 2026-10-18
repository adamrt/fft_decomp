#ifndef CRYSTAL_H
#define CRYSTAL_H

/* The crystal or treasure chest a fallen unit leaves: what picking it up yields, the panel that offers the choice, and
 * applying the choice. */

#include "fft/menu.h"
#include "fft/unit.h"
#include "psx/types.h"

/* pickup result */
/* crystal_pickup_result_t.result. BATTLE leaves NONE, TREASURE or CRYSTAL, the last with LEARNED added when
 * the mover learned abilities from the crystal; the crystal pickup panels also handle NOTHING and a lone LEARNED. */
typedef enum crystal_pickup_kind {
    CRYSTAL_PICKUP_NONE = -1, /* no crystal or treasure on the tile */
    CRYSTAL_PICKUP_NOTHING = 0,
    CRYSTAL_PICKUP_LEARNED = 0x1,
    CRYSTAL_PICKUP_CRYSTAL = 0x2,
    CRYSTAL_PICKUP_TREASURE = 0x4
} crystal_pickup_kind_e;

/* Crystal/treasure pickup result (g_crystal_pickup_result) that crystal_build_pickup_result
 * builds on the main thread stack and returns; each program keeps the pointer in g_crystal_pickup_context. The
 * crystal pickup panels pick their variant from `result` and show `treasure_item`. */
typedef struct crystal_pickup_result {
    s32 result;                 /* 0x00; crystal_pickup_kind_e */
    u8 _unused_04[0x14];        /* 0x04 */
    u8 learned[19][3];          /* 0x18 */
    u8 unit_index;              /* 0x51 */
    u8 treasure_item;           /* 0x52; the roster/item id the panels show */
    u8 learned_abilities[0x40]; /* 0x53 */
} crystal_pickup_result_t;

/* The choices crystal_apply_choice applies, as flags. */
enum {
    CRYSTAL_LEARN_ABILITIES = 0x1,
    CRYSTAL_RESTORE_HP_MP = 0x2,
    CRYSTAL_TAKE_TREASURE = 0x4 /* add the treasure's item instead */
};

extern crystal_pickup_result_t g_crystal_pickup_result;
extern crystal_pickup_result_t* g_crystal_pickup_context;
extern u8 g_crystal_learnable_abilities[];
extern u8 g_crystal_treasure_item_id;

crystal_pickup_result_t* crystal_build_pickup_result(unit_stats_t* mover);
s32 crystal_apply_choice(unit_stats_t* unit, s32 flags);

/* Runs crystal_build_pickup_result for the unit on the main stack, keeps the result in
 * g_crystal_pickup_context and returns the treasure item when there is one, otherwise -1. */
s32 crystal_resolve_pickup(s32 battle_id);

/* pickup panel */
/* The command the panel resolves to (4 for a treasure, 0 for nothing) and the treasure item. */
extern s32 g_crystal_pickup_action;
extern s32 g_crystal_pickup_item;

void crystal_open_pickup_panel(void);

/* Runs the crystal/treasure result panel and its list and choice threads. */
void crystal_start_pickup_threads(void);

#endif
