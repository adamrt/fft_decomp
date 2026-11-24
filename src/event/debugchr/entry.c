/* Show a mounted unit's Status screen or run the developer editor with
 * the same status panels and editable working values. */

#include "fft/battle/command.h"
#include "fft/game/ability.h"
#include "fft/game/battle_unit.h"
#include "fft/game/item.h"
#include "fft/game/job.h"
#include "fft/game/skillset.h"
#include "fft/gfx/gfx_primitive.h"
#include "fft/menu/help.h"
#include "fft/menu/menu_gfx.h"
#include "fft/menu/menu_input.h"
#include "fft/menu/menu_list.h"
#include "fft/party/status_screen.h"
#include "fft/party/unit_editor.h"
#include "fft/party/unit_panel.h"
#include "fft/script/script.h"
#include "fft/sys/memory.h"
#include "fft/sys/sound.h"
#include "fft/sys/thread.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Rows of the editor navigation table and its value limits. */
typedef enum unit_editor_field {
    UNIT_EDITOR_FIELD_LEVEL = 0,
    UNIT_EDITOR_FIELD_EXPERIENCE = 1,
    UNIT_EDITOR_FIELD_JOB = 2,
    UNIT_EDITOR_FIELD_BRAVE = 3,
    UNIT_EDITOR_FIELD_FAITH = 4,
    UNIT_EDITOR_FIELD_ZODIAC = 5,
    UNIT_EDITOR_FIELD_MOVE = 6,
    UNIT_EDITOR_FIELD_JUMP = 7,
    UNIT_EDITOR_FIELD_SPEED = 8,
    UNIT_EDITOR_FIELD_RIGHT_HAND = 9,
    UNIT_EDITOR_FIELD_LEFT_HAND = 10,
    UNIT_EDITOR_FIELD_HEAD = 11,
    UNIT_EDITOR_FIELD_BODY = 12,
    UNIT_EDITOR_FIELD_ACCESSORY = 13,
    UNIT_EDITOR_FIELD_PRIMARY_SKILLSET = 14,
    UNIT_EDITOR_FIELD_SECONDARY_SKILLSET = 15,
    UNIT_EDITOR_FIELD_REACTION = 16,
    UNIT_EDITOR_FIELD_SUPPORT = 17,
    UNIT_EDITOR_FIELD_MOVEMENT = 18,
    UNIT_EDITOR_FIELD_HP = 19,
    UNIT_EDITOR_FIELD_MP = 20,
    UNIT_EDITOR_FIELD_CT = 21,
    UNIT_EDITOR_FIELD_COUNT = 22
} unit_editor_field_e;

typedef struct unit_editor_field_range {
    s32 min;
    s32 max;
} unit_editor_field_range_t;
typedef char unit_editor_field_range_size_must_be_8[sizeof(unit_editor_field_range_t) == 8 ? 1 : -1];

/* The 22nd field is CT. Retail copies only the first 21 ranges when changing
 * unit type, leaving the CT row at its original 1..100 range. */
extern unit_editor_field_range_t g_unit_editor_field_ranges[UNIT_EDITOR_FIELD_COUNT];
static const unit_editor_field_range_t s_unit_editor_nonmonster_field_ranges[UNIT_EDITOR_FIELD_COUNT] = {
    { 1, 99 },                                                               /* level */
    { 1, 99 },                                                               /* experience */
    { JOB_ID_SQUIRE_RAMZA_CHAPTER_1, JOB_ID_ARCHAIC_DEMON },                 /* job */
    { 0, 100 },                                                              /* Brave */
    { 0, 100 },                                                              /* Faith */
    { ZODIAC_SIGN_ARIES, ZODIAC_SIGN_SERPENTARIUS },                         /* zodiac */
    { 1, 9 },                                                                /* Move */
    { 1, 9 },                                                                /* Jump */
    { 1, 99 },                                                               /* Speed */
    { ITEM_ID_DAGGER, ITEM_ID_ESCUTCHEON_RARE },                             /* right hand */
    { ITEM_ID_DAGGER, ITEM_ID_ESCUTCHEON_RARE },                             /* left hand */
    { ITEM_ID_LEATHER_HELMET, ITEM_ID_RIBBON },                              /* head */
    { ITEM_ID_LEATHER_ARMOR, ITEM_ID_ROBE_OF_LORDS },                        /* body */
    { ITEM_ID_BATTLE_BOOTS, ITEM_ID_SALTY_RAGE },                            /* accessory */
    { SKILLSET_ID_ATTACK, SKILLSET_ID_MONSTER_VAMPIRE },                     /* primary skillset */
    { SKILLSET_ID_ATTACK, SKILLSET_ID_MONSTER_VAMPIRE },                     /* secondary skillset */
    { ABILITY_ID_REACTION_REGENERATOR, ABILITY_ID_REACTION_HAMEDO },         /* reaction ability */
    { ABILITY_ID_SUPPORT_FIRST, ABILITY_ID_MOVEMENT_FIRST - 1 },             /* support ability */
    { ABILITY_ID_MOVEMENT_MOVE_PLUS_1, ABILITY_ID_MOVEMENT_MOVE_FIND_ITEM }, /* movement ability */
    { 1, 999 },                                                              /* HP */
    { 1, 999 },                                                              /* MP */
    { 1, 100 },                                                              /* CT */
};
static const unit_editor_field_range_t s_unit_editor_monster_field_ranges[UNIT_EDITOR_FIELD_COUNT] = {
    { 1, 99 },                                                   /* level */
    { 1, 99 },                                                   /* experience */
    { JOB_ID_SQUIRE_RAMZA_CHAPTER_1, JOB_ID_ARCHAIC_DEMON },     /* job */
    { 0, 100 },                                                  /* Brave */
    { 0, 100 },                                                  /* Faith */
    { ZODIAC_SIGN_ARIES, ZODIAC_SIGN_SERPENTARIUS },             /* zodiac */
    { 1, 9 },                                                    /* Move */
    { 1, 9 },                                                    /* Jump */
    { 1, 99 },                                                   /* Speed */
    { ITEM_ID_DAGGER, ITEM_ID_ESCUTCHEON_RARE },                 /* right hand */
    { ITEM_ID_DAGGER, ITEM_ID_ESCUTCHEON_RARE },                 /* left hand */
    { ITEM_ID_LEATHER_HELMET, ITEM_ID_RIBBON },                  /* head */
    { ITEM_ID_LEATHER_ARMOR, ITEM_ID_ROBE_OF_LORDS },            /* body */
    { ITEM_ID_BATTLE_BOOTS, ITEM_ID_SALTY_RAGE },                /* accessory */
    { ABILITY_ID_WHITE_MAGIC_CURE, ABILITY_ID_BYBLOS_PARASITE }, /* primary skillset */
    { ABILITY_ID_WHITE_MAGIC_CURE, ABILITY_ID_BYBLOS_PARASITE }, /* secondary skillset */
    { ABILITY_ID_WHITE_MAGIC_CURE, ABILITY_ID_BYBLOS_PARASITE }, /* reaction ability */
    { ABILITY_ID_WHITE_MAGIC_CURE, ABILITY_ID_BYBLOS_PARASITE }, /* support ability */
    { ABILITY_ID_WHITE_MAGIC_CURE, ABILITY_ID_BYBLOS_PARASITE }, /* movement ability */
    { 1, 999 },                                                  /* HP */
    { 1, 999 },                                                  /* MP */
    { 1, 100 },                                                  /* CT */
};

