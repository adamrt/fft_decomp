/* Deployment resources, native menu activation, unit-view supervision, script camera and movement operations,
 * and scene subsystem initialization and reset. */

#include "fft/ability_list.h"
#include "fft/ai.h"
#include "fft/ai_command.h"
#include "fft/at_list.h"
#include "fft/battle_map.h"
#include "fft/battle_map_light.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/deploy.h"
#include "fft/effect_script.h"
#include "fft/equipment.h"
#include "fft/fade.h"
#include "fft/file.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_selection.h"
#include "fft/menu_window.h"
#include "fft/move.h"
#include "fft/music.h"
#include "fft/option.h"
#include "fft/overlay.h"
#include "fft/portrait.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/skillset.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/state.h"
#include "fft/status.h"
#include "fft/status_screen.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit.h"
#include "fft/unit_event.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "fft/unit_view.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Two bytes per battle_map_surface_type_e; all 64 pairs are 0x19, 0x19. */
extern u8 g_terrain_tile_info_pairs[64][2];
extern s32 g_camera_cross_product_overflow; /* set by the Q12 cross product on overflow */
extern const char g_script_pause_message[];
extern s16 g_script_debug_paused;
extern u16 g_unit_view_cached_battle_id;
extern u16 g_unit_view_cached_comparison_id;

/* Set while a menu thread forces event speed 2; the previous speed is saved. */
extern u16 g_menu_vsync_interval_forced;
extern u16 g_menu_saved_vsync_interval;
extern u8 g_menu_panel_fade_modes[];
extern u8 g_text_glyph_bitmaps[];
extern char g_text_reading_character_message[]; /* "reading charactor%d \n" */

/* Provisional camera interpolation key: frame time and target value. */
typedef struct camera_key {
    s32 time;  /* 0x00; cumulative frame of this key */
    s32 value; /* 0x04 */
    u8 _unused_08[8];
} camera_key_t;
typedef char camera_key_size_must_be_16[sizeof(camera_key_t) == 16 ? 1 : -1];

/* Provisional per-component camera fusion track (0xa4 bytes), seeded by
 * camera_fusion_thread and advanced by its camera_step_track. */
typedef struct camera_track {
    camera_key_t keys[8]; /* 0x00 */
    s32 key_count;        /* 0x80 */
    s32 segment;          /* 0x84; first key of the active segment */
    s32 frame;            /* 0x88; frames since the track started */
    s32 segment_frame;    /* 0x8c; frames into the active segment */
    s32 prev_time;        /* 0x90; Q12 */
    s32 prev_value;       /* 0x94; Q12 */
    s32 start_value;      /* 0x98; Q12 */
    s32 segment_duration; /* 0x9c */
    s32 finished;         /* 0xa0 */
} camera_track_t;
typedef char camera_track_size_must_be_164[sizeof(camera_track_t) == 164 ? 1 : -1];

/* Desired threads for one unit-view mode: four native entrypoints, scheduler slots, and task IDs.
 * UNIT_VIEW_NO_THREAD leaves that slot stopped after any previous task exits. */
#define UNIT_VIEW_NO_THREAD ((void (*)(void)) - 1)
typedef struct unit_view_thread_set {
    void (*entries[4])(void); /* 0x00 */
    s32 thread_ids[4];        /* 0x10 */
    s32 task_ids[4];          /* 0x20 */
} unit_view_thread_set_t;
typedef char unit_view_thread_set_size_must_be_48[(sizeof(unit_view_thread_set_t) == 48) ? 1 : -1];

