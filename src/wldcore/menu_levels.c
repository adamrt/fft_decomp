/* The world map runs one stack of menu levels. Its root handles travel and map requests; message windows, location
 * services, status Help and debug editors suspend and resume that same stack. */

#include "fft/bar.h"
#include "fft/brave_story.h"
#include "fft/calendar.h"
#include "fft/display_object.h"
#include "fft/encounter.h"
#include "fft/file.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/input.h"
#include "fft/location.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/menu_window.h"
#include "fft/message.h"
#include "fft/music.h"
#include "fft/proposition.h"
#include "fft/save.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/shop.h"
#include "fft/sound.h"
#include "fft/sound_novel.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit_panel.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef struct deep_dungeon_battle {
    u8 map_id;   /* 0x00 */
    u8 squad_id; /* 0x01: deployment zone id */
    u8 entd_ids[8];
} deep_dungeon_battle_t;

/* The horizontal and vertical map scroll axes. */
extern world_map_scroll_axis_t g_world_map_scroll_x;
extern world_map_scroll_axis_t g_world_map_scroll_y;

menu_stack_level_e g_menu_stack_types[MENU_STACK_CAPACITY];
extern u32* g_location_picture_frame_tim;
extern deep_dungeon_battle_t* g_deep_dungeon_battles;
extern help_navigation_record_t g_unit_panel_help_rows[];

static void cursor_level_open(void);
static void cursor_level_resume(menu_stack_record_t* level);
static void cursor_level_update(menu_stack_record_t* level);
static void focus_level_open(s32 dot_index);
static void focus_level_update(menu_stack_record_t* entry);
static void path_level_open(s32 from, s32 to, s32 erase);
static void path_level_update(menu_stack_record_t* level);
static void visibility_level_open(s32 dot_index, s32 erase);
static void visibility_level_update(menu_stack_record_t* level);
static void map_menu_return_update(menu_stack_record_t* level);
static void text_level_update(menu_stack_record_t* level);
static void message_level_update(menu_stack_record_t* level);
static void variable_list_open(void);
static void variable_list_draw(debug_variable_list_level_t* level);
static void variable_list_update(menu_stack_record_t* level);
static void variable_edit_open(s32 variable_id, s32 value);
static void variable_edit_update(menu_stack_record_t* level);
static void help_cursor_move(unit_panel_help_level_t* request);
static void panel_help_resume(menu_stack_record_t* level);
static void panel_help_suspend(menu_stack_record_t* unused_level);
static void panel_help_update(menu_stack_record_t* request);
static void location_menu_resume(menu_stack_record_t* level);
static void location_menu_suspend(menu_stack_record_t* slots);
static void location_menu_update(menu_stack_record_t* level);
static void level_records_release(menu_stack_slot_set_t* slots, s32 restore);
static void location_shop_run(s32 shop_id);
static void scrollable_text_level_update(menu_stack_record_t* level);
static void party_number_open(void);
static void party_number_update(menu_stack_record_t* level);

/* Indexed by menu_stack_level_e. */
static void (*s_level_update_handlers[MENU_STACK_LEVEL_COUNT])(menu_stack_record_t*) = {
    cursor_level_update,                           /* MENU_STACK_LEVEL_WORLD_MAP */
    map_menu_return_update,                        /* MENU_STACK_LEVEL_WORLD_MENU */
    text_level_update,                             /* MENU_STACK_LEVEL_TEXT_WINDOW */
    message_level_update,                          /* MENU_STACK_LEVEL_MESSAGE */
    location_menu_update,                          /* MENU_STACK_LEVEL_LOCATION */
    bar_menu_update,                               /* MENU_STACK_LEVEL_BAR */
    rumor_list_input_update,                       /* MENU_STACK_LEVEL_RUMOR_LIST */
    proposition_available_list_input_update,       /* MENU_STACK_LEVEL_PROPOSITION_AVAILABLE_LIST */
    rumor_wait_detail_then_pop,                    /* MENU_STACK_LEVEL_RUMOR_DETAIL */
    proposition_detail_update,                     /* MENU_STACK_LEVEL_PROPOSITION_DETAIL */
    proposition_accept_update,                     /* MENU_STACK_LEVEL_PROPOSITION_ACCEPT */
    variable_list_update,                          /* MENU_STACK_LEVEL_DEBUG_VARIABLE_LIST */
    proposition_days_update,                       /* MENU_STACK_LEVEL_PROPOSITION_DAYS */
    proposition_dispatch_confirm_update,           /* MENU_STACK_LEVEL_PROPOSITION_DISPATCH_CONFIRM */
    proposition_returned_list_input_update,        /* MENU_STACK_LEVEL_PROPOSITION_RETURNED_LIST */
    proposition_report_confirm_update,             /* MENU_STACK_LEVEL_PROPOSITION_REPORT_CONFIRM */
    proposition_report_update,                     /* MENU_STACK_LEVEL_PROPOSITION_REPORT */
    proposition_result_update,                     /* MENU_STACK_LEVEL_PROPOSITION_RESULT */
    proposition_active_list_input_update,          /* MENU_STACK_LEVEL_PROPOSITION_ACTIVE_LIST */
    proposition_recall_update,                     /* MENU_STACK_LEVEL_PROPOSITION_RECALL */
    sound_novel_reading_choice_update,             /* MENU_STACK_LEVEL_SOUND_NOVEL_RESTART */
    sound_novel_reader_update,                     /* MENU_STACK_LEVEL_SOUND_NOVEL */
    sound_novel_quit_update,                       /* MENU_STACK_LEVEL_SOUND_NOVEL_QUIT */
    sound_novel_history_update,                    /* MENU_STACK_LEVEL_SOUND_NOVEL_HISTORY */
    brave_story_unexplored_land_list_input_update, /* MENU_STACK_LEVEL_BRAVE_STORY_UNEXPLORED_LANDS */
    brave_story_unexplored_land_level_update,      /* MENU_STACK_LEVEL_BRAVE_STORY_UNEXPLORED_LAND */
    brave_story_treasure_list_input_update,        /* MENU_STACK_LEVEL_BRAVE_STORY_TREASURES */
    brave_story_treasure_level_update,             /* MENU_STACK_LEVEL_BRAVE_STORY_TREASURE */
    brave_story_menu_update,                       /* MENU_STACK_LEVEL_BRAVE_STORY */
    brave_story_event_list_input_update,           /* MENU_STACK_LEVEL_BRAVE_STORY_EVENTS */
    brave_story_event_input_update,                /* MENU_STACK_LEVEL_BRAVE_STORY_EVENT */
    brave_story_person_list_input_update,          /* MENU_STACK_LEVEL_BRAVE_STORY_PERSONS */
    brave_story_person_level_update,               /* MENU_STACK_LEVEL_BRAVE_STORY_PERSON */
    tutorial_topic_list_input_update,              /* MENU_STACK_LEVEL_TUTORIAL_TOPICS */
    tutorial_wait_topic_then_pop,                  /* MENU_STACK_LEVEL_TUTORIAL_TOPIC */
    event_transition_update,                       /* MENU_STACK_LEVEL_EVENT_TRANSITION */
    proposition_participants_update,               /* MENU_STACK_LEVEL_PROPOSITION_SEND_UNIT */
    brave_story_propositions_input_update,         /* MENU_STACK_LEVEL_BRAVE_STORY_PROPOSITIONS */
    brave_story_advancing_list_input_update,       /* MENU_STACK_LEVEL_BRAVE_STORY_ADVANCING */
    brave_story_participant_list_input_update,     /* MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANTS */
    brave_story_participant_input_update,          /* MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANT */
    brave_story_work_history_input_update,         /* MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY */
    brave_story_wait_work_history_then_pop,        /* MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY_ENTRY */
    tutorial_menu_update,                          /* MENU_STACK_LEVEL_TUTORIAL */
    tutorial_formation_list_input_update,          /* MENU_STACK_LEVEL_TUTORIAL_FORMATION_LIST */
    tutorial_battle_list_input_update,             /* MENU_STACK_LEVEL_TUTORIAL_BATTLE_LIST */
    brave_story_fact_list_input_update,            /* MENU_STACK_LEVEL_BRAVE_STORY_FACTS */
    brave_story_wait_fact_then_pop,                /* MENU_STACK_LEVEL_BRAVE_STORY_FACT */
    variable_edit_update,                          /* MENU_STACK_LEVEL_DEBUG_VARIABLE_EDIT */
    screen_transition_update,                      /* MENU_STACK_LEVEL_SCREEN_TRANSITION */
    focus_level_update,                            /* MENU_STACK_LEVEL_WORLD_MAP_FOCUS */
    scrollable_text_level_update,                  /* MENU_STACK_LEVEL_SCROLLABLE_TEXT */
    party_number_update,                           /* MENU_STACK_LEVEL_DEBUG_PARTY_NUMBER */
    path_level_update,                             /* MENU_STACK_LEVEL_WORLD_MAP_PATH */
    visibility_level_update,                       /* MENU_STACK_LEVEL_WORLD_MAP_LOCATION_VISIBILITY */
    proposition_discovery_update,                  /* MENU_STACK_LEVEL_PROPOSITION_DISCOVERY */
    panel_help_update,                             /* MENU_STACK_LEVEL_UNIT_PANEL_HELP */
};

/* Indexed by menu_stack_level_e; zero entries preserve the original table. */
static void (*s_level_resume_handlers[MENU_STACK_LEVEL_COUNT])(menu_stack_record_t*) = {
    cursor_level_resume,                             /* MENU_STACK_LEVEL_WORLD_MAP */
    0,                                               /* MENU_STACK_LEVEL_WORLD_MENU */
    0,                                               /* MENU_STACK_LEVEL_TEXT_WINDOW */
    0,                                               /* MENU_STACK_LEVEL_MESSAGE */
    location_menu_resume,                            /* MENU_STACK_LEVEL_LOCATION */
    bar_menu_resume,                                 /* MENU_STACK_LEVEL_BAR */
    rumor_list_prompt_restore,                       /* MENU_STACK_LEVEL_RUMOR_LIST */
    proposition_available_list_prompt_restore,       /* MENU_STACK_LEVEL_PROPOSITION_AVAILABLE_LIST */
    0,                                               /* MENU_STACK_LEVEL_RUMOR_DETAIL */
    proposition_detail_level_resume,                 /* MENU_STACK_LEVEL_PROPOSITION_DETAIL */
    0,                                               /* MENU_STACK_LEVEL_PROPOSITION_ACCEPT */
    0,                                               /* MENU_STACK_LEVEL_DEBUG_VARIABLE_LIST */
    0,                                               /* MENU_STACK_LEVEL_PROPOSITION_DAYS */
    0,                                               /* MENU_STACK_LEVEL_PROPOSITION_DISPATCH_CONFIRM */
    proposition_returned_list_prompt_restore,        /* MENU_STACK_LEVEL_PROPOSITION_RETURNED_LIST */
    0,                                               /* MENU_STACK_LEVEL_PROPOSITION_REPORT_CONFIRM */
    0,                                               /* MENU_STACK_LEVEL_PROPOSITION_REPORT */
    0,                                               /* MENU_STACK_LEVEL_PROPOSITION_RESULT */
    proposition_active_list_prompt_restore,          /* MENU_STACK_LEVEL_PROPOSITION_ACTIVE_LIST */
    0,                                               /* MENU_STACK_LEVEL_PROPOSITION_RECALL */
    0,                                               /* MENU_STACK_LEVEL_SOUND_NOVEL_RESTART */
    sound_novel_reader_resume,                       /* MENU_STACK_LEVEL_SOUND_NOVEL */
    sound_novel_quit_resume,                         /* MENU_STACK_LEVEL_SOUND_NOVEL_QUIT */
    0,                                               /* MENU_STACK_LEVEL_SOUND_NOVEL_HISTORY */
    brave_story_unexplored_land_list_prompt_restore, /* MENU_STACK_LEVEL_BRAVE_STORY_UNEXPLORED_LANDS */
    0,                                               /* MENU_STACK_LEVEL_BRAVE_STORY_UNEXPLORED_LAND */
    brave_story_treasure_list_prompt_restore,        /* MENU_STACK_LEVEL_BRAVE_STORY_TREASURES */
    0,                                               /* MENU_STACK_LEVEL_BRAVE_STORY_TREASURE */
    brave_story_menu_resume,                         /* MENU_STACK_LEVEL_BRAVE_STORY */
    brave_story_event_list_prompt_restore,           /* MENU_STACK_LEVEL_BRAVE_STORY_EVENTS */
    0,                                               /* MENU_STACK_LEVEL_BRAVE_STORY_EVENT */
    brave_story_person_list_prompt_restore,          /* MENU_STACK_LEVEL_BRAVE_STORY_PERSONS */
    0,                                               /* MENU_STACK_LEVEL_BRAVE_STORY_PERSON */
    tutorial_topic_list_buttons_restore,             /* MENU_STACK_LEVEL_TUTORIAL_TOPICS */
    0,                                               /* MENU_STACK_LEVEL_TUTORIAL_TOPIC */
    0,                                               /* MENU_STACK_LEVEL_EVENT_TRANSITION */
    proposition_participants_resume,                 /* MENU_STACK_LEVEL_PROPOSITION_SEND_UNIT */
    brave_story_propositions_level_resume,           /* MENU_STACK_LEVEL_BRAVE_STORY_PROPOSITIONS */
    brave_story_advancing_list_resume,               /* MENU_STACK_LEVEL_BRAVE_STORY_ADVANCING */
    0,                                               /* MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANTS */
    brave_story_participant_level_resume,            /* MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANT */
    brave_story_work_history_resume,                 /* MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY */
    0,                                               /* MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY_ENTRY */
    tutorial_menu_resume,                            /* MENU_STACK_LEVEL_TUTORIAL */
    tutorial_formation_list_buttons_restore,         /* MENU_STACK_LEVEL_TUTORIAL_FORMATION_LIST */
    tutorial_battle_list_buttons_restore,            /* MENU_STACK_LEVEL_TUTORIAL_BATTLE_LIST */
    brave_story_fact_list_buttons_restore,           /* MENU_STACK_LEVEL_BRAVE_STORY_FACTS */
    0,                                               /* MENU_STACK_LEVEL_BRAVE_STORY_FACT */
    0,                                               /* MENU_STACK_LEVEL_DEBUG_VARIABLE_EDIT */
    0,                                               /* MENU_STACK_LEVEL_SCREEN_TRANSITION */
    0,                                               /* MENU_STACK_LEVEL_WORLD_MAP_FOCUS */
    0,                                               /* MENU_STACK_LEVEL_SCROLLABLE_TEXT */
    0,                                               /* MENU_STACK_LEVEL_DEBUG_PARTY_NUMBER */
    0,                                               /* MENU_STACK_LEVEL_WORLD_MAP_PATH */
    0,                                               /* MENU_STACK_LEVEL_WORLD_MAP_LOCATION_VISIBILITY */
    0,                                               /* MENU_STACK_LEVEL_PROPOSITION_DISCOVERY */
    panel_help_resume,                               /* MENU_STACK_LEVEL_UNIT_PANEL_HELP */
};