/* The status browser stores battle unit IDs here; the editor reuses the slots as availability flags. */
extern u8 g_unit_editor_unit_slots[BATTLE_UNIT_SLOT_COUNT];
extern help_navigation_record_t g_unit_editor_fields[UNIT_EDITOR_FIELD_COUNT];
static unit_panel_thread_t s_unit_editor_status_display_thread_params;

/* Views of g_unit_panel_selected_unit_status.gauges[i].value (values[3 * i])
 * and gauges[i].max (the *_max 16-bit values): apply_values
 * reaches the values from their own base register and the limits by absolute
 * address, which the member spelling does not reproduce. */
extern s16 g_unit_editor_values[];
extern s16 g_unit_editor_hp_max;
extern s16 g_unit_editor_mp_max;
extern s16 g_unit_editor_ct_max;

static POLY_FT4 s_unit_editor_gfx_render_buffers[2][30] = { 0 };
static POLY_FT4 s_unit_editor_cursor_polys[2] = { 0 };
static POLY_FT4 s_unit_editor_cursor_shade_polys[2] = { 0 };
static help_navigation_record_t* s_unit_editor_field_table = 0;
static help_navigation_record_t* s_unit_editor_current_field = 0;
static s32 s_unit_editor_selected_unit_slot = 0;
static s32 s_unit_editor_field_edit_active = 0;

/* Write the editor's values back to battle unit `unit_id`: HP, MP and CT
 * (each first capped at its maximum), level, experience, Brave, Faith, the
 * zodiac sign (the top nibble of the birthday 16-bit value), move, speed and
 * jump. */
static void apply_values(s32 unit_id) {
    battle_unit_t* unit = battle_unit_get(unit_id);
    s32 zodiac;
    u32 unit_flags;

    if (g_unit_editor_hp_max < g_unit_editor_values[0])
        g_unit_editor_values[0] = g_unit_editor_hp_max;
    if (g_unit_editor_mp_max < g_unit_editor_values[3])
        g_unit_editor_values[3] = g_unit_editor_mp_max;
    if (g_unit_editor_ct_max < g_unit_editor_values[6])
        g_unit_editor_values[6] = g_unit_editor_ct_max;
    unit->hp = g_unit_editor_values[0];
    unit->mp = g_unit_editor_values[3];
    unit->ct = g_unit_editor_values[6];
    unit->level = g_unit_panel_selected_unit_status.level;
    unit->experience = g_unit_panel_selected_unit_status.experience;
    /* 16-bit objects (the loader stores sh); this function reads only the
     * low byte, and the lvalue form emits the target's bare lbu. */
    unit->brave = g_unit_panel_selected_unit.brave;
    unit->faith = g_unit_panel_selected_unit.faith;
    /* Named bitfield assignment narrows the signed 16-bit zodiac load
     * and reorders these stores. */
    zodiac = g_unit_panel_selected_unit.zodiac << 12;
    unit_flags = *(u16*)&unit->birthday & 0x0fff;
    *(u16*)&unit->birthday = unit_flags | zodiac;
    unit->move = g_unit_panel_selected_unit_fields.state.move;
    unit->attributes[UNIT_ATTRIBUTE_SPEED] = g_unit_panel_selected_unit_fields.state.speed;
    unit->jump = g_unit_panel_selected_unit_fields.state.jump;
}

/*
 * Thread task of the unit status screen opened on one battle unit: the
 * read-only use of the DEBUGCHR panels (g_debug_unit_editor_active stays 0).
 *
 * Loads `unit_id` and starts the status banner, numeric and character status
 * panel threads. L1/R1 switch between the unit and its mount partner, Circle
 * opens the status-effect list as the previous thread slot and waits for it
 * (refused for a generic monster with no status), and Cross closes the
 * panels and ends the loop, after which the first unit is loaded again. The
 * menu-building thread holds the button prompts' animation bytes at 1 while
 * it runs, and the panels are dimmed only while a THREAD_TASK_RESUME
 * thread is running as well.
 *
 * `sound_latched` is read before anything assigns it when the menu-building
 * thread is already running on the first frame.
 */
