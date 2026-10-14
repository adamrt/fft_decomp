#ifndef BATTLE_SCRIPT_H
#define BATTLE_SCRIPT_H

/* Battle scene actions and orchestration invoked by scenario bytecode. */

#include "fft/map.h"
#include "fft/menu.h"
#include "fft/script.h"

/* interpreter */
extern s16 g_script_threads_idle;

void script_process_pending_requests(void);
/* Returns the event result that the battle state handlers switch on;
 * proven by 0x80078fb4 and 0x800785ac, which both consume it. */
void script_set_event_speed(s32 speed);

/* input */
extern u32 g_script_saved_event_input;

void script_route_event_input(void);

/* conditions */

s32 script_is_tutorial_event_slot(void);

/* unit creation and removal */
s32 script_add_ghost_unit_event_instruction(
    s32 map_x, s32 map_y, s32 map_level, s16 facing, s16 spritesheet_id, s32 runtime_id, s32 flags);
void script_add_unit_start_thread(void);
void script_blue_remove_all_enemy_units(void);
void script_blue_remove_unit(s32 unit_id);
void script_teleportout_event_instruction(s32 arg, s32 remove);

/* unit movement */
extern map_tile_coordinates_t g_script_warp_coords;

void script_waitspritemove_event_instruction(s32 unit_id);
void script_warp_unit(const event_warp_unit_parameters_t* parameters);
void script_warp_unit_display_to_paired_unit(s32 runtime_id);

/* unit statuses */

/* camera */
void script_earthquake_start(void);

/* map and lighting */
extern SVECTOR g_script_light_angles;
extern MATRIX g_script_light_base_color_matrix;
extern MATRIX g_script_light_base_direction_matrix;
extern VECTOR g_script_light_scale;

void script_dark_screen_thread(void);

/* graphics and messages */

void script_set_units_palette_update_suppression(s32 unit_id, s32 enable);

/* frame hooks */

#endif