static unit_view_thread_set_t g_unit_view_thread_sets[7] = {
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

static s32 camera_step_track(camera_track_t* track, s32 component);
static void battle_map_store_selected_tile_data(void);
static void menu_init_subsystems(void);
static void unit_view_supervisor_thread(void);
static s32 script_interpolate_range_q12(s32 start, s32 end, s32 fraction, s32 value);
static void unit_reset_animation_states(void);

/* Load ATTACK.OUT (unless preloaded) with its scenario conditions and graphics, reset menu
 * and unit state and, for a tutorial, fix the game options. Then start the deployment
 * screen and return 0, or start the map-load thread and return 1 when deployment is skipped. */
s32 deploy_open(void) {
    s32 result;
    s32 return_value;
    game_options_fields_t* options;
    g_unit_panel_hide_numeric_values = 0;
    menu_init_subsystems();
    unit_reset_animation_states();
    command_reset_unit_records();
    if (g_script_attack_entry_mode == SCRIPT_ATTACK_ENTRY_PRELOADED) {
        g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_PRELOAD_USED;
    } else {
        g_thread_call_target = file_load_checked_to_address;
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
        music_play_tracks(MUSIC_DEPLOYMENT, MUSIC_NONE);
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
    thread_set_parameters(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[MENU_COMMAND_ID_MINI_MENU], 0, 0);
    g_thread_contexts[THREAD_SLOT_MENU].task_id = THREAD_TASK_MINI_MENU;
}

#ifdef FFT_BATTLE
/* Report whether the menu layer is busy.
 *
 * Returns g_menu_transition_state when it is nonzero (4 during the unit
 * list fade-out), 2 while the menu-building thread runs or while menu input is
 * blocked, 4 when one of threads 4-9 is running, and 0 when all are idle. The `i
 * != 3` test inside the scan of threads 4-9 always passes. */
s32 menu_get_thread_activity(void) {
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
s32 menu_get_thread_activity(void) {
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
void at_list_finish_selection(void) {
    g_at_list_selection_finished = 1;
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * at_list_finish_selection and menu_noop_6. Nothing on the disc calls it
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
        thread_start(THREAD_SLOT_UNIT_VIEW, unit_view_supervisor_thread);
    }
    thread_set_parameters(THREAD_SLOT_UNIT_VIEW, mode, 0, 0);
}

enum { MENU_UNIT_VIEW_SLOT_IDLE = 0, MENU_UNIT_VIEW_SLOT_RESTART = 1, MENU_UNIT_VIEW_SLOT_STARTED = 2 };

/* Starts the requested unit-view panel threads, stops stale panels, and restarts
 * them after they exit. A new parameter replaces the pending view request. */
static void unit_view_supervisor_thread(void) {
    s32 states[16];
    unit_view_thread_set_t* set;
    s32 mode;
    s32 i;
    s32 task;
    thread_t* thread;

    for (;;) {
        mode = thread_get_current_parameter_1();
        set = &g_unit_view_thread_sets[mode];
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
                thread_set_parameters(set->thread_ids[i], 0, 0, 1);
            } else if (set->entries[i] != UNIT_VIEW_NO_THREAD) {
                thread_start(set->thread_ids[i], set->entries[i]);
                thread_set_parameters(set->thread_ids[i], 0, 0, 0);
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
            thread_wait_frames(1);
        }
        for (i = 0; i < 4; i++) {
            if (states[set->thread_ids[i]] == MENU_UNIT_VIEW_SLOT_RESTART && set->entries[i] != UNIT_VIEW_NO_THREAD) {
                thread_start(set->thread_ids[i], set->entries[i]);
                thread_set_parameters(set->thread_ids[i], 0, 0, 0);
                g_thread_contexts[set->thread_ids[i]].task_id = set->task_ids[i];
            }
        }
        if (thread_get_current_parameter_1() == 0xff) {
            thread_exit_current();
        }
    }
}

/* Set the event speed, which also divides the input repeat delays; BATTLE
 * sets the vsync interval with it. */
void script_set_event_speed(s32 speed) {
    g_script_event_speed = speed;
#ifdef FFT_BATTLE
    system_set_vsync_interval(speed);
#endif
}

/* Yield until the four unit view panel threads (hovered and right unit stats, unit summary, tile
 * info) have stopped. */
void unit_view_wait_for_panels(void) {
    thread_wait_until_inactive(THREAD_SLOT_HOVERED_UNIT_STATS);
    thread_wait_until_inactive(THREAD_SLOT_RIGHT_UNIT_STATS);
    thread_wait_until_inactive(THREAD_SLOT_UNIT_SUMMARY_PANEL);
    thread_wait_until_inactive(THREAD_SLOT_TILE_INFO_PANEL);
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * unit_view_wait_for_panels and script_noop_5. Nothing on the disc
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
 * script_assign_bitset_flag. Nothing on the disc calls it or points at it; the
 * name is a neutral placeholder until a reference turns up. */
void script_noop_7(void) {
}

/* Set or clear one flag in a 32-bit-word bitmap. */
static void script_assign_bitset_flag(u32* bits, s32 index, s32 set) {
    s32 word = index / 32;
    u32 mask = 1U << (index & 0x1F);

    if (set != 0) {
        bits[word] |= mask;
    } else {
        bits[word] &= ~mask;
    }
}

/* Store five day bits and four month bits in the indexed nine-bit date record. */
void script_opcode_85_unlock_date(u32* bits, s32 record, s32 month, s32 day) {
    s32 base = record * 9;
    s32 i;
    u8 mask = 1;

    for (i = 0; i < 5; i++) {
        script_assign_bitset_flag(bits, base + i, day & mask);
        mask <<= 1;
    }
    mask = 1;
    for (i = 0; i < 4; i++) {
        s32 bit = i + 5;
        script_assign_bitset_flag(bits, base + bit, month & mask);
        mask <<= 1;
    }
}

/* CameraFusion thread: take the CAMERA instructions up to the end marker as keyframes (key 0 is
 * the current camera; Focus, RelativeCamera and FocusSpeed between them adjust the values), then
 * move the camera along a smooth path through them, one frame per yield, until the tilt track
 * finishes. */
void camera_fusion_thread(void) {
    camera_track_t* tracks;
    camera_track_t* track;
    camera_key_t* key;
    u8* script;
    u8* cursor;
    s32 values[EVENT_CAMERA_COMPONENT_COUNT];
    s32 key_count;
    s32 key_index;
    s32 i;
    s16 value;
    s32 component;
    s32 opcode;

    thread_set_current_task_id(THREAD_TASK_CAMERA);
    tracks = menu_alloc_memory(sizeof(camera_track_t) * EVENT_CAMERA_COMPONENT_COUNT);
    script = (u8*)thread_get_current_parameter_1();
    menu_clear_render_buffer(tracks, sizeof(camera_track_t) * EVENT_CAMERA_COMPONENT_COUNT);

    /* One key per CAMERA instruction up to the end marker, plus the starting key. */
    cursor = script;
    key_count = 0;
    for (;;) {
        opcode = *cursor;
        if (opcode == EVENT_OPCODE_CAMERA) {
            key_count++;
        }
        if (opcode == EVENT_OPCODE_CAMERA_FUSION_END) {
            break;
        }
        cursor += g_script_event_instruction_sizes[opcode] + 1;
    }
    key_count++;

    cursor = script;
    camera_store_yaw_band(g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW]);

    /* Key 0 is the camera as it stands. Each later key is the next CAMERA
     * instruction, after the Focus, RelativeCamera and FocusSpeed instructions
     * in front of it have run. */
    for (key_index = 0; key_index < key_count; key_index++) {
        if (key_index == 0) {
            for (i = 0; i < EVENT_CAMERA_COMPONENT_COUNT; i++) {
                track = &tracks[i];
                track->keys[key_index].value = g_script_variables[g_camera_script_variable_indices[i]];
                values[i] = track->keys[key_index].value;
                if (i < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                    track->keys[key_index].value = track->keys[key_index].value / 1024;
                }
                track->keys[key_index].time = 0;
                track->key_count = key_count;
            }
        } else {
            for (;;) {
                if (*cursor == EVENT_OPCODE_FOCUS) {
                    script_focus((event_focus_parameters_t*)(cursor + 1));
                } else if (*cursor == EVENT_OPCODE_RELATIVE_CAMERA) {
                    script_apply_relative_camera((event_camera_values_t*)(cursor + 1), values);
                } else if (*cursor == EVENT_OPCODE_FOCUS_SPEED) {
                    script_focus_speed(cursor + 1, values, values + EVENT_CAMERA_COMPONENT_TILT);
                } else if (*cursor == EVENT_OPCODE_CAMERA) {
                    for (i = 0; i < EVENT_CAMERA_COMPONENT_COUNT; i++) {
                        /* Indexed field access replaces retail's two additions with a shift. */
                        value
                            = script_read_s16(cursor + i + i + offsetof(event_camera_instruction_t, parameters.values));
                        if (value == CAMERA_FUSION_HOLD_SENTINEL) {
                            if (i < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                                value = values[i] / 1024;
                            } else {
                                value = values[i];
                            }
                        }
                        key = &tracks[i].keys[key_index];
                        key->value = value;
                        key->time = script_read_s16(((event_camera_instruction_t*)cursor)->parameters.duration_le);
                        key->time += tracks[i].keys[key_index - 1].time;
                        component = key->value;
                        values[i] = component;
                        if (i < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                            values[i] = component << 10;
                        }
                        if (i == EVENT_CAMERA_COMPONENT_YAW) {
                            camera_store_yaw_band(key->value);
                        }
                    }
                    break;
                }
                cursor += g_script_event_instruction_sizes[*cursor] + 1;
            }
            cursor += g_script_event_instruction_sizes[*cursor] + 1;
        }
    }

    /* Play the tracks; the three position tracks stand still while the position is locked. */
    for (;;) {
        track = tracks;
        for (key_index = 0; key_index < EVENT_CAMERA_POSITION_COMPONENT_COUNT; key_index++, track++) {
            if (g_camera_position_locked == 0) {
                g_script_variables[g_camera_script_variable_indices[key_index]] = camera_step_track(track, key_index);
            }
        }
        for (key_index = EVENT_CAMERA_COMPONENT_TILT; key_index < EVENT_CAMERA_COMPONENT_COUNT; key_index++, track++) {
            g_script_variables[g_camera_script_variable_indices[key_index]] = camera_step_track(track, key_index);
        }
        if (tracks[EVENT_CAMERA_COMPONENT_TILT].finished == 1) {
            break;
        }
        camera_clear_zoom_variable_low_bit();
        thread_yield();
    }

    menu_free_memory(tracks);
    thread_exit_current();
}

/* Advances one camera-track component by a frame and returns its new value.
 * Called once for each component by the scene's camera-fusion thread.
 *
 * Each call samples the two tangent lines of the active key segment at this
 * frame and the next, intersects them (fixed_cross_product_q12 is a Q12 cross product
 * that sets g_camera_cross_product_overflow on overflow) and moves the value along the resulting
 * control point. The locals span, frame and lead are reused across the
 * stages, as the target's register map requires. */
static s32 camera_step_track(camera_track_t* track, s32 component) {
    s32 time[4];  /* line 0: time[0]->time[1]; line 1: time[2]->time[3] */
    s32 value[4]; /* matching values for the two sampled lines */
    s32 value_delta0;
    s32 flag;
    s32 i;
    s32 frame;
    s32 span;
    s32 result;
    s32 segment;
    s32 start_value;
    s32 t0;
    s32 t1;
    s32 t2;
    s32 lead;
    s32 time_delta0;
    s32 time_delta1;
    s32 value_delta1;
    s32 divisor;
    s32 time0;
    s32 time1;
    s32 time2;
    s32 time3;
    s32 value0;
    s32 value1;
    s32 value2;
    s32 value3;

    /* Entering a segment: its length in frames and the value it starts from. */
    if (track->segment_frame == 0) {
        segment = track->segment;
        t2 = track->keys[segment + 2].time;
        t1 = track->keys[segment + 1].time;
        t0 = track->keys[segment].time;
        span = t2 - t1;
        lead = t1 - t0;
        frame = t2 - t0; /* whole two-key span */
        /* With four or more keys each segment runs between interval midpoints; the first
         * and last segments reach the end keys. */
        if (track->key_count >= 4) {
            if (segment == 0) {
                track->segment_duration = span / 2 + lead;
            } else if (segment == track->key_count - 3) {
                track->segment_duration = span + lead / 2;
            } else {
                track->segment_duration = frame / 2;
            }
        } else {
            track->segment_duration = frame;
        }

        start_value = track->keys[track->segment].value;
        track->prev_value = start_value;
        if (track->key_count >= 4 && track->segment != 0) {
            track->prev_value
                = (track->keys[track->segment + 1].value - track->keys[track->segment].value) / 2 + start_value;
        }
        track->prev_value *= ONE;
        track->prev_time = track->frame * ONE;
        track->start_value = track->prev_value;
    }

    /* Three equal keys hold their value. */
    if (track->keys[track->segment].value == track->keys[track->segment + 1].value
        && track->keys[track->segment].value == track->keys[track->segment + 2].value) {
        result = track->keys[track->segment].value * ONE;
    } else {
        /* Line i joins the points on the two key intervals at frame + i. */
        for (i = 0; i < 2; i++) {
            frame = i + track->segment_frame;
            /* After the first segment, the first interval is sampled from its midpoint. */
            if (track->segment == 0) {
                time[i * 2] = script_interpolate_range_q12(track->keys[track->segment].time,
                    track->keys[track->segment + 1].time, frame, track->segment_duration);
                value[i * 2] = script_interpolate_range_q12(track->keys[track->segment].value,
                    track->keys[track->segment + 1].value, frame, track->segment_duration);
            } else {
                time[i * 2] = script_interpolate_range_q12(track->keys[track->segment].time,
                                  track->keys[track->segment + 1].time, frame, track->segment_duration * 2)
                    - track->keys[track->segment].time
                    + (track->keys[track->segment + 1].time - track->keys[track->segment].time) / 2;
                value[i * 2] = script_interpolate_range_q12(track->keys[track->segment].value,
                                   track->keys[track->segment + 1].value, frame, track->segment_duration * 2)
                    - track->keys[track->segment].value
                    + (track->keys[track->segment + 1].value - track->keys[track->segment].value) / 2;
            }
            span = track->segment_duration;
            if (track->segment != track->key_count - 3) {
                span *= 2;
            }
            time[i * 2 + 1] = script_interpolate_range_q12(
                track->keys[track->segment + 1].time, track->keys[track->segment + 2].time, frame, span);
            value[i * 2 + 1] = script_interpolate_range_q12(
                track->keys[track->segment + 1].value, track->keys[track->segment + 2].value, frame, span);
        }

        flag = 0;
        g_camera_cross_product_overflow = 0;
        value1 = value[1];
        value0 = value[0];
        time3 = time[3];
        time2 = time[2];
        time0 = time[0];
        value3 = value[3];
        value2 = value[2];
        time1 = time[1];
        value_delta0 = value1 - value0;
        time_delta1 = time3 - time2;
        value_delta1 = value3 - value2;
        time_delta0 = time1 - time0;

        divisor = fixed_cross_product_q12(value_delta0, time_delta1, time_delta0, value_delta1);
        result = fixed_cross_product_q12(time_delta1, value2 - value0, value_delta1, time2 - time0);
        if (divisor != 0) {
            span = mul_div_s64(time_delta0, result, divisor) + time0;   /* crossing time */
            lead = mul_div_s64(value_delta0, result, divisor) + value0; /* crossing value */
            divisor = span - track->prev_time;
            if (divisor != 0) {
                result = mul_div_s64(lead - track->prev_value, (track->frame * ONE) - track->prev_time, divisor)
                    + track->prev_value;
                track->prev_time = span;
                track->prev_value = lead;
            } else {
                flag = 1;
            }
        } else {
            flag = 1;
        }

        /* Parallel lines, a crossing at the previous time or an overflow keep the previous value. */

        if (g_camera_cross_product_overflow != 0 || flag != 0) {
            result = track->prev_value;
        }
    }

    track->frame++;
    if (++track->segment_frame >= track->segment_duration) {
        track->segment_frame = 0;
        if (track->key_count - 3 < ++track->segment) {
            track->finished = 1;
        }
    }

    /* Angles and zoom round Q12 to the nearest integer; positions return in
     * the 1/1024 units of their script variables. */
    if (component >= EVENT_CAMERA_COMPONENT_TILT) {
        if (result > 0) {
            if ((result & 0xFFF) > 0x800) {
                result += ONE;
            }
        } else if ((result & 0xFFF) < 0x800) {
            result -= ONE;
        }
        return result / ONE;
    }
    return result / 4;
}

/* Linear interpolation from start to end at fraction / value, returned in 20.12
 * fixed point: start * ONE + (end - start) * fraction * ONE / value. */
static s32 script_interpolate_range_q12(s32 start, s32 end, s32 fraction, s32 value) {
    return mul_div_s64(end - start, fraction * ONE, value) + (start * ONE);
}

/* DebugPause event instruction (0x72): print the pause message with the debug font, yield once and
 * set g_script_debug_paused, which nothing reads. */
void script_print_debug_message(void) {
    FntPrint(g_script_pause_message);
    thread_yield();
    g_script_debug_paused = 1;
}

/* WalkTo: start the unit walking to the tile at the signed speed. A terrain_cost_flag of 1 makes
 * the path respect terrain costs; 0 ignores them. */
void script_opcode_28_walk_to(const event_walk_to_parameters_t* parameters) {
    s32 id = script_read_s16(parameters->unit_id_le);
    battle_map_tile_coordinates_t coordinates;

    id = script_resolve_runtime_id(id);
    if (id != EVENT_RUNTIME_ID_NONE) {
        coordinates.x = parameters->x;
        coordinates.y = parameters->y;
        coordinates.layer = parameters->elevation;
        move_start_walk_to(id, &coordinates,
            MOVE_PATHFIND_IGNORE_TERRAIN_COST - parameters->terrain_cost_flag * MOVE_PATHFIND_IGNORE_TERRAIN_COST, 3,
            script_read_s16(parameters->speed_le));
    }
}

/* Mirror the unit sprite for mode 1 and restore it for every other mode. */
void script_opcode_68_mirror_sprite(const event_mirror_sprite_parameters_t* parameters) {
    s32 mode;
    s32 runtime_id = script_read_s16(parameters->unit_id_le);

    mode = parameters->mode;
    runtime_id = script_resolve_runtime_id(runtime_id);
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        if (mode == 1) {
            sprite_set_horizontal_flip_flag(runtime_id);
        } else {
            sprite_clear_horizontal_flip_flag(runtime_id);
        }
    }
}

/* ResetPalette: resolve the unit id operand and reapply the weather, time-of-day
 * palette modulation without tile effects, unless the unit is missing. */
void script_opcode_97_reset_palette(const event_unit_parameters_t* parameters) {
    s32 runtime_id = script_resolve_runtime_id(script_read_s16(parameters->unit_id_le));
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        sprite_palette_reset_by_runtime_id(runtime_id);
    }
}

