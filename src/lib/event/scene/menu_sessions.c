/* Activate deployment, native mini menus and the supervised unit-view panels. */

#include "fft/at_list.h"
#include "fft/battle_map.h"
#include "fft/command.h"
#include "fft/deploy.h"
#include "fft/file.h"
#include "fft/formation.h"
#include "fft/gfx_primitive.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/music.h"
#include "fft/option.h"
#include "fft/portrait.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/status_screen.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit_event.h"
#include "fft/unit_panel.h"
#include "fft/unit_view.h"
#include "psx/types.h"

extern u16 g_unit_view_cached_battle_id;
extern u16 g_unit_view_cached_comparison_id;
extern u8 g_menu_panel_fade_modes[];

/* Desired threads for one unit-view mode: four native entrypoints, scheduler slots, and task IDs.
 * UNIT_VIEW_NO_THREAD leaves that slot stopped after any previous task exits. */
#define UNIT_VIEW_NO_THREAD ((void (*)(void)) - 1)
typedef struct view_threads {
    void (*entries[4])(void); /* 0x00 */
    s32 thread_ids[4];        /* 0x10 */
    s32 task_ids[4];          /* 0x20 */
} view_threads_t;
typedef char view_threads_size_must_be_48[(sizeof(view_threads_t) == 48) ? 1 : -1];

static view_threads_t g_view_threads[7] = {
    { { UNIT_VIEW_NO_THREAD, UNIT_VIEW_NO_THREAD, UNIT_VIEW_NO_THREAD, UNIT_VIEW_NO_THREAD },
        { THREAD_SLOT_TILE_INFO_PANEL, THREAD_SLOT_UNIT_SUMMARY_PANEL, THREAD_SLOT_RIGHT_UNIT_STATS,
            THREAD_SLOT_HOVERED_UNIT_STATS },
        { -1, -1, -1, -1 } },
    { { unit_view_tile_info_thread, UNIT_VIEW_NO_THREAD, UNIT_VIEW_NO_THREAD, UNIT_VIEW_NO_THREAD },
        { THREAD_SLOT_TILE_INFO_PANEL, THREAD_SLOT_UNIT_SUMMARY_PANEL, THREAD_SLOT_RIGHT_UNIT_STATS,
            THREAD_SLOT_HOVERED_UNIT_STATS },
        { THREAD_TASK_TILE_INFO_PANEL, -1, -1, -1 } },
    { { unit_view_tile_info_thread, UNIT_VIEW_NO_THREAD, UNIT_VIEW_NO_THREAD, unit_view_refresh_hovered_stats },
        { THREAD_SLOT_TILE_INFO_PANEL, THREAD_SLOT_UNIT_SUMMARY_PANEL, THREAD_SLOT_RIGHT_UNIT_STATS,
            THREAD_SLOT_HOVERED_UNIT_STATS },
        { THREAD_TASK_TILE_INFO_PANEL, -1, -1, THREAD_TASK_HOVERED_UNIT_STATS } },
    { { unit_view_tile_info_thread, unit_view_summary_thread, UNIT_VIEW_NO_THREAD, unit_view_refresh_hovered_stats },
        { THREAD_SLOT_TILE_INFO_PANEL, THREAD_SLOT_UNIT_SUMMARY_PANEL, THREAD_SLOT_RIGHT_UNIT_STATS,
            THREAD_SLOT_HOVERED_UNIT_STATS },
        { THREAD_TASK_TILE_INFO_PANEL, THREAD_TASK_UNIT_SUMMARY_PANEL, -1, THREAD_TASK_HOVERED_UNIT_STATS } },
    { { unit_view_tile_info_thread, UNIT_VIEW_NO_THREAD, unit_view_refresh_right_stats, UNIT_VIEW_NO_THREAD },
        { THREAD_SLOT_TILE_INFO_PANEL, THREAD_SLOT_UNIT_SUMMARY_PANEL, THREAD_SLOT_RIGHT_UNIT_STATS,
            THREAD_SLOT_HOVERED_UNIT_STATS },
        { THREAD_TASK_TILE_INFO_PANEL, -1, THREAD_TASK_RIGHT_UNIT_STATS, -1 } },
    { { unit_view_tile_info_thread, UNIT_VIEW_NO_THREAD, unit_view_preview_caster_stats,
          unit_view_preview_target_stats },
        { THREAD_SLOT_TILE_INFO_PANEL, THREAD_SLOT_UNIT_SUMMARY_PANEL, THREAD_SLOT_RIGHT_UNIT_STATS,
            THREAD_SLOT_HOVERED_UNIT_STATS },
        { THREAD_TASK_TILE_INFO_PANEL, -1, THREAD_TASK_ATTACK_CASTER_STATS, THREAD_TASK_ATTACK_TARGET_STATS } },
    { { status_screen_numbers_thread, unit_view_summary_thread, UNIT_VIEW_NO_THREAD, unit_view_refresh_hovered_stats },
        { THREAD_SLOT_TILE_INFO_PANEL, THREAD_SLOT_UNIT_SUMMARY_PANEL, THREAD_SLOT_RIGHT_UNIT_STATS,
            THREAD_SLOT_HOVERED_UNIT_STATS },
        { THREAD_TASK_NUMERIC_DISPLAY_PANEL, 36, -1, 37 } },
};
static void views_supervise(void);