/* Indexed by menu_stack_level_e; zero entries preserve the original table. */
static void (*s_level_suspend_handlers[MENU_STACK_LEVEL_COUNT])(menu_stack_record_t*) = {
    world_map_cursor_level_suspend,                 /* MENU_STACK_LEVEL_WORLD_MAP */
    0,                                              /* MENU_STACK_LEVEL_WORLD_MENU */
    0,                                              /* MENU_STACK_LEVEL_TEXT_WINDOW */
    0,                                              /* MENU_STACK_LEVEL_MESSAGE */
    location_menu_suspend,                          /* MENU_STACK_LEVEL_LOCATION */
    bar_menu_suspend,                               /* MENU_STACK_LEVEL_BAR */
    rumor_block_list_buttons,                       /* MENU_STACK_LEVEL_RUMOR_LIST */
    proposition_block_available_list_buttons,       /* MENU_STACK_LEVEL_PROPOSITION_AVAILABLE_LIST */
    0,                                              /* MENU_STACK_LEVEL_RUMOR_DETAIL */
    proposition_detail_level_suspend,               /* MENU_STACK_LEVEL_PROPOSITION_DETAIL */
    0,                                              /* MENU_STACK_LEVEL_PROPOSITION_ACCEPT */
    0,                                              /* MENU_STACK_LEVEL_DEBUG_VARIABLE_LIST */
    0,                                              /* MENU_STACK_LEVEL_PROPOSITION_DAYS */
    0,                                              /* MENU_STACK_LEVEL_PROPOSITION_DISPATCH_CONFIRM */
    proposition_block_returned_list_buttons,        /* MENU_STACK_LEVEL_PROPOSITION_RETURNED_LIST */
    0,                                              /* MENU_STACK_LEVEL_PROPOSITION_REPORT_CONFIRM */
    0,                                              /* MENU_STACK_LEVEL_PROPOSITION_REPORT */
    0,                                              /* MENU_STACK_LEVEL_PROPOSITION_RESULT */
    proposition_block_active_list_buttons,          /* MENU_STACK_LEVEL_PROPOSITION_ACTIVE_LIST */
    0,                                              /* MENU_STACK_LEVEL_PROPOSITION_RECALL */
    0,                                              /* MENU_STACK_LEVEL_SOUND_NOVEL_RESTART */
    sound_novel_reader_suspend,                     /* MENU_STACK_LEVEL_SOUND_NOVEL */
    sound_novel_quit_suspend,                       /* MENU_STACK_LEVEL_SOUND_NOVEL_QUIT */
    0,                                              /* MENU_STACK_LEVEL_SOUND_NOVEL_HISTORY */
    brave_story_block_unexplored_land_list_buttons, /* MENU_STACK_LEVEL_BRAVE_STORY_UNEXPLORED_LANDS */
    0,                                              /* MENU_STACK_LEVEL_BRAVE_STORY_UNEXPLORED_LAND */
    brave_story_block_treasure_list_buttons,        /* MENU_STACK_LEVEL_BRAVE_STORY_TREASURES */
    0,                                              /* MENU_STACK_LEVEL_BRAVE_STORY_TREASURE */
    brave_story_menu_suspend,                       /* MENU_STACK_LEVEL_BRAVE_STORY */
    brave_story_block_event_list_buttons,           /* MENU_STACK_LEVEL_BRAVE_STORY_EVENTS */
    0,                                              /* MENU_STACK_LEVEL_BRAVE_STORY_EVENT */
    brave_story_block_person_list_buttons,          /* MENU_STACK_LEVEL_BRAVE_STORY_PERSONS */
    0,                                              /* MENU_STACK_LEVEL_BRAVE_STORY_PERSON */
    tutorial_block_topic_list_buttons,              /* MENU_STACK_LEVEL_TUTORIAL_TOPICS */
    0,                                              /* MENU_STACK_LEVEL_TUTORIAL_TOPIC */
    0,                                              /* MENU_STACK_LEVEL_EVENT_TRANSITION */
    proposition_participants_suspend,               /* MENU_STACK_LEVEL_PROPOSITION_SEND_UNIT */
    brave_story_propositions_level_suspend,         /* MENU_STACK_LEVEL_BRAVE_STORY_PROPOSITIONS */
    brave_story_advancing_list_suspend,             /* MENU_STACK_LEVEL_BRAVE_STORY_ADVANCING */
    0,                                              /* MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANTS */
    brave_story_participant_level_suspend,          /* MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANT */
    brave_story_work_history_suspend,               /* MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY */
    0,                                              /* MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY_ENTRY */
    tutorial_menu_suspend,                          /* MENU_STACK_LEVEL_TUTORIAL */
    tutorial_block_formation_list_buttons,          /* MENU_STACK_LEVEL_TUTORIAL_FORMATION_LIST */
    tutorial_block_battle_list_buttons,             /* MENU_STACK_LEVEL_TUTORIAL_BATTLE_LIST */
    brave_story_block_fact_list_buttons,            /* MENU_STACK_LEVEL_BRAVE_STORY_FACTS */
    0,                                              /* MENU_STACK_LEVEL_BRAVE_STORY_FACT */
    0,                                              /* MENU_STACK_LEVEL_DEBUG_VARIABLE_EDIT */
    0,                                              /* MENU_STACK_LEVEL_SCREEN_TRANSITION */
    0,                                              /* MENU_STACK_LEVEL_WORLD_MAP_FOCUS */
    0,                                              /* MENU_STACK_LEVEL_SCROLLABLE_TEXT */
    0,                                              /* MENU_STACK_LEVEL_DEBUG_PARTY_NUMBER */
    0,                                              /* MENU_STACK_LEVEL_WORLD_MAP_PATH */
    0,                                              /* MENU_STACK_LEVEL_WORLD_MAP_LOCATION_VISIBILITY */
    0,                                              /* MENU_STACK_LEVEL_PROPOSITION_DISCOVERY */
    panel_help_suspend,                             /* MENU_STACK_LEVEL_UNIT_PANEL_HELP */
};

/* Init step: load WORLD's menu text and unit portraits, build the formation unit name strings,
 * then apply and clear any day advance pending in EVENT_SCRIPT_VAR_DATE_ADVANCE. */
void world_map_menu_state_init(void) {
    s32 value;

    screen_world_resources_init();
    formation_roster_names_build();
    value = script_variable_get(EVENT_SCRIPT_VAR_DATE_ADVANCE);
    if (value != 0) {
        calendar_days_advance(value);
        script_variable_set(EVENT_SCRIPT_VAR_DATE_ADVANCE, 0);
    }
}

/*
 * Runs the WORLD menu code for one world-map frame, drawing into `context`.
 *
 * Skipped while WORLD_MAP_FLAG_SKIP_MENU_FRAME is set. While the unit panel
 * threads are in use it draws the window frames, runs the event input update
 * and plays the frame's sound effect itself; otherwise it runs the WORLD
 * screen/menu frame. Buttons in g_menu_blocked_buttons are withheld from
 * the menus.
 */
void world_map_menus_update(GsOT* context) {
    u32* otag_entries;
    u32* sprite_entries;
    u32* menu_entries;

    if (g_world_map_flags & WORLD_MAP_FLAG_SKIP_MENU_FRAME) {
        return;
    }
    formation_backdrop_otag_set((u32*)(context->org + 15));
    screen_allowed_buttons_set(~g_menu_blocked_buttons);
    if (g_world_map_flags & WORLD_MAP_FLAG_UNIT_PANEL_THREADS) {
        otag_entries = (u32*)context->org;
        g_gfx_current_otag_entry = otag_entries + g_menu_stack_ot_offset;
        menu_draw_active_window_frames();
        sprite_entries = (u32*)context->org;
        menu_input_event_state_update(
            sprite_entries + g_menu_stack_ot_offset + 2, g_input_current_buttons, g_gfx_active_buffer_index * 240);
        if (g_sound_pending_sfx_id != -1) {
            sound_play_effect(g_sound_pending_sfx_id);
        }
    } else {
        menu_entries = (u32*)context->org;
        screen_menu_frame_update(menu_entries + g_menu_stack_ot_offset, g_input_current_buttons);
    }
}

/* Empty the menu stack, push the root level and place its window at the
 * projected location marker (4 pixels above it). */
void menu_stack_reset(void) {
    g_menu_stack_depth = 0;
    cursor_level_open();
    g_display_object_animated_pool[g_menu_stack_records[0].window_index].position.x
        = g_world_map_marker_state.marker.render.position.x;
    g_display_object_animated_pool[g_menu_stack_records[0].window_index].position.y
        = g_world_map_marker_state.marker.render.position.y - 4;
}

/* Run the per-frame handler of the menu level on top of the stack, passing its level record. */
void menu_stack_dispatch_update(void) {
    s32 index = g_menu_stack_depth;

    s_level_update_handlers[g_menu_stack_types[index - 1]](&g_menu_stack_records[index - 1]);
}

/* Run the resume handler of the level now on top of the stack after the level above it closed:
 * list levels restore their prompt and blocked buttons, window levels their palettes and cursor. */
void menu_stack_dispatch_resume(void) {
    s32 index = g_menu_stack_depth;

    s_level_resume_handlers[g_menu_stack_types[index - 1]](&g_menu_stack_records[index - 1]);
}

/* Run the top level's suspend handler, called just before another level opens over it: list levels
 * withhold every button, window levels dim their windows (palettes 10, 6, 2 and the pressed
 * cursor). The resume handler undoes it. */
void menu_stack_dispatch_suspend(void) {
    s32 index = g_menu_stack_depth;

    s_level_suspend_handlers[g_menu_stack_types[index - 1]](&g_menu_stack_records[index - 1]);
}

/* Pushes a two-window menu level: appends two window records, points the new
 * menu-stack record at both, resets both scroll states and records the
 * current location (+1) for the level.
 *
 * The level record is re-read into its own local after the script call; reusing
 * the window-level depth local changes the register allocation. */
static void cursor_level_open(void) {
    display_object_position_stride_t* xy;
    display_object_animated_t* records;
    s32 window_index;
    s32 depth;
    s32 level;

    window_index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);

    /* Keep the position-column base separate from the record base. */
    xy = (display_object_position_stride_t*)&g_display_object_animated_pool[0].position;
    records = g_display_object_animated_pool;

    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].map_cursor.window_index = window_index;
    g_display_object_animated_pool[window_index].sequence = DISPLAY_OBJECT_ANIM_CURSOR_STILL;
    g_display_object_animated_pool[window_index].priority = 12;
    xy[window_index].x = 0;
    xy[window_index].y = 0;
    records[window_index].anim_counter = 0;
    g_display_object_animated_pool[window_index].frame_index = 0;

    window_index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    depth = g_menu_stack_depth;
    g_hud_value_mode = HUD_VALUE_FUNDS;
    g_menu_stack_records[depth].map_cursor.alt_window_index = window_index;
    g_display_object_animated_pool[window_index].sequence = DISPLAY_OBJECT_ANIM_MAP_EDGE_ARROWS;
    g_display_object_animated_pool[window_index].priority = 12;
    g_display_object_animated_pool[window_index].flags |= DISPLAY_OBJECT_HIDDEN;
    xy[window_index].x = 0;
    xy[window_index].y = 0;
    records[window_index].anim_counter = 0;
    g_display_object_animated_pool[window_index].frame_index = 0;

    g_world_map_scroll_y.limit = 0x400;
    g_world_map_scroll_x.limit = 0x400;
    g_world_map_scroll_y.value = 0;
    g_world_map_scroll_x.value = 0;
    g_world_map_scroll_y.output = 0;
    g_world_map_scroll_x.output = 0;
    g_world_map_scroll_y.decay = 0x80;
    g_world_map_scroll_x.decay = 0x80;
    g_world_map_scroll_y.step = 0x80;
    g_world_map_scroll_x.step = 0x80;

    g_hud_value = script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS);
    level = g_menu_stack_depth;
    g_menu_stack_records[level].map_cursor.wide_view = 0;
    g_menu_stack_records[level].map_cursor.snap_hit = g_world_map_marker_state.marker.location_id + 1;
    g_menu_stack_records[level].map_cursor.pending_mode = 0;
    g_menu_stack_depth = level + 1;
    g_menu_stack_types[level] = MENU_STACK_LEVEL_WORLD_MAP;
}