void debugchr_mounted_unit_entrypoint(s32 unit_id) {
    s32 frame;
    s32 one;
    u8* animation;
    POLY_FT4* render_buffers;
    /* A record-base cursor changes the saved address and all three retail offsets. */
    s16* selected_unit;
    u8* animation_second;
    u8* animation_third;
    s32 sound_latched;
    s32 event_mode;
    battle_unit_t* unit;
    s32 input;
    /* Shared by the R1 and L1 blocks: set twice in the loop, the address is
     * not a loop invariant candidate and is rebuilt in each block. */
    s16* status_unit_index;

    thread_current_set_task(THREAD_TASK_MOUNTED_STATUS_SCREEN);
    g_debug_unit_editor_active = 0;
    g_unit_panel_button_prompt_count = 2;

    /* Slot 0 is this unit and slot 1 its mount partner, between which L1 and R1 toggle. The store to slot 1 makes
     * the target re-read slot 0 for each of the three fields. */
    unit = battle_unit_get(unit_id);
    g_unit_editor_unit_slots[0] = unit_id;
    g_unit_editor_unit_slots[1] = unit->mount_info & UNIT_MOUNT_INFO_PARTNER_ID_MASK;
    g_unit_panel_selected_unit.battle_unit_id = g_unit_editor_unit_slots[0];
    g_unit_panel_selected_unit.name_id = g_unit_editor_unit_slots[0];
    g_unit_panel_selected_unit_status.unit_index = g_unit_editor_unit_slots[0];
    unit_panel_load_selected();
    s_unit_editor_selected_unit_slot = 0;

    /* The range copy is repeated in both arms: jump2 cross-jumps the
     * identical tails only after sched2, so the join keeps the store ahead of
     * the thread-setup constant loads. */
    if (g_unit_panel_selected_unit_fields.state.generic_monster != 0) {
        mem_copy_bytes_nonempty(
            g_unit_editor_field_ranges, s_unit_editor_monster_field_ranges, 21 * sizeof(unit_editor_field_range_t));
        g_unit_editor_fields[UNIT_EDITOR_FIELD_SECONDARY_SKILLSET].destination[0] = UNIT_EDITOR_FIELD_PRIMARY_SKILLSET;
    } else {
        mem_copy_bytes_nonempty(
            g_unit_editor_field_ranges, s_unit_editor_nonmonster_field_ranges, 21 * sizeof(unit_editor_field_range_t));
        g_unit_editor_fields[UNIT_EDITOR_FIELD_SECONDARY_SKILLSET].destination[0] = UNIT_EDITOR_FIELD_PRIMARY_SKILLSET;
    }

    s_unit_editor_status_display_thread_params.style = 0;
    g_unit_panel_numbers_thread_params.style = 0;
    g_unit_panel_character_status_frame_config.style = 0;
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, unit_panel_banner_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, &s_unit_editor_status_display_thread_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_NUMERIC_PANEL, unit_panel_numbers_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_PANEL, &g_unit_panel_numbers_thread_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, unit_panel_equipment_ability_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, &g_unit_panel_character_status_frame_config, 0, 0);

    frame = 0;
    render_buffers = s_unit_editor_gfx_render_buffers[0];
    one = 1;
    animation = g_unit_panel_button_prompt_animation;
    animation_second = animation + 1;
    animation_third = animation + 2;
    selected_unit = &g_unit_panel_selected_unit.battle_unit_id;
    g_menu_modal_open = 0;

    for (;; frame++) {
        thread_yield();
        g_menu_input_controller = script_get_controller_input_pointer(0);
        {
            s32 render_offset;
            render_offset = (frame & 1) * sizeof(s_unit_editor_gfx_render_buffers[0]);
            /* Typed polygon indexing reverses the ADDU operands. */
            unit_panel_build_button_prompts((POLY_FT4*)(render_offset + (s32)render_buffers));
        }

        if (thread_is_running_asm(THREAD_SLOT_MENU_BUILDING)) {
            /* `one` doubles as SOUND_SFX_CONFIRM, played once per menu-building run. */
            if (sound_latched == 0) {
                g_sound_pending_sfx_id = one;
                sound_latched = 1;
            }
            if (thread_find_task(THREAD_TASK_RESUME) != 0) {
                s_unit_editor_status_display_thread_params.style = one;
                g_unit_panel_numbers_thread_params.style = one;
                g_unit_panel_character_status_frame_config.style = one;
            } else {
                s_unit_editor_status_display_thread_params.style = 0;
                g_unit_panel_numbers_thread_params.style = 0;
                g_unit_panel_character_status_frame_config.style = 0;
            }
            animation[0] = one;
            animation[1] = one;
            animation[2] = one;
            continue;
        }

        event_mode = g_menu_input_lock_mode;
        sound_latched = 0;
        animation[0] = 0;
        animation[1] = 0;
        animation[2] = 0;

        if (event_mode == 0 && g_help_open == 0 && !thread_is_previous_running()) {
            input = *g_menu_input_controller;
            if (input & PSX_PAD_CROSS) {
                thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
                thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_PANEL, 0, 0, 1);
                thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
                g_sound_pending_sfx_id = SOUND_SFX_CANCEL;
                break;
            }

            if ((input & PSX_PAD_CIRCLE) && !thread_is_running_asm(THREAD_SLOT_MENU_BUILDING)
                && (battle_unit_has_any_current_status(g_unit_panel_selected_unit.battle_unit_id)
                    || g_unit_panel_selected_unit_fields.state.generic_monster == 0)) {
                s_unit_editor_status_display_thread_params.style = one;
                g_unit_panel_numbers_thread_params.style = one;
                g_unit_panel_character_status_frame_config.style = one;
                thread_start(g_thread_current_id - 1, status_screen_status_list_thread);
                thread_set_parameters(
                    g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[COMMAND_MENU_STATUS_LIST], 0, 0);
                animation[0] = one;
                animation_second[0] = one;
                animation_third[0] = one;

                /* Keep the button prompts drawn until the status list thread in the previous slot exits. */
                for (;;) {
                    thread_yield();
                    {
                        s32 render_offset;
                        render_offset = (frame & 1) * sizeof(s_unit_editor_gfx_render_buffers[0]);
                        /* Typed polygon indexing reverses the ADDU operands. */
                        unit_panel_build_button_prompts((POLY_FT4*)(render_offset + (s32)render_buffers));
                    }
                    if (!thread_is_running_asm(g_thread_current_id - 1)) {
                        break;
                    }
                    frame++;
                }

                animation[0] = 0;
                animation[1] = 0;
                animation[2] = 0;
                s_unit_editor_status_display_thread_params.style = 0;
                g_unit_panel_numbers_thread_params.style = 0;
                g_unit_panel_character_status_frame_config.style = 0;
            }
        }

        if (*g_menu_input_controller & PSX_PAD_R1) {
            s32 slot;
            u8* chosen_unit;
            unit_panel_unit_t* unit_data;

            slot = s_unit_editor_selected_unit_slot;
            g_sound_pending_sfx_id = SOUND_SFX_PAGE_SWITCH;
            g_unit_panel_button_prompt_animation[1] = 7;
            slot ^= 1;
            s_unit_editor_selected_unit_slot = slot;

            chosen_unit = &g_unit_editor_unit_slots[slot];
            *selected_unit = *chosen_unit;
            unit_data = (unit_panel_unit_t*)((u8*)selected_unit - offsetof(unit_panel_unit_t, battle_unit_id));
            unit_data->name_id = *chosen_unit;
            status_unit_index = &g_unit_panel_selected_unit_status.unit_index;
            *status_unit_index = *chosen_unit;
            unit_panel_load_selected();
            unit_panel_copy_billboard_in(
                &g_unit_panel_selected_unit_status, unit_data, &g_unit_panel_selected_unit_fields);

            /* The status display takes its redraw request as the portrait wipe direction; L1 passes 2. */
            s_unit_editor_status_display_thread_params.redraw_request = one;
            g_unit_panel_numbers_thread_params.redraw_request = one;
            g_unit_panel_character_status_frame_config.redraw_request = one;
        }

        if (*g_menu_input_controller & PSX_PAD_L1) {
            s32 slot;
            u8* chosen_unit;
            unit_panel_unit_t* unit_data;

            slot = s_unit_editor_selected_unit_slot;
            g_sound_pending_sfx_id = SOUND_SFX_PAGE_SWITCH;
            g_unit_panel_button_prompt_animation[0] = 7;
            slot ^= 1;
            s_unit_editor_selected_unit_slot = slot;

            chosen_unit = &g_unit_editor_unit_slots[slot];
            *selected_unit = *chosen_unit;
            unit_data = (unit_panel_unit_t*)((u8*)selected_unit - offsetof(unit_panel_unit_t, battle_unit_id));
            unit_data->name_id = *chosen_unit;
            status_unit_index = &g_unit_panel_selected_unit_status.unit_index;
            *status_unit_index = *chosen_unit;
            unit_panel_load_selected();
            unit_panel_copy_billboard_in(
                &g_unit_panel_selected_unit_status, unit_data, &g_unit_panel_selected_unit_fields);

            /* 2 runs the portrait wipe in the opposite direction from R1's. */
            s_unit_editor_status_display_thread_params.redraw_request = 2;
            g_unit_panel_numbers_thread_params.redraw_request = one;
            g_unit_panel_character_status_frame_config.redraw_request = one;
        }
    }

    /* Leave the panels on the unit the thread was started with. */
    g_unit_panel_selected_unit.battle_unit_id = g_unit_editor_unit_slots[0];
    g_unit_panel_selected_unit.name_id = g_unit_editor_unit_slots[0];
    g_unit_panel_selected_unit_status.unit_index = g_unit_editor_unit_slots[0];
    unit_panel_load_selected();
    unit_panel_copy_billboard_in(
        &g_unit_panel_selected_unit_status, &g_unit_panel_selected_unit, &g_unit_panel_selected_unit_fields);
}