/* Jump: cancel the unit's accumulated event offset, then build its one-step path
 * of length_plus_one - 1 tiles in the encoded direction through
 * move_start_jump_by_runtime_id. The unit's Jump stat is 6 while that runs
 * and is restored afterwards. A unit id that does not resolve does nothing. */
void script_opcode_70_jump(const event_jump_parameters_t* parameters) {
    SVECTOR vec;
    s32 runtime_id = script_resolve_runtime_id(script_read_s16(parameters->unit_id_le));
    const SVECTOR* coords;
    unit_stats_t* stats;
    u8 saved;

    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        coords = unit_get_event_offset_by_runtime_id(runtime_id);
        setVector(&vec, -coords->vx, -coords->vy, -coords->vz);
        unit_add_event_offset_by_runtime_id(runtime_id, &vec);
        stats = unit_get_stats(unit_get_battle_id_by_runtime_id(runtime_id));
        saved = stats->jump;
        stats->jump = 6;
        move_start_jump_by_runtime_id(runtime_id, parameters->direction, parameters->length_plus_one - 1);
        stats->jump = saved;
    }
}

/* Yield at least once, then wait until the resolved unit stops moving. */
void script_opcode_29_wait_walk(s32 unit_id) {
    s32 runtime_id = script_resolve_runtime_id(unit_id);

    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        do {
            thread_yield();
        } while (move_is_unit_moving_by_runtime_id(runtime_id) != 0);
    }
}