/* Resume handler of the world map's root level: show the war funds in the
 * context value display, reset the level's window palette, restore the live
 * map if the captured frame is showing and run any pending script requests. */
static void cursor_level_resume(menu_stack_record_t* level) {
    g_hud_value_mode = HUD_VALUE_FUNDS;
    g_display_object_animated_pool[level->map_cursor.window_index].palette = 0;
    g_hud_value = script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS);
    if (g_display_object_animated_pool[g_world_map_window_index].flags & DISPLAY_OBJECT_HIDDEN) {
        world_map_backdrop_mode_set(0);
    }
    location_script_requests_dispatch();
}

/* Reset the window palette and both map-cursor scroll axes. The scroll fields
 * preserve the original store order after the indexed window store. */
void world_map_cursor_level_suspend(menu_stack_record_t* level) {
    g_display_object_animated_pool[level->window_index].palette = 10;
    g_world_map_scroll_y.value = 0;
    g_world_map_scroll_x.value = 0;
    g_world_map_scroll_y.output = 0;
    g_world_map_scroll_x.output = 0;
}

/* Once the current location's script yields a map-menu action, reset the
 * current level's window and dispatch its path, location-visibility or focus
 * request. A focus location of 0xff means the current location.
 *
 * The flag tests are signed `> 0` comparisons; `!= 0` folds to srl/andi. */
void location_script_requests_dispatch(void) {
    s32* location = &g_world_map_marker_state.marker.location_id;
    s32 id;

    if (location_script_run(*location, LOCATION_SCRIPT_ACTION_MAP_MENU_MASK) == 0) {
        return;
    }
    if (g_location_script_state.flags & LOCATION_SCRIPT_ACTION_PATH_MASK) {
        world_map_cursor_level_suspend(&g_menu_stack_records[g_menu_stack_depth - 1]);
        path_level_open(g_location_script_state.args[0], g_location_script_state.args[1],
            (g_location_script_state.flags & LOCATION_SCRIPT_ACTION_ERASE_PATH) > 0);
    } else if (g_location_script_state.flags & LOCATION_SCRIPT_ACTION_LOCATION_VISIBILITY_MASK) {
        world_map_cursor_level_suspend(&g_menu_stack_records[g_menu_stack_depth - 1]);
        visibility_level_open(g_location_script_state.args[0],
            (g_location_script_state.flags & LOCATION_SCRIPT_ACTION_ERASE_LOCATION) > 0);
    } else if (g_location_script_state.flags & LOCATION_SCRIPT_ACTION_FOCUS_LOCATION) {
        world_map_cursor_level_suspend(&g_menu_stack_records[g_menu_stack_depth - 1]);
        id = g_location_script_state.args[0];
        if (id == 0xFF) {
            id = *location;
        }
        focus_level_open(id);
    }
}

/* Per-frame step of the world map cursor (the root level).
 *
 * While route travel runs it only keeps the map redrawn. While a zoom runs
 * (WORLD_MAP_PAN_ACTIVE) it interpolates the view origin and the
 * projection scale over `duration` frames, out (ZOOMED_OUT) or back in, and
 * redraws the map each frame.
 *
 * Otherwise it handles input: Circle on the marker's own dot opens its
 * location menu, on another dot walks the marker there
 * (travel_start), and off every dot hides the cursor and
 * pans back to the marker (focus_level_open); Triangle
 * or Start opens the entry window (world_map_menu_level_open);
 * Select on a dot shows the location's help message (0x8000 + location) over
 * a captured frame; L1 or R1 starts or reverses the zoom; holding Square
 * swaps the cursor window for the wide-view window. With no modal state
 * it accelerates the two scroll axes, snaps the cursor toward the nearest dot
 * through world_map_dot_snap_get, scrolls the projection origin when
 * the cursor approaches a screen edge, and finally clamps the cursor window to
 * x in [-0x70, 0x78] and y in [-0x68, 0x68].
 *
 * The zoom interpolation uses four signed divisions by `duration`
 * (gcc-2.7.2_O2_aspsx-2.21_divcheck). `point` is never written; it
 * reproduces the target's unused 8-byte slot at sp+0x18. The chained scale
 * stores keep each branch's scale value a single-set temporary; one variable
 * shared by both branches is multi-set, is scheduled early and changes the
 * register allocation of the interpolation. */
static void cursor_level_update(menu_stack_record_t* level) {
    point32_t amount;
    point32_t point;
    point32_t target;
    VECTOR saved;
    s32 step_x;
    s32 step_y;
    s32 flags;
    s32 motion;
    s32 buttons;
    s32 held;
    s32 hit;
    s32 frame;
    s32 duration;

    /* The exits funnel into the shared snap-hit tail at `finish`; in the
     * target's block order these need gotos. */
    if (g_travel.flags & TRAVEL_ACTIVE) {
        g_world_map_flags |= WORLD_MAP_FLAG_DRAW_MAP;
        goto finish;
    }

    if (level->map_cursor.wide_view == 0 && !(g_world_map_previous_flags & WORLD_MAP_FLAG_DRAW_MAP)) {
        if (level->map_cursor.pending_mode != 0) {
            flags = g_world_map_flags;
            if (flags & WORLD_MAP_FLAG_FADING) {
                return;
            }
            g_world_map_flags = flags ^ WORLD_MAP_FLAG_INPUT_BLOCKED;
            if (level->map_cursor.pending_mode == 3) {
                g_world_map_flags = flags ^ (WORLD_MAP_FLAG_RUNNING | WORLD_MAP_FLAG_INPUT_BLOCKED);
            }
            level->map_cursor.pending_mode = 0;
        }
        if (g_world_map_flags & WORLD_MAP_FLAG_FADING) {
            /* Skips the zoom handling and the wide-view toggle below. */
            goto scroll;
        }
        if (!(g_world_map_zoom.flags & (WORLD_MAP_PAN_ACTIVE | WORLD_MAP_PAN_ZOOMED_OUT)) && g_music_queue.count == 0) {
            if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
                /* The hit test returns a one-based dot index, 0 for no dot. */
                hit = world_map_dot_find(g_display_object_animated_pool[level->map_cursor.window_index].position);
                if (hit != 0) {
                    sound_play_effect(SOUND_SFX_CONFIRM);
                    hit--;

                    /* A dot other than the marker's walks the marker there; the marker's own dot opens its menu. */
                    if (hit != g_world_map_marker_state.marker.location_id) {
                        travel_start(g_world_map_marker_state.marker.location_id, hit);
                        return;
                    }
                    g_menu_stack_records[g_menu_stack_depth].panel.entry_count
                        = location_menu_entries_build(hit, g_menu_stack_records[g_menu_stack_depth].panel.entries);
                    if (g_menu_stack_records[g_menu_stack_depth].panel.entry_count != 0) {
                        world_map_cursor_level_suspend(level);
                        location_menu_open(hit);
                        return;
                    }
                } else {
                    g_display_object_animated_pool[level->map_cursor.window_index].flags |= DISPLAY_OBJECT_HIDDEN;
                    focus_level_open(g_world_map_marker_state.marker.location_id);
                    return;
                }
            }

            buttons = g_world_map_input_new_button_presses;
            if (buttons & (PSX_PAD_TRIANGLE | PSX_PAD_START)) {
                sound_play_effect(SOUND_SFX_CONFIRM);
                world_map_cursor_level_suspend(level);
                world_map_menu_level_open();
                return;
            }
            if (buttons & PSX_PAD_SELECT) {
                hit = world_map_dot_find(g_display_object_animated_pool[level->map_cursor.window_index].position);
                if (hit != 0) {
                    hit--;
                    world_map_cursor_level_suspend(level);
                    world_map_backdrop_mode_set(1);
                    menu_stack_message_open(hit + 0x8000, 1);
                    return;
                }
            }
        }
    }

    /* L1 or R1 starts a zoom; while one runs, interpolate it and skip the cursor. */
    motion = g_world_map_zoom.flags;
    if (!(motion & WORLD_MAP_PAN_ACTIVE)) {
        if (level->map_cursor.wide_view == 0 && (g_world_map_input_new_button_presses & (PSX_PAD_L1 | PSX_PAD_R1))) {
            g_display_object_animated_pool[level->map_cursor.window_index].flags |= DISPLAY_OBJECT_HIDDEN;
            g_world_map_zoom.duration = 0x20;
            g_world_map_zoom.frame = 0;
            g_world_map_zoom.flags = (g_world_map_zoom.flags ^ WORLD_MAP_PAN_ZOOMED_OUT) | WORLD_MAP_PAN_ACTIVE;

            /* Zooming out remembers the view origin so zooming back in can return to it. */
            if (g_world_map_zoom.flags & WORLD_MAP_PAN_ZOOMED_OUT) {
                g_world_map_zoom.origin = g_world_map_projection_view.origin;
            }
            level->map_cursor.snap_hit = 0;
            if (g_world_map_zoom.flags & WORLD_MAP_PAN_ZOOMED_OUT) {
                sound_play_effect(SOUND_SFX_WORLD_MAP_ZOOM_OUT);
            } else {
                sound_play_effect(SOUND_SFX_WORLD_MAP_ZOOM_IN);
            }
            goto finish;
        }
    } else {
        /* The scale moves between ONE and 0x800 (half size); the zoomed-out view is centered at (0, 8). */
        frame = g_world_map_zoom.frame;
        duration = g_world_map_zoom.duration;
        step_x = (frame << 11) / duration;
        if (motion & WORLD_MAP_PAN_ZOOMED_OUT) {
            g_world_map_projection_view.origin.vx
                = g_world_map_zoom.origin.vx + ((0 - g_world_map_zoom.origin.vx) * frame) / duration;
            g_world_map_projection_view.origin.vy
                = g_world_map_zoom.origin.vy + ((8 - g_world_map_zoom.origin.vy) * frame) / duration;
            g_world_map_projection_view.scale.vx = g_world_map_projection_view.scale.vy = ONE - step_x;
            g_world_map_zoom.frame = frame + 1;
            g_world_map_flags |= WORLD_MAP_FLAG_HIDE_MAP_DOTS | WORLD_MAP_FLAG_HIDE_MAP_ROUTES;
            if (frame + 1 == duration) {
                g_world_map_projection_view.scale.vy = 0x800;
                g_world_map_projection_view.scale.vx = 0x800;
                g_world_map_projection_view.origin.vx = 0;
                g_world_map_projection_view.origin.vy = 8;
                g_world_map_zoom.flags = motion ^ WORLD_MAP_PAN_ACTIVE;
            }
        } else {
            g_world_map_projection_view.origin.vx = (g_world_map_zoom.origin.vx * frame) / duration;
            g_world_map_projection_view.origin.vy = ((g_world_map_zoom.origin.vy - 8) * frame) / duration + 8;
            g_world_map_projection_view.scale.vx = g_world_map_projection_view.scale.vy = step_x + 0x800;
            g_world_map_zoom.frame = frame + 1;
            if (frame + 1 == duration) {
                g_world_map_projection_view.scale.vy = ONE;
                g_world_map_projection_view.scale.vx = ONE;
                g_world_map_projection_view.origin = g_world_map_zoom.origin;
                g_world_map_zoom.flags ^= WORLD_MAP_PAN_ACTIVE;
                g_world_map_flags &= ~(WORLD_MAP_FLAG_HIDE_MAP_DOTS | WORLD_MAP_FLAG_HIDE_MAP_ROUTES);
            }
        }

        g_world_map_flags |= WORLD_MAP_FLAG_DRAW_MAP;
        g_world_map_marker_state.flags |= WORLD_MAP_MARKER_UPDATE_POSITION;
        goto finish;
    }

    if (g_world_map_zoom.flags & WORLD_MAP_PAN_ZOOMED_OUT) {
        goto finish;
    }

    /* Holding square swaps the cursor window for the wide-view window. */
    if (!(g_input_current_buttons & PSX_PAD_SQUARE)) {
        if (level->map_cursor.wide_view != 0) {
            level->map_cursor.wide_view = 0;
            g_display_object_animated_pool[level->map_cursor.alt_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        }
        g_display_object_animated_pool[level->map_cursor.window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
    } else if (level->map_cursor.wide_view != 1) {
        level->map_cursor.wide_view = 1;
        g_display_object_animated_pool[level->map_cursor.window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[level->map_cursor.alt_window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_display_object_animated_pool[level->map_cursor.alt_window_index].anim_counter
            = g_display_object_animated_pool[level->map_cursor.alt_window_index].frame_index = 0;
        g_world_map_scroll_y.value = 0;
        g_world_map_scroll_x.value = 0;
        g_world_map_scroll_y.output = 0;
        g_world_map_scroll_x.output = 0;
    }

scroll:
    if (level->map_cursor.wide_view == 0) {
        held = g_input_current_buttons;
        if (held & (PSX_PAD_RIGHT | PSX_PAD_LEFT)) {
            if (held & PSX_PAD_LEFT) {
                world_map_scroll_axis_update(&g_world_map_scroll_x, -1);
            } else {
                world_map_scroll_axis_update(&g_world_map_scroll_x, 1);
            }
        } else {
            world_map_scroll_axis_update(&g_world_map_scroll_x, 0);
        }

        held = g_input_current_buttons;
        if (held & (PSX_PAD_UP | PSX_PAD_DOWN)) {
            if (held & PSX_PAD_UP) {
                world_map_scroll_axis_update(&g_world_map_scroll_y, -1);
            } else {
                world_map_scroll_axis_update(&g_world_map_scroll_y, 1);
            }
        } else {
            world_map_scroll_axis_update(&g_world_map_scroll_y, 0);
        }

        /* Pull the move toward the nearest dot, then scroll the map when the cursor nears a screen edge. */
        amount.x = g_world_map_scroll_x.output;
        amount.y = g_world_map_scroll_y.output;
        if (!(g_world_map_zoom.flags & WORLD_MAP_PAN_ZOOMED_OUT)) {
            target.x = g_display_object_animated_pool[level->map_cursor.window_index].position.x + amount.x;
            target.y = g_display_object_animated_pool[level->map_cursor.window_index].position.y + amount.y;
            level->map_cursor.snap_hit = world_map_dot_snap_get(target, &step_x, &step_y);
            amount.x += step_x;
            amount.y += step_y;

            target.x = g_display_object_animated_pool[level->map_cursor.window_index].position.x + amount.x;
            target.y = g_display_object_animated_pool[level->map_cursor.window_index].position.y + amount.y;
            saved = g_world_map_projection_view.origin;
            if ((amount.x < 0 && target.x < -0x40) || (amount.x > 0 && target.x >= 0x49)) {
                world_map_horizontal_scroll_apply(&amount.x, &g_world_map_projection_view.origin.vx);
            }
            if ((amount.y < 0 && target.y < -0x40) || (amount.y > 0 && target.y >= 0x41)) {
                world_map_vertical_scroll_apply(&amount, &g_world_map_projection_view.origin);
            }
            if (saved.vx != g_world_map_projection_view.origin.vx
                || saved.vy != g_world_map_projection_view.origin.vy) {
                g_world_map_flags |= WORLD_MAP_FLAG_DRAW_MAP;
                g_world_map_marker_state.flags |= WORLD_MAP_MARKER_UPDATE_POSITION;
            }
        }

        g_display_object_animated_pool[level->map_cursor.window_index].position.x += amount.x;
        g_display_object_animated_pool[level->map_cursor.window_index].position.y += amount.y;
        if (g_display_object_animated_pool[level->map_cursor.window_index].position.x < -0x70) {
            g_display_object_animated_pool[level->map_cursor.window_index].position.x = -0x70;
        }
        if (g_display_object_animated_pool[level->map_cursor.window_index].position.x >= 0x79) {
            g_display_object_animated_pool[level->map_cursor.window_index].position.x = 0x78;
        }
        if (g_display_object_animated_pool[level->map_cursor.window_index].position.y < -0x68) {
            g_display_object_animated_pool[level->map_cursor.window_index].position.y = -0x68;
        }
        if (g_display_object_animated_pool[level->map_cursor.window_index].position.y >= 0x69) {
            g_display_object_animated_pool[level->map_cursor.window_index].position.y = 0x68;
        }
    } else {
        /* In wide view the d-pad scrolls the map 4 units a frame instead of moving the cursor. */
        held = g_input_current_buttons;
        if (held & PSX_PAD_DPAD_MASK) {
            amount.y = 0;
            amount.x = 0;
            if (held & PSX_PAD_LEFT) {
                amount.x = -4;
            }
            if (held & PSX_PAD_RIGHT) {
                amount.x = 4;
            }
            if (held & PSX_PAD_UP) {
                amount.y = -4;
            }
            if (held & PSX_PAD_DOWN) {
                amount.y = 4;
            }

            saved = g_world_map_projection_view.origin;
            world_map_scroll_apply(&amount, &g_world_map_projection_view.origin);
            if (saved.vx != g_world_map_projection_view.origin.vx
                || saved.vy != g_world_map_projection_view.origin.vy) {
                g_world_map_flags |= WORLD_MAP_FLAG_DRAW_MAP;
                g_world_map_marker_state.flags |= WORLD_MAP_MARKER_UPDATE_POSITION;
            }
        }
    }

finish:
    if (level->map_cursor.wide_view == 0 && (hit = level->map_cursor.snap_hit) != 0) {
        g_location_selected = hit;
        return;
    }

    g_location_selected = 0;
}

/* Push a map-dot focus level and hide its parent window during the projection move.
 * Keeping the render pointer as the base preserves the two related call addresses. */
static void focus_level_open(s32 dot_index) {
    display_object_animated_t* render;

    render = &g_world_map_dots[dot_index].render;
    g_menu_stack_records[g_menu_stack_depth].focus_dot_index = dot_index;
    world_map_pan_start(&render->position, &g_world_map_dots[dot_index].position);
    if (g_world_map_pan.flags & WORLD_MAP_PAN_ACTIVE) {
        g_world_map_flags |= WORLD_MAP_FLAG_HIDE_LOCATION_LABEL;
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth - 1].window_index].flags
            |= DISPLAY_OBJECT_HIDDEN;
    }
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_WORLD_MAP_FOCUS;
    g_menu_stack_depth = g_menu_stack_depth + 1;
}

/* Step of the focus-location level. While the view pans it advances the pan
 * and redraws the map; afterwards it pops the level, moves the parent level's
 * window (the map cursor) onto the focused dot (y - 4), shows the location
 * label again and runs the parent's resume handler.
 *
 * The entry must be a struct pointer: the target hoists its reload above the
 * scalar g_world_map_flags store, which GCC only allows for an in-struct
 * access. */
static void focus_level_update(menu_stack_record_t* entry) {
    world_map_marker_state_t* state;
    s32 level;

    if (g_world_map_pan.flags & WORLD_MAP_PAN_ACTIVE) {
        world_map_pan_update();
        state = &g_world_map_marker_state;
        state->flags |= WORLD_MAP_MARKER_UPDATE_POSITION;
        g_world_map_flags |= WORLD_MAP_FLAG_DRAW_MAP;
        return;
    }
    level = g_menu_stack_depth;
    g_menu_stack_depth = level - 1;
    g_display_object_animated_pool[g_menu_stack_records[level - 2].window_index].position.x
        = g_world_map_dots[entry->focus_dot_index].render.position.x;
    g_world_map_flags &= ~WORLD_MAP_FLAG_HIDE_LOCATION_LABEL;
    g_display_object_animated_pool[g_menu_stack_records[level - 2].window_index].position.y
        = g_world_map_dots[entry->focus_dot_index].render.position.y - 4;
    menu_stack_dispatch_resume();
}

/* Start drawing or erasing the map route between locations `from` and `to`
 * and hide the parent window until the animation finishes. Indexed stores
 * keep their named globals to preserve the original address sequences. */
static void path_level_open(s32 from, s32 to, s32 erase) {
    s32 depth;
    s32 window_index;

    world_map_path_animation_prepare(from, to);
    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].path.from_location = from;
    g_menu_stack_records[depth].path.to_location = to;
    g_menu_stack_records[depth].path.erase = erase;
    g_menu_stack_records[depth].path.pending_sound = 1;
    g_world_map_flags |= WORLD_MAP_FLAG_HIDE_LOCATION_LABEL;
    window_index = g_menu_stack_records[depth - 1].window_index;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_WORLD_MAP_PATH;
    g_display_object_animated_pool[window_index].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth = depth + 1;
}