/* Ordinary indexing changes the ADDU operand order at the primitive bases. */
#define PRIM_AT(base, index) ((POLY_FT4*)((s32)((index) * sizeof(POLY_FT4)) + (s32)(base)))

/* Run the debug unit editor on the battle units present on the field.
 *
 * It opens the status, numeric and character panels and alternates between two
 * modes. Unit selection: R1 and L1 page through the selectable units, CIRCLE enters the
 * field editor and CROSS closes the panels and returns. Field editor: the D-pad follows
 * each field's links to its neighbors, CIRCLE starts or commits an edit and CROSS commits
 * one or goes back to unit selection. While editing, RIGHT/UP and LEFT/DOWN step the value
 * (by 10 while L1 is held) within the field's range, TRIANGLE clears equipment slots, and every change is written back
 * to the battle unit. Each level step also applies one level's stat growth or its reversal. */
void debugchr_editor_entrypoint(void) {
    s32 battle_unit_id;
    help_navigation_record_t* field;
    u32* input;
    POLY_FT4* upright_cursor;
    POLY_FT4* upright_shadow;
    POLY_FT4* cursor_prims;
    POLY_FT4* upright_edit_cursor;
    POLY_FT4* upright_edit_shadow;
    POLY_FT4* cursor;
    POLY_FT4* shadow;
    POLY_FT4* edit_cursor;
    POLY_FT4* edit_shadow;
    u32 buttons;
    u32 keys;
    u32 nav_keys;
    s32 i;
    s32 row;
    s32 steps;
    s32 direction;
    s32 bob_y;
    s32 bob_x;
    u8 uv_lo;
    u8 uv_v_hi;
    u8 uv_v_far;
    u8 uv_h_hi;
    u8 uv_h_far;
    u8 uv_h_mid;
    u8 uv_s_hi;
    u8 uv_s_far;
    u8 uv_s_mid;
    u8 primary_skillset_field;
    s16* selected_battle_unit_id;
    u16 unit_index;
    s32 clearable;
    s32 span;
    s32 buffer;
    s32* max_limit;
    s32* min_limit;
    s32* up_min;
    s32* up_max;
    s32* up_clamp_max;
    s32* down_min;
    s32* down_max;
    s16* value;
    s16* down_value;
    s16 current;
    s16 current_up;
    s16 current_down;
    s32 x;
    s32 y;
    s32 shadow_bob_x;
    s32 cursor_x;

    g_debug_unit_editor_active = 1;
    g_unit_panel_button_prompt_count = 2;

    /* A slot is selectable when the lookup of its ENTD unit id returns that same slot: a present unit,
     * the first one with that id. */
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        battle_unit_find_by_unit_id(battle_unit_get(i)->unit_id, &battle_unit_id);
        if (battle_unit_id >= 0 && i == battle_unit_id) {
            g_unit_editor_unit_slots[i] = 1;
        } else {
            g_unit_editor_unit_slots[i] = 0;
        }
    }

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (g_unit_editor_unit_slots[i] != 0) {
            g_unit_panel_selected_unit.battle_unit_id = i;
            g_unit_panel_selected_unit.name_id = i;
            g_unit_panel_selected_unit_status.unit_index = i;
            unit_panel_load_selected();
            s_unit_editor_selected_unit_slot = i;
            break;
        }
    }

    /* Retail reloads the same unit and resets the slot to 0, even when the first selectable unit is a
     * later slot. */
    unit_panel_load_selected();
    s_unit_editor_selected_unit_slot = 0;
    if (g_unit_panel_selected_unit_fields.state.generic_monster != 0) {
        mem_copy_bytes_nonempty(
            g_unit_editor_field_ranges, s_unit_editor_monster_field_ranges, 21 * sizeof(unit_editor_field_range_t));
        g_unit_editor_fields[UNIT_EDITOR_FIELD_SECONDARY_SKILLSET].destination[0] = UNIT_EDITOR_FIELD_PRIMARY_SKILLSET;
    } else {
        mem_copy_bytes_nonempty(
            g_unit_editor_field_ranges, s_unit_editor_nonmonster_field_ranges, 21 * sizeof(unit_editor_field_range_t));
        g_unit_editor_fields[UNIT_EDITOR_FIELD_SECONDARY_SKILLSET].destination[0] = UNIT_EDITOR_FIELD_PRIMARY_SKILLSET;
    }

    s_unit_editor_status_display_thread_params.style = 0;
    g_unit_panel_numbers_thread_params.style = 0;
    g_unit_panel_character_status_frame_config.style = 0;
    thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, unit_panel_banner_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)&s_unit_editor_status_display_thread_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_NUMERIC_PANEL, unit_panel_numbers_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_PANEL, (s32)&g_unit_panel_numbers_thread_params, 0, 0);
    thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, unit_panel_equipment_ability_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)&g_unit_panel_character_status_frame_config, 0, 0);

    row = UNIT_EDITOR_FIELD_LEVEL;
    /* The unit menu and the field editor re-enter each other by label: an
       enclosing loop's notes reweight the allocation of the whole body. */