/* Preload ATTACK.OUT when EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS is clear:
 * load companion executable 0xd in preload mode, look up the pending
 * scenario's special portrait and unit, and build the formation sprites and
 * portraits, so the later ATTACK entry skips reloading the overlay, sprites
 * and portraits. */
void script_opcode_82_load_attack_graphics(void) {
    if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
        g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_PRELOADED;
        overlay_request_companion(COMPANION_OVERLAY_ATTACK);
        thread_yield();
        scenario_load_conditionals();
        formation_build_sprites();
        portrait_load_party();
    }
}

/* Plays the current effect through the inner-subroutine caller, then yields
 * to other threads until the animation continue check reports done. */
void script_play_effect_and_wait(void) {
    g_thread_call_target = effect_call_play;
    thread_call_on_main_stack();

    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))state_get_animation_continue_check;
    } while (thread_call_on_main_stack());
}

/* Thread body of EffectEnd: play the effect that Effect and EffectStart prepared
 * and exit when it has finished. */
void script_opcode_8b_effect_end_thread(void) {
    thread_set_current_task_id(THREAD_TASK_PLAY_EFFECT);
    script_play_effect_and_wait();
    thread_exit_current();
}

/* Thread body of BlockStart: run the instructions up to BlockEnd in this thread, then exit.
 * Only a subset of the unit, wait, sound and effect instructions run here; any other
 * instruction in the block is skipped. */
