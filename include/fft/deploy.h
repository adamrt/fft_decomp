#ifndef DEPLOY_H
#define DEPLOY_H

/* Deployment before a battle: BATTLE's side of it and ATTACK's placement screen. */

#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* BATTLE's side */
extern s32 g_deploy_skipped;
extern s32 g_deploy_finished;
extern unit_deployment_coordinates_t g_deploy_deployed_unit_map_coordinates[6];

s32 deploy_run_frame_and_check(u32* ot, u32 buttons);
void deploy_init_display(s32 width, s32 height, s32 projection, u8 red, u8 green, u8 blue);
s32 deploy_sync_and_submit_frame(u32* ordering_table);
s32 deploy_update_controller_input(void);
void deploy_mark_finished(void);
void deploy_init_and_reset_sprite_load_state(void);
s32 deploy_build_units(unit_deployment_coordinates_t* formation, s32 unused_1);
s32 deploy_init_party_member(s32 unused0, s32 unused1, s32 unit_index);
void deploy_init_units_for_debug_red_team(unit_deployment_coordinates_t* formation, s32 unused_1);

/* Loads ATTACK.OUT with its scenario conditions and graphics and starts the deployment screen (returns 0), or
 * starts the map-load thread when deployment is skipped (returns 1). */
s32 deploy_open(void);

/* squads and roster */
/* The roster id of a deployment tile or placement record that holds no unit
 * (g_deploy_roster_id_by_tile, g_deploy_units_by_squad and
 * unit_deployment_coordinates_t.party_id). */
enum { DEPLOY_ROSTER_ID_NONE = 0xff };

/* Twelve-byte deployment-area record. The roster-fieldability routine proves
 * the stride, unit limit and map id; the valid-deployment-tile accessors
 * establish the 5-by-5 bitmap. */
typedef struct deploy_squad {
    u32 valid_tile_bitmap; /* 0x00; 5-by-5 deployment mask */
    s8 center_x;           /* 0x04 */
    s8 center_y;           /* 0x05 */
    u8 upper_level;        /* 0x06 */
    u8 orientation_flags;  /* 0x07; original and rotated orientations */
    u8 unit_limit;         /* 0x08 */
    u8 map_id;             /* 0x09 */
    u16 placement_id;      /* 0x0a */
} deploy_squad_t;
typedef char deploy_squad_size_must_be_12[(sizeof(deploy_squad_t) == 12) ? 1 : -1];

extern deploy_squad_t* g_deploy_squad_data;
extern s32 g_deploy_current_squad;
extern s32 g_deploy_current_squad_id;
extern s32 g_deploy_tiles_only_mode;
extern u8 g_deploy_roster_id_by_tile[5][5];
extern u8 g_deploy_units_by_squad[4][25];
extern s32 g_deploy_slot_offsets[5];
extern s32 g_deploy_fieldable_unit_count;
extern u8 g_deploy_unit_fieldable[PARTY_ROSTER_SLOT_COUNT];
extern u8 g_deploy_roster_unit_deployable[PARTY_ROSTER_SLOT_COUNT];
extern s32 g_deploy_selected_roster_index;

/* Run ATTACK's deployment screen on the calling thread. */
void attack_deployment_entrypoint(void);
void deploy_find_fieldable_units(void);
void deploy_find_deployable_units(void);
s32 deploy_is_roster_unit_deployed(s32 roster_id);
s32 deploy_has_roster_unit_on_tiles(s32 roster_id);
unit_deployment_coordinates_t* deploy_get_coordinate_slots(void);

/* deployment help */
void deploy_run_help_cursor(void);
void deploy_build_strings(void);

#endif