loop_outer:
    i = 0;
    /* name_id is five 16-bit values before battle_unit_id; record access changes the store displacements. */
    selected_battle_unit_id = &g_unit_panel_selected_unit.battle_unit_id;
    for (;;) {
        primary_skillset_field = UNIT_EDITOR_FIELD_PRIMARY_SKILLSET;
        /* Removing the tied use changes the marker and menu-pointer allocation. */
        __asm__("" : "=r"(primary_skillset_field) : "0"(primary_skillset_field));
        thread_yield();
        unit_panel_build_button_prompts(s_unit_editor_gfx_render_buffers[i & 1]);

        input = script_get_controller_input_pointer(0);
        g_menu_input_controller = input;
        buttons = *input;
        if (buttons & PSX_PAD_CIRCLE) {
            goto editor;
        }
        if (buttons & PSX_PAD_CROSS) {
            thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
            thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_PANEL, 0, 0, 1);
            thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
            return;
        }

        if (buttons & PSX_PAD_R1) {
            g_unit_panel_button_prompt_animation[1] = 7;
            /* Label loops: a do-while's loop notes change the allocation. */
        next_unit:
            s_unit_editor_selected_unit_slot++;
            if (s_unit_editor_selected_unit_slot == BATTLE_UNIT_SLOT_COUNT) {
                s_unit_editor_selected_unit_slot = 0;
            }
            if (g_unit_editor_unit_slots[s_unit_editor_selected_unit_slot] == 0) {
                goto next_unit;
            }

            unit_index = s_unit_editor_selected_unit_slot;
            selected_battle_unit_id[0] = unit_index;
            selected_battle_unit_id[-5] = unit_index;
            g_unit_panel_selected_unit_status.unit_index = unit_index;
            unit_panel_load_selected();
            if (g_unit_panel_selected_unit_fields.state.generic_monster != 0) {
                mem_copy_bytes_nonempty(g_unit_editor_field_ranges, s_unit_editor_monster_field_ranges,
                    21 * sizeof(unit_editor_field_range_t));
            } else {
                mem_copy_bytes_nonempty(g_unit_editor_field_ranges, s_unit_editor_nonmonster_field_ranges,
                    21 * sizeof(unit_editor_field_range_t));
            }
            g_unit_editor_fields[UNIT_EDITOR_FIELD_SECONDARY_SKILLSET].destination[0] = primary_skillset_field;

            /* The status display takes its redraw request as the portrait wipe direction; L1 passes 2. */
            s_unit_editor_status_display_thread_params.redraw_request = 1;
            g_unit_panel_numbers_thread_params.redraw_request = 1;
            g_unit_panel_character_status_frame_config.redraw_request = 1;
        }

        if (*g_menu_input_controller & PSX_PAD_L1) {
            g_unit_panel_button_prompt_animation[0] = 7;
        prev_unit:
            s_unit_editor_selected_unit_slot--;
            if (s_unit_editor_selected_unit_slot == -1) {
                s_unit_editor_selected_unit_slot = BATTLE_UNIT_SLOT_COUNT - 1;
            }
            if (g_unit_editor_unit_slots[s_unit_editor_selected_unit_slot] == 0) {
                goto prev_unit;
            }

            unit_index = s_unit_editor_selected_unit_slot;
            selected_battle_unit_id[0] = unit_index;
            selected_battle_unit_id[-5] = unit_index;
            g_unit_panel_selected_unit_status.unit_index = unit_index;
            unit_panel_load_selected();
            if (g_unit_panel_selected_unit_fields.state.generic_monster != 0) {
                mem_copy_bytes_nonempty(g_unit_editor_field_ranges, s_unit_editor_monster_field_ranges,
                    21 * sizeof(unit_editor_field_range_t));
            } else {
                mem_copy_bytes_nonempty(g_unit_editor_field_ranges, s_unit_editor_nonmonster_field_ranges,
                    21 * sizeof(unit_editor_field_range_t));
            }
            g_unit_editor_fields[UNIT_EDITOR_FIELD_SECONDARY_SKILLSET].destination[0] = primary_skillset_field;
            /* Keeps the `li v0,2` for the redraw store below the byte store. */
            __asm__("");
            /* 2 runs the portrait wipe in the opposite direction from R1's. */
            s_unit_editor_status_display_thread_params.redraw_request = 2;
            g_unit_panel_numbers_thread_params.redraw_request = 1;
            g_unit_panel_character_status_frame_config.redraw_request = 1;
        }

        i++;
    }