/*
 * Advances a map path animation one frame. The first frame plays the path
 * sound; an erase also clears the path's visible variable and starts at the
 * last page. While the animation runs, the page timer steps forwards to draw
 * the route (setting the visible variable once all pages are drawn) or
 * backwards to erase it; afterwards the level pops and its parent resumes.
 */
static void path_level_update(menu_stack_record_t* level) {
    s32 depth;
    s32 window_index;

    if (level->path.pending_sound != 0) {
        if (g_world_map_flags & WORLD_MAP_FLAG_FADING) {
            return;
        }
        if (level->path.erase != 0) {
            script_variable_set(g_world_map_path_animation.route_id + EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST, 0);
            g_world_map_path_animation.segment_index = g_world_map_path_animation.segment_count - 1;
            g_world_map_path_animation.frame_timer
                = g_world_map_path_animation.segment_frames[g_world_map_path_animation.segment_count - 1] - 1;
            sound_play_effect(SOUND_SFX_WORLD_MAP_PATH);
        } else {
            sound_play_effect(SOUND_SFX_WORLD_MAP_PATH);
        }
        level->path.pending_sound = 0;
    }

    if (g_world_map_path_animation.flags & WORLD_MAP_PATH_ANIMATION_RUNNING) {
        if (level->path.erase == 0) {
            g_world_map_path_animation.frame_timer++;
            if (g_world_map_path_animation.frame_timer
                >= g_world_map_path_animation.segment_frames[g_world_map_path_animation.segment_index]) {
                g_world_map_path_animation.segment_index++;
                g_world_map_path_animation.frame_timer = 0;
                if (g_world_map_path_animation.segment_index >= g_world_map_path_animation.segment_count) {
                    script_variable_set(g_world_map_path_animation.route_id + EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST, 1);
                }
            }
        } else {
            g_world_map_path_animation.frame_timer--;
            if (g_world_map_path_animation.frame_timer <= 0) {
                g_world_map_path_animation.segment_index--;
                if (g_world_map_path_animation.segment_index >= 0) {
                    g_world_map_path_animation.frame_timer
                        = g_world_map_path_animation.segment_frames[g_world_map_path_animation.segment_index];
                }
            }
        }
        g_world_map_flags |= WORLD_MAP_FLAG_DRAW_MAP;
        return;
    }

    sound_play_effect(SOUND_SFX_NONE);
    g_world_map_flags |= WORLD_MAP_FLAG_DRAW_MAP;
    depth = g_menu_stack_depth;
    g_menu_stack_depth = depth - 1;
    window_index = g_menu_stack_records[depth - 2].window_index;
    g_world_map_flags &= ~WORLD_MAP_FLAG_HIDE_LOCATION_LABEL;
    g_display_object_animated_pool[window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
    menu_stack_dispatch_resume();
}

/* Start a delayed map-location visibility change and hide the parent window
 * until it finishes. Indexed stores keep their named globals to preserve
 * the original address sequences. */
static void visibility_level_open(s32 dot_index, s32 erase) {
    s32 depth = g_menu_stack_depth;
    s32 window_index;

    g_menu_stack_records[depth].visibility.dot_index = dot_index;
    g_menu_stack_records[depth].visibility.erase = erase;
    g_menu_stack_records[depth].visibility.delay = 0x10;
    g_menu_stack_records[depth].visibility.pending_sound = 1;
    g_world_map_flags |= WORLD_MAP_FLAG_HIDE_LOCATION_LABEL;
    window_index = g_menu_stack_records[depth - 1].window_index;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_WORLD_MAP_LOCATION_VISIBILITY;
    g_display_object_animated_pool[window_index].flags |= DISPLAY_OBJECT_HIDDEN;
    g_menu_stack_depth = depth + 1;
}

/* Step of the location visibility level, which reveals or erases one
 * location's dot. Once the screen fade is over it plays the sound; a reveal
 * sets the location's visible variable (EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST
 * + dot) at once. The map is redrawn for `delay` frames, then an erase hides
 * the dot and clears the variable, and the level closes.
 *
 * The window index is read into a local before the flag clear, which keeps
 * the target's load and store order. */
static void visibility_level_update(menu_stack_record_t* level) {
    s32 index;

    if (level->visibility.pending_sound != 0) {
        if (g_world_map_flags & WORLD_MAP_FLAG_FADING) {
            return;
        }
        if (level->visibility.erase != 0) {
            sound_play_effect(SOUND_SFX_CANCEL);
        } else {
            script_variable_set(level->visibility.dot_index + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST, 1);
            sound_play_effect(SOUND_SFX_WORLD_MAP_LOCATION_APPEAR);
        }
        level->visibility.pending_sound = 0;
    }
    if (level->visibility.delay != 0) {
        g_world_map_flags |= WORLD_MAP_FLAG_DRAW_MAP;
        level->visibility.delay--;
        return;
    }
    if (level->visibility.erase != 0) {
        g_world_map_dots[level->visibility.dot_index].render.flags |= DISPLAY_OBJECT_HIDDEN;
        script_variable_set(level->visibility.dot_index + EVENT_SCRIPT_VAR_LOCATION_VISIBLE_FIRST, 0);
    }
    g_world_map_flags |= WORLD_MAP_FLAG_DRAW_MAP;
    g_menu_stack_depth--;
    index = g_menu_stack_records[g_menu_stack_depth - 1].window_index;
    g_world_map_flags &= ~WORLD_MAP_FLAG_HIDE_LOCATION_LABEL;
    g_display_object_animated_pool[index].flags &= ~DISPLAY_OBJECT_HIDDEN;
    menu_stack_dispatch_resume();
}

/* Open the entry window and push its world-map return level. */
void world_map_menu_level_open(void) {
    u32 flags;
    s32 depth;
    s32* counter;

    g_menu_blocked_buttons = -1;
    screen_entry_window_open(4, -0x30, -0x38);
    g_hud_value_mode = HUD_VALUE_FUNDS;
    g_hud_value = script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS);
    depth = g_menu_stack_depth;
    flags = g_world_map_flags;
    g_menu_stack_records[depth].return_level.delay = 3;
    g_menu_stack_records[depth].return_level.pending = 0;
    g_world_map_flags = flags & ~WORLD_MAP_FLAG_SCREEN_CLEARED;
    if ((flags & WORLD_MAP_FLAG_FADING) != 0) {
        g_world_map_flags = (flags & ~WORLD_MAP_FLAG_SCREEN_CLEARED) | WORLD_MAP_FLAG_FREEZE_ANIMATION;
        counter = &g_world_map_fade.elapsed;
        *counter = *counter - 4;
    }
    {
        s32 last;
        last = g_menu_stack_depth;
        g_menu_stack_types[last] = MENU_STACK_LEVEL_WORLD_MENU;
        g_menu_stack_depth = last + 1;
    }
}