void script_opcode_2a_block_start_thread(void) {
    /* Preserve the target's unused 40-byte stack area. */
    s32 unused[10];
    u8* script;
    u8* parameters;
    s16 first_halfword;
    s16 runtime_id;
    s32 thread_id;

    g_thread_contexts[g_thread_current_id].task_id = THREAD_TASK_EVENT_BLOCK;
    script = (u8*)thread_get_current_parameter_1();
    parameters = script + 1;
    while (script[0] != EVENT_OPCODE_BLOCK_END) {
        first_halfword = script_read_s16(parameters);
        if (script[0] == EVENT_OPCODE_CALL_FUNCTION) {
            if (script[1] == EVENT_CALL_FUNCTION_MUTE_TEXT_AUDIO_CUE) {
                script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 1);
            } else if (script[1] == EVENT_CALL_FUNCTION_UNMUTE_TEXT_AUDIO_CUE) {
                script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 0);
            }
        } else if (script[0] == EVENT_OPCODE_SOUND_EFFECT) {
            g_sound_pending_sfx_id = first_halfword;
        } else if (script[0] == EVENT_OPCODE_WARP_UNIT) {
            script_opcode_5f_warp_unit((const event_warp_unit_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_WAIT_VALUE) {
            script_opcode_7e_wait_value((const event_wait_value_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_DEBUG_PAUSE) {
            script_print_debug_message();
        } else if (script[0] == EVENT_OPCODE_WALK_TO) {
            g_thread_call_target = (void (*)(void))script_opcode_28_walk_to;
            thread_call_on_main_stack(parameters);
        } else if (script[0] == EVENT_OPCODE_WALK_TO_ANIM) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                g_unit_pending_animation_ids[runtime_id] = script_read_s16(script + 3);
            }
            /* Retail bug: this store runs even for EVENT_RUNTIME_ID_NONE (2000), far past the
             * 21-entry array. */
            g_unit_was_moving_latch[runtime_id] = 0;
        } else if (script[0] == EVENT_OPCODE_JUMP) {
            script_opcode_70_jump((const event_jump_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_WAIT_WALK) {
            script_opcode_29_wait_walk(first_halfword);
        } else if (script[0] == EVENT_OPCODE_UNIT_ANIM) {
            script_opcode_11_unit_anim((const event_unit_anim_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_MIRROR_SPRITE) {
            script_opcode_68_mirror_sprite((const event_mirror_sprite_parameters_t*)(script + 1));
        } else if (script[0] == EVENT_OPCODE_RESET_PALETTE) {
            script_opcode_97_reset_palette((const event_unit_parameters_t*)(script + 1));
        } else if (script[0] == EVENT_OPCODE_SET_FIRST_UNIT) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                unit_move_to_head(runtime_id);
            }
        } else if (script[0] == EVENT_OPCODE_ERASE) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                unit_hide_by_runtime_id(runtime_id);
            }
        } else if (script[0] == EVENT_OPCODE_DRAW) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                unit_draw_and_activate_by_runtime_id(runtime_id);
            }
        } else if (script[0] == EVENT_OPCODE_WAIT_UNIT_READY) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                script_wait_for_unit_ready(runtime_id);
            }
        } else if (script[0] == EVENT_OPCODE_WAIT) {
            thread_wait_frames(script_read_s16(script + 1));
        } else if (script[0] == EVENT_OPCODE_ROTATE_UNIT) {
            script_opcode_2d_rotate_unit((const event_rotate_unit_parameters_t*)(script + 1));
        } else if (script[0] == EVENT_OPCODE_FACE_UNIT) {
            script_opcode_2c_face_unit((const event_face_unit_parameters_t*)(script + 1), 1);
        } else if (script[0] == EVENT_OPCODE_SPRITE_MOVE || script[0] == EVENT_OPCODE_SPRITE_MOVE_BETA) {
            thread_id = thread_resolve_id_after_current(THREAD_SLOT_FIRST_IDLE);
            if (script[0] == EVENT_OPCODE_SPRITE_MOVE) {
                thread_start(thread_id, script_sprite_move);
            } else {
                thread_start(thread_id, script_sprite_move_beta);
            }
            g_thread_contexts[thread_id].function_parameter_1 = (s32)(script + 1);
            g_thread_contexts[thread_id].task_id = THREAD_TASK_SPRITE_MOVE;
            g_thread_contexts[thread_id].task_words[SPRITE_MOVE_THREAD_WORD_RUNTIME_ID]
                = script_resolve_runtime_id(first_halfword);
        } else if (script[0] == EVENT_OPCODE_WAIT_SPRITE_MOVE) {
            script_opcode_6f_wait_sprite_move(first_halfword);
        } else if (script[0] == EVENT_OPCODE_WAIT_ROTATE_UNIT) {
            script_opcode_65_wait_rotate_all(script[1]);
        } else if (script[0] == EVENT_OPCODE_WAIT_ROTATE_ALL) {
            script_opcode_65_wait_rotate_all(-1);
        } else if (script[0] == EVENT_OPCODE_UNIT_ANIM_ROTATE) {
            script_opcode_8c_unit_anim_rotate((const event_unit_anim_rotate_parameters_t*)(script + 1));
        } else if (script[0] == EVENT_OPCODE_COLOR_UNIT) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                sprite_palette_apply_modulation_to_units(
                    script[3], script[7], runtime_id, (s8)script[4], (s8)script[5], (s8)script[6]);
            }
        }
        script += 1 + g_script_event_instruction_sizes[script[0]];
        parameters = script + 1;
    }
    thread_exit_current();
}