editor:
    for (i = 0; i < 2; i++) {
        menu_init_default_poly_ft4(&s_unit_editor_cursor_polys[i]);
        SetSemiTrans(&s_unit_editor_cursor_polys[i], 0);
        s_unit_editor_cursor_polys[i].tpage = GetTPage(0, 0, 0x3C0, 0x100);
        s_unit_editor_cursor_polys[i].clut = 0x7D7C;
        menu_init_default_poly_ft4(&s_unit_editor_cursor_shade_polys[i]);
        SetSemiTrans(&s_unit_editor_cursor_shade_polys[i], 1);
        s_unit_editor_cursor_shade_polys[i].tpage = GetTPage(0, 2, 0x3C0, 0x100);
        s_unit_editor_cursor_shade_polys[i].clut = 0x7DBC;
    }

    s_unit_editor_field_table = g_unit_editor_fields;
    cursor_prims = s_unit_editor_cursor_polys;
    i = 0;
    thread_yield();
    for (;;) {
        field = &s_unit_editor_field_table[row];
        x = field->x;
        y = field->y + 8;
        s_unit_editor_current_field = field;

        /* Outside edit mode Cross returns to unit selection; while editing, Cross and Circle both commit the
         * value. */
        keys = *g_menu_input_controller;
        if (keys & PSX_PAD_CROSS) {
            if (s_unit_editor_field_edit_active != 1) {
                goto loop_outer;
            }
            s_unit_editor_field_edit_active = 0;
            apply_values(s_unit_editor_selected_unit_slot);
            unit_panel_load_selected();
        } else if (keys & PSX_PAD_CIRCLE) {
            if (s_unit_editor_field_edit_active == 1) {
                s_unit_editor_field_edit_active = 0;
                apply_values(s_unit_editor_selected_unit_slot);
                unit_panel_load_selected();
            } else {
                s_unit_editor_field_edit_active = 1;
            }
        }

        /* Triangle clears an equipment slot while editing. */
        if (s_unit_editor_field_edit_active != 0 && (u32)(row - UNIT_EDITOR_FIELD_RIGHT_HAND) < 5
            && (*g_menu_input_controller & PSX_PAD_TRIANGLE)) {
            *s_unit_editor_current_field->value = 0;
            apply_values(s_unit_editor_selected_unit_slot);
            unit_panel_load_selected();
            s_unit_editor_status_display_thread_params.redraw_request = 1;
            g_unit_panel_numbers_thread_params.redraw_request = 1;
            g_unit_panel_character_status_frame_config.redraw_request = 1;
        }

        /* Outside edit mode the D-pad follows the field's up, down, right and left links; while editing it
         * steps the value. */
        nav_keys = *g_menu_input_controller;
        if (nav_keys & PSX_PAD_DPAD_MASK) {
            if (s_unit_editor_field_edit_active == 0) {
                /* Removing the wrapper changes direction and input-mask allocation. */
                do {
                    direction = 0;
                } while (0);
                if (!(nav_keys & PSX_PAD_UP)) {
                    if (nav_keys & PSX_PAD_DOWN) {
                        direction = 1;
                    } else {
                        direction = 3;
                        if (nav_keys & PSX_PAD_RIGHT) {
                            direction = 2;
                        }
                    }
                }
                row = s_unit_editor_current_field->destination[direction];
            } else {
                if ((nav_keys & PSX_PAD_RIGHT) || (nav_keys & PSX_PAD_UP)) {
                    /* A value outside its range first snaps to one below the maximum (even from below the minimum),
                     * so stepping up lands on the maximum. */
                    value = s_unit_editor_current_field->value;
                    current = *value;
                    /* Shared pointers or a combined range test change address materialization. */
                    up_min = &g_unit_editor_field_ranges[row].min;
                    if (current < *up_min) {
                        goto clamp_hi;
                    }
                    up_max = &g_unit_editor_field_ranges[row].max;
                    if (*up_max < current) {
                    clamp_hi:
                        up_clamp_max = &g_unit_editor_field_ranges[row].max;
                        *value = *up_clamp_max - 1;
                    }

                    /* PadRead gives the held buttons: holding L1 steps the value by 10. */
                    if (PadRead(1) & PSX_PAD_L1) {
                        steps = 10;
                    } else {
                        steps = 1;
                    }

                    /* `i` is also the frame counter, so each step resets the cursor's buffer index and bob phase. */
                    for (i = 0; i < steps; i++) {
                        max_limit = &g_unit_editor_field_ranges[row].max;
                        span = row - UNIT_EDITOR_FIELD_RIGHT_HAND;
                        clearable = span < 5U;
                        /* The native loop computes this range test without reading its result. */
                        __asm__("" : : "r"(clearable));
                        value = s_unit_editor_current_field->value;
                        current_up = *value;
                        if (current_up < *max_limit) {
                            *value = current_up + 1;
                            /* Growth depends on the level, so it is applied with the old
                             * level written back before the new value is restored. */
                            if (row == UNIT_EDITOR_FIELD_LEVEL) {
                                *s_unit_editor_current_field->value -= 1;
                                apply_values(s_unit_editor_selected_unit_slot);
                                battle_unit_apply_level_growth(battle_unit_get(s_unit_editor_selected_unit_slot), 0);
                                *s_unit_editor_current_field->value += 1;
                            }
                            apply_values(s_unit_editor_selected_unit_slot);
                            unit_panel_load_selected();
                        }
                    }
                } else {
                    /* Likewise a value outside its range snaps to one above the minimum. */
                    down_value = s_unit_editor_current_field->value;
                    current = *down_value;
                    down_min = &g_unit_editor_field_ranges[row].min;
                    if (current < *down_min) {
                        goto clamp_lo;
                    }
                    down_max = &g_unit_editor_field_ranges[row].max;
                    if (*down_max < current) {
                    clamp_lo:
                        *down_value = *down_min + 1;
                    }

                    if (PadRead(1) & PSX_PAD_L1) {
                        steps = 10;
                    } else {
                        steps = 1;
                    }

                    for (i = 0; i < steps; i++) {
                        min_limit = &g_unit_editor_field_ranges[row].min;
                        span = row - UNIT_EDITOR_FIELD_RIGHT_HAND;
                        clearable = span < 5U;
                        /* The native loop computes this range test without reading its result. */
                        __asm__("" : : "r"(clearable));
                        value = s_unit_editor_current_field->value;
                        current_down = *value;
                        if (*min_limit < current_down) {
                            *value = current_down - 1;
                            if (row == UNIT_EDITOR_FIELD_LEVEL) {
                                *s_unit_editor_current_field->value += 1;
                                apply_values(s_unit_editor_selected_unit_slot);
                                battle_unit_apply_level_growth(battle_unit_get(s_unit_editor_selected_unit_slot), 1);
                                *s_unit_editor_current_field->value -= 1;
                            }
                            apply_values(s_unit_editor_selected_unit_slot);
                            unit_panel_load_selected();
                        }
                    }
                }

                s_unit_editor_status_display_thread_params.redraw_request = 1;
                g_unit_panel_numbers_thread_params.redraw_request = 1;
                g_unit_panel_character_status_frame_config.redraw_request = 1;
            }
        }

        /* The cursor bobs with the frame count unless a value is being edited, which also switches it to
         * the 0x7E7C palette. */
        buffer = i & 1;
        if (s_unit_editor_current_field->vertical_cursor != 0) {
            /* The upright cursor is the same texture cell transposed. */
            uv_lo = 0xA8;
            s_unit_editor_cursor_polys[buffer].u0 = uv_lo;
            s_unit_editor_cursor_polys[buffer].u1 = uv_lo;
            uv_v_hi = 0xB8;
            s_unit_editor_cursor_polys[buffer].u2 = uv_v_hi;
            s_unit_editor_cursor_polys[buffer].u3 = uv_v_hi;
            s_unit_editor_cursor_polys[buffer].v0 = 0;
            s_unit_editor_cursor_polys[buffer].v1 = 0x10;
            s_unit_editor_cursor_polys[buffer].v2 = 0;
            s_unit_editor_cursor_polys[buffer].v3 = 0x10;

            uv_lo = 0xB8;
            s_unit_editor_cursor_shade_polys[buffer].u0 = uv_lo;
            s_unit_editor_cursor_shade_polys[buffer].u1 = uv_lo;
            uv_v_far = 0xC8;
            s_unit_editor_cursor_shade_polys[buffer].u2 = uv_v_far;
            s_unit_editor_cursor_shade_polys[buffer].u3 = uv_v_far;
            s_unit_editor_cursor_shade_polys[buffer].v0 = 0;
            s_unit_editor_cursor_shade_polys[buffer].v1 = 0x10;
            s_unit_editor_cursor_shade_polys[buffer].v2 = 0;
            s_unit_editor_cursor_shade_polys[buffer].v3 = 0x10;

            if (s_unit_editor_field_edit_active == 0) {
                upright_cursor = PRIM_AT(cursor_prims, buffer);
                upright_shadow = &s_unit_editor_cursor_shade_polys[buffer];
                bob_y = y - (((i * g_script_event_speed) >> 4) & 3);
                setXY4(upright_cursor, x, bob_y, x + 16, bob_y, x, bob_y + 16, x + 16, bob_y + 16);
                setXY4(upright_shadow, x + 2, bob_y + 2, x + 18, bob_y + 2, x + 2, bob_y + 18, x + 18, bob_y + 18);
                s_unit_editor_cursor_polys[buffer].clut = 0x7D7C;
            } else {
                upright_edit_cursor = PRIM_AT(cursor_prims, buffer);
                upright_edit_shadow = &s_unit_editor_cursor_shade_polys[buffer];
                setXY4(upright_edit_cursor, x, y, x + 16, y, x, y + 16, x + 16, y + 16);
                setXY4(upright_edit_shadow, x + 2, y + 2, x + 18, y + 2, x + 2, y + 18, x + 18, y + 18);
                s_unit_editor_cursor_polys[buffer].clut = 0x7E7C;
            }
        } else if (s_unit_editor_field_edit_active == 0) {
            cursor = PRIM_AT(cursor_prims, buffer);
            uv_lo = 0xA8;
            cursor->u0 = uv_lo;
            uv_lo = 0xB8;
            /* Removing the tied use eliminates two native instructions. */
            __asm__("" : "=r"(uv_lo) : "0"(uv_lo));
            cursor->v0 = 0;
            uv_h_hi = 0xB8;
            cursor->u1 = uv_h_hi;
            shadow = &s_unit_editor_cursor_shade_polys[buffer];
            cursor->v1 = 0;
            uv_h_mid = 0xA8;
            cursor->u2 = uv_h_mid;
            cursor->v2 = 0x10;
            cursor->u3 = uv_h_hi;
            cursor->v3 = 0x10;
            shadow->u0 = uv_lo;
            shadow->v0 = 0;
            uv_h_far = 0xC8;
            shadow->u1 = uv_h_far;
            shadow->v1 = 0;
            shadow->u2 = uv_h_hi;
            shadow->v2 = 0x10;
            shadow->u3 = uv_h_far;
            shadow->v3 = 0x10;

            bob_x = ((i * g_script_event_speed) >> 4) & 3;
            cursor_x = x - bob_x;
            setXY4(cursor, cursor_x, y, cursor_x + 16, y, cursor_x, y + 16, cursor_x + 16, y + 16);
            /* A separate temporary schedules the decrement after the
             * cursor's `cursor_x + 0x10` sums; `bob_x -= 2` would move it ahead. */
            shadow_bob_x = bob_x - 2;
            setXY4(shadow, x - shadow_bob_x, y + 2, (x - shadow_bob_x) + 16, y + 2, x - shadow_bob_x, y + 18,
                (x - shadow_bob_x) + 16, y + 18);
            s_unit_editor_cursor_polys[buffer].clut = 0x7D7C;
        } else {
            edit_cursor = PRIM_AT(cursor_prims, buffer);
            edit_shadow = &s_unit_editor_cursor_shade_polys[buffer];
            uv_lo = 0xA8;
            edit_cursor->u0 = uv_lo;
            uv_lo = 0xB8;
            /* Removing the tied use eliminates two native instructions. */
            __asm__("" : "=r"(uv_lo) : "0"(uv_lo));
            edit_cursor->v0 = 0;
            uv_s_hi = 0xB8;
            edit_cursor->u1 = uv_s_hi;
            edit_cursor->v1 = 0;
            uv_s_mid = 0xA8;
            edit_cursor->u2 = uv_s_mid;
            edit_cursor->v2 = 0x10;
            edit_cursor->u3 = uv_s_hi;
            edit_cursor->v3 = 0x10;
            edit_shadow->u0 = uv_lo;
            edit_shadow->v0 = 0;
            uv_s_far = 0xC8;
            edit_shadow->u1 = uv_s_far;
            edit_shadow->v1 = 0;
            edit_shadow->u2 = uv_s_hi;
            edit_shadow->v2 = 0x10;
            edit_shadow->u3 = uv_s_far;
            edit_shadow->v3 = 0x10;

            setXY4(edit_cursor, x, y, x + 16, y, x, y + 16, x + 16, y + 16);
            setXY4(edit_shadow, x + 2, y + 2, x + 18, y + 2, x + 2, y + 18, x + 18, y + 18);
            s_unit_editor_cursor_polys[buffer].clut = 0x7E7C;
        }

        gfx_draw_or_append_gpu_primitive(PRIM_AT(cursor_prims, buffer));
        gfx_draw_or_append_gpu_primitive(&s_unit_editor_cursor_shade_polys[buffer]);
        thread_wait_frames(1);
        i++;
    }
}