/* Per-frame handler of the world-map return level: waits out its own delay,
 * then pops the level and dispatches whichever pending system-flag request
 * asked for the return. */
static void map_menu_return_update(menu_stack_record_t* level) {
    s32 flags;
    s32 cleared;
    s32 location;

    if (g_world_map_flags & WORLD_MAP_FLAG_SCREEN_CLEARED) {
        level->return_level.pending = 1;
    }

    if (g_world_map_flags & WORLD_MAP_FLAG_FADING) {
        if (level->return_level.pending != 0) {
            level->return_level.pending = 0;
            world_map_backdrop_mode_set(0);
            level->return_level.delay = 3;
            g_world_map_flags |= WORLD_MAP_FLAG_FADING | WORLD_MAP_FLAG_FREEZE_ANIMATION;
        }
        g_menu_blocked_buttons = -1;
        return;
    }

    if (level->return_level.delay != 0) {
        level->return_level.delay -= 1;
        if (level->return_level.delay == 0) {
            world_map_backdrop_mode_set(1);
        }
        return;
    }

    g_menu_blocked_buttons = 0;
    if (menu_thread_activity_get() != 0) {
        return;
    }

    world_map_input_repeat_init();
    flags = g_world_map_flags;
    world_map_backdrop_mode_set(0);
    cleared = g_world_map_flags & ~WORLD_MAP_FLAG_FREEZE_ANIMATION;
    g_world_map_flags = cleared;
    g_menu_stack_depth -= 1;
    if (flags & WORLD_MAP_FLAG_REQUEST_BRAVE_STORY) {
        g_world_map_flags = cleared ^ WORLD_MAP_FLAG_DRAW_MAP;
        screen_transition_open(MENU_STACK_LEVEL_BRAVE_STORY, 0, 0);
        return;
    }

    if (flags & WORLD_MAP_FLAG_REQUEST_DESTINATION) {
        location = script_variable_get(EVENT_SCRIPT_VAR_CURRENT_MAP);
        world_map_backdrop_capture();
        menu_stack_dispatch_resume();
        if (g_world_map_marker_state.marker.location_id != location) {
            travel_start(g_world_map_marker_state.marker.location_id, location);
        }
        return;
    }

    if (flags & WORLD_MAP_FLAG_REQUEST_TUTORIAL) {
        g_world_map_flags = cleared ^ WORLD_MAP_FLAG_DRAW_MAP;
        screen_transition_open(MENU_STACK_LEVEL_TUTORIAL, 0, 1);
        return;
    }

    if (flags & WORLD_MAP_FLAG_REQUEST_VARIABLE_LIST) {
        variable_list_open();
        return;
    }

    if (flags & WORLD_MAP_FLAG_REQUEST_DEBUG_PARTY_NUMBER) {
        party_number_open();
        return;
    }

    if (flags & WORLD_MAP_FLAG_RESTART) {
        g_world_map_flags = cleared & ~(WORLD_MAP_FLAG_RUNNING | WORLD_MAP_FLAG_UNLOCK_ALL);
        if (script_variable_get(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT) != 0
            || script_variable_get(EVENT_SCRIPT_VAR_NEXT_SCENARIO) == 0) {
            g_world_map_flags |= WORLD_MAP_FLAG_RESTART;
        } else {
            g_world_map_flags &= ~WORLD_MAP_FLAG_RESTART;
        }
        g_world_map_flags |= WORLD_MAP_FLAG_NO_BACKGROUND_IMAGE;
        return;
    }

    menu_stack_dispatch_resume();
}

/* Pushes the self-sizing message window (MENU_STACK_LEVEL_TEXT_WINDOW), the
 * windowed sibling of menu_stack_message_open. It stores the reload
 * flag and, when set, re-points the text formatter at
 * g_screen_text_section_offsets; appends the window's label record (priority
 * 1); measures the entry, clamped to three rows; fills the shared scrollable
 * window and centers it and the label on the measured box; then saves
 * g_menu_blocked_buttons in the level, blocks every button, plays
 * SOUND_SFX_WINDOW_OPEN and pushes the level.
 *
 * Taking the record base into `records` at the last block-one store lets CSE
 * reuse that register after the measure call, which gives the base
 * priority over `text` in global allocation (s1/s2 as in the target). The
 * combined-away address pseudo of that store keeps a reload stack slot, part
 * of the target's 40-byte locals area. `box` is an aggregate so its 32-bit values stay
 * in memory across text_window_open; `dims` is an aggregate so
 * it is laid out in declaration order at 32(sp). `unused_point` and `unused`
 * are sized from the target frame only; their original types are unknown. */
void menu_stack_text_window_open(s32 text, s32 reload_text) {
    text_window_t* window;
    display_object_animated_t* records;
    point32_t* positions;
    point32_t* position;
    s32* text_id;
    s32 index;
    point32_t box;
    point32_t unused_point;
    text_dimensions_t dims;
    s32 unused[2];
    u8* entry;

    g_menu_stack_records[g_menu_stack_depth].message.reload_text = reload_text;
    if (reload_text != 0) {
        text_section_pointers_save();
        text_pointer_table_relocate(g_screen_text_section_offsets);
    }

    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].message.window_index = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_MESSAGE_LABEL;
    g_display_object_animated_pool[index].priority = 1;
    g_display_object_animated_pool[index].anim_counter = 0;
    records = g_display_object_animated_pool;
    records[index].frame_index = 0;

    entry = text_entry_find(text);
    text_pixels_measure(&dims.width, &dims.rows, entry);
    if (dims.rows > 3) {
        dims.rows = 3;
    }

    text_id = &g_text_window.text_id;
    window = &g_text_window;

    *text_id = text;
    window->image_position.x = 0;
    g_text_window.image_position.y = 0xC0;
    box.x = (dims.width + 24) & 0xFFFC;
    box.y = (dims.rows << 4) + 16;
    g_text_window.base.x = -(box.x / 2);
    g_text_window.base.y = -(box.y >> 1);
    g_text_window.text_width = dims.width;
    g_text_window.rows_per_page = dims.rows;
    g_text_window.priority = 1;
    g_text_window.image_coordinate_mode = 0;
    g_text_window.extra_render_index = -1;
    g_text_window.text_substitutions[1] = -1;
    g_text_window.text_substitutions[0] = -1;
    text_window_open(window);

    index = g_menu_stack_records[g_menu_stack_depth].message.window_index;
    positions = &records[0].position;
    /* Keep the position-column base separate; direct record indexing changes the instructions. */
    position = (point32_t*)((u8*)positions + index * sizeof(display_object_animated_t));
    position->x = -(box.x / 2) + 2;
    position->y = -(box.y / 2) - 2;
    sound_play_effect(SOUND_SFX_WINDOW_OPEN);

    g_menu_stack_records[g_menu_stack_depth].message.saved_blocked_buttons = g_menu_blocked_buttons;
    g_menu_blocked_buttons = -1;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_TEXT_WINDOW;
    g_menu_stack_depth = g_menu_stack_depth + 1;
}

/* Steps a message level: once the text window has popped in and
 * text_window_update reports it closed, pops the level, restores the
 * blocked-button mask it saved and (when asked) the text section pointers,
 * and resumes the level underneath. */
static void text_level_update(menu_stack_record_t* level) {
    if (g_display_object_gs_image_pool[g_text_window.render_record_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }
    if (text_window_update(&g_text_window) != 0) {
        return;
    }
    g_input_current_buttons = 0;
    g_menu_blocked_buttons = level->message.saved_blocked_buttons;
    g_display_object_animated_count -= 1;
    g_display_object_queue_count -= 1;
    g_menu_stack_depth -= 1;
    if (level->message.reload_text != 0) {
        text_section_pointers_restore();
    }
    menu_stack_dispatch_resume();
}

/* Pushes MENU_STACK_LEVEL_MESSAGE, a dialog box on message `text`:
 * optionally points the text formatter at WORLD's sections, starts the dialog
 * thread (mode 0x3b while WORLD_MAP_FLAG_UNIT_PANEL_THREADS is set, else
 * 0x33), saves the blocked buttons in the level and blocks them all, and
 * plays SOUND_SFX_WINDOW_OPEN. */
void menu_stack_message_open(s32 text, s32 reload_text) {
    s32 index;

    g_menu_stack_records[g_menu_stack_depth].message.reload_text = reload_text;
    if (reload_text != 0) {
        text_section_pointers_save();
        text_pointer_table_relocate(g_screen_text_section_offsets);
    }
    thread_start(THREAD_SCREEN_SLOT_MESSAGE, message_dialog_thread);
    if (g_world_map_flags & WORLD_MAP_FLAG_UNIT_PANEL_THREADS) {
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x3B, text, 0);
    } else {
        thread_parameters_set(THREAD_SCREEN_SLOT_MESSAGE, 0x33, text, 0);
    }
    g_menu_stack_records[g_menu_stack_depth].message.saved_blocked_buttons = g_menu_blocked_buttons;
    g_menu_blocked_buttons = -1;
    sound_play_effect(SOUND_SFX_WINDOW_OPEN);
    index = g_menu_stack_depth;
    g_menu_stack_records[index].message._unknown_0c = 8;
    g_menu_stack_records[index].message.window_index = 0;
    g_menu_stack_types[index] = MENU_STACK_LEVEL_MESSAGE;
    g_menu_stack_depth = index + 1;
}

/* Steps a level that waits for the message box to close: once the message
 * thread has ended it pops the level, restores the blocked buttons, resumes
 * the level underneath and (when asked) restores the text section pointers.
 * Over the Brave Story or proposition category level it keeps the displayed
 * play time current while waiting. */
static void message_level_update(menu_stack_record_t* level) {
    if (g_menu_stack_types[g_menu_stack_depth - 2] == MENU_STACK_LEVEL_BRAVE_STORY
        || g_menu_stack_types[g_menu_stack_depth - 2] == MENU_STACK_LEVEL_BRAVE_STORY_PROPOSITIONS) {
        g_hud_value = g_save_play_time_hours;
        g_hud_value_secondary = g_save_play_time_minutes;
        g_hud_value_tertiary = g_save_play_time_seconds;
    }
    if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) == 0) {
        g_menu_blocked_buttons = level->message.saved_blocked_buttons;
        g_menu_stack_depth = g_menu_stack_depth - 1;
        menu_stack_dispatch_resume();
        if (level->message.reload_text != 0) {
            text_section_pointers_restore();
        }
    }
}

/* Pushes the debug script-variable list (MENU_STACK_LEVEL_DEBUG_VARIABLE_LIST):
 * one cursor window record and one render record, laid out by
 * variable_list_draw and then positioned from the render
 * record's origin.
 *
 * Both appended indices share one local, which is what holds them in $a2.
 * The window position is written after the render call returns, from the render
 * record origin the callee filled in, so the level fields are read back out
 * of the stack record rather than kept in locals; the `- -` keeps the row
 * offset's constant on the multiply chain, which a plain `+` reassociates
 * onto base_y. */
static void variable_list_open(void) {
    s32 index;
    s32 record_index;
    u8 unused_locals[0x10];

    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].variable_list.window_index = index;
    g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
    g_display_object_animated_pool[index].priority = 3;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
    index = display_object_image_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].variable_list.render_index = index;
    g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_POP_IN;
    g_display_object_gs_image_pool[index].priority = 3;
    g_menu_stack_records[g_menu_stack_depth].variable_list.first_variable = 0;
    variable_list_draw(&g_menu_stack_records[g_menu_stack_depth].variable_list);
    record_index = g_menu_stack_depth;
    g_menu_stack_records[record_index].variable_list.cursor = 0;
    g_display_object_animated_pool[g_menu_stack_records[record_index].variable_list.window_index].position.x
        = g_display_object_gs_image_pool[g_menu_stack_records[record_index].variable_list.render_index].position.x + 8;
    g_menu_stack_depth = record_index + 1;
    g_display_object_animated_pool[g_menu_stack_records[record_index].variable_list.window_index].position.y
        = g_display_object_gs_image_pool[g_menu_stack_records[record_index].variable_list.render_index].position.y
        - -(g_menu_stack_records[record_index].variable_list.cursor * 0x10 + 0xC);
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_DEBUG_VARIABLE_LIST;
}