/*
 * Event thread started by the AddUnitStart instruction: walks the following
 * script instructions through AddUnitEnd, running LoadAttackGraphics and each AddUnit.
 * A unit not yet on the field gets its status snapshot and its graphics loaded (the
 * instruction's flag passed on); one already on the field is drawn and activated unless the
 * flag is set, and its graphics refresh when the snapshot is new. Each AddUnit waits for the
 * graphics load to finish.
 */
void script_opcode_49_add_unit_start_thread(void) {
    s32 index;
    s32 slot;
    const u8* script;
    unit_stats_t* unit;
    s32 runtime_id;
    s16 unit_id;
    u8 opcode;
    u8 flag;

    g_thread_contexts[g_thread_current_id].task_id = THREAD_TASK_ADD_UNIT;
    slot = 0;
    script = (const u8*)thread_get_current_parameter_1();
    while (1) {
        unit_id = script_read_s16(script + 1);
        opcode = script[0];
        flag = script[3];
        if (opcode == EVENT_OPCODE_ADD_UNIT_END) {
            break;
        }
        if (opcode == EVENT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            script_opcode_82_load_attack_graphics();
        } else if (opcode == EVENT_OPCODE_ADD_UNIT) {
            unit = unit_entd_find_stats(unit_id, &index);
            if (index == -2 || index >= 0) {
                for (slot = 0; slot < UNIT_SLOT_COUNT; slot++) {
                    if (unit_get_stats(slot) == unit) {
                        break;
                    }
                }
                runtime_id = unit_get_runtime_id_by_battle_id(slot);
                if (runtime_id == -1) {
                    unit_save_staged_status_data(slot);
                    sprite_load_unit_graphics_by_battle_id(slot, flag);
                    g_unit_graphics_load_pending = 1;
                } else {
                    if (flag == 0) {
                        unit_draw_and_activate_by_runtime_id(runtime_id);
                    }
                    if (unit_save_staged_status_data(slot) != 0) {
                        g_thread_call_target = (void (*)(void))unit_update_graphics_by_runtime_id;
                        thread_call_on_main_stack(runtime_id);
                    }
                }
            }
            while (1) {
                thread_yield();
                if (g_unit_graphics_load_pending == 0) {
                    break;
                }
                if (g_debug_display_enabled != 0) {
                    FntPrint(g_text_reading_character_message, slot);
                }
            }
        }
        slot++;
        script += g_script_event_instruction_sizes[script[0]] + 1;
    }
    thread_exit_current();
}