/* Load ATTACK.OUT (unless preloaded) with its scenario conditions and graphics, reset menu
 * and unit state and, for a tutorial, fix the game options. Then start the deployment
 * screen and return 0, or start the thread that releases the deployment gate and return 1
 * when deployment is skipped. The battle loop drives map loading in either case. */
s32 deploy_open(void) {
    s32 result;
    s32 return_value;
    game_options_fields_t* options;
    g_unit_panel_hide_numeric_values = 0;
    menu_subsystems_init();
    unit_animation_states_reset();
    command_reset_unit_records();
    if (g_script_attack_entry_mode == SCRIPT_ATTACK_ENTRY_PRELOADED) {
        g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_PRELOAD_USED;
    } else {
        g_thread_call_target = file_buffer_load_checked;
        thread_call_on_main_stack(DISC_SECTOR_EVENT_ATTACK_OUT, 0x20000, g_heap_companion_overlay_load_address);
    }
    result = scenario_load_conditionals();
    if (g_script_attack_entry_mode != SCRIPT_ATTACK_ENTRY_PRELOAD_USED) {
        formation_build_sprites();
        portrait_load_party();
    }
    g_menu_panel_fade_mode = MENU_PANEL_FADE_NONE;
    g_menu_panel_fade_intensity = 0;
    if (tutorial_is_current_event() != 0) {
        g_gfx_screen_fade = 0xff;
        g_game_options_saved = g_game_options;
        options = &g_game_options.fields;
        options->cursor_movement = GAME_CURSOR_MOVEMENT_TYPE_A;
        options->multi_height_cursor_speed = GAME_MULTI_HEIGHT_CURSOR_SPEED_REGULAR;
        options->navigation_messages = GAME_OPTION_ON;
        options->effect_messages = GAME_OPTION_OFF;
        options->target_flashing = GAME_OPTION_ON;
        options->show_unequippable_items = GAME_OPTION_ON;
    } else {
        g_gfx_screen_fade = 0;
    }
    deploy_find_fieldable_units();
    if (result != 0) {
        thread_start(THREAD_BORROWED_SLOT_DEPLOYMENT, battle_map_skip_deployment_thread);
        return_value = 1;
    } else {
        g_sound_music_track_1_id = 0;
        g_sound_music_track_2_id = 0;
        music_tracks_play(MUSIC_DEPLOYMENT, MUSIC_NONE);
        deploy_build_strings();
        battle_map_title_load_graphic();
        thread_start(THREAD_BORROWED_SLOT_DEPLOYMENT, attack_deployment_entrypoint);
        return_value = 0;
    }
    return return_value;
}

/* Start the mini menu when its slots are idle and menus are active. */
void mini_menu_start(void) {
    if (thread_is_running_asm(THREAD_SLOT_MENU) != 0) {
        return;
    }
    if (thread_is_running_asm(THREAD_SLOT_MENU_CHILD) != 0) {
        return;
    }
    if (g_menu_overlay_state != 0) {
        return;
    }
    g_command_current_id = MENU_COMMAND_ID_MINI_MENU;
    g_menu_thread_menu_data = g_command_idle_action_entries;
    thread_start(THREAD_SLOT_MENU, mini_menu_select);
    thread_parameters_set(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[MENU_COMMAND_ID_MINI_MENU], 0, 0);
    g_thread_contexts[THREAD_SLOT_MENU].task_id = THREAD_TASK_MINI_MENU;
}