/* Renders up to 10 rows of the debug variable list, from first_variable on
 * (script variables 0 to 1023). TEXT_ID_DEBUG_VARIABLE_VALUE shows a
 * variable and its value; a value of 10000 or more uses
 * TEXT_ID_DEBUG_VARIABLE, which shows the variable alone. The entry is
 * chosen first and its section added at the call.
 *
 * `value` first holds the row count for the panel height and then each
 * variable's value: one register (v1) in the target. */
static void variable_list_draw(debug_variable_list_level_t* level) {
    RECT bounds;
    point32_t point;
    s32 index;
    point32_t* base;
    point32_t* records;
    s32 variable = level->first_variable;
    s32 text_id;
    u32 value;
    s32 i;

    if (variable + 10 >= 1024) {
        level->row_count = 1024 - variable;
    } else {
        level->row_count = 10;
    }
    value = level->row_count;
    index = level->render_index;
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = 0x68;
    bounds.h = value * 0x10 + 0x10;
    window_build_render_record_image(index, bounds, 0, g_window_image_buffer);
    /* Direct record indexing moves the symbol load after the index scaling
     * and breaks the exact match. Retain the target's biased base and stride. */
    records = &g_display_object_gs_image_pool[0].position;
    base = (point32_t*)((u8*)records + index * sizeof(display_object_gs_image_t));
    base->x = -(bounds.w / 2);
    base->y = -88;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < level->row_count; i++) {
        value = script_variable_get(variable);
        if (value < 10000) {
            g_text_substitution_values[0] = variable;
            g_text_substitution_values[1] = value;
            text_id = TEXT_ID_DEBUG_VARIABLE_VALUE & TEXT_ID_ENTRY_MASK;
        } else {
            g_text_substitution_values[0] = variable;
            text_id = TEXT_ID_DEBUG_VARIABLE & TEXT_ID_ENTRY_MASK;
        }
        window_display_text_entry(index, text_id | TEXT_ID_WORLD_MENU_BASE, point, g_window_image_buffer);
        point.y += 0x10;
        variable++;
    }
    window_load_render_record_image(index, g_window_image_buffer);
}

/* Input handler for the debug script-variable list.
 *
 * Circle opens the number panel for the selected variable, Cross pops the
 * level. Up/down move the cursor, or scroll the first displayed variable by
 * 100 with Triangle held and by 10 with Square held (Square wraps between 0
 * and 1020). Scrolling redraws the rows through
 * variable_list_draw and clamps the cursor; a plain cursor move only repositions
 * the cursor window.
 *
 * Every scroll arm falls into the one redraw, which clamps the cursor and
 * joins the cursor move. The cursor-down arm sits between the redraw and the
 * move in the target, so it and the move are reached by goto: a redraw
 * repeated in each arm is not merged back (its clamp store is cross-jumped
 * into the cursor-down arm first). */
static void variable_list_update(menu_stack_record_t* level) {
    s32 variable;
    s32 dead[2];

    if (g_display_object_gs_image_pool[level->variable_list.render_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
        g_display_object_animated_pool[level->variable_list.window_index].palette = 0xA;
        g_display_object_gs_image_pool[level->variable_list.render_index].palette = 2;
        g_display_object_animated_pool[level->variable_list.window_index].sequence = DISPLAY_OBJECT_ANIM_CURSOR_PRESS;
        g_display_object_animated_pool[level->variable_list.window_index].anim_counter
            = g_display_object_animated_pool[level->variable_list.window_index].frame_index = 0;
        variable = level->variable_list.first_variable + level->variable_list.cursor;
        variable_edit_open(variable, script_variable_get(variable));
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        g_display_object_animated_count -= 1;
        g_display_object_image_count -= 1;
        g_display_object_queue_count -= 2;
        g_menu_stack_depth -= 1;
        menu_stack_dispatch_resume();
        return;
    }

    if (world_map_input_repeat_update(PSX_PAD_UP) != 0) {
        if (g_input_current_buttons & PSX_PAD_TRIANGLE) {
            if (level->variable_list.first_variable < 0x64) {
                level->variable_list.first_variable = 0;
            } else {
                level->variable_list.first_variable -= 0x64;
            }
        } else if (g_input_current_buttons & PSX_PAD_SQUARE) {
            if (level->variable_list.first_variable == 0) {
                level->variable_list.first_variable = 0x3fc;
            } else {
                level->variable_list.first_variable -= 0xa;
            }
        } else {
            if (level->variable_list.cursor == 0) {
                level->variable_list.cursor = level->variable_list.row_count - 1;
            } else {
                level->variable_list.cursor--;
            }
            goto move_cursor;
        }
    } else if (world_map_input_repeat_update(PSX_PAD_DOWN) != 0) {
        if (g_input_current_buttons & PSX_PAD_TRIANGLE) {
            if (level->variable_list.first_variable + 0x64 < 0x3fd) {
                level->variable_list.first_variable += 0x64;
            } else {
                level->variable_list.first_variable = 0x3fc;
            }
        } else if (g_input_current_buttons & PSX_PAD_SQUARE) {
            if (level->variable_list.first_variable == 0x3fc) {
                level->variable_list.first_variable = 0;
            } else {
                level->variable_list.first_variable += 0xa;
            }
        } else {
            goto cursor_down;
        }
    } else {
        return;
    }

    variable_list_draw(&level->variable_list);
    if (level->variable_list.cursor < level->variable_list.row_count) {
        return;
    }

    level->variable_list.cursor = level->variable_list.row_count - 1;
    goto move_cursor;
cursor_down:
    if (level->variable_list.cursor == level->variable_list.row_count - 1) {
        level->variable_list.cursor = 0;
    } else {
        level->variable_list.cursor++;
    }
move_cursor:
    g_display_object_animated_pool[level->variable_list.window_index].position.x
        = g_display_object_gs_image_pool[level->variable_list.render_index].position.x + 8;
    g_display_object_animated_pool[level->variable_list.window_index].position.y
        = g_display_object_gs_image_pool[level->variable_list.render_index].position.y
        - -(level->variable_list.cursor * 0x10 + 0xC);
}

/* Pushes MENU_STACK_LEVEL_DEBUG_VARIABLE_EDIT, a number panel (0 to
 * 9999) that edits script variable `variable_id`, starting at `value`. */
static void variable_edit_open(s32 variable_id, s32 value) {
    point32_t origin;
    s32 record_index;

    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS;
    g_menu_stack_ot_offset = 2;
    g_window_panel_value = value;
    origin.x = 0x20;
    origin.y = 0x20;
    window_number_panel_thread_start(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 9999 << 16, 0, &origin);
    record_index = g_menu_stack_depth;
    g_menu_stack_records[record_index].panel.result = 0;
    g_menu_stack_records[record_index].panel.variable_id = variable_id;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_DEBUG_VARIABLE_EDIT;
    g_menu_stack_depth = record_index + 1;
}

/* Step handler of the variable number panel. While the panel runs it accepts
 * Circle, which plays SOUND_SFX_CONFIRM and writes the panel's value to the
 * level's script variable, or Cross; either way it tells the panel thread to
 * close. Once the
 * panel thread has stopped it pops the level, redraws the script-variable
 * list on a confirm, and restores the parent level's window and render
 * records.
 *
 * Reading the parent window index before the render-record store is what
 * lets the scheduler place that load inside the render index's multiply
 * sequence, as the target does. */
static void variable_edit_update(menu_stack_record_t* level) {
    s32 depth;
    s32 parent;
    s32 slot;
    display_object_animated_t* record;
    u32 buttons;

    if (level->panel.result != 0 && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_blocked_buttons = 0;
        g_menu_stack_ot_offset = 1;
        depth = g_menu_stack_depth - 1;
        g_menu_stack_depth = depth;
        if (level->panel.result == 1) {
            variable_list_draw(&g_menu_stack_records[depth - 1].variable_list);
        }
        parent = g_menu_stack_depth - 1;
        g_display_object_animated_pool[g_menu_stack_records[parent].variable_list.window_index].sequence
            = DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
        slot = g_menu_stack_records[parent].variable_list.window_index;
        g_display_object_gs_image_pool[g_menu_stack_records[parent].variable_list.render_index].palette = 0;
        record = &g_display_object_animated_pool[slot];
        record->palette = 0;
        record->frame_index = 0;
        g_display_object_animated_pool[slot].anim_counter = 0;
        return;
    }
    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        level->panel.result = 2;
    } else if (buttons & PSX_PAD_CIRCLE) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        script_variable_set(level->panel.variable_id, g_window_panel_value);
    } else {
        return;
    }
    thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
}

/* Appends a window-list record: saves g_menu_stack_ot_offset into it, allocates a window
 * record from the render-object queue, initializes that window's delay and
 * counters, and applies layout 0 to it before advancing the list count.
 *
 * Writing the window columns as fields of g_display_object_animated_pool (not separate stride
 * arrays) keeps the scheduler from sinking the record's window_index store
 * below them. */
void unit_panel_help_open(void) {
    s32 window;

    g_menu_stack_records[g_menu_stack_depth].layout_window.saved_state = g_menu_stack_ot_offset;
    g_menu_stack_ot_offset = 1;
    window = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].layout_window.window_index = window;
    g_display_object_animated_pool[window].sequence = DISPLAY_OBJECT_ANIM_LAYOUT_CURSOR_IDLE;
    g_display_object_animated_pool[window].priority = 2;
    g_display_object_animated_pool[window].anim_counter = 0;
    g_display_object_animated_pool[window].frame_index = 0;
    g_menu_stack_records[g_menu_stack_depth].layout_window.layout = 0;
    g_menu_stack_records[g_menu_stack_depth].layout_window.cached_layout_kind = 1;
    help_cursor_move(&g_menu_stack_records[g_menu_stack_depth].layout_window);
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_UNIT_PANEL_HELP;
    g_menu_stack_depth++;
}

/* Move the layout window's cursor onto row `layout` of
 * g_unit_panel_help_rows, switching the cursor's sequence when the
 * row's kind differs from the last one.
 *
 * Routing both stores through one base variable (rather than
 * direct record indexing) preserves the separate position-column base. */
static void help_cursor_move(unit_panel_help_level_t* request) {
    display_object_position_stride_t* xy
        = (display_object_position_stride_t*)&g_display_object_animated_pool[0].position;
    s32 index = request->window_index;
    s32 layout = request->layout;
    s32 kind;

    xy[index].x = g_unit_panel_help_rows[layout].x + 14;
    xy[index].y = g_unit_panel_help_rows[layout].y + 12;
    kind = g_unit_panel_help_rows[layout].vertical_cursor;
    if (kind != request->cached_layout_kind) {
        request->cached_layout_kind = kind;
        g_display_object_animated_pool[index].sequence
            = (kind != 0) ? DISPLAY_OBJECT_ANIM_LAYOUT_CURSOR_IDLE : DISPLAY_OBJECT_ANIM_CURSOR_IDLE;
        g_display_object_animated_pool[index].anim_counter = 0;
        g_display_object_animated_pool[index].frame_index = 0;
    }
}

/* Clear the text-task flag and move a window record to the sequence after
 * its current one, restarting the animation with the default palette. */
static void panel_help_resume(menu_stack_record_t* level) {
    s32 index = level->layout_window.window_index;
    display_object_animated_t* entry = &g_display_object_animated_pool[index];

    g_menu_input_lock_mode = 0;
    entry->sequence = g_display_object_animated_pool[index].sequence + 1;
    g_display_object_animated_pool[index].palette = 0;
    entry->anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;
}

/* The g_level_suspend_handlers entry of MENU_STACK_LEVEL_UNIT_PANEL_HELP: nothing to suspend. */
static void panel_help_suspend(menu_stack_record_t* unused_level) {
}

/* Input handler for a layout-window menu level.
 *
 * Cross pops the level, Circle opens the row's help message, and a direction moves
 * to the neighboring layout named by the row. A row with a counter pointer
 * adds the counter to the base message id, and suppresses the message entirely
 * when the counter reads 0 or -1 (except for layout 0x0b). */
static void panel_help_update(menu_stack_record_t* request) {
    s32 layout;
    s16* counter;
    s16 message;
    display_object_animated_t* record;
    s32 window;

    if ((g_world_map_input_new_button_presses & PSX_PAD_CROSS) != 0) {
        g_display_object_animated_count -= 1;
        g_display_object_queue_count -= 1;
        g_menu_stack_ot_offset = request->layout_window.saved_state;
        g_menu_stack_depth -= 1;
        menu_stack_dispatch_resume();
        sound_play_effect(SOUND_SFX_CANCEL);
        return;
    }

    if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) != 0) {
        layout = request->layout_window.layout;
        counter = g_unit_panel_help_rows[layout].value;
        if (counter != 0) {
            if (layout != 0xB && *counter >= -1 && *counter < 1) {
                return;
            }
            message = g_unit_panel_help_rows[layout].help_base + *counter;
        } else {
            message = g_unit_panel_help_rows[layout].help_base;
        }
        menu_stack_message_open(message, 1);
        g_menu_input_lock_mode = 1;
        window = request->layout_window.window_index;
        record = &g_display_object_animated_pool[window];
        record->sequence = g_display_object_animated_pool[window].sequence - 1;
        g_display_object_animated_pool[window].palette = 0xA;
        record->anim_counter = 0;
        g_display_object_animated_pool[window].frame_index = 0;
        return;
    }

    if (world_map_input_repeat_update(PSX_PAD_UP) != 0) {
        if (g_unit_panel_help_rows[request->layout_window.layout].destination[0] != request->layout_window.layout) {
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
            request->layout_window.layout = g_unit_panel_help_rows[request->layout_window.layout].destination[0];
            help_cursor_move(&request->layout_window);
            return;
        }
    }
    if (world_map_input_repeat_update(PSX_PAD_DOWN) != 0) {
        if (g_unit_panel_help_rows[request->layout_window.layout].destination[1] != request->layout_window.layout) {
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
            request->layout_window.layout = g_unit_panel_help_rows[request->layout_window.layout].destination[1];
            help_cursor_move(&request->layout_window);
            return;
        }
    }
    if (world_map_input_repeat_update(PSX_PAD_RIGHT) != 0) {
        if (g_unit_panel_help_rows[request->layout_window.layout].destination[2] != request->layout_window.layout) {
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
            request->layout_window.layout = g_unit_panel_help_rows[request->layout_window.layout].destination[2];
            help_cursor_move(&request->layout_window);
            return;
        }
    }
    if (world_map_input_repeat_update(PSX_PAD_LEFT) != 0) {
        if (g_unit_panel_help_rows[request->layout_window.layout].destination[3] != request->layout_window.layout) {
            sound_play_effect(SOUND_SFX_CURSOR_MOVE);
            request->layout_window.layout = g_unit_panel_help_rows[request->layout_window.layout].destination[3];
            help_cursor_move(&request->layout_window);
        }
    }
}