/* Reset the menu library for a battle's setup: subsystem pointers, palette and text, the
 * menu subsystems, the script light transform and the panel fade tiles. */
static void menu_init_subsystems(void) {
    menu_init_subsystem_pointers();
    menu_init_palette_and_text(0);
    menu_init_stub();
    menu_reset_subsystems();
    battle_map_reset_script_light_transform();
    menu_init_panel_fade_tiles();
}

/* Bind the scene working pointers and clear its menu-session flags. */
void menu_init_subsystem_pointers(void) {
    g_threads = g_thread_contexts;
    g_text_glyph_bitmap_data = g_text_glyph_bitmaps;
    g_menu_glyph_sheet = &g_menu_glyph_image_data;
    g_script_event_block = &g_script_event_block_buffer;
    g_script_variables = g_script_variable_storage;
    g_menu_thread_menu_data = g_command_idle_action_entries;
    g_help_active_text_id_tables = (u16* const*)-1;
    g_unit_graphics_load_pending = 0;
    g_script_threads_idle = 0;
    g_script_frame_result_override = 0;
}

/* Reset what the menu layer shares: reseed the event script's random generator,
 * free every window buffer pair, the tpage-7 VRAM grid and the menu buffer arena,
 * and reset the thread scheduler. */
void menu_reset_subsystems(void) {
    script_seed_random_from_vsync();
    menu_reset_window_buffer_slots();
    gfx_clear_texture_allocation_grid();
    menu_init_buffer_allocator();
    thread_reset_scheduler();
}

/* Put the map light transform of the event scripts back to the map's own light: no rotation, unit
 * scale, and the current light direction and color matrices as the base battle_map_light_thread
 * interpolates from. WORLD has no map light, so its copy is empty. */
void battle_map_reset_script_light_transform(void) {
#ifdef FFT_BATTLE
    MATRIX* buffer;

    setVector(&g_script_light_angles, 0, 0, 0);
    setVector(&g_script_light_scale, ONE, ONE, ONE);
    buffer = battle_map_light_matrix_command(BATTLE_MAP_LIGHT_MATRIX_COMMAND_GET_LIGHT, buffer, buffer, 0);
    mem_copy_bytes_nonempty(
        &g_script_light_base_direction_matrix, buffer, sizeof(g_script_light_base_direction_matrix));
    mem_copy_bytes_nonempty(&g_script_light_base_color_matrix,
        battle_map_light_matrix_command(BATTLE_MAP_LIGHT_MATRIX_COMMAND_GET_COLOR, buffer, buffer, 0),
        sizeof(g_script_light_base_color_matrix));
#endif
}

/* Stop every unit's event rotation and pending animation, and record each present unit's
 * facing as its initial facing (0xff for an empty slot). */
static void unit_reset_animation_states(void) {
    unit_animation_state_t* state;
    s32 i = 0;

    state = g_unit_animation_states;
    for (; i < UNIT_SLOT_COUNT; i++, state++) {
        state->rotating = 0;
        state->delay = 0;
        if (unit_has_runtime_id(i) != 0) {
            state->initial_facing = unit_facing_get_sixteenth_by_runtime_id(i);
        } else {
            state->initial_facing = 0xff;
        }
        g_unit_pending_animation_ids[i] = 0;
        g_unit_was_moving_latch[i] = 0;
    }
}

/* Per frame, apply each unit's pending animation on the frame it stops moving
 * and step each rotating unit one facing (of 16) toward its target, after its
 * delay and once every frames_per_step frames. The shortest mode turns
 * whichever way is nearer; the other two modes always increment or decrement.
 * Nothing calls it in WORLD, where its caller script_process_pending_requests
 * is empty. */
void unit_update_facing_and_pending_animations(void) {
    s32 i;
    s32 facing;
    s32 current;
    s32 next;
    s32 moving;
    unit_animation_state_t* state = g_unit_animation_states;

    for (i = 0; i < UNIT_SLOT_COUNT; i++, state++) {
        if (unit_has_runtime_id(i) != 0) {
            if (g_unit_pending_animation_ids[i] != 0) {
                moving = move_is_unit_moving_by_runtime_id(i);
                if (moving == 0 && g_unit_was_moving_latch[i] != 0) {
                    unit_set_animation(i, g_unit_pending_animation_ids[i]);
                    g_unit_pending_animation_ids[i] = 0;
                }
                g_unit_was_moving_latch[i] = moving;
            }
            current = unit_facing_get_sixteenth_by_runtime_id(i);
            if (state->rotating == 0) {
                continue;
            }
            if (current == state->target_facing) {
                state->rotating = 0;
                continue;
            }
            state->step_counter++;
            if (state->delay != 0) {
                state->delay--;
                continue;
            }
            if (state->step_counter < state->frames_per_step) {
                continue;
            }
            state->step_counter = 0;
            if (state->rotate_mode == UNIT_ROTATE_MODE_SHORTEST) {
                moving = state->target_facing - current + 0x10;
                facing = current - state->target_facing + 0x10;
                moving &= 0xF;
                facing &= 0xF;
                if (facing < moving) {
                    next = current + 0xF;
                } else {
                    next = current + 1;
                }
                facing = next & 0xF;
            } else if (state->rotate_mode == UNIT_ROTATE_MODE_INCREMENT) {
                facing = (current + 1) & 0xF;
            } else if (state->rotate_mode == UNIT_ROTATE_MODE_DECREMENT) {
                facing = (current + 0xF) & 0xF;
            }
            unit_set_facing(i, facing);
        } else {
            g_unit_was_moving_latch[i] = 0;
            g_unit_pending_animation_ids[i] = 0;
        }
    }
}