#ifdef FFT_BATTLE
/* Report whether the menu layer is busy.
 *
 * Returns g_menu_transition_state when it is nonzero (4 during the unit
 * list fade-out), 2 while the menu-building thread runs or while menu input is
 * blocked, 4 when one of threads 4-9 is running, and 0 when all are idle. The `i
 * != 3` test inside the scan of threads 4-9 always passes. */
s32 menu_thread_activity_get(void) {
    s32 i;
    s32 result;

    if (g_menu_transition_state != 0) {
        return g_menu_transition_state;
    }
    if (thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        return 2;
    }
    i = 4;
    do {
        if (i != 3) {
            if (thread_is_running_asm(i) != 0) {
                break;
            }
        }
        i += 1;
    } while (i < 0xA);
    if (g_menu_input_mode != MENU_INPUT_BLOCKED) {
        result = i != 0xA;
        return result * 4;
    }
    return 2;
}
#endif

#ifdef FFT_WORLD
/* Menu activity summary: 2 while the menu-building thread runs, else 4 when
 * any of thread slots 4..8 runs, else 0. */
s32 menu_thread_activity_get(void) {
    s32 i;

    if (thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        return 2;
    }
    for (i = 4; i < 9; i++) {
        if (thread_is_running_asm(i) != 0) {
            break;
        }
    }
    return (i != 9) << 2;
}
#endif

/* Raise the flag that releases at_list_select_unit_thread once a unit picked
 * from the AT list has been viewed. */