/* Pushes the location menu (MENU_STACK_LEVEL_LOCATION) for menu id `menu`.
 *
 * Loads the location's picture, appends the cursor window (window kind
 * menu + 0x37) and, when a picture is present, uploads the frame TIM and the
 * location's picture at g_picture_buffer to VRAM, binds a render
 * record to the picture and adds its border window at (-8, 0x10). It then
 * measures every
 * entry's text through text_entry_find / text_pixels_measure to
 * size the panel, copies the entry values into g_menu_stack_entry_values with
 * cleared row flags, clamps the cursor to at most four visible rows, and
 * starts the panel render state centered on the measured width.
 *
 * The identical `menu += 0x37` arms split the block for the scheduler (jump2
 * cross-jumps them back together), keeping the add after the picture call so
 * reorg puts it in the wait call's delay slot. The label window's position is
 * stored through a stride-36 view based at &records->position, as the target forms
 * (records + 0x18) + index * 36.
 */
void location_menu_open(s32 menu) {
    GsIMAGE image;
    RECT bounds;
    point32_t origin;
    window_panel_layout_t layout;
    s16 text_width;
    s16 text_rows;
    s32 index;
    s32 count;
    s32 depth;
    s16 widest;
    s32 value;
    s32 kind;
    u32 flags;
    s32 i;
    CVECTOR* color;
    display_object_animated_t* records;
    location_menu_level_t* level;
    point32_t* base;
    display_object_position_stride_t* position;

    /* menu + 0x37 is the cursor window's sequence; the identical arms are explained above. */
    g_world_map_marker_state.marker.picture_id = location_picture_load(g_world_map_marker_state.marker.location_id);
    if (g_world_map_marker_state.marker.picture_id != 0) {
        menu += 0x37;
    } else {
        menu += 0x37;
    }
    file_pending_wait();

    flags = g_world_map_flags | WORLD_MAP_FLAG_HIDE_LOCATION_LABEL;
    level = &g_menu_stack_records[g_menu_stack_depth].location_menu;
    level->slots.render_b = -1;
    level->slots.window_b = -1;
    level->slots.render_a = -1;
    g_menu_stack_records[g_menu_stack_depth].location_menu.slots.window_a = -1;
    g_menu_stack_records[g_menu_stack_depth].location_menu.fade = 0;
    g_world_map_flags = flags;

    index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
    g_menu_stack_records[g_menu_stack_depth].location_menu.slots.window_a = index;
    records = g_display_object_animated_pool;
    /* Direct record.color access changes the column base and instruction scheduling. */
    color = &records->color;
    g_display_object_animated_pool[index].priority = 0xB;
    g_display_object_animated_pool[index].sequence = menu;
    g_display_object_animated_pool[index].anim_counter = 0;
    g_display_object_animated_pool[index].frame_index = 0;

    /* The windows start black and brighten from fade level 8. */
    ((CVECTOR*)((u8*)color + index * sizeof(g_display_object_animated_pool[0])))->r = 0;
    ((CVECTOR*)((u8*)color + index * sizeof(g_display_object_animated_pool[0])))->g = 0;
    ((CVECTOR*)((u8*)color + index * sizeof(g_display_object_animated_pool[0])))->b = 0;
    g_menu_stack_records[g_menu_stack_depth].location_menu.fade = 8;

    /* With a picture: upload the frame and picture TIMs, then add the picture and its border window. */
    if (g_world_map_marker_state.marker.picture_id != 0) {
        GsGetTimInfo(g_location_picture_frame_tim + 1, &image);
        LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
        LoadClut(image.clut, image.cx, image.cy);
        GsGetTimInfo(g_picture_buffer + 1, &image);
        LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
        LoadClut(image.clut, image.cx, image.cy);

        index = display_object_image_append(g_display_object_queue, &g_display_object_queue_count);
        g_menu_stack_records[g_menu_stack_depth].location_menu.slots.render_a = index;
        bounds.x = image.px;
        bounds.y = image.py;
        bounds.w = image.pw;
        bounds.h = image.ph;
        origin.x = image.cx;
        origin.y = image.cy;
        display_object_gs_image_init(index, bounds, origin, DISPLAY_OBJECT_8BIT_CLUT);
        base = &g_display_object_gs_image_pool[index].position;
        g_display_object_gs_image_pool[index].priority = 0xB;
        base->x = -0x40;
        base->y = -0x4C;
        g_display_object_gs_image_pool[index].flags |= DISPLAY_OBJECT_POP_IN;

        index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
        g_menu_stack_records[g_menu_stack_depth].location_menu.slots.window_b = index;
        g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_LOCATION_PICTURE_BORDER;
        g_display_object_animated_pool[index].priority = 0xB;
        records[index].anim_counter = 0;
        g_display_object_animated_pool[index].frame_index = 0;
        position = (display_object_position_stride_t*)&records->position;
        position[index].x = -8;
        position[index].y = 0x10;
        ((CVECTOR*)((u8*)color + index * sizeof(g_display_object_animated_pool[0])))->r = 0;
        ((CVECTOR*)((u8*)color + index * sizeof(g_display_object_animated_pool[0])))->g = 0;
        ((CVECTOR*)((u8*)color + index * sizeof(g_display_object_animated_pool[0])))->b = 0;
    }

    /* Size the panel to the widest entry plus an 8-pixel margin. */
    widest = 0;
    count = g_menu_stack_records[g_menu_stack_depth].location_menu.entry_count;
    for (i = 0; i < count; i++) {
        text_pixels_measure(&text_width, &text_rows,
            text_entry_find(g_menu_stack_records[g_menu_stack_depth].location_menu.entries[i]));
        if (widest < text_width) {
            widest = text_width;
        }
    }
    widest += 8;

    for (i = 0; i < count; i++) {
        value = g_menu_stack_records[g_menu_stack_depth].location_menu.entries[i];
        g_menu_stack_entry_values[i] = value;
        g_menu_stack_row_text_colors[i] = 0;
    }

    /* At most four rows show; the panel is centered on x, and without a picture it rises 8 pixels a row. */
    i = 4;
    if (count < 4) {
        i = count;
    }
    layout.text_width = widest;
    layout.visible_rows = i;
    origin.x = 0x80 - (((layout.text_width + 0x18) & 0xFFFC) >> 1);
    if (g_world_map_marker_state.marker.picture_id != 0) {
        origin.y = 0x80;
    } else {
        origin.y = 0x70 - i * 8;
    }
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_LOCATION, count, i);
    window_panel_state_init(0xC, g_menu_stack_records[g_menu_stack_depth].location_menu.entry_count, &layout, &origin);
    g_window_panel_render_state.vram_origin.x = 0x240;
    g_window_panel_render_state.header_id = 0;
    g_window_panel_render_state.vram_origin.y = 0x100;

    if (g_world_map_marker_state.marker.picture_id != 0) {
        s32 unused[6]; /* frame padding: the target frame is 0xb8 */

        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth].location_menu.slots.window_a].position.x
            = -4;
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth].location_menu.slots.window_a].position.y
            = 0;
    } else {
        /* Here `index` is still the cursor window's record index, yet it reads the render record pool (retail). */
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth].location_menu.slots.window_a].position.x
            = -4;
        g_display_object_animated_pool[g_menu_stack_records[g_menu_stack_depth].location_menu.slots.window_a].position.y
            = g_display_object_gs_image_pool[index].position.y - 8;
    }

    depth = g_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_records[depth].location_menu.delay = 8;
    g_world_map_flags |= WORLD_MAP_FLAG_FREEZE_ANIMATION;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_LOCATION;
    g_menu_stack_depth = depth + 1;
}

/* Restore the location list's optional renderer palette. */
static void location_menu_resume(menu_stack_record_t* level) {
    if (level->location_menu.slots.render_b != -1) {
        g_display_object_gs_image_pool[level->location_menu.slots.render_b].palette = 0;
    }
}

/* Sets the second owned render record's palette selector when its slot is present. */
static void location_menu_suspend(menu_stack_record_t* slots) {
    if (slots->location_menu.slots.render_b != -1) {
        g_display_object_gs_image_pool[slots->location_menu.slots.render_b].palette = 2;
    }
}

/* Per-frame step of the location menu level: fades its two windows in, starts
 * the menu thread after the delay and handles input. A confirmed entry opens
 * the bar, a shop, a deep-dungeon battle chosen by the location's conditional
 * set or the selected destination map; the help button shows the entry's
 * message and cancel closes the level. */