/* Reset the unit animation states and, when the current-event variable is set,
 * fade the screen color modulation to zero and start the event thread (slot 1)
 * on script_run_next_event.
 *
 * While the battle has not started it first stores the camera in the script
 * variables and resets the script light transform. Where WORLD's copy calls two
 * empty functions, BATTLE does that camera work. */
void script_start_current_event_thread(void) {
    unit_reset_animation_states();
    g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_NORMAL;
    if (script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
        camera_store_state_to_script_variables();
        battle_map_reset_script_light_transform();
        g_camera_position_locked = 0;
        g_camera_speed_curve = 0;
    }
    if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != 0) {
        g_camera_position_locked = 0;
        g_camera_speed_curve = 0;
        fade_tint_start_fade(2, 0, 0, 0, 2);
        thread_start(THREAD_SLOT_EVENT, script_run_next_event);
    }
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * script_start_current_event_thread and script_reset_event_state. Nothing on
 * the disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void script_noop_8(void) {
}

/* Reset unit animation state; while the event slot is idle, run the scene
 * camera/light reset hooks and clear camera interpolation and position lock. */
void script_reset_event_state(void) {
    unit_reset_animation_states();
    if (thread_is_running_asm(THREAD_SLOT_EVENT) == 0) {
        camera_store_state_to_script_variables();
        battle_map_reset_script_light_transform();
        g_camera_speed_curve = 0;
        g_camera_position_locked = 0;
    }
}

/* Run one event frame, then return whether the deployment thread (borrowed slot
 * 6) is still running. */
s32 deploy_run_frame_and_check(u32* ot, u32 buttons) {
    script_run_event_frame(ot, buttons);
    return thread_is_running(THREAD_BORROWED_SLOT_DEPLOYMENT);
}

/* Run one menu and event frame and return the resolved menu selection. It records the cursor
 * tile's info, clears the pending selections, and forces event speed 2 while a command menu other
 * than the mini menu is open, restoring the saved speed afterwards. Without player input it uses
 * the unfiltered script input. */
s32 menu_update_menus_and_event_speed(u32* otag, s32 controller_input) {
    s32 i;
    s32 result;
    s32 fill;

    g_ai_workspace_ptr = (event_work_t*)g_ai_workspace;
    if (g_menu_input_mode != MENU_INPUT_PLAYER) {
        controller_input = g_script_unfiltered_controller_input;
    }
    battle_map_store_selected_tile_data();
    fill = -2;
    for (i = 3; i >= 0; i--) {
        g_command_pending_selection[i] = fill;
    }
    script_run_event_frame(otag, controller_input);
    menu_update_panel_fade();
    result = command_resolve_selection();
    if ((thread_is_running_asm(THREAD_SLOT_MENU) != 0
            && g_thread_contexts[THREAD_SLOT_MENU].task_id != THREAD_TASK_MINI_MENU)
        || thread_is_running_asm(THREAD_SLOT_MENU_CHILD) != 0
        || thread_is_running_asm(THREAD_SLOT_MENU_GRANDCHILD) != 0) {
        if (g_menu_vsync_interval_forced == 0) {
            g_menu_vsync_interval_forced = 1;
            g_menu_saved_vsync_interval = system_get_vsync_interval();
            script_set_event_speed(2);
        }
    } else if (g_menu_vsync_interval_forced == 1) {
        g_menu_vsync_interval_forced = 0;
        script_set_event_speed(g_menu_saved_vsync_interval);
    }
    return result;
}

/* Fill g_battle_map_selected_tile_data for the tile under the cursor, which the
 * tile information display shows: the surface type, the depth, the displayed
 * height in half-height units (height * 2 + slope half-height + depth * 2) and
 * the surface type's two bytes of g_terrain_tile_info_pairs. */
static void battle_map_store_selected_tile_data(void) {
    SVECTOR coords;
    s16* surface_type;
    s32 depth;

    battle_map_store_selected_tile_coordinates(&coords);
    surface_type = &g_battle_map_selected_tile_data.surface_type;
    *surface_type
        = battle_map_tile_get_value(BATTLE_MAP_TILE_DATA_SURFACE_TYPE, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_battle_map_selected_tile_data.display_height
        = battle_map_tile_get_value(BATTLE_MAP_TILE_DATA_HALF_HEIGHT, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_battle_map_selected_tile_data.display_height
        += (battle_map_tile_get_value(BATTLE_MAP_TILE_DATA_HEIGHT, coords.vx, coords.vz, coords.vy) & 0xFF) * 2;
    depth = battle_map_tile_get_value(BATTLE_MAP_TILE_DATA_DEPTH, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_battle_map_selected_tile_data.depth = depth;
    g_battle_map_selected_tile_data.display_height += depth * 2;
    g_battle_map_selected_tile_data._unknown_02 = g_terrain_tile_info_pairs[*surface_type][0];
    g_battle_map_selected_tile_data._unknown_04 = g_terrain_tile_info_pairs[*surface_type][1];
}