void at_list_selection_finish(void) {
    g_at_list_selection_finished = 1;
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * at_list_selection_finish and menu_noop_6. Nothing on the disc calls it
 * or points at it; the name is a neutral placeholder until a reference turns
 * up. */
void menu_noop_5(void) {
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between menu_noop_5 and
 * unit_view_start. Nothing on the disc calls it or points at it;
 * the name is a neutral placeholder until a reference turns up. */
void menu_noop_6(void) {
}

/* Requests a unit view mode from the unit view supervisor thread, starting the
 * thread when it is not running.
 *
 * The unit name list is rebuilt and the panel fade mode taken from
 * g_menu_panel_fade_modes[mode]. unit_a becomes the active-turn unit of
 * the banner and unit_b the previewed target of the status billboard; 0xff leaves
 * either unchanged. Ignored while g_menu_overlay_state is 1. */
void unit_view_start(s32 mode, s32 unit_a, s32 unit_b) {
    if (g_menu_overlay_state == 1) {
        return;
    }
    text_build_unit_name_list();
    g_menu_panel_fade_mode = g_menu_panel_fade_modes[mode];
    if (unit_a != 0xff) {
        g_unit_panel_selected_unit.battle_id = unit_a;
        g_unit_panel_selected_unit_status.unit_index = unit_a;
    }
    if (unit_b != 0xff) {
        g_unit_panel_comparison_unit_data.battle_id = unit_b;
        g_unit_panel_comparison_unit_status.unit_index = unit_b;
    }
    if (thread_is_running_asm(THREAD_SLOT_UNIT_VIEW) == 0) {
        thread_start(THREAD_SLOT_UNIT_VIEW, views_supervise);
    }
    thread_parameters_set(THREAD_SLOT_UNIT_VIEW, mode, 0, 0);
}

enum { MENU_UNIT_VIEW_SLOT_IDLE = 0, MENU_UNIT_VIEW_SLOT_RESTART = 1, MENU_UNIT_VIEW_SLOT_STARTED = 2 };

/* Starts the requested unit-view panel threads, stops stale panels, and restarts
 * them after they exit. A new parameter replaces the pending view request. */
static void views_supervise(void) {
    s32 states[16];
    view_threads_t* set;
    s32 mode;
    s32 i;
    s32 task;
    thread_t* thread;

    for (;;) {
        mode = thread_current_parameter_1_get();
        set = &g_view_threads[mode];
        g_unit_view_mode = mode;
        g_thread_contexts[g_thread_current_id].function_parameter_1 = 0xff;
        for (i = 15; i >= 0; i--) {
            states[i] = MENU_UNIT_VIEW_SLOT_IDLE;
        }
        for (i = 0; i < 4; i++) {
            thread = &g_thread_contexts[set->thread_ids[i]];
            if (thread->is_running != 0) {
                task = thread->task_id;
                if (task == set->task_ids[i]
                    && (task != THREAD_TASK_HOVERED_UNIT_STATS
                        || g_unit_view_cached_battle_id == g_unit_panel_selected_unit.battle_id)
                    && (task != THREAD_TASK_RIGHT_UNIT_STATS
                        || g_unit_view_cached_comparison_id == g_unit_panel_comparison_unit_data.battle_id)
                    && (task != THREAD_TASK_UNIT_SUMMARY_PANEL
                        || g_unit_view_cached_battle_id == g_unit_panel_selected_unit.battle_id)) {
                    continue;
                }
                states[set->thread_ids[i]] = MENU_UNIT_VIEW_SLOT_RESTART;
                thread_parameters_set(set->thread_ids[i], 0, 0, 1);
            } else if (set->entries[i] != UNIT_VIEW_NO_THREAD) {
                thread_start(set->thread_ids[i], set->entries[i]);
                thread_parameters_set(set->thread_ids[i], 0, 0, 0);
                thread->task_id = set->task_ids[i];
                states[set->thread_ids[i]] = MENU_UNIT_VIEW_SLOT_STARTED;
            }
        }
        g_unit_view_cached_battle_id = g_unit_panel_selected_unit.battle_id;
        g_unit_view_cached_comparison_id = g_unit_panel_comparison_unit_data.battle_id;
        for (;;) {
            for (i = 0; i < 4; i++) {
                if (states[set->thread_ids[i]] == MENU_UNIT_VIEW_SLOT_RESTART
                    && g_thread_contexts[set->thread_ids[i]].is_running != 0) {
                    break;
                }
            }
            if (i == 4) {
                break;
            }
            thread_frames_wait(1);
        }
        for (i = 0; i < 4; i++) {
            if (states[set->thread_ids[i]] == MENU_UNIT_VIEW_SLOT_RESTART && set->entries[i] != UNIT_VIEW_NO_THREAD) {
                thread_start(set->thread_ids[i], set->entries[i]);
                thread_parameters_set(set->thread_ids[i], 0, 0, 0);
                g_thread_contexts[set->thread_ids[i]].task_id = set->task_ids[i];
            }
        }
        if (thread_current_parameter_1_get() == 0xff) {
            thread_current_exit();
        }
    }
}

/* Set the event speed, which also divides the input repeat delays; BATTLE
 * sets the vsync interval with it. */
void script_event_speed_set(s32 speed) {
    g_script_event_speed = speed;
#ifdef FFT_BATTLE
    system_vsync_interval_set(speed);
#endif
}

/* Yield until the four unit view panel threads (hovered and right unit stats, unit summary, tile
 * info) have stopped. */
void unit_view_panels_wait(void) {
    thread_inactive_wait(THREAD_SLOT_HOVERED_UNIT_STATS);
    thread_inactive_wait(THREAD_SLOT_RIGHT_UNIT_STATS);
    thread_inactive_wait(THREAD_SLOT_UNIT_SUMMARY_PANEL);
    thread_inactive_wait(THREAD_SLOT_TILE_INFO_PANEL);
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * unit_view_panels_wait and script_noop_5. Nothing on the disc
 * calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void script_noop_4(void) {
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_noop_4 and
 * script_frame_after_threads_stub. Nothing on the disc calls it or points at
 * it; the name is a neutral placeholder until a reference turns up. */
void script_noop_5(void) {
}

/* Empty. The event frame runner calls it each frame once the threads have run. */
void script_frame_after_threads_stub(void) {
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_frame_after_threads_stub
 * and script_noop_7. Nothing on the disc calls it or points at it; the name is a
 * neutral placeholder until a reference turns up. */
void script_noop_6(void) {
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_noop_6 and
 * flag_assign. Nothing on the disc calls it or points at it; the
 * name is a neutral placeholder until a reference turns up. */
void script_noop_7(void) {
}