static void location_menu_update(menu_stack_record_t* level) {
    s16* selected;
    u8 unused[0x28];
    s16* values;
    s32 value;
    s32 text;
    s32 offset;
    deep_dungeon_battle_t* records;
    deep_dungeon_battle_t* record;

    /* Fade both windows in by 7 a frame. Without a picture window_b is -1, so the
     * second set of stores lands one record before the pool (retail). */
    if (level->location_menu.fade != 0) {
        if (level->location_menu.fade >= 0x80) {
            level->location_menu.fade = 0x80;
        }
        g_display_object_animated_pool[level->location_menu.slots.window_a].color.r = level->location_menu.fade;
        g_display_object_animated_pool[level->location_menu.slots.window_a].color.g = level->location_menu.fade;
        g_display_object_animated_pool[level->location_menu.slots.window_a].color.b = level->location_menu.fade;
        g_display_object_animated_pool[level->location_menu.slots.window_b].color.r = level->location_menu.fade;
        g_display_object_animated_pool[level->location_menu.slots.window_b].color.g = level->location_menu.fade;
        g_display_object_animated_pool[level->location_menu.slots.window_b].color.b = level->location_menu.fade;
        if (level->location_menu.fade == 0x80) {
            level->location_menu.fade = 0;
            world_map_backdrop_mode_set(1);
        } else {
            level->location_menu.fade += 7;
        }
        return;
    }

    /* Start the panel thread on the first delay frame, then ignore input until the delay ends. */
    if (level->location_menu.delay != 0) {
        if (level->location_menu.delay == 8) {
            thread_start(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, screen_text_list_thread);
            thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, (s32)&g_window_panel_render_state, 0, 0);
        }
        level->location_menu.delay--;
        return;
    }

    if (g_world_map_flags & WORLD_MAP_FLAG_FADING) {
        return;
    }

    /* A confirm or cancel below closed the panel; act on slots.result once its thread has finished. */
    if ((g_world_map_flags & WORLD_MAP_FLAG_INPUT_BLOCKED)
        && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        world_map_backdrop_mode_set(0);
        g_world_map_flags = (g_world_map_flags | WORLD_MAP_FLAG_INPUT_BLOCKED)
            & ~(WORLD_MAP_FLAG_DRAW_MAP | WORLD_MAP_FLAG_FREEZE_ANIMATION);
        if (level->location_menu.slots.result != 0) {
            menu_stack_cursor_store(MENU_STACK_CURSOR_SLOT_LOCATION);
            selected = &g_window_panel_render_state.selected_index;
            values = g_menu_stack_entry_values;
            value = values[*selected];

            /* World-menu entries are services and deep-dungeon floors; anything else names a map. */
            if ((value & TEXT_ID_SECTION_MASK) == TEXT_ID_WORLD_MENU_BASE) {
                g_world_map_flags ^= WORLD_MAP_FLAG_INPUT_BLOCKED;
                value &= TEXT_ID_ENTRY_MASK;
                switch (value) {
                case TEXT_ID_BAR& TEXT_ID_ENTRY_MASK:
                    level_records_release(&level->location_menu.slots, 0);
                    music_queue_track_start(MUSIC_QUEUE_TRACK(2, MUSIC_BAR));
                    bar_menu_open();
                    return;
                case TEXT_ID_SHOP& TEXT_ID_ENTRY_MASK:
                    level_records_release(&level->location_menu.slots, 0);
                    location_shop_run(0);
                    return;
                case TEXT_ID_SOLDIER_OFFICE& TEXT_ID_ENTRY_MASK:
                    level_records_release(&level->location_menu.slots, 0);
                    location_shop_run(SCREEN_CONTEXT_SOLDIER_OFFICE);
                    return;
                case TEXT_ID_FUR_SHOP& TEXT_ID_ENTRY_MASK:
                    level_records_release(&level->location_menu.slots, 0);
                    location_shop_run(SCREEN_CONTEXT_FUR_SHOP);
                    return;
                default:
                    script_variable_set(EVENT_SCRIPT_VAR_DEEP_DUNGEON_SELECTION,
                        value - (TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST & TEXT_ID_ENTRY_MASK));
                    level_records_release(&level->location_menu.slots, 0);
                    if (location_script_run(
                            g_world_map_marker_state.marker.location_id, LOCATION_SCRIPT_ACTION_DEEP_DUNGEON_ENTRY)
                        != 0) {
                        /* rand() is 15 bits: pick one of the battle's eight ENTDs and an exit below args[2]. */
                        offset = g_location_script_state.args[0] * sizeof(deep_dungeon_battle_t);
                        records = g_deep_dungeon_battles;
                        record = &records[g_location_script_state.args[0]];
                        g_travel_next_map_id = record->map_id;
                        /* Direct ENTD indexing folds the table base and removes a retail addition. */
                        script_variable_set(
                            EVENT_SCRIPT_VAR_CURRENT_ENTD, (&records->entd_ids[0])[((rand() * 8) >> 15) + offset]);
                        script_variable_set(EVENT_SCRIPT_VAR_DEPLOYMENT_SQUAD_COUNT, 1);
                        script_variable_set(EVENT_SCRIPT_VAR_DEPLOYMENT_PRIMARY_SQUAD_ID, record->squad_id);
                        script_variable_set(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_location_script_state.args[1]);
                        script_variable_set(
                            EVENT_SCRIPT_VAR_DEEP_DUNGEON_EXIT, (rand() * g_location_script_state.args[2]) >> 15);
                        g_world_map_flags
                            = (g_world_map_flags | WORLD_MAP_FLAG_DEEP_DUNGEON_BATTLE) ^ WORLD_MAP_FLAG_RUNNING;
                    } else {
                        world_map_fade_start(0, 0x10);
                        music_queue_track_start(MUSIC_QUEUE_TRACK(1, MUSIC_WORLD_MAP));
                    }
                    return;
                }
            }

            /* A map entry runs the row's event and travels to map entry + 1. */
            script_variable_set(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_location_script_state.args[*selected]);
            g_world_map_flags ^= WORLD_MAP_FLAG_RUNNING;
            value = values[*selected] & TEXT_ID_ENTRY_MASK;
            g_travel_next_map_id = value + 1;
            return;
        }

        g_world_map_flags = (g_world_map_flags | WORLD_MAP_FLAG_DRAW_MAP) & ~WORLD_MAP_FLAG_INPUT_BLOCKED;
        menu_stack_cursor_clear(MENU_STACK_CURSOR_SLOT_LOCATION);
        level_records_release(&level->location_menu.slots, 1);
        return;
    }

    /* Cancel only works while the cursor is on a world-menu entry, not a map name. */
    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        value = g_menu_stack_entry_values[g_window_panel_render_state.selected_index];
        if ((value & TEXT_ID_SECTION_MASK) == TEXT_ID_WORLD_MENU_BASE) {
            sound_play_effect(SOUND_SFX_CANCEL);
            level->location_menu.slots.result = 0;
            thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
            g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
        }
    } else if (g_world_map_input_new_button_presses & PSX_PAD_SELECT) {
        /* Help: map names, services and deep-dungeon floors each have their own message range. */
        location_menu_suspend(level);
        value = g_menu_stack_entry_values[g_window_panel_render_state.selected_index];
        /* A map's help is 0x8801 + map, a service's 0x104b on and a floor's 0x1090 on. */
        if ((value & TEXT_ID_SECTION_MASK) == TEXT_ID_MAP_NAME_BASE) {
            menu_stack_message_open((value & TEXT_ID_ENTRY_MASK) + 0x8801, 1);
        } else {
            value &= TEXT_ID_ENTRY_MASK;
            text = value + 0xFEE;
            if (value >= (TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST & TEXT_ID_ENTRY_MASK)) {
                text = value + 0xFA3;
            }
            menu_stack_message_open(text, 1);
        }
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->location_menu.entry_count != 0
        && g_file_cd_state.state == FILE_LOAD_STATE_IDLE) {
        world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
        music_queue_add(MUSIC_QUEUE_COMMAND_FADE_OUT, 0x10);
        music_queue_add(MUSIC_QUEUE_COMMAND_UNLOAD, 2);
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->location_menu.slots.result = 1;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_world_map_flags |= WORLD_MAP_FLAG_INPUT_BLOCKED;
    }
}

/* Pop the current menu level and give back the display records it appended:
 * each of the four slots that is not -1 releases one window or render record
 * and its queue entry. With `restore` the level underneath is resumed. Also
 * unblocks the menu buttons and re-shows the location label. */
static void level_records_release(menu_stack_slot_set_t* slots, s32 restore) {
    g_menu_blocked_buttons = 0;
    if (slots->window_a != -1) {
        g_display_object_animated_count -= 1;
        g_display_object_queue_count -= 1;
    }
    if (slots->render_a != -1) {
        g_display_object_image_count -= 1;
        g_display_object_queue_count -= 1;
    }
    if (slots->window_b != -1) {
        g_display_object_animated_count -= 1;
        g_display_object_queue_count -= 1;
    }
    if (slots->render_b != -1) {
        g_display_object_image_count -= 1;
        g_display_object_queue_count -= 1;
    }
    g_menu_stack_depth -= 1;
    if (restore != 0) {
        menu_stack_dispatch_resume();
    }
    g_world_map_flags &= ~WORLD_MAP_FLAG_HIDE_LOCATION_LABEL;
}

/* At full brightness, run WORLD's shop, soldier office or fur shop screen
 * (shop_screen_run), then reload the war funds, reset the parent
 * level's window, push the location menu again, fade in, blank the backdrop
 * image and restart the world-map music.
 *
 * The local alias for the current location id reproduces the target's
 * saved-register base for its two loads. */
static void location_shop_run(s32 shop_id) {
    s32* selected;
    u32* clear;
    s32 i;

    screen_brightness_set(0x80, 0x80, 0x80);
    music_queue_wait_until_empty();
    shop_screen_run(shop_id);
    g_hud_value = script_variable_get(EVENT_SCRIPT_VAR_WAR_FUNDS);
    world_map_cursor_level_suspend(&g_menu_stack_records[g_menu_stack_depth - 1]);
    selected = &g_world_map_marker_state.marker.location_id;
    g_menu_stack_records[g_menu_stack_depth].panel.entry_count
        = location_menu_entries_build(*selected, g_menu_stack_records[g_menu_stack_depth].panel.entries);
    location_menu_open(*selected);
    world_map_fade_start(WORLD_MAP_FADE_REDRAW_MAP, 0x10);
    world_map_clear_screen_and_set_clip_rect(0);
    /* Blank the backdrop image from the end. */
    i = WORLD_MAP_BACKDROP_WORDS - 1;
    clear = &g_world_map_scratch_buffer[WORLD_MAP_BACKDROP_WORDS - 1];
    for (; i >= 0; i--) {
        *clear = 0;
        clear--;
    }
    music_queue_track_start(MUSIC_QUEUE_TRACK(1, MUSIC_WORLD_MAP));
}

/* Pushes g_text_window on message `text` as
 * MENU_STACK_LEVEL_SCROLLABLE_TEXT: it appends one window record for
 * the header, measures the text, sizes the window around it (at most four
 * rows), initializes the scrollable text state, hides the text render record
 * and the parent level's window until the step shows them, and hides the
 * location label and freezes the map animation. */
void menu_stack_scrollable_text_open(s32 text) {
    s16 width;
    s16 rows;

    text_pixels_measure(&width, &rows, text_entry_find(text));
    if (rows > 4) {
        rows = 4;
    }

    {
        /* Sixteen frame bytes above the measured size that retail reserves
         * and never touches. */
        s32 unused[4];
        s32 index;
        s32 depth;

        index = display_object_animated_append(g_display_object_queue, &g_display_object_queue_count);
        g_menu_stack_records[g_menu_stack_depth].window_index = index;
        g_display_object_animated_pool[index].sequence = DISPLAY_OBJECT_ANIM_CHOICE_PANEL_HEADER;
        g_display_object_animated_pool[index].priority = 3;
        g_display_object_animated_pool[index].anim_counter = 0;
        g_display_object_animated_pool[index].frame_index = 0;
        g_display_object_animated_pool[index].flags |= DISPLAY_OBJECT_HIDDEN;

        g_text_window.text_id = text;
        g_text_window.image_position.x = 0;
        g_text_window.image_position.y = 0x60;
        g_text_window.base.x = -(((width + 24) & 0xFFFC) >> 1);
        g_text_window.base.y = -((rows << 3) + 8);
        g_text_window.text_width = width;
        g_text_window.rows_per_page = rows;
        g_text_window.priority = 3;
        g_text_window.image_coordinate_mode = 0;
        g_text_window.extra_render_index = -1;
        g_text_window.text_substitutions[1] = -1;
        g_text_window.text_substitutions[0] = -1;
        text_window_open(&g_text_window);

        g_display_object_gs_image_pool[g_text_window.render_record_index].flags |= DISPLAY_OBJECT_HIDDEN;
        depth = g_menu_stack_depth;
        g_display_object_animated_pool[g_menu_stack_records[depth].window_index].position.x = g_text_window.base.x + 3;
        g_display_object_animated_pool[g_menu_stack_records[depth].window_index].position.y = g_text_window.base.y - 2;
        g_world_map_flags |= WORLD_MAP_FLAG_HIDE_LOCATION_LABEL | WORLD_MAP_FLAG_FREEZE_ANIMATION;
        g_menu_stack_depth = depth + 1;
        g_display_object_animated_pool[g_menu_stack_records[depth - 1].window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        g_menu_stack_records[depth].scrollable_text._unknown_20 = 0x28;
        g_menu_stack_types[depth] = MENU_STACK_LEVEL_SCROLLABLE_TEXT;
    }
}

/* Menu-level step for the scrollable text window g_text_window.
 * While its render record is hidden, the first frame without a screen fade
 * plays SOUND_SFX_WINDOW_OPEN and shows the header window and the text.
 * Otherwise, once text_window_update reports the window
 * closed, it releases the header window, pops the level, shows the location label
 * and unfreezes the map, shows the parent level's window and resumes the
 * parent level. */
static void scrollable_text_level_update(menu_stack_record_t* level) {
    s32* index = &g_text_window.render_record_index;
    s32 depth;

    if (g_display_object_gs_image_pool[*index].flags & DISPLAY_OBJECT_HIDDEN) {
        if (!(g_world_map_flags & WORLD_MAP_FLAG_FADING)) {
            sound_play_effect(SOUND_SFX_WINDOW_OPEN);
            g_display_object_animated_pool[level->window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
            g_display_object_gs_image_pool[*index].flags &= ~DISPLAY_OBJECT_HIDDEN;
        }
    } else if (!(g_display_object_gs_image_pool[*index].flags & DISPLAY_OBJECT_POP_IN)
        && text_window_update(&g_text_window) == 0) {
        g_display_object_animated_count--;
        g_display_object_queue_count--;
        depth = g_menu_stack_depth;
        g_menu_stack_depth = depth - 1;
        g_world_map_flags &= ~(WORLD_MAP_FLAG_HIDE_LOCATION_LABEL | WORLD_MAP_FLAG_FREEZE_ANIMATION);
        g_display_object_animated_pool[g_menu_stack_records[depth - 2].window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
        menu_stack_dispatch_resume();
    }
}

/* Push the debug "Party number" panel (WORLD_MAP_FLAG_REQUEST_DEBUG_PARTY_NUMBER):
 * a number from 0 to 999, starting at 0. Sibling of
 * variable_edit_open. */
static void party_number_open(void) {
    point32_t origin;
    s32 record_index;

    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS;
    g_menu_stack_ot_offset = 3;
    origin.x = -0x28;
    origin.y = -0x16;
    g_window_panel_value = 0;
    window_number_panel_thread_start(
        WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 999 << 16, TEXT_ID_PARTY_NUMBER_LABEL, &origin);
    record_index = g_menu_stack_depth;
    g_menu_stack_records[record_index].panel.result = 0;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_DEBUG_PARTY_NUMBER;
    g_menu_stack_depth = record_index + 1;
}

/* Step of the debug party number panel: Circle or Cross closes it, and once
 * the panel thread has ended a confirmed number below 0x200 is loaded with
 * encounter_party_init and the roster names are rebuilt.
 *
 * The panel result is 0 while waiting, 1 on confirm and -1 on cancel. */
static void party_number_update(menu_stack_record_t* level) {
    u32 buttons;

    if ((level->panel.result != 0) && (thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0)) {
        g_menu_blocked_buttons = 0;
        g_menu_stack_ot_offset = 1;
        g_menu_stack_depth--;
        if (level->panel.result == 1) {
            if (g_window_panel_value < 0x200) {
                encounter_party_init(g_window_panel_value);
                formation_roster_names_build();
                g_world_map_flags |= WORLD_MAP_FLAG_DRAW_MAP;
            }
        }
        menu_stack_dispatch_resume();
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if ((buttons & PSX_PAD_CROSS) != 0) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = -1;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
    } else if ((buttons & PSX_PAD_CIRCLE) != 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        thread_parameters_set(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
    }
}
